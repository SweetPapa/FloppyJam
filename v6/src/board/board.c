#include "board.h"
#include "content/content.h"
#include "flags/flags.h"
#include "art/artkit.h"
#include "audio/synth.h"
#include "save/save.h"
#include "puzzle/puzzle.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_LINES  10
#define MAX_BLANKS 12
#define MAX_POOLS  4
#define MAX_TOKENS 14
#define MAX_FROM   4

typedef struct {
    char name[16];                       /* p1 */
    char answer[48];                     /* token.otto */
    char pool[16];
    char from[MAX_FROM][48];
    int  nfrom;
    int  filled;                         /* index into the pool, -1 empty */
    int  hint_tier;
    bool locked;                         /* revealed by a tier-3 hint */
} Blank;

typedef struct {
    char name[16];
    char tok[MAX_TOKENS][48];
    int  n;
} Pool;

static char  g_id[48];
static char  g_title[128];
static char  g_lines[MAX_LINES][256];
static int   g_nlines;
static Blank g_blank[MAX_BLANKS];
static int   g_nblanks;
static Pool  g_pool[MAX_POOLS];
static int   g_npools;
static char  g_solved_kind[16];
static char  g_solved_id[48];

static int   g_sel = -1;                 /* selected blank */
static int   g_attempts;
static int   g_last_correct = -1;
static float g_flash;
static bool  g_solved;
static float g_time;

const char *board_id(void) { return g_id; }
const char *board_solved_kind(void) { return g_solved_kind; }
const char *board_solved_id(void) { return g_solved_id; }

bool board_is_solved(const char *id)
{
    char key[FLAG_MAX_KEY];
    snprintf(key, sizeof key, "board.%s", id);
    return flag_get(key) != 0;
}

static Pool *pool_by_name(const char *n)
{
    for (int i = 0; i < g_npools; i++)
        if (eq(g_pool[i].name, n)) return &g_pool[i];
    return NULL;
}

bool board_start(const char *id)
{
    Block b;
    if (!content_block("board", id, &b)) return false;
    snprintf(g_id, sizeof g_id, "%s", id);
    g_nlines = g_nblanks = g_npools = 0;
    g_title[0] = 0;
    g_solved_kind[0] = g_solved_id[0] = 0;
    g_sel = -1;
    g_attempts = 0;
    g_last_correct = -1;
    g_solved = false;
    g_time = 0;

    Cursor c;
    cur_open(&c, &b);
    char line[512];
    while (cur_line(&c, line, sizeof line)) {
        const char *p = line;
        char kw[32];
        word(&p, kw, sizeof kw);

        if (eq(kw, "title")) {
            rest(p, g_title, sizeof g_title);
        } else if (eq(kw, "text")) {
            if (g_nlines < MAX_LINES) rest(p, g_lines[g_nlines++], 256);
        } else if (eq(kw, "requires")) {
            /* the fairness contract; bakery proved it, nothing to do at runtime */
        } else if (eq(kw, "pool")) {
            if (g_npools >= MAX_POOLS) continue;
            Pool *pl = &g_pool[g_npools++];
            pl->n = 0;
            word(&p, pl->name, sizeof pl->name);
            char t[48];
            while (word(&p, t, sizeof t) && t[0] && pl->n < MAX_TOKENS)
                snprintf(pl->tok[pl->n++], 48, "%s", t);
        } else if (eq(kw, "blank")) {
            if (g_nblanks >= MAX_BLANKS) continue;
            Blank *bl = &g_blank[g_nblanks++];
            memset(bl, 0, sizeof *bl);
            bl->filled = -1;
            word(&p, bl->name, sizeof bl->name);
            char t[48];
            while (word(&p, t, sizeof t) && t[0]) {
                if (eq(t, "answer"))    word(&p, bl->answer, sizeof bl->answer);
                else if (eq(t, "pool")) word(&p, bl->pool, sizeof bl->pool);
                else if (eq(t, "from")) {
                    while (word(&p, t, sizeof t) && t[0] && bl->nfrom < MAX_FROM)
                        snprintf(bl->from[bl->nfrom++], 48, "%s", t);
                    break;
                }
            }
        } else if (eq(kw, "on_solved")) {
            word(&p, g_solved_kind, sizeof g_solved_kind);
            word(&p, g_solved_id, sizeof g_solved_id);
        }
    }
    music_mood(MOOD_BOARD);
    return g_nblanks > 0;
}

static const char *blank_label(const Blank *bl)
{
    if (bl->filled < 0) return NULL;
    Pool *pl = pool_by_name(bl->pool);
    if (!pl || bl->filled >= pl->n) return NULL;
    return ui_str(pl->tok[bl->filled]);
}

