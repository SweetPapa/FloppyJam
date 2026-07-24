#include "dlg.h"
#include "content/content.h"
#include "flags/flags.h"
#include "art/artkit.h"
#include "audio/synth.h"
#include "save/save.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#define MAX_CHOICES 4
#define LINE_CAP    320

typedef struct {
    char text[LINE_CAP];
    char tone[16];
    char target[48];
} Choice;

typedef enum { S_IDLE = 0, S_EXEC, S_TYPING, S_CHOOSING, S_YIELD, S_END } State;

static State  g_state;
static Block  g_block;
static Cursor g_cur;
static char   g_node[48];

static char  g_say[LINE_CAP];
static int   g_speaker = -1;
static int   g_emote;
static float g_reveal;
static float g_blip_acc;

static Choice g_choice[MAX_CHOICES];
static int    g_nchoice;
static int    g_hover = -1;

static char g_request[48];
static int  g_request_kind = DLG_DONE;
static char g_pending_goto[48];

/* condition stack: [if ...] / [else] / [end] */
static bool g_cond[8];
static bool g_taken[8];
static int  g_depth;

static bool live(void)
{
    for (int i = 0; i < g_depth; i++) if (!g_cond[i]) return false;
    return true;
}

/* ---------------------------------------------------------------- control */
bool dlg_start(const char *node_id)
{
    if (!content_block("@node", node_id, &g_block)) return false;
    snprintf(g_node, sizeof g_node, "%s", node_id);
    cur_open(&g_cur, &g_block);
    g_depth = 0;
    g_state = S_EXEC;
    g_nchoice = 0;
    g_say[0] = 0;
    g_speaker = -1;
    g_request[0] = 0;
    g_pending_goto[0] = 0;
    music_duck(true);
    return true;
}

bool dlg_active(void) { return g_state != S_IDLE && g_state != S_END; }
int  dlg_speaker(void) { return g_speaker; }
const char *dlg_request(void) { return g_request; }

void dlg_resume(void)
{
    if (g_state == S_YIELD) g_state = S_EXEC;
}

/* A "SPEAKER(emote): text" line? Speakers are ALL CAPS by convention so a
 * line of prose can never be mistaken for one. */
static bool parse_say(const char *line)
{
    const char *colon = strchr(line, ':');
    if (!colon) return false;
    int n = (int)(colon - line);
    if (n <= 0 || n > 40) return false;
    for (int i = 0; i < n; i++) {
        char c = line[i];
        if (!(isupper((unsigned char)c) || c == '_' || c == '(' || c == ')' ||
              islower((unsigned char)c) || c == '.' || c == ' '))
            return false;
    }
    /* require at least one uppercase letter before '(' or ':' */
    bool caps = false;
    for (int i = 0; i < n && line[i] != '('; i++)
        if (isupper((unsigned char)line[i])) caps = true;
    if (!caps) return false;

    char who[48] = "", em[24] = "";
    int wi = 0, ei = 0;
    bool in_em = false;
    for (int i = 0; i < n; i++) {
        char c = line[i];
        if (c == '(') { in_em = true; continue; }
        if (c == ')') { in_em = false; continue; }
        if (in_em) { if (ei < 23) em[ei++] = (char)tolower((unsigned char)c); }
        else if (c != ' ' || wi) { if (wi < 47) who[wi++] = (char)tolower((unsigned char)c); }
    }
    who[wi] = 0; em[ei] = 0;
    while (wi > 0 && who[wi - 1] == ' ') who[--wi] = 0;

    g_speaker = npc_by_name(who);
    g_emote = em[0] ? npc_emote_by_name(em) : EM_NEUTRAL;
    if (g_speaker >= 0) {
        /* the journal's People tab writes itself: anyone who speaks is met */
        char key[FLAG_MAX_KEY];
        snprintf(key, sizeof key, "met.%s", npc_key(g_speaker));
        flag_set(key, 1);
    }
    rest(colon + 1, g_say, LINE_CAP);
    g_reveal = 0;
    g_blip_acc = 0;
    return true;
}

