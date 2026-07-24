#include "puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"
#include "flags/flags.h"
#include "content/content.h"
#include "save/save.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define PZ_MAX 32

static const pz_def *g_reg[PZ_MAX];
static int           g_nreg;

void pz_register(const pz_def *def)
{
    if (g_nreg < PZ_MAX && def && def->id) g_reg[g_nreg++] = def;
}

int puzzle_count(void) { return g_nreg; }
const pz_def *puzzle_at(int i) { return (i >= 0 && i < g_nreg) ? g_reg[i] : NULL; }

const pz_def *puzzle_find(const char *id)
{
    for (int i = 0; i < g_nreg; i++)
        if (strcmp(g_reg[i]->id, id) == 0) return g_reg[i];
    return NULL;
}

/* ------------------------------------------------------------------- host */
static const pz_def *g_cur;
static pz_ctx        g_ctx;
static const char   *g_hint_text;
static bool          g_skipped;
static float         g_solve_flash;
static bool          g_solved;

static Rectangle R_HINT  = { 150, 596, 240, 60 };
static Rectangle R_SKIP  = { 410, 596, 240, 60 };
static Rectangle R_LEAVE = { 1000, 596, 180, 60 };

const char *puzzle_current_id(void) { return g_cur ? g_cur->id : ""; }
const char *puzzle_clue(void) { return (g_cur && g_cur->clue_granted) ? g_cur->clue_granted : ""; }
bool puzzle_was_skipped(void) { return g_skipped; }

bool puzzle_start(const char *id, int difficulty, unsigned seed)
{
    const pz_def *d = puzzle_find(id);
    if (!d) return false;
    g_cur = d;
    memset(&g_ctx, 0, sizeof g_ctx);
    g_ctx.seed = seed ? seed : 0x9e3779b9u;
    g_ctx.difficulty = difficulty;
    g_ctx.area = (Rectangle){ 170, 168, 940, 392 };
    g_hint_text = NULL;
    g_skipped = false;
    g_solved = false;
    g_solve_flash = 0;
    if (d->init) d->init(&g_ctx);
    music_mood(MOOD_PUZZLE);
    return true;
}

void pz_attempt_failed(pz_ctx *ctx)
{
    ctx->attempts++;
    sfx_play(SFX_CLUNK);
}

void pz_button(Rectangle r, const char *label, bool enabled, int seed)
{
    bool hot = enabled && art_hover(r);
    Vector2 p[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                     { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
    Color fill = !enabled ? col_paper_dark()
               : hot      ? col_accent_a()
                          : (Color){ 230, 219, 196, 255 };
    ink_fill(p, 4, HATCH_NONE, seed, fill);
    ink_rect(r, 2.2f, 0.8f, seed + 1, enabled ? col_ink() : col_ink_soft());
    float w = art_text_w(label, 18);
    art_text(label, r.x + (r.width - w) * 0.5f, r.y + r.height * 0.5f - 12, 18,
             enabled ? col_ink() : col_ink_soft());
}

bool pz_button_clicked(Rectangle r, bool enabled)
{
    if (!enabled) return false;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
    if (!art_hover(r)) return false;
    sfx_play(SFX_CLICK);
    return true;
}

static void spend_hint(void)
{
    if (!g_cur || g_ctx.hint_tier >= 3) { g_hint_text = ui_str("hint.exhausted"); return; }
    if (flag_get("feathers") <= 0) { g_hint_text = ui_str("hint.nofeathers"); return; }
    flag_add("feathers", -1);
    g_ctx.hint_tier++;
    sfx_play(SFX_SPARKLE);
    g_hint_text = g_cur->hint ? g_cur->hint(&g_ctx, g_ctx.hint_tier) : NULL;
    if (!g_hint_text) g_hint_text = ui_str("hint.none");
}

pz_status puzzle_update(float dt)
{
    if (!g_cur) return PZ_EXITED;
    g_ctx.t += dt;

    if (g_solved) {
        g_solve_flash -= dt;
        if (g_solve_flash <= 0) return PZ_SOLVED;
        return PZ_RUNNING;
    }

    if (IsKeyPressed(KEY_ESCAPE)) return PZ_EXITED;
    if (pz_button_clicked(R_LEAVE, true)) return PZ_EXITED;
    if (pz_button_clicked(R_HINT, flag_get("feathers") > 0 && g_ctx.hint_tier < 3))
        spend_hint();
    /* three honest attempts, then a free skip that costs nothing (§1.2.1) */
    if (pz_button_clicked(R_SKIP, g_ctx.attempts >= 3)) {
        g_skipped = true;
        g_solved = true;
        g_solve_flash = 0.9f;
        sfx_play(SFX_SOLVE);
        return PZ_RUNNING;
    }

    pz_status s = g_cur->update ? g_cur->update(&g_ctx, dt) : PZ_RUNNING;
    if (s == PZ_SOLVED) {
        g_solved = true;
        g_solve_flash = 1.3f;
        sfx_play(SFX_SOLVE);
        return PZ_RUNNING;
    }
    return s;
}

void puzzle_draw(void)
{
    if (!g_cur) return;
    paper_grain(0.5f);

    Rectangle frame = { 140, 70, 1000, 500 };
    paper_panel(frame, 5.0f, 6161);
    art_text(g_cur->title ? g_cur->title : g_cur->id, frame.x + 26, frame.y + 20, 23,
             col_ink());
    ink_line(frame.x + 24, frame.y + 58, frame.x + frame.width - 24, frame.y + 58,
             2.0f, 1.2f, 6162, col_ink_soft());

    if (g_cur->draw) g_cur->draw(&g_ctx);

    char hb[64];
    snprintf(hb, sizeof hb, "%s (%d)", ui_str("pz.hint"), flag_get("feathers"));
    pz_button(R_HINT, hb, flag_get("feathers") > 0 && g_ctx.hint_tier < 3, 6170);
    pz_button(R_SKIP, ui_str("pz.skip"), g_ctx.attempts >= 3, 6172);
    pz_button(R_LEAVE, ui_str("pz.leave"), true, 6174);

    const char *msg = g_hint_text;
    char buf[256];
    if (g_solved) {
        snprintf(buf, sizeof buf, "%s",
                 g_skipped ? ui_str("pz.skipped") : ui_str("pz.solved"));
        msg = buf;
    } else if (!msg) {
        if (g_ctx.attempts >= 3) msg = ui_str("pz.skiphint");
        else                     msg = ui_str("pz.tryit");
    }
    art_text_wrap(msg, 676, 600, 310, 15, g_solved ? col_cool() : col_ink(), -1);

    if (g_ctx.attempts > 0 && !g_solved) {
        char a[64];
        snprintf(a, sizeof a, "attempts: %d", g_ctx.attempts);
        art_text(a, 676, 654, 13, col_ink_soft());
    }
    vignette(0.5f);
}
