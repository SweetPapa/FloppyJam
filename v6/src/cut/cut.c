#include "cut.h"
#include "content/content.h"
#include "scene/scene.h"
#include "flags/flags.h"
#include "art/artkit.h"
#include "audio/synth.h"
#include "save/save.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_CMD    220
#define MAX_ACTORS 8
#define MAX_PROPS  12

typedef struct {
    char verb[12];
    char a[64];
    char b[TEXT_MAX];      /* a spoken line, whole */
    float f[5];
    int   nf;
} Cmd;

typedef struct {
    char  key[24];
    int   npc;
    float x, y, x0, y0, tx, ty;
    float mt, mdur;
    int   emote;
    bool  used;
} CActor;

typedef struct { int id; float x, y, scale; } Prop;

static Cmd   g_cmd[MAX_CMD];
static int   g_ncmd, g_ip;
static bool  g_active;
static float g_wait;
static float g_time;

static char  g_bg[32] = "square";
static CActor g_act[MAX_ACTORS];
static int    g_nact;
static Prop   g_prop[MAX_PROPS];
static int    g_nprop;

static char  g_say[TEXT_MAX];
static int   g_say_who = -1;
static float g_say_reveal;
static float g_say_hold;
static float g_blip_acc;
/* same paging contract as dialogue: a line longer than the panel is read a
 * page at a time, never clipped. Set by cut_draw, read by cut_update. */
static int   g_page, g_page_next = -1, g_page_len;

static char  g_title_a[64], g_title_b[64];
static float g_title_t;

/* camera / fade tweens */
static float g_cam_x0, g_cam_y0, g_cam_x1, g_cam_y1, g_cam_t, g_cam_dur;
static float g_fade_from, g_fade_to, g_fade_t, g_fade_dur;
static float g_hue_t, g_hue_dur;
static int   g_hue_target;

bool cut_active(void) { return g_active; }

static int sfx_by_name(const char *n)
{
    static const char *names[SFX_COUNT] = {
        "tick", "click", "page", "blip", "chime", "clunk", "solve", "deduce",
        "feather", "sparkle", "door", "step", "mend"
    };
    for (int i = 0; i < SFX_COUNT; i++) if (eq(names[i], n)) return i;
    return SFX_TICK;
}

static int mood_by_name(const char *n)
{
    static const char *names[MOOD_COUNT] = {
        "MOOD_SILENT", "MOOD_TOWN", "MOOD_HARBOR", "MOOD_MARKET", "MOOD_QUARTER",
        "MOOD_GARDEN", "MOOD_TOWER", "MOOD_PUZZLE", "MOOD_BOARD", "MOOD_SAD",
        "MOOD_FESTIVAL"
    };
    for (int i = 0; i < MOOD_COUNT; i++) if (eq(names[i], n)) return i;
    return MOOD_TOWN;
}

static int doodle_by_name(const char *n)
{
    for (int i = 0; i < D_COUNT; i++) if (eq(doodle_name(i), n)) return i;
    return D_STAR;
}

static CActor *actor_get(const char *key, bool create)
{
    for (int i = 0; i < g_nact; i++)
        if (g_act[i].used && eq(g_act[i].key, key)) return &g_act[i];
    if (!create || g_nact >= MAX_ACTORS) return NULL;
    CActor *a = &g_act[g_nact++];
    memset(a, 0, sizeof *a);
    snprintf(a->key, sizeof a->key, "%s", key);
    a->npc = npc_by_name(key);
    a->used = true;
    a->x = a->y = a->tx = a->ty = 0;
    a->emote = EM_NEUTRAL;
    return a;
}