static bool blank_correct(const Blank *bl)
{
    if (bl->filled < 0) return false;
    Pool *pl = pool_by_name(bl->pool);
    if (!pl || bl->filled >= pl->n) return false;
    return eq(pl->tok[bl->filled], bl->answer);
}

static int filled_count(void)
{
    int n = 0;
    for (int i = 0; i < g_nblanks; i++) if (g_blank[i].filled >= 0) n++;
    return n;
}

/* ------------------------------------------------------------------ layout
 * Blanks live inline in the sentence, so the board reads like a sentence
 * with holes in it rather than a form. Layout is recomputed each frame and
 * the hit rectangles fall out of it. */
static Rectangle g_blank_rect[MAX_BLANKS];
static Rectangle g_tok_rect[MAX_POOLS][MAX_TOKENS];

static int blank_by_name(const char *n)
{
    for (int i = 0; i < g_nblanks; i++) if (eq(g_blank[i].name, n)) return i;
    return -1;
}

static float layout_and_draw_text(Rectangle area, bool draw, float size)
{
    float x = area.x, y = area.y;
    float lh = size * text_scale() * 1.75f;

    for (int li = 0; li < g_nlines; li++) {
        const char *p = g_lines[li];
        char tokbuf[128];
        x = area.x;
        while (*p) {
            int n = 0;
            while (p[n] && p[n] != ' ') n++;
            if (n > 127) n = 127;
            memcpy(tokbuf, p, (size_t)n);
            tokbuf[n] = 0;
            p += n;
            while (*p == ' ') p++;

            if (tokbuf[0] == '{') {
                char nm[16];
                int m = 0;
                for (int i = 1; tokbuf[i] && tokbuf[i] != '}' && m < 15; i++) nm[m++] = tokbuf[i];
                nm[m] = 0;
                /* punctuation glued to the placeholder, e.g. "{p3}." */
                const char *tail = strchr(tokbuf, '}');
                tail = tail ? tail + 1 : "";

                int bi = blank_by_name(nm);
                const char *lab = (bi >= 0) ? blank_label(&g_blank[bi]) : NULL;
                float w = fmaxf(150.0f, lab ? art_text_w(lab, size) + 22 : 0);
                if (x + w > area.x + area.width) { x = area.x; y += lh; }
                Rectangle r = { x, y - 4, w, size * text_scale() + 12 };
                if (bi >= 0) g_blank_rect[bi] = r;

                if (draw) {
                    bool sel = (bi == g_sel);
                    Color under = sel ? col_accent_b() : col_ink_soft();
                    if (bi >= 0 && g_blank[bi].locked) under = col_accent_a();
                    ink_line(r.x, r.y + r.height, r.x + r.width, r.y + r.height,
                             sel ? 3.4f : 2.2f, 0.6f, 500 + bi, under);
                    if (lab) art_text(lab, r.x + 10, r.y + 4, size, col_ink());
                    else if (sel) {
                        float a = 0.5f + 0.5f * sinf(g_time * 4.0f);
                        Color c = col_accent_b();
                        c.a = (unsigned char)(120 + 100 * a);
                        art_text("?", r.x + r.width * 0.5f - 6, r.y + 4, size, c);
                    }
                }
                x += w + 8;
                if (tail[0] && draw) {
                    art_text(tail, x - 6, y, size, col_ink());
                    x += art_text_w(tail, size);
                }
            } else {
                float w = art_text_w(tokbuf, size);
                if (x + w > area.x + area.width) { x = area.x; y += lh; }
                if (draw) art_text(tokbuf, x, y, size, col_ink());
                x += w + art_text_w(" ", size);
            }
        }
        y += lh;
    }
    return y - area.y;
}

/* Lays the evidence chips out at `size`; returns the height they need. With
 * `draw` false it only measures, which is how the caller finds a size that
 * fits before anything is on screen. */
