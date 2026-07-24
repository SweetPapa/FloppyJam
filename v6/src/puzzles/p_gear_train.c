/* Edwina Cogg — gear train (§5.1: mechanical/simulation).
 *
 * The shop clock's train is missing three cogs. Drop cogs onto the empty
 * spindles so the hand turns the right way at the right speed. The train
 * runs live, so you can watch a wrong answer be wrong.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#define NSPINDLE 5
#define NTRAY    5

typedef struct {
    float x, y;
    int   teeth;        /* 0 = empty spindle */
    bool  fixed;
} Spindle;

typedef struct {
    Spindle sp[NSPINDLE];
    int     tray[NTRAY];
    bool    used[NTRAY];
    int     held;       /* tray index, -1 none */
    float   phase;
} Train;

static Train G;

/* The hand must turn clockwise at exactly a quarter of the drive's speed.
 * Ratio through a train is drive_teeth / last_teeth; direction alternates. */
static bool solved(void)
{
    for (int i = 0; i < NSPINDLE; i++) if (G.sp[i].teeth == 0) return false;
    float ratio = (float)G.sp[0].teeth / (float)G.sp[NSPINDLE - 1].teeth;
    bool clockwise = ((NSPINDLE - 1) % 2) == 0;
    return clockwise && fabsf(ratio - 0.25f) < 0.001f;
}

static void init(pz_ctx *ctx)
{
    memset(&G, 0, sizeof G);
    G.held = -1;
    float xs[NSPINDLE] = { 0.10f, 0.30f, 0.50f, 0.70f, 0.90f };
    float ys[NSPINDLE] = { 0.55f, 0.32f, 0.58f, 0.30f, 0.56f };
    for (int i = 0; i < NSPINDLE; i++) { G.sp[i].x = xs[i]; G.sp[i].y = ys[i]; }
    G.sp[0].teeth = 12; G.sp[0].fixed = true;          /* the drive  */
    G.sp[2].teeth = 24; G.sp[2].fixed = true;          /* the idler  */
    int tray[NTRAY] = { 16, 48, 20, 36, 30 };
    memcpy(G.tray, tray, sizeof tray);
}

static Vector2 spos(pz_ctx *ctx, int i)
{
    return (Vector2){ ctx->area.x + G.sp[i].x * ctx->area.width,
                      ctx->area.y + G.sp[i].y * (ctx->area.height - 120) + 30 };
}
static Rectangle tray_rect(pz_ctx *ctx, int i)
{
    return (Rectangle){ ctx->area.x + 120 + i * 140.0f,
                        ctx->area.y + ctx->area.height - 108, 108, 92 };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    G.phase += dt;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();

    for (int i = 0; i < NSPINDLE; i++) {
        if (G.sp[i].fixed) continue;
        if (!CheckCollisionPointCircle(m, spos(ctx, i), 52)) continue;
        if (G.held >= 0) {
            if (G.sp[i].teeth) {                       /* swap back to the tray */
                for (int k = 0; k < NTRAY; k++)
                    if (G.used[k] && G.tray[k] == G.sp[i].teeth) { G.used[k] = false; break; }
            }
            G.sp[i].teeth = G.tray[G.held];
            G.used[G.held] = true;
            G.held = -1;
            sfx_play(SFX_CHIME);
            if (solved()) return PZ_SOLVED;
            bool full = true;
            for (int k = 0; k < NSPINDLE; k++) if (!G.sp[k].teeth) full = false;
            if (full) pz_attempt_failed(ctx);
        } else if (G.sp[i].teeth) {
            for (int k = 0; k < NTRAY; k++)
                if (G.used[k] && G.tray[k] == G.sp[i].teeth) { G.used[k] = false; break; }
            G.sp[i].teeth = 0;
            sfx_play(SFX_TICK);
        }
        return PZ_RUNNING;
    }
    for (int i = 0; i < NTRAY; i++) {
        if (G.used[i] || !CheckCollisionPointRec(m, tray_rect(ctx, i))) continue;
        G.held = (G.held == i) ? -1 : i;
        sfx_play(SFX_TICK);
        return PZ_RUNNING;
    }
    return PZ_RUNNING;
}

