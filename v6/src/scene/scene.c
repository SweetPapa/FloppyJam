#include "scene.h"
#include "content/content.h"
#include "flags/flags.h"
#include "art/artkit.h"
#include "audio/synth.h"
#include "save/save.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#define MAX_HOT    16
#define MAX_ACTORS 8

typedef struct {
    Rectangle r;
    int  kind;
    char arg[48];
    char label[160];
    char cond[96];
    bool has_cond;
} Hot;

typedef struct { int npc; float x, y; int pose; } Actor;

static char  g_id[48];
static char  g_title[96];
static char  g_bg[32];
static int   g_mood = MOOD_TOWN;
static Rectangle g_walk = { 120, 540, 1040, 130 };
static Hot   g_hot[MAX_HOT];
static int   g_nhot;
static Actor g_actor[MAX_ACTORS];
static int   g_nactors;

static char  g_on_enter[48];
static char  g_on_enter_cut[48];
static char  g_request[48];
static float g_time;

/* the Detective */
static float g_px, g_py, g_tx, g_ty;
static bool  g_walking;
static float g_step_acc;

/* pending: walk there first, then do the thing */
static int   g_pending = -1;
static char  g_inspect[240];
static float g_inspect_t;

const char *scene_current(void) { return g_id; }
const char *scene_title(void) { return g_title; }
const char *scene_request_id(void) { return g_request; }
int scene_district_mood(void) { return g_mood; }

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

/* ==========================================================================
 * backdrops — every screen in the game is a few dozen strokes
 * ========================================================================== */
static void bg_sky(Color top, float horizon)
{
    Vector2 s[4] = { { 0, 0 }, { VW, 0 }, { VW, horizon }, { 0, horizon } };
    ink_fill(s, 4, HATCH_NONE, 1, top);
    for (int i = 0; i < 5; i++) {
        float x = 120 + i * 260.0f + art_noise(11, i) * 60.0f;
        float y = 70 + art_noise(12, i) * 50.0f;
        doodle(D_CLOUD, x, y, 46 + art_noise(13, i) * 14, 0,
               (Color){ 250, 248, 242, 210 });
    }
}

static void bg_ground(float horizon, Color c, int hatch)
{
    Vector2 g[4] = { { 0, horizon }, { VW, horizon }, { VW, VH }, { 0, VH } };
    ink_fill(g, 4, hatch, 2, c);
    ink_line(0, horizon, VW, horizon, 2.4f, 2.0f, 3, col_ink_soft());
}

static void building(float x, float baseY, float w, float h, Color wall,
                     Color roof, int seed, int windows)
{
    Vector2 b[4] = { { x, baseY - h }, { x + w, baseY - h }, { x + w, baseY },
                     { x, baseY } };
    ink_fill(b, 4, HATCH_NONE, seed, wall);
    ink_stroke((Vector2[]){ { x, baseY }, { x, baseY - h }, { x + w, baseY - h },
                            { x + w, baseY } }, 4, 2.6f, 1.0f, seed + 1, col_ink());
    Vector2 r[3] = { { x - 10, baseY - h }, { x + w * 0.5f, baseY - h - h * 0.34f },
                     { x + w + 10, baseY - h } };
    ink_fill(r, 3, HATCH_DIAG, seed + 2, roof);
    ink_stroke((Vector2[]){ r[0], r[1], r[2] }, 3, 2.6f, 1.0f, seed + 3, col_ink());

    for (int i = 0; i < windows; i++) {
        float wx = x + w * (0.18f + 0.64f * (windows == 1 ? 0.5f : (float)i / (windows - 1)));
        float wy = baseY - h * 0.62f;
        Rectangle wr = { wx - 17, wy - 20, 34, 40 };
        Vector2 q[4] = { { wr.x, wr.y }, { wr.x + wr.width, wr.y },
                         { wr.x + wr.width, wr.y + wr.height }, { wr.x, wr.y + wr.height } };
        ink_fill(q, 4, HATCH_NONE, seed + 10 + i, (Color){ 236, 222, 176, 255 });
        ink_rect(wr, 2.0f, 0.7f, seed + 20 + i, col_ink());
        ink_line(wx, wr.y, wx, wr.y + wr.height, 1.8f, 0.4f, seed + 30 + i, col_ink());
    }
}