static float layout_pools(Rectangle pools, float size, bool draw)
{
    float row = size * text_scale() + 16.0f;
    float y = pools.y + 56;
    for (int p = 0; p < g_npools; p++) {
        if (draw) art_text(g_pool[p].name, pools.x + 22, y, size - 1, col_ink_soft());
        y += size * text_scale() + 8.0f;
        float x = pools.x + 22;
        for (int t = 0; t < g_pool[p].n; t++) {
            const char *lab = ui_str(g_pool[p].tok[t]);
            float w = art_text_w(lab, size) + 20;
            if (x + w > pools.x + pools.width - 22) { x = pools.x + 22; y += row + 6.0f; }
            Rectangle r = { x, y, w, row };
            g_tok_rect[p][t] = r;

            if (draw) {
                bool used = false;
                for (int i = 0; i < g_nblanks; i++)
                    if (eq(g_blank[i].pool, g_pool[p].name) && g_blank[i].filled == t)
                        used = true;
                bool hot = art_hover(r);
                Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                                 { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
                ink_fill(q, 4, used ? HATCH_DIAG : HATCH_NONE, 700 + p * 20 + t,
                         used ? col_paper_dark() : (hot ? col_accent_a()
                                                        : (Color){ 232, 224, 206, 255 }));
                ink_rect(r, 1.8f, 0.7f, 720 + p * 20 + t, col_ink_soft());
                art_text(lab, r.x + 10, r.y + 6, size, used ? col_ink_soft() : col_ink());
            }
            x += w + 8;
        }
        y += row + 14.0f;
    }
    return y - pools.y;
}

/* --------------------------------------------------------------- hints */
static const char *g_hint_msg;
static char g_hint_buf[256];

static void spend_hint(int bi)
{
    Blank *bl = &g_blank[bi];
    if (bl->hint_tier >= 3) { g_hint_msg = ui_str("hint.exhausted"); return; }
    if (flag_get("feathers") <= 0) { g_hint_msg = ui_str("hint.nofeathers"); return; }
    flag_add("feathers", -1);
    bl->hint_tier++;
    sfx_play(SFX_SPARKLE);

    if (bl->hint_tier == 1) {
        snprintf(g_hint_buf, sizeof g_hint_buf, "%s %s.",
                 ui_str("hint.tier1"), bl->pool);
    } else if (bl->hint_tier == 2) {
        if (bl->nfrom > 0)
            snprintf(g_hint_buf, sizeof g_hint_buf, "%s %s",
                     ui_str("hint.tier2"), ui_str(bl->from[0]));
        else
            snprintf(g_hint_buf, sizeof g_hint_buf, "%s", ui_str("hint.tier2"));
    } else {
        Pool *pl = pool_by_name(bl->pool);
        if (pl) for (int i = 0; i < pl->n; i++)
            if (eq(pl->tok[i], bl->answer)) { bl->filled = i; bl->locked = true; }
        snprintf(g_hint_buf, sizeof g_hint_buf, "%s", ui_str("hint.tier3"));
    }
    g_hint_msg = g_hint_buf;
}

/* --------------------------------------------------------------- update */
static Rectangle g_deduce_rect, g_hint_rect, g_leave_rect;

board_status board_update(float dt)
{
    g_time += dt;
    if (g_flash > 0) g_flash -= dt;
    if (g_solved) {
        if (g_flash <= 0) return BOARD_SOLVED;
        return BOARD_RUNNING;
    }

    Vector2 m = art_mouse();
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (IsKeyPressed(KEY_ESCAPE)) return BOARD_EXITED;
    if (click && CheckCollisionPointRec(m, g_leave_rect)) {
        sfx_play(SFX_CLICK);
        return BOARD_EXITED;
    }

    /* pick a blank */
    for (int i = 0; i < g_nblanks; i++) {
        if (!CheckCollisionPointRec(m, g_blank_rect[i])) continue;
        if (click) {
            sfx_play(SFX_TICK);
            if (g_blank[i].locked) { g_sel = i; break; }
            if (g_sel == i && g_blank[i].filled >= 0) g_blank[i].filled = -1;
            else g_sel = i;
        }
        break;
    }

    /* pick a token: fills the selected blank, or the first empty one */
    for (int p = 0; p < g_npools; p++) {
        for (int t = 0; t < g_pool[p].n; t++) {
            if (!CheckCollisionPointRec(m, g_tok_rect[p][t])) continue;
            if (!click) break;
            int target = g_sel;
            if (target < 0 || !eq(g_blank[target].pool, g_pool[p].name)) {
                target = -1;
                for (int i = 0; i < g_nblanks; i++)
                    if (g_blank[i].filled < 0 && !g_blank[i].locked &&
                        eq(g_blank[i].pool, g_pool[p].name)) { target = i; break; }
            }
            if (target < 0) { sfx_play(SFX_CLUNK); break; }
            if (g_blank[target].locked) { sfx_play(SFX_CLUNK); break; }
            /* a token can only sit in one blank at a time */
            for (int i = 0; i < g_nblanks; i++)
                if (i != target && eq(g_blank[i].pool, g_pool[p].name) &&
                    g_blank[i].filled == t) g_blank[i].filled = -1;
            g_blank[target].filled = t;
            g_sel = -1;
            g_last_correct = -1;
            sfx_play(SFX_TICK);
            break;
        }
    }

    if (click && g_sel >= 0 && CheckCollisionPointRec(m, g_hint_rect))
        spend_hint(g_sel);

    /* Deduce! — only when every blank is committed (anti-brute-force, §3.3) */
    bool ready = filled_count() == g_nblanks;
    if (click && ready && CheckCollisionPointRec(m, g_deduce_rect)) {
        g_attempts++;
        int right = 0;
        for (int i = 0; i < g_nblanks; i++) if (blank_correct(&g_blank[i])) right++;
        g_last_correct = right;
        if (right == g_nblanks) {
            g_solved = true;
            g_flash = 1.4f;
            char key[FLAG_MAX_KEY];
            snprintf(key, sizeof key, "board.%s", g_id);
            flag_set(key, 1);
            flag_add("boards_solved", 1);
            sfx_play(SFX_DEDUCE);
        } else {
            g_flash = 0.6f;
            sfx_play(SFX_CLUNK);
        }
    }
    return BOARD_RUNNING;
}

/* ----------------------------------------------------------------- draw */
void board_draw(void)
{
    paper_grain(0.5f);

    Rectangle scroll = { 52, 60, 700, 520 };
    paper_panel(scroll, 5.0f, 8181);

    const char *btitle = g_title[0] ? g_title : "Case Board";
    art_text(btitle, scroll.x + 26, scroll.y + 22,
             art_text_size_for(btitle, scroll.width - 52, 24), col_ink());
    ink_line(scroll.x + 24, scroll.y + 60, scroll.x + scroll.width - 24,
             scroll.y + 60, 2.0f, 1.2f, 8182, col_ink_soft());

    Rectangle text_area = { scroll.x + 30, scroll.y + 86, scroll.width - 60, 396 };
    /* Every blank has to be on screen at once or the sentence is unreadable,
     * so the scroll shrinks its type rather than paging or clipping. */
    float size = 21.0f;
    while (size > 12.0f && layout_and_draw_text(text_area, false, size) > text_area.height)
        size -= 1.0f;
    layout_and_draw_text(text_area, true, size);

    /* --- the pools ---
     * Every token has to be visible at once or the player cannot deduce, so
     * the chips step their type down until the whole set fits the panel. */
    Rectangle pools = { 786, 60, 442, 520 };
    paper_panel(pools, 4.0f, 8183);
    art_text("Evidence", pools.x + 22, pools.y + 18,
             art_text_size_for("Evidence", pools.width - 44, 20), col_ink());

    float chip = 16.0f;
    while (chip > 10.0f && layout_pools(pools, chip, false) > pools.height - 70)
        chip -= 1.0f;
    layout_pools(pools, chip, true);

    /* --- controls --- */
    bool ready = filled_count() == g_nblanks;
    g_deduce_rect = (Rectangle){ 52, 606, 240, 60 };
    g_hint_rect   = (Rectangle){ 312, 606, 210, 60 };
    g_leave_rect  = (Rectangle){ 1060, 606, 168, 60 };

    pz_button(g_deduce_rect, ui_str("board.deduce"), ready, 8190);
    char hb[64];
    snprintf(hb, sizeof hb, "%s (%d)", ui_str("board.hint"), flag_get("feathers"));
    pz_button(g_hint_rect, hb, g_sel >= 0 && flag_get("feathers") > 0, 8192);
    pz_button(g_leave_rect, ui_str("board.leave"), true, 8194);

    /* --- feedback: enough to keep momentum, not enough to guess through --- */
    char msg[192];
    if (g_solved) {
        snprintf(msg, sizeof msg, "%s", ui_str("board.solved"));
    } else if (g_last_correct >= 0) {
        snprintf(msg, sizeof msg, "%d of %d are correct.", g_last_correct, g_nblanks);
    } else if (g_hint_msg) {
        snprintf(msg, sizeof msg, "%s", g_hint_msg);
    } else if (!ready) {
        snprintf(msg, sizeof msg, "%s", ui_str("board.fillall"));
    } else {
        snprintf(msg, sizeof msg, "%s", ui_str("board.ready"));
    }
    Color mc = g_solved ? col_cool() : (g_flash > 0 && g_last_correct >= 0
                                        ? col_accent_b() : col_ink());
    art_text_fit(msg, (Rectangle){ 542, 600, 500, 46 }, 18, mc);

    if (g_attempts > 0 && !g_solved) {
        char a[48];
        snprintf(a, sizeof a, "attempt %d — no penalty, ever", g_attempts);
        art_text(a, 548, 654, 13, col_ink_soft());
    }

    vignette(0.5f);
}