/* Is this line the rest of the sentence above it?
 *
 * Content files wrap at eighty columns because that is how you read a diff.
 * A wrapped line is not a new beat, so anything that is not a verb, a
 * condition, a choice, a speaker or a node header belongs to the line before
 * it — the writer's paragraph, not the writer's line breaks, is the unit. */
static bool is_continuation(const char *line)
{
    if (!line[0] || line[0] == '*' || line[0] == '[' || line[0] == '@') return false;
    if (strncmp(line, "YOU:", 4) == 0) return false;

    static const char *verbs[] = { "grant", "set", "add", "trust", "start_puzzle",
                                   "play_cut", "open_board", "goto", "end", 0 };
    char kw[32];
    const char *p = line;
    word(&p, kw, sizeof kw);
    for (int i = 0; verbs[i]; i++) if (eq(verbs[i], kw)) return false;

    /* a SPEAKER(emote): line starts a new beat */
    const char *colon = strchr(line, ':');
    if (colon && colon - line <= 40) {
        bool all_speaker = true, caps = false;
        for (const char *q = line; q < colon; q++) {
            if (isupper((unsigned char)*q)) caps = true;
            else if (!(islower((unsigned char)*q) || *q == ' ' || *q == '.' ||
                       *q == '_' || *q == '(' || *q == ')')) all_speaker = false;
        }
        if (all_speaker && caps) return false;
    }
    return true;
}

/* Pull any wrapped remainder of the current beat onto the end of g_say.
 *
 * Two paragraphs in a row are two beats, not one, so the merge stops at a
 * finished sentence followed by a fresh capital. A line that begins in
 * lower case, or follows a line that did not finish its sentence, is the
 * same thought carried over. */
static void absorb_continuations(void)
{
    char line[LINE_CAP];
    for (;;) {
        size_t n = strlen(g_say);
        while (n > 0 && g_say[n - 1] == ' ') n--;
        char last = n ? g_say[n - 1] : '.';
        bool sentence_closed = (last == '.' || last == '!' || last == '?' ||
                                last == '"' || last == ':');

        Cursor probe = g_cur;
        if (!cur_line(&probe, line, sizeof line)) return;
        if (!is_continuation(line)) return;
        if (sentence_closed && isupper((unsigned char)line[0])) return;

        g_cur = probe;
        if (n + 2 < LINE_CAP) {
            g_say[n] = ' ';
            snprintf(g_say + n + 1, (size_t)LINE_CAP - n - 1, "%s", line);
        }
    }
}

/* gather the "* (tone) text -> node" lines that follow a YOU: */
static void gather_choices(void)
{
    g_nchoice = 0;
    Cursor probe = g_cur;
    char line[LINE_CAP];
    while (cur_line(&probe, line, sizeof line)) {
        if (line[0] != '*') break;
        g_cur = probe;                     /* consume it */
        if (g_nchoice >= MAX_CHOICES) continue;
        Choice *c = &g_choice[g_nchoice];
        c->tone[0] = 0; c->target[0] = 0;

        const char *p = line + 1;
        while (*p == ' ') p++;
        if (*p == '(') {
            p++;
            int i = 0;
            while (*p && *p != ')') { if (i < 15) c->tone[i++] = *p; p++; }
            c->tone[i] = 0;
            if (*p == ')') p++;
        }
        const char *arrow = strstr(p, "->");
        if (arrow) {
            const char *q = arrow + 2;
            word(&q, c->target, sizeof c->target);
            int len = (int)(arrow - p);
            while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
            if (len > LINE_CAP - 1) len = LINE_CAP - 1;
            memcpy(c->text, p, (size_t)len);
            c->text[len] = 0;
        } else {
            snprintf(c->text, LINE_CAP, "%s", p);
        }
        /* trim leading space left by the tone tag */
        char *t = c->text;
        while (*t == ' ') t++;
        if (t != c->text) memmove(c->text, t, strlen(t) + 1);
        g_nchoice++;
    }
}