static void gear(float x, float y, int teeth, float ang, Color c)
{
    float r = 16.0f + teeth * 0.85f;
    doodle(D_GEAR, x, y, r, ang, c);
    char t[8];
    snprintf(t, sizeof t, "%d", teeth);
    float w = art_text_w(t, 15);
    art_text(t, x - w * 0.5f, y - 8, 15, col_ink());
}

static void draw(pz_ctx *ctx)
{
    art_text("The hand must turn clockwise at a quarter of the drive's speed.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    float w = 1.0f;
    for (int i = 0; i < NSPINDLE; i++) {
        Vector2 p = spos(ctx, i);
        if (i > 0 && G.sp[i - 1].teeth && G.sp[i].teeth) {
            Vector2 q = spos(ctx, i - 1);
            ink_line(q.x, q.y, p.x, p.y, 1.6f, 1.0f, 1400 + i, col_paper_dark());
            w = -w * (float)G.sp[i - 1].teeth / (float)G.sp[i].teeth;
        }
        if (G.sp[i].teeth)
            gear(p.x, p.y, G.sp[i].teeth, G.phase * w * 1.4f,
                 G.sp[i].fixed ? (Color){ 150, 146, 140, 255 }
                               : (Color){ 214, 176, 72, 255 });
        else {
            ink_circle(p.x, p.y, 22, 2.2f, 0.8f, 1420 + i, col_ink_soft());
            doodle(D_STAR, p.x, p.y, 8, 0, col_ink_soft());
        }
    }
    /* the hand, driven by the last cog */
    Vector2 last = spos(ctx, NSPINDLE - 1);
    if (G.sp[NSPINDLE - 1].teeth) {
        float a = G.phase * w * 1.4f;
        ink_line(last.x, last.y, last.x + cosf(a) * 60, last.y + sinf(a) * 60, 4.0f,
                 0.6f, 1440, col_accent_b());
    }

    for (int i = 0; i < NTRAY; i++) {
        if (G.used[i]) continue;
        Rectangle r = tray_rect(ctx, i);
        if (G.held == i) ink_rect(r, 2.6f, 0.9f, 1460 + i, col_accent_b());
        gear(r.x + r.width * 0.5f, r.y + r.height * 0.5f, G.tray[i], 0,
             (Color){ 190, 180, 160, 255 });
    }
    char b[96];
    if (G.sp[NSPINDLE - 1].teeth)
        snprintf(b, sizeof b, "hand: %s, %.2fx drive", w < 0 ? "anticlockwise" : "clockwise",
                 (double)fabsf(w));
    else
        snprintf(b, sizeof b, "the train is not connected yet");
    art_text(b, ctx->area.x, ctx->area.y + ctx->area.height - 130, 15, col_ink_soft());
}

static const char *hint(pz_ctx *ctx, int tier)
{
    switch (tier) {
    case 1: return "Each cog turns the next one the other way. Five cogs means "
                   "the last one turns the same way as the first.";
    case 2: return "Only the first and last cogs decide the speed; the ones "
                   "between just carry it along. A quarter speed means the last "
                   "cog has four times the teeth of the drive.";
    default: return "The drive has 12 teeth, so the last spindle needs 48. The "
                    "other two empty spindles will take anything at all.";
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    /* last spindle must be 4x the drive; the middles are free */
    for (int i = 0; i < NTRAY; i++) {
        if (G.tray[i] == G.sp[0].teeth * 4) { G.sp[NSPINDLE - 1].teeth = G.tray[i]; G.used[i] = true; }
    }
    for (int s = 0; s < NSPINDLE; s++) {
        if (G.sp[s].teeth) continue;
        for (int i = 0; i < NTRAY; i++)
            if (!G.used[i]) { G.sp[s].teeth = G.tray[i]; G.used[i] = true; break; }
    }
    return solved();
}

static const pz_def def = {
    .id = "gear_train",
    .title = "Edwina's Idle Clock",
    .clue_granted = "clue.cogg_ledger",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