bool cut_play(const char *id)
{
    Block b;
    if (!content_block("cut", id, &b)) return false;

    g_ncmd = 0;
    Cursor c;
    cur_open(&c, &b);
    char line[512];
    while (cur_line(&c, line, sizeof line) && g_ncmd < MAX_CMD) {
        Cmd *cm = &g_cmd[g_ncmd];
        memset(cm, 0, sizeof *cm);
        const char *p = line;
        char w[64];
        if (!word(&p, w, sizeof w)) continue;
        snprintf(cm->verb, sizeof cm->verb, "%s", w);

        /* SAY and TITLE take quoted strings; everything else takes tokens */
        if (eq(cm->verb, "SAY")) {
            word(&p, cm->a, sizeof cm->a);
            rest(p, cm->b, sizeof cm->b);
        } else if (eq(cm->verb, "TITLE")) {
            word(&p, cm->a, sizeof cm->a);
            word(&p, cm->b, sizeof cm->b);
        } else {
            int slot = 0;
            char t[64];
            while (word(&p, t, sizeof t) && t[0]) {
                if (eq(t, "AT")) continue;                 /* ACTOR id AT x y */
                bool numeric = (t[0] == '-' || t[0] == '.' ||
                                (t[0] >= '0' && t[0] <= '9'));
                if (numeric && cm->nf < 5) cm->f[cm->nf++] = (float)atof(t);
                else if (slot == 0) { snprintf(cm->a, 64, "%s", t); slot = 1; }
                else if (slot == 1) { snprintf(cm->b, TEXT_MAX, "%s", t); slot = 2; }
            }
        }
        g_ncmd++;
    }

    g_ip = 0;
    g_active = true;
    g_wait = 0;
    g_time = 0;
    g_nact = 0;
    g_nprop = 0;
    g_say[0] = 0;
    g_say_who = -1;
    g_title_t = 0;
    g_cam_dur = g_fade_dur = g_hue_dur = 0;
    art_camera(0, 0);
    art_fade_set(0);
    return true;
}

void cut_skip(void)
{
    /* Skipping runs the remaining state changes without the theatre, so a
     * skipped cutscene can never leave the world half-changed. */
    for (int i = g_ip; i < g_ncmd; i++) {
        Cmd *cm = &g_cmd[i];
        if (eq(cm->verb, "HUE"))   palette_set_stage((int)cm->f[0]);
        if (eq(cm->verb, "BLOOM")) palette_set_stage((int)cm->f[2]);
        if (eq(cm->verb, "MUSIC")) music_mood(mood_by_name(cm->a));
    }
    g_active = false;
    art_camera(0, 0);
    art_fade_set(0);
}

static void step(void)
{
    while (g_ip < g_ncmd) {
        Cmd *cm = &g_cmd[g_ip++];

        if (eq(cm->verb, "BG")) {
            snprintf(g_bg, sizeof g_bg, "%s", scene_bg_of(cm->a));
        } else if (eq(cm->verb, "CAM")) {
            art_camera(cm->f[0], cm->f[1]);
        } else if (eq(cm->verb, "PAN")) {
            g_cam_x0 = cm->f[0]; g_cam_y0 = cm->f[1];
            g_cam_x1 = cm->f[2]; g_cam_y1 = cm->f[3];
            g_cam_dur = settings()->reduce_motion ? 0.01f : cm->f[4];
            g_cam_t = 0;
        } else if (eq(cm->verb, "ACTOR")) {
            CActor *a = actor_get(cm->a, true);
            if (a) { a->x = a->tx = cm->f[0]; a->y = a->ty = cm->f[1]; a->mdur = 0; }
        } else if (eq(cm->verb, "MOVE")) {
            CActor *a = actor_get(cm->a, true);
            if (a) {
                a->x0 = a->x; a->y0 = a->y;
                a->tx = cm->f[0]; a->ty = cm->f[1];
                a->mdur = settings()->reduce_motion ? 0.01f : cm->f[2];
                a->mt = 0;
            }
        } else if (eq(cm->verb, "EMOTE")) {
            CActor *a = actor_get(cm->a, true);
            if (a) a->emote = npc_emote_by_name(cm->b);
        } else if (eq(cm->verb, "MUSIC")) {
            music_mood(mood_by_name(cm->a));
        } else if (eq(cm->verb, "SFX")) {
            sfx_play(sfx_by_name(cm->a));
        } else if (eq(cm->verb, "DOODLE")) {
            if (g_nprop < MAX_PROPS)
                g_prop[g_nprop++] = (Prop){ doodle_by_name(cm->a), cm->f[0],
                                            cm->f[1], cm->nf > 2 ? cm->f[2] : 26.0f };
        } else if (eq(cm->verb, "SHAKE")) {
            art_shake(cm->f[0], cm->f[1]);
        } else if (eq(cm->verb, "FADE")) {
            bool in = eq(cm->a, "in");
            g_fade_from = art_fade_get();
            g_fade_to = in ? 0.0f : 1.0f;
            g_fade_dur = cm->f[0] > 0 ? cm->f[0] : 0.6f;
            g_fade_t = 0;
        } else if (eq(cm->verb, "HUE")) {
            g_hue_target = (int)cm->f[0];
            g_hue_dur = cm->nf > 1 ? cm->f[1] : 0.8f;
            g_hue_t = 0;
        } else if (eq(cm->verb, "BLOOM")) {
            palette_bloom(cm->f[0], cm->f[1], (int)cm->f[2]);
            flag_set("stage", (int)cm->f[2]);
        } else if (eq(cm->verb, "TITLE")) {
            snprintf(g_title_a, sizeof g_title_a, "%s", cm->a);
            snprintf(g_title_b, sizeof g_title_b, "%s", cm->b);
            g_title_t = 3.2f;
        } else if (eq(cm->verb, "EXIT")) {
            g_active = false;
            art_camera(0, 0);
            return;
        } else if (eq(cm->verb, "WAIT")) {
            g_wait = settings()->reduce_motion ? fminf(cm->f[0], 0.5f) : cm->f[0];
            return;
        } else if (eq(cm->verb, "SAY")) {
            g_say_who = npc_by_name(cm->a);
            snprintf(g_say, sizeof g_say, "%s", cm->b);
            g_say_reveal = 0;
            g_say_hold = 0;
            g_blip_acc = 0;
            g_page = 0;
            g_page_next = -1;
            g_page_len = 0;
            music_duck(true);
            return;
        }
    }
    g_active = false;
    music_duck(false);
    art_camera(0, 0);
}

