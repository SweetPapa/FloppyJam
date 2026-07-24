/* Otto Brine — weights & sorting (§5.1: logic/deduction-lite).
 *
 * Five crates of fish, no scale markings, one honest pan balance. Compare as
 * often as you like — Otto has all morning — then lay them out lightest to
 * heaviest. Nothing here is timed and nothing is taken away.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define NCRATE 5

typedef struct {
    int  weight[NCRATE];      /* hidden truth */
    int  slot[NCRATE];        /* slot i holds crate index, -1 empty */
    int  held;                /* crate being carried, -1 none */
    int  panL, panR;          /* crates on the balance, -1 empty */
    int  weighings;
    float tilt, tilt_target;
} Scale;

static Scale S;

static bool solved(void)
{
    for (int i = 0; i < NCRATE; i++) if (S.slot[i] < 0) return false;
    for (int i = 1; i < NCRATE; i++)
        if (S.weight[S.slot[i - 1]] > S.weight[S.slot[i]]) return false;
    return true;
}

static void init(pz_ctx *ctx)
{
    memset(&S, 0, sizeof S);
    S.held = S.panL = S.panR = -1;
    /* deterministic from the save seed: re-entering shows the same crates */
    int base[NCRATE] = { 3, 7, 11, 16, 22 };
    unsigned r = ctx->seed | 1u;
    for (int i = 0; i < NCRATE; i++) S.weight[i] = base[i];
    for (int i = NCRATE - 1; i > 0; i--) {
        r = r * 1664525u + 1013904223u;
        int j = (int)(r >> 16) % (i + 1);
        int t = S.weight[i]; S.weight[i] = S.weight[j]; S.weight[j] = t;
    }
    for (int i = 0; i < NCRATE; i++) S.slot[i] = -1;
}

static Rectangle crate_rect(pz_ctx *ctx, int i)
{
    return (Rectangle){ ctx->area.x + 40 + i * 108.0f, ctx->area.y + 46, 88, 76 };
}
static Rectangle slot_rect(pz_ctx *ctx, int i)
{
    return (Rectangle){ ctx->area.x + 200 + i * 108.0f,
                        ctx->area.y + ctx->area.height - 116, 88, 76 };
}
static Rectangle pan_rect(pz_ctx *ctx, int side)
{
    return (Rectangle){ ctx->area.x + (side ? 620 : 300),
                        ctx->area.y + 200 + (side ? -S.tilt : S.tilt) * 26.0f, 96, 62 };
}

static bool loose(int c)
{
    if (S.held == c || S.panL == c || S.panR == c) return false;
    for (int i = 0; i < NCRATE; i++) if (S.slot[i] == c) return false;
    return true;
}

static void pick_up(int c)
{
    if (S.panL == c) S.panL = -1;
    if (S.panR == c) S.panR = -1;
    for (int i = 0; i < NCRATE; i++) if (S.slot[i] == c) S.slot[i] = -1;
    S.held = c;
    sfx_play(SFX_TICK);
}

static pz_status update(pz_ctx *ctx, float dt)
{
    S.tilt += (S.tilt_target - S.tilt) * fminf(1.0f, dt * 6.0f);
    if (S.panL >= 0 && S.panR >= 0)
        S.tilt_target = (S.weight[S.panL] > S.weight[S.panR]) ? 1.0f : -1.0f;
    else
        S.tilt_target = 0.0f;

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();

    for (int i = 0; i < NCRATE; i++) {
        if (loose(i) && CheckCollisionPointRec(m, crate_rect(ctx, i))) { pick_up(i); return PZ_RUNNING; }
        if (S.slot[i] >= 0 && CheckCollisionPointRec(m, slot_rect(ctx, i))) {
            pick_up(S.slot[i]);
            return PZ_RUNNING;
        }
    }
    for (int side = 0; side < 2; side++) {
        if (!CheckCollisionPointRec(m, pan_rect(ctx, side))) continue;
        int *pan = side ? &S.panR : &S.panL;
        if (S.held >= 0) {
            if (*pan >= 0) return PZ_RUNNING;
            *pan = S.held; S.held = -1;
            sfx_play(SFX_CHIME);
            if (S.panL >= 0 && S.panR >= 0) S.weighings++;
        } else if (*pan >= 0) {
            pick_up(*pan);
        }
        return PZ_RUNNING;
    }
    for (int i = 0; i < NCRATE; i++) {
        if (!CheckCollisionPointRec(m, slot_rect(ctx, i))) continue;
        if (S.held >= 0 && S.slot[i] < 0) {
            S.slot[i] = S.held;
            S.held = -1;
            sfx_play(SFX_CHIME);
            if (solved()) return PZ_SOLVED;
            bool full = true;
            for (int k = 0; k < NCRATE; k++) if (S.slot[k] < 0) full = false;
            if (full) pz_attempt_failed(ctx);
        }
        return PZ_RUNNING;
    }
    return PZ_RUNNING;
}