static void exec(void)
{
    char line[LINE_CAP];
    while (cur_line(&g_cur, line, sizeof line)) {
        /* --- condition control flow --- */
        if (line[0] == '[') {
            const char *p = line + 1;
            char kw[16];
            word(&p, kw, sizeof kw);
            char *br = strchr(kw, ']');
            if (br) *br = 0;
            const char *save = p;
            if (eq(kw, "if")) {
                bool ok = live() ? content_cond(save) : false;
                if (g_depth < 8) { g_cond[g_depth] = ok; g_taken[g_depth] = ok; g_depth++; }
                continue;
            }
            if (eq(kw, "else")) {
                if (g_depth > 0) {
                    int d = g_depth - 1;
                    g_cond[d] = !g_taken[d];
                    g_taken[d] = true;
                }
                continue;
            }
            if (eq(kw, "end")) {
                if (g_depth > 0) g_depth--;
                continue;
            }
            continue;
        }

        if (!live()) continue;

        /* --- choices --- */
        if (strncmp(line, "YOU:", 4) == 0 || strncmp(line, "you:", 4) == 0) {
            gather_choices();
            if (g_nchoice) {
                g_speaker = npc_by_name("you");
                g_emote = EM_NEUTRAL;
                g_hover = -1;
                g_state = S_CHOOSING;
                return;
            }
            continue;
        }
        if (line[0] == '*') continue;      /* stray choice, already gathered */

        /* --- verbs --- */
        char kw[32];
        const char *p = line;
        word(&p, kw, sizeof kw);

        if (eq(kw, "grant")) {
            char id[64];
            while (word(&p, id, sizeof id) && id[0]) {
                clue_grant(id);
                if (g_speaker >= 0) {
                    char src[FLAG_MAX_KEY];
                    snprintf(src, sizeof src, "src.%s", id);
                    flag_set(src, g_speaker + 1);
                }
                sfx_play(SFX_CHIME);
            }
            continue;
        }
        if (eq(kw, "set")) {
            char key[64], op[8], val[16];
            word(&p, key, sizeof key);
            word(&p, op, sizeof op);
            if (eq(op, "=")) word(&p, val, sizeof val);
            else snprintf(val, sizeof val, "%s", op);
            flag_set(key, atoi(val));
            continue;
        }
        if (eq(kw, "add")) {
            char key[64], val[16];
            word(&p, key, sizeof key);
            word(&p, val, sizeof val);
            flag_add(key, atoi(val));
            continue;
        }
        if (eq(kw, "trust")) {
            char who[48], val[16];
            word(&p, who, sizeof who);
            word(&p, val, sizeof val);
            trust_add(who, atoi(val));
            continue;
        }
        if (eq(kw, "start_puzzle") || eq(kw, "play_cut") || eq(kw, "open_board")) {
            word(&p, g_request, sizeof g_request);
            g_request_kind = eq(kw, "start_puzzle") ? DLG_WANT_PUZZLE
                           : eq(kw, "play_cut")     ? DLG_WANT_CUT
                                                    : DLG_WANT_BOARD;
            g_state = S_YIELD;
            music_duck(false);
            return;
        }
        if (eq(kw, "goto")) {
            char id[48];
            word(&p, id, sizeof id);
            if (dlg_start(id)) return;
            g_state = S_END;
            return;
        }
        if (eq(kw, "end")) { g_state = S_END; music_duck(false); return; }

        /* --- a spoken line --- */
        if (parse_say(line)) { absorb_continuations(); g_state = S_TYPING; return; }

        /* --- narration: no speaker, still worth reading --- */
        g_speaker = -1;
        g_emote = EM_NEUTRAL;
        snprintf(g_say, LINE_CAP, "%s", line);
        absorb_continuations();
        g_reveal = 0;
        g_blip_acc = 0;
        g_state = S_TYPING;
        return;
    }
    g_state = S_END;
    music_duck(false);
}

/* --------------------------------------------------------------- update */
static bool clicked(void) { return IsMouseButtonPressed(MOUSE_BUTTON_LEFT); }
static bool advanced(void)
{
    return clicked() || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER);
}