bool cut_update(float dt)
{
    if (!g_active) return true;
    g_time += dt;

    /* tweens run regardless of what is blocking */
    if (g_cam_dur > 0) {
        g_cam_t += dt;
        float u = g_cam_t / g_cam_dur;
        if (u >= 1) { u = 1; g_cam_dur = 0; }
        float e = u * u * (3 - 2 * u);                   /* smoothstep */
        art_camera(g_cam_x0 + (g_cam_x1 - g_cam_x0) * e,
                   g_cam_y0 + (g_cam_y1 - g_cam_y0) * e);
    }
    if (g_fade_dur > 0) {
        g_fade_t += dt;
        float u = g_fade_t / g_fade_dur;
        if (u >= 1) { u = 1; g_fade_dur = 0; }
        art_fade_set(g_fade_from + (g_fade_to - g_fade_from) * u);
    }
    if (g_hue_dur > 0) {
        g_hue_t += dt;
        if (g_hue_t >= g_hue_dur) {
            g_hue_dur = 0;
            palette_set_stage(g_hue_target);
            flag_set("stage", g_hue_target);
        }
    }
    for (int i = 0; i < g_nact; i++) {
        CActor *a = &g_act[i];
        if (a->mdur <= 0) continue;
        a->mt += dt;
        float u = a->mt / a->mdur;
        if (u >= 1) { u = 1; a->mdur = 0; }
        float e = u * u * (3 - 2 * u);
        a->x = a->x0 + (a->tx - a->x0) * e;
        a->y = a->y0 + (a->ty - a->y0) * e;
    }
    if (g_title_t > 0) g_title_t -= dt;

    bool advance = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                   IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER);
    if (IsKeyPressed(KEY_ESCAPE)) { cut_skip(); return true; }

    if (g_wait > 0) {
        g_wait -= dt;
        if (advance) g_wait = 0;
        if (g_wait > 0) return false;
        step();
        return !g_active;
    }

    if (g_say[0]) {
        int len = g_page_len > 0 ? g_page_len : (int)strlen(g_say) - g_page;
        if (len < 0) len = 0;
        if (g_say_reveal < len) {
            float sp = settings()->reduce_motion ? 9999.0f : 50.0f;
            g_say_reveal += dt * sp;
            g_blip_acc += dt * sp;
            while (g_blip_acc > 2.4f) { g_blip_acc -= 2.4f; sfx_blip(g_say_who); }
            if (advance) g_say_reveal = (float)len;
        } else {
            g_say_hold += dt;
            /* a line with more to it waits for the reader; only the last page
             * of a line ever auto-advances */
            bool more = g_page_next >= 0;
            if (advance || (!more && g_say_hold > 6.0f)) {
                if (more) {
                    g_page = g_page_next;
                    g_say_reveal = 0;
                    g_say_hold = 0;
                    g_blip_acc = 0;
                    sfx_play(SFX_PAGE);
                } else {
                    g_say[0] = 0;
                    music_duck(false);
                    step();
                }
            }
        }
        return !g_active;
    }

    step();
    return !g_active;
}

