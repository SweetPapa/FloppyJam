/* Tansy — shell memory (§5.1: pattern/memory).
 *
 * Tansy lines her shells up on the harbour wall and turns them over one at a
 * time. Find the pairs. There is no clock; she will wait, and she will tell
 * you what she thinks of the magpie while you look.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define COLS 4
#define ROWS 3
#define NCARD (COLS * ROWS)

typedef struct {
    int   face[NCARD];        /* doodle id */
    bool  up[NCARD];
    bool  gone[NCARD];
    int   first, second;
    float settle;             /* seconds left showing a wrong pair */
    int   found;
} Shells;

static Shells H;

static bool solved(void) { return H.found == NCARD / 2; }

static void init(pz_ctx *ctx)
{
    memset(&H, 0, sizeof H);
    H.first = H.second = -1;
    static const int faces[NCARD / 2] = { D_FISH, D_STAR, D_FEATHER, D_WAVE,
                                          D_FLOWER, D_ANCHOR };
    int pool[NCARD];
    for (int i = 0; i < NCARD; i++) pool[i] = faces[i / 2];
    unsigned r = ctx->seed | 1u;
    for (int i = NCARD - 1; i > 0; i--) {
        r = r * 1664525u + 1013904223u;
        int j = (int)(r >> 16) % (i + 1);
        int t = pool[i]; pool[i] = pool[j]; pool[j] = t;
    }
    memcpy(H.face, pool, sizeof pool);
}

static Rectangle card_rect(pz_ctx *ctx, int i)
{
    float w = fminf(160.0f, (ctx->area.width - 200) / COLS - 14);
    float h = fminf(110.0f, (ctx->area.height - 40) / ROWS - 14);
    float ox = ctx->area.x + (ctx->area.width - COLS * (w + 14)) * 0.5f;
    float oy = ctx->area.y + (ctx->area.height - ROWS * (h + 14)) * 0.5f;
    return (Rectangle){ ox + (i % COLS) * (w + 14), oy + (i / COLS) * (h + 14), w, h };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    if (H.settle > 0) {
        H.settle -= dt;
        if (H.settle <= 0) {
            H.up[H.first] = H.up[H.second] = false;
            H.first = H.second = -1;
        }
        return PZ_RUNNING;
    }
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();
    for (int i = 0; i < NCARD; i++) {
        if (H.gone[i] || H.up[i]) continue;
        if (!CheckCollisionPointRec(m, card_rect(ctx, i))) continue;
        H.up[i] = true;
        sfx_play(SFX_TICK);
        if (H.first < 0) { H.first = i; return PZ_RUNNING; }
        H.second = i;
        if (H.face[H.first] == H.face[H.second]) {
            H.gone[H.first] = H.gone[H.second] = true;
            H.first = H.second = -1;
            H.found++;
            sfx_play(SFX_CHIME);
            if (solved()) return PZ_SOLVED;
        } else {
            H.settle = 1.1f;
            pz_attempt_failed(ctx);
        }
        return PZ_RUNNING;
    }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("Turn two shells. If they match, Tansy lets you keep them.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());
    for (int i = 0; i < NCARD; i++) {
        Rectangle r = card_rect(ctx, i);
        if (H.gone[i]) {
            ink_rect(r, 1.6f, 0.8f, 400 + i, col_paper_dark());
            doodle(H.face[i], r.x + r.width * 0.5f, r.y + r.height * 0.5f, 24, 0,
                   col_paper_dark());
            continue;
        }
        Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                         { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
        ink_fill(q, 4, H.up[i] ? HATCH_NONE : HATCH_DIAG, 420 + i,
                 H.up[i] ? (Color){ 240, 232, 214, 255 } : (Color){ 190, 176, 152, 255 });
        ink_rect(r, 2.4f, 0.9f, 440 + i, col_ink());
        if (H.up[i])
            doodle(H.face[i], r.x + r.width * 0.5f, r.y + r.height * 0.5f, 30, 0,
                   col_accent_b());
        else
            doodle(D_WAVE, r.x + r.width * 0.5f, r.y + r.height * 0.5f, 22, 0,
                   col_ink_soft());
    }
    char b[64];
    snprintf(b, sizeof b, "pairs: %d of %d", H.found, NCARD / 2);
    art_text(b, ctx->area.x, ctx->area.y + ctx->area.height - 4, 15, col_ink_soft());
}

static const char *hint(pz_ctx *ctx, int tier)
{
    switch (tier) {
    case 1: return "Turning a shell you already know costs nothing. Use one of "
                   "your two turns to look somewhere new.";
    case 2: return "Work along the top row first and remember what sits under "
                   "each. Six faces, twelve shells: after one sweep you know half.";
    default: return "Matching faces sit apart, never side by side. Check the far "
                    "corners against each other.";
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    for (int i = 0; i < NCARD; i++)
        for (int j = i + 1; j < NCARD; j++) {
            if (H.gone[i] || H.gone[j]) continue;
            if (H.face[i] != H.face[j]) continue;
            H.gone[i] = H.gone[j] = true;
            H.found++;
        }
    return solved();
}

static const pz_def def = {
    .id = "shell_memory",
    .title = "Tansy's Shell Wall",
    .clue_granted = "clue.tansy_saw",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