dlg_status dlg_update(float dt)
{
    switch (g_state) {
    case S_IDLE: return DLG_DONE;
    case S_EXEC: exec(); return dlg_update(0);
    case S_TYPING: {
        int len = (int)strlen(g_say);
        if (g_reveal < len) {
            float speed = settings()->reduce_motion ? 9999.0f : 52.0f;
            g_reveal += dt * speed;
            g_blip_acc += dt * speed;
            while (g_blip_acc > 2.4f) { g_blip_acc -= 2.4f; sfx_blip(g_speaker); }
            if (advanced()) g_reveal = (float)len;    /* impatience is allowed */
        } else if (advanced()) {
            g_state = S_EXEC;
            return dlg_update(0);
        }
        return DLG_RUNNING;
    }
    case S_CHOOSING: return DLG_RUNNING;
    case S_YIELD:
        /* the app reads dlg_request(), handles it, then calls dlg_resume() */
        return (dlg_status)g_request_kind;
    case S_END:
        g_state = S_IDLE;
        music_duck(false);
        return DLG_DONE;
    }
    return DLG_RUNNING;
}

/* ----------------------------------------------------------------- draw */
#define PANEL_H 176.0f

void dlg_draw(void)
{
    if (g_state == S_IDLE || g_state == S_END) return;

    Rectangle panel = { 40, VH - PANEL_H - 12, VW - 80, PANEL_H };
    paper_panel(panel, 3.5f, 4242);

    float px = panel.x + 24, py = panel.y + 18;
    float tx = px;

    /* portrait */
    if (g_speaker >= 0) {
        Rectangle port = { px, py, 116, 116 };
        ink_rect(port, 2.0f, 0.8f, 77, col_ink_soft());
        npc_bust(g_speaker, port, g_emote, (float)GetTime());
        art_text(npc_display(g_speaker), px, py + 122, 14, col_ink());
        tx = px + 138;
    }

    float wide = panel.x + panel.width - tx - 24;

    if (g_state == S_CHOOSING) {
        art_text("You:", tx, py, 18, col_ink_soft());
        float y = py + 30;
        Vector2 m = art_mouse();
        g_hover = -1;
        for (int i = 0; i < g_nchoice; i++) {
            float h = 34 + text_scale() * 8;
            Rectangle r = { tx, y, wide, h };
            bool hot = CheckCollisionPointRec(m, r);
            if (hot) g_hover = i;
            Color c = hot ? col_accent_b() : col_ink();
            if (hot) {
                Vector2 bg[4] = { { r.x - 6, r.y }, { r.x + r.width, r.y },
                                  { r.x + r.width, r.y + r.height }, { r.x - 6, r.y + r.height } };
                ink_fill(bg, 4, HATCH_NONE, 900 + i, (Color){ 232, 222, 200, 190 });
            }
            char buf[LINE_CAP + 24];
            if (g_choice[i].tone[0])
                snprintf(buf, sizeof buf, "(%s)  %s", g_choice[i].tone, g_choice[i].text);
            else
                snprintf(buf, sizeof buf, "%s", g_choice[i].text);
            art_text(buf, r.x + 4, r.y + 6, 17, c);
            y += h + 4;
        }
        if (g_hover >= 0 && clicked()) {
            sfx_play(SFX_CLICK);
            char tgt[48];
            snprintf(tgt, sizeof tgt, "%s", g_choice[g_hover].target);
            if (tgt[0]) { dlg_start(tgt); }
            else        { g_state = S_EXEC; }
        }
        return;
    }

    if (g_speaker < 0)
        art_text(settings()->detective, tx, py, 15, col_ink_soft());

    art_text_wrap(g_say, tx, py + (g_speaker < 0 ? 24 : 0), wide, 19, col_ink(),
                  (int)g_reveal);

    if (g_reveal >= (float)strlen(g_say)) {
        float t = (float)GetTime();
        float bob = settings()->reduce_motion ? 0.0f : sinf(t * 4.0f) * 3.0f;
        doodle(D_FEATHER, panel.x + panel.width - 40, panel.y + panel.height - 34 + bob,
               13, 0.5f, col_ink_soft());
    }
}