static void crate(Rectangle r, int i, bool ghost)
{
    Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                     { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
    ink_fill(q, 4, ghost ? HATCH_DIAG : HATCH_NONE, 300 + i,
             ghost ? col_paper_dark() : (Color){ 196, 168, 120, 255 });
    ink_rect(r, 2.4f, 0.9f, 320 + i, col_ink());
    doodle(D_FISH, r.x + r.width * 0.5f, r.y + r.height * 0.52f, 20, 0.1f, col_sea());
    char lab[4] = { (char)('A' + i), 0, 0, 0 };
    art_text(lab, r.x + 8, r.y + 4, 16, col_ink());
}

static void draw(pz_ctx *ctx)
{
    art_text("Weigh as much as you like, then lay them out lightest to heaviest.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    for (int i = 0; i < NCRATE; i++)
        if (loose(i)) crate(crate_rect(ctx, i), i, false);

    /* the balance */
    float bx = ctx->area.x + 500, by = ctx->area.y + 160;
    ink_line(bx, by + 190, bx, by, 6.0f, 0.9f, 340, col_ink());
    Rectangle pl = pan_rect(ctx, 0), pr = pan_rect(ctx, 1);
    ink_line(pl.x + pl.width * 0.5f, pl.y, pr.x + pr.width * 0.5f, pr.y, 5.0f, 0.9f,
             341, col_ink());
    for (int side = 0; side < 2; side++) {
        Rectangle p = pan_rect(ctx, side);
        Vector2 q[4] = { { p.x, p.y }, { p.x + p.width, p.y },
                         { p.x + p.width - 12, p.y + p.height }, { p.x + 12, p.y + p.height } };
        ink_fill(q, 4, HATCH_NONE, 350 + side, (Color){ 176, 172, 164, 255 });
        ink_stroke((Vector2[]){ q[0], q[1], q[2], q[3], q[0] }, 5, 2.2f, 0.8f,
                   360 + side, col_ink());
        int c = side ? S.panR : S.panL;
        if (c >= 0) crate((Rectangle){ p.x + 4, p.y - 70, 88, 66 }, c, false);
    }

    for (int i = 0; i < NCRATE; i++) {
        Rectangle r = slot_rect(ctx, i);
        ink_rect(r, 2.0f, 0.8f, 380 + i, col_ink_soft());
        char n[8];
        snprintf(n, sizeof n, "%d", i + 1);
        art_text(n, r.x + r.width * 0.5f - 5, r.y + r.height + 4, 15, col_ink_soft());
        if (S.slot[i] >= 0) crate(r, S.slot[i], false);
    }

    if (S.held >= 0) {
        Vector2 m = art_mouse();
        crate((Rectangle){ m.x - 44, m.y - 38, 88, 76 }, S.held, false);
    }
    char b[80];
    snprintf(b, sizeof b, "weighings: %d   (Otto is in no hurry)", S.weighings);
    art_text(b, ctx->area.x, ctx->area.y + ctx->area.height - 8, 15, col_ink_soft());
}

static const char *hint(pz_ctx *ctx, int tier)
{
    switch (tier) {
    case 1: return "Put one crate on each pan. The heavier side sinks. That is all "
                   "the information there is, and it is enough.";
    case 2: return "Sort by insertion: place A in slot 1, then weigh B against it "
                   "and slide B in on the correct side. Repeat. Seven weighings "
                   "will always do it.";
    default: return "Weigh every pair you are unsure of. If X sinks against Y, X "
                    "goes to the right of Y on the bench.";
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    int order[NCRATE];
    for (int i = 0; i < NCRATE; i++) order[i] = i;
    for (int i = 1; i < NCRATE; i++) {          /* insertion sort by "weighing" */
        int k = order[i], j = i - 1;
        while (j >= 0 && S.weight[order[j]] > S.weight[k]) { order[j + 1] = order[j]; j--; }
        order[j + 1] = k;
    }
    for (int i = 0; i < NCRATE; i++) S.slot[i] = order[i];
    return solved();
}

static const pz_def def = {
    .id = "weights_sorting",
    .title = "Otto's Morning Catch",
    .clue_granted = "clue.otto_alibi",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