void cut_draw(void)
{
    scene_draw_backdrop(g_bg, g_time);

    for (int i = 0; i < g_nprop; i++)
        doodle(g_prop[i].id, g_prop[i].x, g_prop[i].y, g_prop[i].scale, 0,
               col_accent_a());

    /* back to front */
    int order[MAX_ACTORS];
    int n = 0;
    for (int i = 0; i < g_nact; i++) if (g_act[i].used) order[n++] = i;
    for (int i = 1; i < n; i++) {
        int k = order[i], j = i - 1;
        while (j >= 0 && g_act[order[j]].y > g_act[k].y) { order[j + 1] = order[j]; j--; }
        order[j + 1] = k;
    }
    for (int i = 0; i < n; i++) {
        CActor *a = &g_act[order[i]];
        npc_draw(a->npc, a->x, a->y, 1.0f, a->mdur > 0 ? POSE_WALK : POSE_STAND,
                 a->emote, g_time);
    }

    vignette(0.55f);

    if (g_title_t > 0) {
        float k = fminf(1.0f, g_title_t);
        Rectangle r = { 260, 210, VW - 520, 190 };
        paper_panel(r, 6.0f, 3131);
        float w1 = art_text_w(g_title_a, 34);
        float w2 = art_text_w(g_title_b, 24);
        Color c = col_ink(); c.a = (unsigned char)(255 * k);
        art_text(g_title_a, r.x + (r.width - w1) * 0.5f, r.y + 44, 34, c);
        art_text(g_title_b, r.x + (r.width - w2) * 0.5f, r.y + 108, 24, c);
    }

    if (g_say[0]) {
        Rectangle panel = { 60, VH - 162, VW - 120, 146 };
        paper_panel(panel, 3.5f, 3232);
        float tx = panel.x + 26;
        if (g_say_who >= 0) {
            Rectangle port = { panel.x + 20, panel.y + 14, 104, 104 };
            npc_bust(g_say_who, port, actor_get(npc_key(g_say_who), false)
                                       ? actor_get(npc_key(g_say_who), false)->emote
                                       : EM_NEUTRAL, g_time);
            ink_rect(port, 2.0f, 0.8f, 3233, col_ink_soft());
            art_text(npc_display(g_say_who), panel.x + 20, panel.y + 122, 12,
                     col_ink_soft());
            tx = panel.x + 140;
        }
        float ty = panel.y + 24;
        float room = panel.y + panel.height - ty - 18;
        g_page_next = art_text_flow(g_say, g_page, tx, ty,
                                    panel.x + panel.width - tx - 26, room, 19,
                                    col_ink(), (int)g_say_reveal, true, NULL);
        g_page_len = (g_page_next < 0 ? (int)strlen(g_say) : g_page_next) - g_page;
        if (g_page_next >= 0 && g_say_reveal >= (float)g_page_len) {
            float mx = panel.x + panel.width - 34, my = panel.y + panel.height - 26;
            ink_line(mx - 9, my - 4, mx, my + 5, 2.6f, 0.5f, 3241, col_accent_b());
            ink_line(mx + 9, my - 4, mx, my + 5, 2.6f, 0.5f, 3242, col_accent_b());
        }
    }

    art_text(ui_str("cut.skip"), 22, 16, 13, col_ink_soft());
}
