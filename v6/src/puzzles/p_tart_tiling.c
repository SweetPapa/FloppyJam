/* Bruno Crumb — tart tiling (§5.1: spatial/fit, pentomino school).
 *
 * Six tarts of awkward shapes, one tray, and a baker who refuses to cut
 * anything to make it fit. Click a tart to pick it up, R to turn it, click
 * the tray to lay it down.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define TW 6
#define TH 5
#define NPIECE 6
#define MAXCELL 5

typedef struct { signed char x, y; } Cell;

typedef struct {
    Cell cell[MAXCELL];
    int  n;
    int  rot;            /* 0..3 */
    int  px, py;         /* placed origin, -1 when in the basket */
    bool placed;
} Piece;

typedef struct {
    Piece p[NPIECE];
    int   held;          /* index or -1 */
    unsigned char grid[TH][TW];
} Tray;

static Tray T;

/* Six pieces whose cells total exactly 30 = 6x5, and which do fit. */
static const Cell SHAPES[NPIECE][MAXCELL] = {
    { {0,0},{1,0},{2,0},{3,0},{4,0} },     /* the long one */
    { {0,0},{1,0},{2,0},{3,0},{0,1} },     /* the elbow     */
    { {1,0},{2,0},{3,0},{0,1},{1,1} },     /* the step      */
    { {0,0},{1,0},{2,0},{0,1},{1,1} },     /* the slab      */
    { {0,0},{1,0},{2,0},{1,1},{1,2} },     /* the anchor    */
    { {0,0},{0,1},{0,2},{1,2},{2,2} },     /* the corner    */
};
static const int SHAPE_N[NPIECE] = { 5, 5, 5, 5, 5, 5 };

static void rot_cells(const Piece *p, Cell *out)
{
    int minx = 99, miny = 99;
    for (int i = 0; i < p->n; i++) {
        int x = p->cell[i].x, y = p->cell[i].y, rx = x, ry = y;
        switch (p->rot & 3) {
        case 1: rx = -y; ry = x; break;
        case 2: rx = -x; ry = -y; break;
        case 3: rx = y;  ry = -x; break;
        default: break;
        }
        out[i].x = (signed char)rx;
        out[i].y = (signed char)ry;
        if (rx < minx) minx = rx;
        if (ry < miny) miny = ry;
    }
    for (int i = 0; i < p->n; i++) { out[i].x -= (signed char)minx; out[i].y -= (signed char)miny; }
}

static bool fits(int idx, int ox, int oy)
{
    Cell c[MAXCELL];
    rot_cells(&T.p[idx], c);
    for (int i = 0; i < T.p[idx].n; i++) {
        int x = ox + c[i].x, y = oy + c[i].y;
        if (x < 0 || y < 0 || x >= TW || y >= TH) return false;
        if (T.grid[y][x]) return false;
    }
    return true;
}

static void stamp(int idx, int ox, int oy, unsigned char v)
{
    Cell c[MAXCELL];
    rot_cells(&T.p[idx], c);
    for (int i = 0; i < T.p[idx].n; i++)
        T.grid[oy + c[i].y][ox + c[i].x] = v;
}

static bool solved(void)
{
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) if (!T.grid[y][x]) return false;
    return true;
}

static void init(pz_ctx *ctx)
{
    memset(&T, 0, sizeof T);
    T.held = -1;
    for (int i = 0; i < NPIECE; i++) {
        memcpy(T.p[i].cell, SHAPES[i], sizeof SHAPES[i]);
        T.p[i].n = SHAPE_N[i];
        T.p[i].px = T.p[i].py = -1;
    }
}

static float CS(void) { return 66.0f; }
static Rectangle tray_rect(pz_ctx *ctx)
{
    return (Rectangle){ ctx->area.x + 330, ctx->area.y + 60, TW * CS(), TH * CS() };
}
static Rectangle basket_rect(pz_ctx *ctx, int i)
{
    return (Rectangle){ ctx->area.x + 20 + (i % 2) * 140.0f,
                        ctx->area.y + 40 + (i / 2) * 120.0f, 128, 108 };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    if (IsKeyPressed(KEY_R) && T.held >= 0) { T.p[T.held].rot++; sfx_play(SFX_TICK); }
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();
    Rectangle tr = tray_rect(ctx);

    if (CheckCollisionPointRec(m, tr)) {
        int gx = (int)((m.x - tr.x) / CS()), gy = (int)((m.y - tr.y) / CS());
        if (T.held >= 0) {
            if (fits(T.held, gx, gy)) {
                stamp(T.held, gx, gy, (unsigned char)(T.held + 1));
                T.p[T.held].px = gx; T.p[T.held].py = gy;
                T.p[T.held].placed = true;
                T.held = -1;
                sfx_play(SFX_CHIME);
                if (solved()) return PZ_SOLVED;
            } else {
                pz_attempt_failed(ctx);
            }
        } else {
            unsigned char v = T.grid[gy][gx];
            if (v) {
                int idx = v - 1;
                stamp(idx, T.p[idx].px, T.p[idx].py, 0);
                T.p[idx].placed = false;
                T.held = idx;
                sfx_play(SFX_TICK);
            }
        }
        return PZ_RUNNING;
    }
    for (int i = 0; i < NPIECE; i++) {
        if (T.p[i].placed) continue;
        if (!CheckCollisionPointRec(m, basket_rect(ctx, i))) continue;
        T.held = (T.held == i) ? -1 : i;
        sfx_play(SFX_TICK);
        return PZ_RUNNING;
    }
    return PZ_RUNNING;
}

