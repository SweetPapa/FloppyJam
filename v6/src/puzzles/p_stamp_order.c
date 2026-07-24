/* Mayor Aurelius Grand — the stamp order (§5.1: logic/deduction-lite).
 *
 * To view a municipal ledger one must first stamp six forms, and the forms
 * have opinions about one another. Played entirely for comedy; solved
 * entirely by reading. Click two forms to swap them.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define NFORM 6

typedef struct {
    int  order[NFORM];        /* order[slot] = form id */
    int  answer[NFORM];
    int  sel;                 /* selected slot, -1 none */
} Desk;

static Desk D;

static const char *FORM[NFORM] = {
    "Form A: Request to Request",
    "Form B: Declaration of Intent to Ask",
    "Form C: Confirmation of the Above",
    "Form D: Notice of Confirmation",
    "Form E: Acknowledgement of Notice",
    "Form F: The Actual Question",
};
static char g_rule[4][110];

static bool solved(void)
{
    for (int i = 0; i < NFORM; i++) if (D.order[i] != D.answer[i]) return false;
    return true;
}

static void init(pz_ctx *ctx)
{
    memset(&D, 0, sizeof D);
    D.sel = -1;
    int a[NFORM] = { 1, 3, 0, 5, 2, 4 };      /* the one true bureaucratic order */
    memcpy(D.answer, a, sizeof a);
    int start[NFORM] = { 0, 1, 2, 3, 4, 5 };
    unsigned r = ctx->seed | 1u;
    for (int i = NFORM - 1; i > 0; i--) {
        r = r * 1664525u + 1013904223u;
        int j = (int)(r >> 16) % (i + 1);
        int t = start[i]; start[i] = start[j]; start[j] = t;
    }
    memcpy(D.order, start, sizeof start);

    static const char L[NFORM] = { 'A', 'B', 'C', 'D', 'E', 'F' };
    snprintf(g_rule[0], 110, "Form %c must be stamped first. It says so on Form %c.",
             L[a[0]], L[a[0]]);
    snprintf(g_rule[1], 110, "Form %c may not be stamped until Form %c has been.",
             L[a[2]], L[a[1]]);
    snprintf(g_rule[2], 110, "Form %c comes immediately after Form %c. Immediately.",
             L[a[3]], L[a[2]]);
    snprintf(g_rule[3], 110, "Form %c is last, and Form %c is the one before it.",
             L[a[5]], L[a[4]]);
}

static Rectangle slot(pz_ctx *ctx, int i)
{
    return (Rectangle){ ctx->area.x + 40, ctx->area.y + 130 + i * 52.0f,
                        ctx->area.width - 80, 44 };
}
static Rectangle R_STAMP(pz_ctx *c)
{
    return (Rectangle){ c->area.x + c->area.width - 300, c->area.y + 56, 280, 52 };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    if (pz_button_clicked(R_STAMP(ctx), true)) {
        if (solved()) return PZ_SOLVED;
        pz_attempt_failed(ctx);
        return PZ_RUNNING;
    }
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();
    for (int i = 0; i < NFORM; i++) {
        if (!CheckCollisionPointRec(m, slot(ctx, i))) continue;
        if (D.sel < 0) { D.sel = i; sfx_play(SFX_TICK); }
        else if (D.sel == i) { D.sel = -1; }
        else {
            int t = D.order[i]; D.order[i] = D.order[D.sel]; D.order[D.sel] = t;
            D.sel = -1;
            sfx_play(SFX_CHIME);
        }
        return PZ_RUNNING;
    }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("Click two forms to swap them, then present them to the Mayor.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());
    for (int i = 0; i < 4; i++)
        art_text(g_rule[i], ctx->area.x + 20, ctx->area.y + 10 + i * 24.0f, 14,
                 col_ink_soft());

    for (int i = 0; i < NFORM; i++) {
        Rectangle r = slot(ctx, i);
        Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                         { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
        ink_fill(q, 4, HATCH_NONE, 2400 + i,
                 i == D.sel ? (Color){ 232, 200, 110, 255 } : (Color){ 238, 231, 214, 255 });
        ink_rect(r, 2.0f, 0.7f, 2420 + i, col_ink_soft());
        char n[8];
        snprintf(n, sizeof n, "%d.", i + 1);
        art_text(n, r.x + 12, r.y + 10, 17, col_ink_soft());
        art_text(FORM[D.order[i]], r.x + 50, r.y + 10, 17, col_ink());
    }
    pz_button(R_STAMP(ctx), "Present them, with feeling", true, 2440);
    doodle(D_ENVELOPE, ctx->area.x + ctx->area.width - 60, ctx->area.y + 400, 30, 0.2f,
           col_ink_soft());
}

static const char *hint(pz_ctx *ctx, int tier)
{
    static char buf[128];
    switch (tier) {
    case 1: return "Four rules, six forms. Place the two the rules pin down "
                   "absolutely — first and last — and work inward.";
    case 2: return "'Immediately after' is the strong one: those two forms are "
                   "glued together and move as a pair.";
    default: {
        static const char L[NFORM] = { 'A', 'B', 'C', 'D', 'E', 'F' };
        snprintf(buf, sizeof buf, "%c, %c, %c, %c, %c, %c.",
                 L[D.answer[0]], L[D.answer[1]], L[D.answer[2]],
                 L[D.answer[3]], L[D.answer[4]], L[D.answer[5]]);
        return buf;
    }
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    memcpy(D.order, D.answer, sizeof D.order);
    return solved();
}

static const pz_def def = {
    .id = "stamp_order",
    .title = "The Mayor's Six Forms",
    .clue_granted = "clue.budget_cut",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
