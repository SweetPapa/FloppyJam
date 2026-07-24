/* Mo Hum — the waggle path (§5.1: pattern/memory).
 *
 * "They remember the festival. They dance the way to it." The bees walk a
 * path across the comb; watch it, then walk it back. Untimed on the way in
 * and untimed on the way out — the only rhythm here is optional (§5.1).
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define HW 5
#define HH 4
#define NHEX (HW * HH)
#define MAXSTEP 8

typedef struct {
    int   path[MAXSTEP];
    int   len;
    int   shown;             /* how much of the dance has played */
    float timer;
    int   entered;
    bool  watching;
} Comb;

static Comb B;

static bool solved(void) { return !B.watching && B.entered == B.len; }

static bool adjacent_hex(int a, int b)
{
    int ax = a % HW, ay = a / HW, bx = b % HW, by = b / HW;
    int dy = by - ay;
    if (dy < -1 || dy > 1) return false;
    if (dy == 0) return (bx - ax == 1) || (ax - bx == 1);
    int shift = (ay & 1) ? 0 : -1;      /* odd rows are offset right */
    int dx = bx - ax;
    return dx == shift || dx == shift + 1;
}

static void init(pz_ctx *ctx)
{
    memset(&B, 0, sizeof B);
    B.len = 4 + ctx->difficulty;
    if (B.len > MAXSTEP) B.len = MAXSTEP;
    unsigned r = ctx->seed | 1u;
    r = r * 1664525u + 1013904223u;
    B.path[0] = (int)((r >> 16) % (unsigned)HW);         /* start on the top row */
    for (int i = 1; i < B.len; i++) {
        int cand[6], n = 0;
        for (int c = 0; c < NHEX; c++) {
            if (!adjacent_hex(B.path[i - 1], c)) continue;
            bool seen = false;
            for (int k = 0; k < i; k++) if (B.path[k] == c) seen = true;
            if (!seen) cand[n < 6 ? n++ : 5] = c;
        }
        if (!n) { B.len = i; break; }
        r = r * 1664525u + 1013904223u;
        B.path[i] = cand[(r >> 16) % (unsigned)n];
    }
    B.watching = true;
    B.shown = 0;
    B.timer = 0.6f;
}

static Vector2 hpos(pz_ctx *ctx, int i)
{
    int x = i % HW, y = i / HW;
    float s = fminf((ctx->area.width - 160) / (HW + 0.5f),
                    (ctx->area.height - 40) / (HH * 0.86f + 0.4f));
    float ox = ctx->area.x + (ctx->area.width - (HW + 0.5f) * s) * 0.5f + s * 0.5f
             + ((y & 1) ? s * 0.5f : 0.0f);
    float oy = ctx->area.y + (ctx->area.height - HH * s * 0.86f) * 0.5f + s * 0.4f;
    return (Vector2){ ox + x * s, oy + y * s * 0.86f };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    if (B.watching) {
        B.timer -= dt;
        if (B.timer <= 0) {
            if (B.shown < B.len) {
                B.shown++;
                sfx_play(SFX_TICK);
                B.timer = 0.62f;
            } else {
                B.watching = false;
                B.entered = 0;
            }
        }
        return PZ_RUNNING;
    }
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();
    for (int i = 0; i < NHEX; i++) {
        float rad = fabsf(hpos(ctx, 1).x - hpos(ctx, 0).x) * 0.44f;
        if (!CheckCollisionPointCircle(m, hpos(ctx, i), rad)) continue;
        if (i == B.path[B.entered]) {
            B.entered++;
            sfx_play(SFX_CHIME);
            if (solved()) return PZ_SOLVED;
        } else {
            pz_attempt_failed(ctx);
            B.entered = 0;
            B.watching = true;              /* Mo just shows you again. Always. */
            B.shown = 0;
            B.timer = 0.5f;
        }
        return PZ_RUNNING;
    }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text(B.watching ? "Watch the dance."
                        : "Now walk it back, cell by cell.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    for (int i = 0; i < NHEX; i++) {
        Vector2 p = hpos(ctx, i);
        float rad = fabsf(hpos(ctx, 1).x - hpos(ctx, 0).x) * 0.46f;
        Vector2 hexp[7];
        for (int k = 0; k < 7; k++) {
            float a = (float)k / 6.0f * 2 * 3.14159265f + 0.5236f;
            hexp[k] = (Vector2){ p.x + cosf(a) * rad, p.y + sinf(a) * rad };
        }
        bool lit = false;
        if (B.watching) { for (int k = 0; k < B.shown; k++) if (B.path[k] == i) lit = true; }
        else            { for (int k = 0; k < B.entered; k++) if (B.path[k] == i) lit = true; }

        ink_fill(hexp, 6, HATCH_NONE, 2000 + i,
                 lit ? (Color){ 232, 196, 84, 255 } : (Color){ 236, 228, 208, 255 });
        ink_stroke(hexp, 7, 2.2f, 0.8f, 2040 + i, col_ink_soft());
        if (lit) doodle(D_BEE, p.x, p.y, 18, 0, col_ink());
    }
    char b[64];
    if (B.watching) snprintf(b, sizeof b, "step %d of %d", B.shown, B.len);
    else            snprintf(b, sizeof b, "walked %d of %d", B.entered, B.len);
    art_text(b, ctx->area.x, ctx->area.y + ctx->area.height - 4, 15, col_ink_soft());
}

static const char *hint(pz_ctx *ctx, int tier)
{
    switch (tier) {
    case 1: return "The dance never doubles back on a cell it has already used.";
    case 2: return "Say the turns out loud as they happen — left, left, down-right "
                   "— and the shape sticks better than the cells do.";
    default: return "Get one wrong and Mo simply shows you again, as many times "
                    "as you like. Nothing here is lost.";
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    B.watching = false;
    B.entered = B.len;
    return solved();
}

static const pz_def def = {
    .id = "hex_path",
    .title = "Mo's Waggle Comb",
    .clue_granted = "clue.bees_remember",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