static void draw_piece(int idx, float ox, float oy, float s, Color c)
{
    Cell cl[MAXCELL];
    rot_cells(&T.p[idx], cl);
    for (int i = 0; i < T.p[idx].n; i++) {
        Rectangle r = { ox + cl[i].x * s, oy + cl[i].y * s, s - 4, s - 4 };
        Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                         { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
        ink_fill(q, 4, HATCH_NONE, 700 + idx * 8 + i, c);
        ink_rect(r, 1.8f, 0.6f, 760 + idx * 8 + i, col_ink());
    }
}

static Color piece_col(int i)
{
    static const Color c[NPIECE] = {
        { 214, 152,  96, 255 }, { 196,  96,  92, 255 }, { 226, 190,  86, 255 },
        { 152, 176, 112, 255 }, { 176, 130, 190, 255 }, { 120, 156, 190, 255 },
    };
    return c[i % NPIECE];
}

static void draw(pz_ctx *ctx)
{
    art_text("Fill Bruno's tray. Click a tart, press R to turn it, click the tray.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());
    Rectangle tr = tray_rect(ctx);
    Vector2 q[4] = { { tr.x, tr.y }, { tr.x + tr.width, tr.y },
                     { tr.x + tr.width, tr.y + tr.height }, { tr.x, tr.y + tr.height } };
    ink_fill(q, 4, HATCH_DOT, 800, (Color){ 214, 200, 172, 255 });
    ink_rect(tr, 3.0f, 1.0f, 801, col_ink());
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            Rectangle r = { tr.x + x * CS(), tr.y + y * CS(), CS() - 4, CS() - 4 };
            unsigned char v = T.grid[y][x];
            if (v) {
                Vector2 p[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                                 { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
                ink_fill(p, 4, HATCH_NONE, 820 + y * TW + x, piece_col(v - 1));
                ink_rect(r, 1.6f, 0.5f, 880 + y * TW + x, col_ink());
            } else {
                ink_rect(r, 1.2f, 0.5f, 900 + y * TW + x, col_ink_soft());
            }
        }

    for (int i = 0; i < NPIECE; i++) {
        if (T.p[i].placed) continue;
        Rectangle b = basket_rect(ctx, i);
        if (T.held == i) ink_rect(b, 2.6f, 0.9f, 940 + i, col_accent_b());
        draw_piece(i, b.x + 10, b.y + 10, 20, piece_col(i));
    }
    if (T.held >= 0) {
        Vector2 m = art_mouse();
        draw_piece(T.held, m.x - CS() * 0.5f, m.y - CS() * 0.5f, CS() * 0.5f,
                   piece_col(T.held));
    }
    doodle(D_BREAD, ctx->area.x + 150, ctx->area.y + 370, 34, 0, (Color){ 208, 168, 108, 255 });
}

static const char *hint(pz_ctx *ctx, int tier)
{
    switch (tier) {
    case 1: return "Corners first. A shape with a right angle in it wants a corner "
                   "of the tray, and there are only four of those.";
    case 2: return "The long straight tart is five squares — it can only lie along "
                   "a full row. Decide which row it owns before anything else.";
    default: return "Lay the long one along a full row and put the corner piece "
                    "into a corner. Everything else follows from those two.";
    }
}

/* Depth-first fill: proves the tray is fillable without any window. */
static bool search(int placed_mask)
{
    if (solved()) return true;
    int fx = -1, fy = -1;
    for (int y = 0; y < TH && fy < 0; y++)
        for (int x = 0; x < TW; x++) if (!T.grid[y][x]) { fx = x; fy = y; break; }
    if (fy < 0) return true;

    for (int i = 0; i < NPIECE; i++) {
        if (placed_mask & (1 << i)) continue;
        for (int rot = 0; rot < 4; rot++) {
            T.p[i].rot = rot;
            Cell c[MAXCELL];
            rot_cells(&T.p[i], c);
            for (int anchor = 0; anchor < T.p[i].n; anchor++) {
                int ox = fx - c[anchor].x, oy = fy - c[anchor].y;
                if (!fits(i, ox, oy)) continue;
                stamp(i, ox, oy, (unsigned char)(i + 1));
                T.p[i].px = ox; T.p[i].py = oy; T.p[i].placed = true;
                if (search(placed_mask | (1 << i))) return true;
                stamp(i, ox, oy, 0);
                T.p[i].placed = false;
            }
        }
    }
    return false;
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    return search(0) && solved();
}

static const pz_def def = {
    .id = "tart_tiling",
    .title = "Bruno's Awkward Tray",
    .clue_granted = "clue.bruno_order",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
