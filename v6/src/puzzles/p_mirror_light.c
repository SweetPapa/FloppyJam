/* Nona Ember — light and mirrors (§5.1: spatial/fit).
 *
 * A lamplighter's trick: the lamp is over there and the wick is over here,
 * and between them are mirrors that will do as they are told. Click a mirror
 * to turn it. The beam is drawn live, so you are never guessing.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define GW 7
#define GH 5
#define MAXBEAM 64

enum { M_EMPTY = 0, M_SLASH, M_BACK, M_WALL };

typedef struct {
    unsigned char cell[GH][GW];
    bool          fixed[GH][GW];
    int   sx, sy, sdir;          /* lamp: 0 up 1 right 2 down 3 left */
    int   wx, wy;
    int   beam[MAXBEAM][2];
    int   beamn;
    bool  lit;
} Room;

static Room N;

static const int DX[4] = { 0, 1, 0, -1 };
static const int DY[4] = { -1, 0, 1, 0 };

static void trace(void)
{
    N.beamn = 0;
    N.lit = false;
    int x = N.sx, y = N.sy, d = N.sdir;
    for (int step = 0; step < MAXBEAM; step++) {
        x += DX[d]; y += DY[d];
        if (x < 0 || y < 0 || x >= GW || y >= GH) return;
        N.beam[N.beamn][0] = x;
        N.beam[N.beamn][1] = y;
        N.beamn++;
        if (x == N.wx && y == N.wy) { N.lit = true; return; }
        switch (N.cell[y][x]) {
        case M_WALL:  return;
        case M_SLASH: d = (d == 0) ? 1 : (d == 1) ? 0 : (d == 2) ? 3 : 2; break;
        case M_BACK:  d = (d == 0) ? 3 : (d == 3) ? 0 : (d == 2) ? 1 : 2; break;
        default: break;
        }
    }
}

static bool solved(void) { trace(); return N.lit; }

static void init(pz_ctx *ctx)
{
    memset(&N, 0, sizeof N);
    N.sx = -1; N.sy = 2; N.sdir = 1;        /* lamp shines in from the left */
    N.wx = 6;  N.wy = 0;                    /* the wick, up in the corner   */
    N.cell[2][2] = M_WALL; N.fixed[2][2] = true;
    N.cell[0][3] = M_WALL; N.fixed[0][3] = true;
    if (ctx->difficulty >= 1) { N.cell[3][5] = M_WALL; N.fixed[3][5] = true; }
    trace();
}

static Rectangle cellr(pz_ctx *ctx, int x, int y)
{
    float s = fminf((ctx->area.width - 120) / GW, (ctx->area.height - 40) / GH);
    float ox = ctx->area.x + (ctx->area.width - GW * s) * 0.5f;
    float oy = ctx->area.y + (ctx->area.height - GH * s) * 0.5f;
    return (Rectangle){ ox + x * s, oy + y * s, s - 6, s - 6 };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            if (N.fixed[y][x] || (x == N.wx && y == N.wy)) continue;
            if (!CheckCollisionPointRec(m, cellr(ctx, x, y))) continue;
            N.cell[y][x] = (unsigned char)((N.cell[y][x] + 1) % 3);   /* empty / \ */
            sfx_play(SFX_TICK);
            if (solved()) return PZ_SOLVED;
            trace();
            return PZ_RUNNING;
        }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("Click a square to place or turn a mirror. Light the wick.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            Rectangle r = cellr(ctx, x, y);
            bool onbeam = false;
            for (int i = 0; i < N.beamn; i++)
                if (N.beam[i][0] == x && N.beam[i][1] == y) onbeam = true;
            Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                             { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
            ink_fill(q, 4, HATCH_NONE, 2600 + y * GW + x,
                     onbeam ? (Color){ 244, 232, 176, 255 } : (Color){ 232, 226, 210, 255 });
            ink_rect(r, 1.4f, 0.5f, 2650 + y * GW + x, col_paper_dark());

            float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f;
            if (N.cell[y][x] == M_WALL) {
                ink_fill(q, 4, HATCH_CROSS, 2700 + y * GW + x, (Color){ 150, 142, 130, 255 });
            } else if (N.cell[y][x] == M_SLASH) {
                ink_line(r.x + 10, r.y + r.height - 10, r.x + r.width - 10, r.y + 10,
                         6.0f, 0.6f, 2720 + y * GW + x, (Color){ 148, 186, 214, 255 });
            } else if (N.cell[y][x] == M_BACK) {
                ink_line(r.x + 10, r.y + 10, r.x + r.width - 10, r.y + r.height - 10,
                         6.0f, 0.6f, 2760 + y * GW + x, (Color){ 148, 186, 214, 255 });
            }
            if (x == N.wx && y == N.wy)
                doodle(D_LANTERN, cx, cy, 24, 0,
                       N.lit ? (Color){ 246, 214, 120, 255 } : col_ink_soft());
        }

    Rectangle lampc = cellr(ctx, 0, N.sy);
    doodle(D_LANTERN, lampc.x - 54, lampc.y + lampc.height * 0.5f, 26, 0,
           (Color){ 246, 214, 120, 255 });
}

static const char *hint(pz_ctx *ctx, int tier)
{
    switch (tier) {
    case 1: return "A mirror only ever turns the beam a quarter turn. It cannot "
                   "send it back the way it came.";
    case 2: return "Work backwards from the wick: what direction must the beam "
                   "arrive in, and which square could have sent it that way?";
    default: return "The beam cannot pass the stone in its own row, so turn it "
                    "down early, run it along the bottom, and lift it up the far "
                    "wall into the wick.";
    }
}

/* §10.5: search the beam itself. At every square the beam reaches we may
 * leave it clear or set a mirror; a square already committed keeps whatever
 * it was given. Backtracking over that is exact and finishes instantly. */
static bool beam_search(int x, int y, int d, int depth)
{
    if (depth > 40) return false;
    x += DX[d]; y += DY[d];
    if (x < 0 || y < 0 || x >= GW || y >= GH) return false;
    if (x == N.wx && y == N.wy) return true;
    if (N.cell[y][x] == M_WALL) return false;
    if (N.fixed[y][x]) return beam_search(x, y, d, depth + 1);

    unsigned char had = N.cell[y][x];
    if (had != M_EMPTY) {
        int nd = (had == M_SLASH)
               ? ((d == 0) ? 1 : (d == 1) ? 0 : (d == 2) ? 3 : 2)
               : ((d == 0) ? 3 : (d == 3) ? 0 : (d == 2) ? 1 : 2);
        return beam_search(x, y, nd, depth + 1);
    }
    for (int choice = 0; choice < 3; choice++) {
        int nd = d;
        if (choice == 1) nd = (d == 0) ? 1 : (d == 1) ? 0 : (d == 2) ? 3 : 2;
        if (choice == 2) nd = (d == 0) ? 3 : (d == 3) ? 0 : (d == 2) ? 1 : 2;
        N.cell[y][x] = (unsigned char)(choice == 0 ? M_EMPTY
                                     : choice == 1 ? M_SLASH : M_BACK);
        if (beam_search(x, y, nd, depth + 1)) return true;
    }
    N.cell[y][x] = M_EMPTY;
    return false;
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    return beam_search(N.sx, N.sy, N.sdir, 0) && solved();
}

static const pz_def def = {
    .id = "mirror_light",
    .title = "Nona's Lamplighting",
    .clue_granted = "clue.nona_key",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