/* the Prismworks: the thing every outdoor scene can see, and the reason the
 * player always knows where they are going */
static void prismworks(float x, float baseY, float h, float t)
{
    float w = h * 0.22f;
    Vector2 body[4] = { { x - w * 0.62f, baseY }, { x - w * 0.40f, baseY - h },
                        { x + w * 0.40f, baseY - h }, { x + w * 0.62f, baseY } };
    ink_fill(body, 4, HATCH_NONE, 4001, (Color){ 236, 230, 216, 255 });
    ink_stroke((Vector2[]){ body[0], body[1], body[2], body[3] }, 4, 2.8f, 1.0f,
               4002, col_ink());
    for (int i = 1; i < 5; i++) {
        float y = baseY - h * (float)i / 5.0f;
        float ww = w * (0.62f - 0.22f * (float)i / 5.0f);
        ink_line(x - ww, y, x + ww, y, 1.8f, 1.2f, 4003 + i, col_ink_soft());
    }
    /* lantern room */
    Rectangle lr = { x - w * 0.56f, baseY - h - h * 0.14f, w * 1.12f, h * 0.14f };
    Vector2 q[4] = { { lr.x, lr.y }, { lr.x + lr.width, lr.y },
                     { lr.x + lr.width, lr.y + lr.height }, { lr.x, lr.y + lr.height } };
    ink_fill(q, 4, HATCH_NONE, 4010,
             palette_stage() >= 6 ? (Color){ 250, 226, 140, 255 }
                                  : (Color){ 214, 206, 190, 255 });
    ink_rect(lr, 2.6f, 0.9f, 4011, col_ink());
    Vector2 cap[3] = { { lr.x - 12, lr.y }, { x, lr.y - h * 0.13f }, { lr.x + lr.width + 12, lr.y } };
    ink_fill(cap, 3, HATCH_DIAG, 4012, (Color){ 150, 118, 96, 255 });

    if (palette_stage() >= 6) {
        float k = 0.5f + 0.5f * sinf(t * 1.4f);
        Color glow = { 250, 226, 140, (unsigned char)(70 + 60 * k) };
        DrawCircleV((Vector2){ x, lr.y + lr.height * 0.5f }, h * 0.20f, glow);
    }
}

static void sea(float horizon, float t)
{
    Vector2 s[4] = { { 0, horizon }, { VW, horizon }, { VW, horizon + 190 },
                     { 0, horizon + 190 } };
    ink_fill(s, 4, HATCH_NONE, 5, col_sea());
    if (settings()->reduce_motion) t = 0;
    for (int i = 0; i < 16; i++) {
        float y = horizon + 16 + (float)(i % 8) * 22.0f;
        float x = fmodf((float)i * 173.0f + t * (12.0f + i), VW + 200.0f) - 100.0f;
        doodle(D_WAVE, x, y, 22, 0, (Color){ 226, 236, 242, 190 });
    }
}

