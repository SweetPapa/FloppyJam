/* Sage Fern — water flow (§5.1: routing/flow).
 *
 * The garden's pipes were laid by somebody in a hurry. Turn each length until
 * the rill runs from the pump to both beds. Water shows live, so a half-right
 * answer looks half-right.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define GW 6
#define GH 4

/* pipe kinds by open sides: bit0 up, bit1 right, bit2 down, bit3 left */
enum { PIPE_NONE = 0, PIPE_STRAIGHT, PIPE_BEND, PIPE_TEE, PIPE_SOURCE, PIPE_SINK };

typedef struct {
    unsigned char kind[GH][GW];
    unsigned char rot[GH][GW];
    bool          wet[GH][GW];
} Garden;

static Garden W;

/* open sides, before rotation: straight, bend, tee, pump (right), bed (up) */
static const unsigned char BASE_MASK[6] = { 0, 0x5, 0x3, 0x7, 0x2, 0x1 };

static unsigned char mask_of(int x, int y)
{
    unsigned char m = BASE_MASK[W.kind[y][x]];
    int r = W.rot[y][x] & 3;
    return (unsigned char)(((m << r) | (m >> (4 - r))) & 0xf);
}

static void flood(void)
{
    memset(W.wet, 0, sizeof W.wet);
    int stack[GW * GH][2], sp = 0;
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (W.kind[y][x] == PIPE_SOURCE) {
                W.wet[y][x] = true;
                stack[sp][0] = x; stack[sp][1] = y; sp++;
            }
    static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
    while (sp) {
        int x = stack[--sp][0], y = stack[sp][1];
        unsigned char m = mask_of(x, y);
        for (int d = 0; d < 4; d++) {
            if (!((m >> d) & 1)) continue;
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= GW || ny >= GH) continue;
            if (W.kind[ny][nx] == PIPE_NONE || W.wet[ny][nx]) continue;
            if (!((mask_of(nx, ny) >> ((d + 2) & 3)) & 1)) continue;   /* must face back */
            W.wet[ny][nx] = true;
            stack[sp][0] = nx; stack[sp][1] = ny; sp++;
        }
    }
}

static bool solved(void)
{
    flood();
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (W.kind[y][x] == PIPE_SINK && !W.wet[y][x]) return false;
    return true;
}

/* A layout that is solvable by construction: lay a solved path, then spin it. */
static void init(pz_ctx *ctx)
{
    memset(&W, 0, sizeof W);
    static const unsigned char LAY[GH][GW] = {
        { PIPE_SOURCE, PIPE_STRAIGHT, PIPE_BEND, PIPE_NONE,     PIPE_NONE,     PIPE_NONE },
        { PIPE_NONE,   PIPE_NONE,     PIPE_STRAIGHT, PIPE_NONE, PIPE_NONE,     PIPE_NONE },
        { PIPE_NONE,   PIPE_NONE,     PIPE_TEE,  PIPE_STRAIGHT, PIPE_STRAIGHT, PIPE_BEND },
        { PIPE_NONE,   PIPE_NONE,     PIPE_SINK, PIPE_NONE,     PIPE_NONE,     PIPE_SINK },
    };
    /* the rotations that solve it; init then spins every turnable length */
    static const unsigned char ROT[GH][GW] = {
        { 0, 1, 2, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 1, 1, 2 },
        { 0, 0, 0, 0, 0, 0 },
    };
    memcpy(W.kind, LAY, sizeof LAY);
    memcpy(W.rot, ROT, sizeof ROT);

    unsigned r = ctx->seed | 1u;
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            if (W.kind[y][x] == PIPE_NONE) continue;
            if (W.kind[y][x] == PIPE_SOURCE || W.kind[y][x] == PIPE_SINK) continue;
            r = r * 1664525u + 1013904223u;
            W.rot[y][x] = (unsigned char)((W.rot[y][x] + 1 + (r >> 16) % 3u) & 3u);
        }
    flood();
}

static Rectangle cellr(pz_ctx *ctx, int x, int y)
{
    float s = fminf((ctx->area.width - 80) / GW, (ctx->area.height - 30) / GH);
    float ox = ctx->area.x + (ctx->area.width - GW * s) * 0.5f;
    float oy = ctx->area.y + (ctx->area.height - GH * s) * 0.5f;
    return (Rectangle){ ox + x * s, oy + y * s, s - 8, s - 8 };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            if (W.kind[y][x] == PIPE_NONE) continue;
            if (W.kind[y][x] == PIPE_SOURCE || W.kind[y][x] == PIPE_SINK) continue;
            if (!CheckCollisionPointRec(m, cellr(ctx, x, y))) continue;
            W.rot[y][x] = (unsigned char)((W.rot[y][x] + 1) & 3);
            sfx_play(SFX_TICK);
            if (solved()) return PZ_SOLVED;
            flood();
            return PZ_RUNNING;
        }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("Click a length of pipe to turn it. The rill must reach both beds.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            if (W.kind[y][x] == PIPE_NONE) continue;
            Rectangle r = cellr(ctx, x, y);
            Vector2 c = { r.x + r.width * 0.5f, r.y + r.height * 0.5f };
            Color pipe = W.wet[y][x] ? (Color){ 70, 138, 178, 255 }
                                     : (Color){ 168, 162, 150, 255 };
            ink_rect(r, 1.4f, 0.5f, 1800 + y * GW + x, col_paper_dark());
            unsigned char m = mask_of(x, y);
            static const float DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
            for (int d = 0; d < 4; d++)
                if ((m >> d) & 1)
                    ink_line(c.x, c.y, c.x + DX[d] * r.width * 0.5f,
                             c.y + DY[d] * r.height * 0.5f, 11.0f, 0.8f,
                             1830 + (y * GW + x) * 4 + d, pipe);
            ink_blob(c.x, c.y, 9, 1.0f, 1900 + y * GW + x, pipe);

            if (W.kind[y][x] == PIPE_SOURCE)
                doodle(D_WAVE, c.x, c.y - 22, 16, 0, col_sea());
            if (W.kind[y][x] == PIPE_SINK)
                doodle(D_FLOWER, c.x, c.y, 18, 0,
                       W.wet[y][x] ? (Color){ 214, 118, 128, 255 } : col_paper_dark());
        }
}

static const char *hint(pz_ctx *ctx, int tier)
{
    switch (tier) {
    case 1: return "Water only crosses where two pipe ends actually meet. A pipe "
                   "pointing at a wall is a pipe pointing nowhere.";
    case 2: return "Start at the pump and follow the wet colour. The first dry "
                   "length after the wet ones is the one to turn.";
    default: return "The junction in the middle column has to feed downward and "
                    "rightward at once. Set that one first and the rest follows.";
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    /* exhaustive over rotations of the turnable pipes, in reading order */
    int idx[GW * GH][2], n = 0;
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            if (W.kind[y][x] != PIPE_NONE && W.kind[y][x] != PIPE_SOURCE &&
                W.kind[y][x] != PIPE_SINK) { idx[n][0] = x; idx[n][1] = y; n++; }
    long total = 1;
    for (int i = 0; i < n; i++) total *= 4;
    for (long v = 0; v < total; v++) {
        long t = v;
        for (int i = 0; i < n; i++) { W.rot[idx[i][1]][idx[i][0]] = (unsigned char)(t & 3); t >>= 2; }
        if (solved()) return true;
    }
    return false;
}

static const pz_def def = {
    .id = "water_flow",
    .title = "Sage's Rill",
    .clue_granted = "clue.garden_gray",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