void scene_draw_backdrop(const char *kind, float t)
{
    if (eq(kind, "harbor")) {
        bg_sky(col_sky(), 300);
        sea(300, t);
        bg_ground(470, (Color){ 206, 190, 164, 255 }, HATCH_DOT);
        prismworks(1090, 462, 300, t);
        building(90, 470, 210, 150, (Color){ 214, 200, 176, 255 },
                 (Color){ 150, 96, 84, 255 }, 100, 2);
        building(330, 470, 160, 120, (Color){ 224, 212, 188, 255 },
                 (Color){ 96, 118, 142, 255 }, 130, 1);
        for (int i = 0; i < 4; i++)
            ink_line(560 + i * 90.0f, 470, 560 + i * 90.0f, 400, 6.0f, 1.2f,
                     160 + i, (Color){ 118, 96, 74, 255 });
        doodle(D_ANCHOR, 690, 496, 44, -0.2f, (Color){ 112, 108, 104, 255 });
        doodle(D_FISH, 250, 502, 30, 0.15f, col_sea());
    } else if (eq(kind, "market")) {
        bg_sky(col_sky(), 430);
        bg_ground(430, (Color){ 212, 198, 170, 255 }, HATCH_DOT);
        prismworks(1120, 430, 300, t);
        building(60, 430, 240, 210, (Color){ 228, 214, 186, 255 },
                 (Color){ 178, 108, 88, 255 }, 200, 3);
        building(330, 430, 200, 180, (Color){ 220, 208, 184, 255 },
                 (Color){ 122, 140, 106, 255 }, 230, 2);
        building(570, 430, 220, 200, (Color){ 232, 220, 194, 255 },
                 (Color){ 154, 118, 158, 255 }, 260, 2);
        /* awnings — Market Row's yellow is the second hue home */
        for (int i = 0; i < 3; i++) {
            float x = 110 + i * 250.0f;
            Vector2 a[4] = { { x, 400 }, { x + 190, 400 }, { x + 170, 442 }, { x + 20, 442 } };
            ink_fill(a, 4, HATCH_VERT, 280 + i, (Color){ 228, 186, 74, 255 });
            ink_stroke((Vector2[]){ a[0], a[1], a[2], a[3], a[0] }, 5, 2.2f, 0.9f,
                       290 + i, col_ink());
        }
        doodle(D_BREAD, 200, 476, 30, 0, (Color){ 208, 168, 108, 255 });
        doodle(D_SPOOL, 470, 476, 26, 0.2f, (Color){ 172, 108, 168, 255 });
        doodle(D_ENVELOPE, 720, 476, 26, -0.1f, (Color){ 224, 214, 196, 255 });
    } else if (eq(kind, "quarter")) {
        bg_sky((Color){ 176, 182, 196, 255 }, 420);
        bg_ground(420, (Color){ 196, 186, 168, 255 }, HATCH_CROSS);
        building(40, 420, 230, 300, (Color){ 210, 198, 178, 255 },
                 (Color){ 128, 92, 96, 255 }, 300, 3);
        building(300, 420, 190, 260, (Color){ 218, 206, 186, 255 },
                 (Color){ 96, 86, 110, 255 }, 330, 2);
        building(900, 420, 260, 320, (Color){ 206, 194, 174, 255 },
                 (Color){ 142, 100, 86, 255 }, 360, 3);
        doodle(D_CLOCK, 1030, 200, 62, 0, (Color){ 214, 176, 72, 255 });
        doodle(D_LOCK, 640, 470, 30, 0, (Color){ 130, 126, 120, 255 });
        doodle(D_BOOK, 760, 476, 28, 0.1f, (Color){ 122, 96, 132, 255 });
    } else if (eq(kind, "garden")) {
        bg_sky(col_sky(), 410);
        bg_ground(410, (Color){ 132, 168, 118, 255 }, HATCH_DIAG);
        prismworks(1130, 410, 320, t);
        for (int i = 0; i < 5; i++)
            doodle(D_TREE, 90 + i * 190.0f, 400, 62 + art_noise(41, i) * 12, 0,
                   (Color){ 104, 154, 104, 255 });
        for (int i = 0; i < 11; i++)
            doodle(D_FLOWER, 120 + i * 96.0f, 470 + art_noise(42, i) * 22, 15, 0,
                   (i & 1) ? (Color){ 214, 118, 128, 255 } : (Color){ 226, 194, 96, 255 });
        doodle(D_BEE, 860, 330, 20, 0, (Color){ 224, 178, 62, 255 });
    } else if (eq(kind, "towergreen")) {
        bg_sky(col_sky(), 430);
        bg_ground(430, (Color){ 138, 166, 122, 255 }, HATCH_DIAG);
        prismworks(640, 430, 380, t);
        for (int i = 0; i < 8; i++)
            doodle(D_LANTERN, 120 + i * 150.0f, 452 + art_noise(51, i) * 12, 32, 0,
                   (Color){ 226, 196, 120, 255 });
        doodle(D_TREE, 140, 420, 70, 0, (Color){ 104, 148, 104, 255 });
        doodle(D_TREE, 1140, 420, 66, 0, (Color){ 104, 148, 104, 255 });
    } else if (eq(kind, "tower")) {
        Vector2 wall[4] = { { 0, 0 }, { VW, 0 }, { VW, VH }, { 0, VH } };
        ink_fill(wall, 4, HATCH_CROSS, 6, (Color){ 198, 188, 172, 255 });
        /* the rotten stair, drawn as a spiral of planks */
        for (int i = 0; i < 12; i++) {
            float y = 640 - i * 46.0f;
            float w = 300 - i * 6.0f;
            float x = 640 + sinf(i * 0.9f) * 170.0f;
            Vector2 p[4] = { { x - w * 0.5f, y }, { x + w * 0.5f, y },
                             { x + w * 0.5f, y + 18 }, { x - w * 0.5f, y + 18 } };
            ink_fill(p, 4, HATCH_NONE, 600 + i,
                     i == 8 ? (Color){ 150, 104, 88, 255 } : (Color){ 176, 148, 118, 255 });
            ink_stroke((Vector2[]){ p[0], p[1], p[2], p[3], p[0] }, 5, 2.0f, 1.0f,
                       620 + i, col_ink());
        }
        vignette(0.8f);
    } else if (eq(kind, "lantern")) {
        Vector2 wall[4] = { { 0, 0 }, { VW, 0 }, { VW, VH }, { 0, VH } };
        ink_fill(wall, 4, HATCH_NONE, 7, (Color){ 216, 208, 190, 255 });
        for (int i = 0; i < 5; i++) {
            float x = 90 + i * 280.0f;
            Vector2 win[4] = { { x, 90 }, { x + 190, 90 }, { x + 190, 380 }, { x, 380 } };
            ink_fill(win, 4, HATCH_NONE, 700 + i, col_sky());
            ink_stroke((Vector2[]){ win[0], win[1], win[2], win[3], win[0] }, 5,
                       3.0f, 1.0f, 710 + i, col_ink());
        }
        bg_ground(560, (Color){ 180, 166, 146, 255 }, HATCH_DIAG);
        doodle(D_PRISM, 640, 430, 60, 0, (Color){ 168, 140, 210, 255 });
    } else if (eq(kind, "gate")) {
        bg_sky(col_sky(), 450);
        bg_ground(450, (Color){ 204, 190, 166, 255 }, HATCH_DOT);
        prismworks(980, 450, 300, t);
        building(60, 450, 200, 170, (Color){ 220, 206, 180, 255 },
                 (Color){ 158, 104, 88, 255 }, 800, 2);
        /* the gate arch */
        ink_line(430, 450, 430, 210, 9.0f, 1.4f, 810, (Color){ 128, 112, 96, 255 });
        ink_line(760, 450, 760, 210, 9.0f, 1.4f, 811, (Color){ 128, 112, 96, 255 });
        ink_line(410, 210, 780, 210, 9.0f, 1.4f, 812, (Color){ 128, 112, 96, 255 });
        for (int i = 0; i < 5; i++)
            doodle(D_LANTERN, 450 + i * 72.0f, 240, 18, 0, (Color){ 214, 190, 130, 255 });
    } else {   /* "square" and anything a scene forgot to name */
        bg_sky(col_sky(), 440);
        bg_ground(440, (Color){ 208, 194, 170, 255 }, HATCH_DOT);
        prismworks(1060, 440, 330, t);
        building(40, 440, 230, 190, (Color){ 226, 212, 186, 255 },
                 (Color){ 164, 104, 92, 255 }, 900, 3);
        building(300, 440, 190, 160, (Color){ 218, 206, 182, 255 },
                 (Color){ 106, 128, 150, 255 }, 930, 2);
        building(520, 440, 210, 175, (Color){ 230, 218, 192, 255 },
                 (Color){ 124, 148, 110, 255 }, 960, 2);
        /* the fountain */
        ink_circle(640, 486, 92, 3.0f, 1.4f, 970, col_ink());
        ink_blob(640, 486, 86, 0.34f, 971, col_sea());
        doodle(D_PRISM, 640, 440, 26, 0, (Color){ 168, 140, 210, 255 });
    }
    paper_grain(0.55f);
}

const char *scene_bg_of(const char *scene_id)
{
    static char kind[32];
    snprintf(kind, sizeof kind, "%s", "square");
    Block b;
    if (!content_block("scene", scene_id, &b)) return kind;
    Cursor c;
    cur_open(&c, &b);
    char line[512];
    while (cur_line(&c, line, sizeof line)) {
        const char *p = line;
        char kw[32];
        word(&p, kw, sizeof kw);
        if (eq(kw, "bg")) { word(&p, kind, sizeof kind); break; }
    }
    return kind;
}

/* ==========================================================================
 * loading
 * ========================================================================== */
bool scene_load(const char *id)
{
    Block b;
    if (!content_block("scene", id, &b)) return false;
    snprintf(g_id, sizeof g_id, "%s", id);
    g_nhot = g_nactors = 0;
    g_title[0] = 0;
    g_on_enter[0] = g_on_enter_cut[0] = 0;
    snprintf(g_bg, sizeof g_bg, "%s", "square");
    g_mood = MOOD_TOWN;
    g_walk = (Rectangle){ 120, 540, 1040, 130 };
    g_px = 300; g_py = 610;
    g_pending = -1;
    g_inspect[0] = 0;
    g_inspect_t = 0;

    Cursor c;
    cur_open(&c, &b);
    char line[512];
    while (cur_line(&c, line, sizeof line)) {
        const char *p = line;
        char kw[32];
        word(&p, kw, sizeof kw);

        if (eq(kw, "title")) rest(p, g_title, sizeof g_title);
        else if (eq(kw, "bg")) word(&p, g_bg, sizeof g_bg);
        else if (eq(kw, "music")) {
            char m[32];
            word(&p, m, sizeof m);
            g_mood = mood_by_name(m);
        } else if (eq(kw, "walk")) {
            char a[16], bb[16], cc[16], d[16];
            word(&p, a, 16); word(&p, bb, 16); word(&p, cc, 16); word(&p, d, 16);
            g_walk = (Rectangle){ (float)atoi(a), (float)atoi(bb),
                                  (float)(atoi(cc) - atoi(a)), (float)(atoi(d) - atoi(bb)) };
        } else if (eq(kw, "spawn")) {
            char a[16], bb[16];
            word(&p, a, 16); word(&p, bb, 16);
            g_px = (float)atoi(a); g_py = (float)atoi(bb);
        } else if (eq(kw, "on_enter")) {
            word(&p, g_on_enter, sizeof g_on_enter);
        } else if (eq(kw, "on_enter_cut")) {
            word(&p, g_on_enter_cut, sizeof g_on_enter_cut);
        } else if (eq(kw, "actor")) {
            if (g_nactors >= MAX_ACTORS) continue;
            char who[32], a[16], bb[16], pose[16] = "stand";
            word(&p, who, sizeof who);
            word(&p, a, 16); word(&p, bb, 16);
            word(&p, pose, sizeof pose);
            Actor *ac = &g_actor[g_nactors++];
            ac->npc = npc_by_name(who);
            ac->x = (float)atoi(a); ac->y = (float)atoi(bb);
            ac->pose = eq(pose, "sit") ? POSE_SIT : eq(pose, "busy") ? POSE_BUSY
                     : eq(pose, "point") ? POSE_POINT : POSE_STAND;
        } else if (eq(kw, "feather")) {
            if (g_nhot >= MAX_HOT) continue;
            char a[16], bb[16], id[48];
            word(&p, a, 16); word(&p, bb, 16); word(&p, id, sizeof id);
            Hot *h = &g_hot[g_nhot++];
            memset(h, 0, sizeof *h);
            h->r = (Rectangle){ (float)atoi(a) - 26, (float)atoi(bb) - 26, 52, 52 };
            h->kind = 100;                              /* internal: feather */
            snprintf(h->arg, sizeof h->arg, "%s", id);
            snprintf(h->label, sizeof h->label, "%s", "One of Pip's feathers");
        } else if (eq(kw, "hot")) {
            if (g_nhot >= MAX_HOT) continue;
            char a[16], bb[16], cc[16], d[16], kind[24];
            word(&p, a, 16); word(&p, bb, 16); word(&p, cc, 16); word(&p, d, 16);
            word(&p, kind, sizeof kind);
            Hot *h = &g_hot[g_nhot++];
            memset(h, 0, sizeof *h);
            h->r = (Rectangle){ (float)atoi(a), (float)atoi(bb), (float)atoi(cc),
                                (float)atoi(d) };
            if (eq(kind, "talk"))       { h->kind = SC_TALK;  word(&p, h->arg, 48); }
            else if (eq(kind, "exit"))  { h->kind = SC_EXIT;  word(&p, h->arg, 48); }
            else if (eq(kind, "board")) { h->kind = SC_BOARD; word(&p, h->arg, 48); }
            else if (eq(kind, "puzzle")){ h->kind = SC_PUZZLE;word(&p, h->arg, 48); }
            else if (eq(kind, "cut"))   { h->kind = SC_CUT;   word(&p, h->arg, 48); }
            else                        { h->kind = SC_NONE; }

            char tail[240];
            rest(p, tail, sizeof tail);
            /* "<label>" [if <cond>] */
            const char *q = tail;
            char lab[200];
            if (word(&q, lab, sizeof lab)) snprintf(h->label, sizeof h->label, "%s", lab);
            char maybe_if[8];
            const char *save = q;
            if (word(&q, maybe_if, sizeof maybe_if) && eq(maybe_if, "if")) {
                snprintf(h->cond, sizeof h->cond, "%s", q);
                h->has_cond = true;
            } else {
                (void)save;
            }
        }
    }
    g_tx = g_px; g_ty = g_py;
    g_walking = false;
    music_mood(g_mood);
    return true;
}

static bool hot_live(const Hot *h)
{
    if (h->kind == 100) return flag_get(h->arg) == 0;      /* uncollected only */
    if (!h->has_cond) return true;
    return content_cond(h->cond);
}

/* ==========================================================================
 * update
 * ========================================================================== */
static void fire(int i)
{
    Hot *h = &g_hot[i];
    if (h->kind == 100) {
        flag_set(h->arg, 1);
        flag_add("feathers", 1);
        trust_add("pip", 1);        /* the hint economy IS the relationship (§5.2) */
        sfx_play(SFX_FEATHER);
        snprintf(g_inspect, sizeof g_inspect, "%s", ui_str("feather.found"));
        g_inspect_t = 3.0f;
        return;
    }
    if (h->kind == SC_NONE) {
        snprintf(g_inspect, sizeof g_inspect, "%s", h->label);
        g_inspect_t = 5.0f;
        sfx_play(SFX_TICK);
        return;
    }
    snprintf(g_request, sizeof g_request, "%s", h->arg);
}

scene_request scene_update(float dt)
{
    g_time += dt;
    if (g_inspect_t > 0) g_inspect_t -= dt;

    /* on_enter runs once per scene, guarded by a flag so a revisit is quiet.
     * The cutscene goes first: a scene that opens on a cutscene should open
     * on the cutscene, not on somebody talking over it. */
    if (g_on_enter_cut[0]) {
        char key[FLAG_MAX_KEY];
        snprintf(key, sizeof key, "seencut.%s", g_id);
        if (flag_get(key) == 0) {
            flag_set(key, 1);
            snprintf(g_request, sizeof g_request, "%s", g_on_enter_cut);
            g_on_enter_cut[0] = 0;
            return SC_CUT;
        }
        g_on_enter_cut[0] = 0;
    }
    if (g_on_enter[0]) {
        char key[FLAG_MAX_KEY];
        snprintf(key, sizeof key, "seen.%s", g_id);
        if (flag_get(key) == 0) {
            flag_set(key, 1);
            snprintf(g_request, sizeof g_request, "%s", g_on_enter);
            g_on_enter[0] = 0;
            return SC_TALK;
        }
        g_on_enter[0] = 0;
    }

    Vector2 m = art_mouse();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && g_inspect_t <= 0) {
        int hit = -1;
        for (int i = 0; i < g_nhot; i++)
            if (hot_live(&g_hot[i]) && CheckCollisionPointRec(m, g_hot[i].r)) hit = i;

        if (hit >= 0) {
            g_pending = hit;
            g_tx = g_hot[hit].r.x + g_hot[hit].r.width * 0.5f;
            if (g_tx < g_walk.x + 40) g_tx = g_walk.x + 40;
            if (g_tx > g_walk.x + g_walk.width - 40) g_tx = g_walk.x + g_walk.width - 40;
            g_ty = g_walk.y + g_walk.height * 0.62f;
            g_walking = true;
        } else if (CheckCollisionPointRec(m, g_walk)) {
            g_pending = -1;
            g_tx = m.x; g_ty = m.y;
            g_walking = true;
        }
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_inspect_t = 0;                         /* click dismisses flavour text */
    }

    if (g_walking) {
        float dx = g_tx - g_px, dy = g_ty - g_py;
        float d = sqrtf(dx * dx + dy * dy);
        float sp = 300.0f * dt;
        if (d <= sp) {
            g_px = g_tx; g_py = g_ty;
            g_walking = false;
            if (g_pending >= 0) {
                int i = g_pending;
                g_pending = -1;
                int kind = g_hot[i].kind;
                fire(i);
                if (kind != 100 && kind != SC_NONE) return (scene_request)kind;
            }
        } else {
            g_px += dx / d * sp;
            g_py += dy / d * sp;
            g_step_acc += sp;
            if (g_step_acc > 46.0f) { g_step_acc = 0; sfx_play(SFX_STEP); }
        }
    }
    return SC_NONE;
}

/* ==========================================================================
 * draw
 * ========================================================================== */
void scene_draw(void)
{
    scene_draw_backdrop(g_bg, g_time);

    /* actors and the Detective, sorted back to front */
    typedef struct { float y; int npc; float x; int pose; } Drawable;
    Drawable d[MAX_ACTORS + 1];
    int n = 0;
    for (int i = 0; i < g_nactors; i++)
        d[n++] = (Drawable){ g_actor[i].y, g_actor[i].npc, g_actor[i].x, g_actor[i].pose };
    d[n++] = (Drawable){ g_py, npc_by_name("you"), g_px,
                         g_walking ? POSE_WALK : POSE_STAND };
    for (int i = 1; i < n; i++) {
        Drawable k = d[i];
        int j = i - 1;
        while (j >= 0 && d[j].y > k.y) { d[j + 1] = d[j]; j--; }
        d[j + 1] = k;
    }
    for (int i = 0; i < n; i++)
        npc_draw(d[i].npc, d[i].x, d[i].y, 1.0f, d[i].pose, EM_NEUTRAL, g_time);

    /* interactables wear a sparkle (§1.2.1) */
    Vector2 m = art_mouse();
    int hover = -1;
    for (int i = 0; i < g_nhot; i++) {
        if (!hot_live(&g_hot[i])) continue;
        Rectangle r = g_hot[i].r;
        float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f;
        if (g_hot[i].kind == 100)
            doodle(D_FEATHER, cx, cy, 20, sinf(g_time * 1.3f + i) * 0.3f,
                   (Color){ 236, 210, 128, 255 });
        sparkle(cx, cy, fmaxf(r.width, r.height) * 0.45f, g_time + i);
        if (CheckCollisionPointRec(m, r)) hover = i;
    }

    if (hover >= 0 && g_hot[hover].label[0]) {
        const char *lab = g_hot[hover].label;
        float w = art_text_w(lab, 17) + 26;
        Rectangle r = { m.x - w * 0.5f, m.y - 46, w, 32 };
        if (r.x < 8) r.x = 8;
        if (r.x + r.width > VW - 8) r.x = VW - 8 - r.width;
        Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                         { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
        ink_fill(q, 4, HATCH_NONE, 1200, (Color){ 244, 238, 222, 235 });
        ink_rect(r, 2.0f, 0.7f, 1201, col_ink());
        art_text(lab, r.x + 13, r.y + 6, 17, col_ink());
    }

    if (g_inspect_t > 0) {
        Rectangle r = { 220, VH - 190, VW - 440, 120 };
        paper_panel(r, 3.0f, 1300);
        art_text_wrap(g_inspect, r.x + 24, r.y + 22, r.width - 48, 18, col_ink(), -1);
    }

    vignette(0.45f);
}
