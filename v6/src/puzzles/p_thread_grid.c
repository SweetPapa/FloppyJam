/* Greta Spool — thread grid (§5.1: spatial/fit, nonogram school).
 *
 * The pattern for the festival bunting, written the way Greta writes it: how
 * many stitches in each row and column, in order. No guessing required —
 * every square is deducible.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define N 5

typedef struct {
    unsigned char target[N][N];
    signed char   mark[N][N];      /* 0 blank, 1 stitched, -1 crossed out */
    int rowclue[N][3], rown[N];
    int colclue[N][3], coln[N];
} Grid;

static Grid G;

/* three bunting patterns, all deducible; the seed picks one */
static const unsigned char PATTERNS[3][N][N] = {
    { { 0,1,1,1,0 }, { 1,0,0,0,1 }, { 1,0,1,0,1 }, { 1,0,0,0,1 }, { 0,1,1,1,0 } },
    { { 1,0,0,0,1 }, { 0,1,0,1,0 }, { 0,0,1,0,0 }, { 0,1,0,1,0 }, { 1,0,0,0,1 } },
    { { 0,0,1,0,0 }, { 0,1,1,1,0 }, { 1,1,1,1,1 }, { 0,1,1,1,0 }, { 0,0,1,0,0 } },
};

static void runs(const unsigned char *line, int stride, int *out, int *n)
{
    *n = 0;
    int run = 0;
    for (int i = 0; i < N; i++) {
        if (line[i * stride]) run++;
        else if (run) { out[(*n)++] = run; run = 0; }
    }
    if (run) out[(*n)++] = run;
    if (*n == 0) { out[0] = 0; *n = 1; }
}

static bool solved(void)
{
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
            if ((G.mark[y][x] == 1) != (G.target[y][x] != 0)) return false;
    return true;
}

static void init(pz_ctx *ctx)
{
    memset(&G, 0, sizeof G);
    int which = (int)(ctx->seed % 3u);
    memcpy(G.target, PATTERNS[which], sizeof G.target);
    for (int y = 0; y < N; y++) runs(&G.target[y][0], 1, G.rowclue[y], &G.rown[y]);
    for (int x = 0; x < N; x++) runs(&G.target[0][x], N, G.colclue[x], &G.coln[x]);
}

static Rectangle cell(pz_ctx *ctx, int x, int y)
{
    float s = 62;
    float ox = ctx->area.x + 300, oy = ctx->area.y + 110;
    return (Rectangle){ ox + x * s, oy + y * s, s - 6, s - 6 };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    bool l = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool r = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if (!l && !r) return PZ_RUNNING;
    Vector2 m = art_mouse();
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            if (!CheckCollisionPointRec(m, cell(ctx, x, y))) continue;
            if (l) G.mark[y][x] = (G.mark[y][x] == 1) ? 0 : 1;
            else   G.mark[y][x] = (G.mark[y][x] == -1) ? 0 : -1;
            sfx_play(SFX_TICK);
            if (solved()) return PZ_SOLVED;
            return PZ_RUNNING;
        }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("Left click to stitch, right click to cross out. Numbers are runs, in order.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    for (int y = 0; y < N; y++) {
        char b[32] = "";
        for (int i = 0; i < G.rown[y]; i++) {
            char t[8];
            snprintf(t, sizeof t, "%d ", G.rowclue[y][i]);
            strncat(b, t, sizeof b - strlen(b) - 1);
        }
        Rectangle c0 = cell(ctx, 0, y);
        float w = art_text_w(b, 19);
        art_text(b, c0.x - w - 16, c0.y + 14, 19, col_ink());
    }
    for (int x = 0; x < N; x++) {
        Rectangle c0 = cell(ctx, x, 0);
        float yy = c0.y - 24 - (G.coln[x] - 1) * 22.0f;
        for (int i = 0; i < G.coln[x]; i++) {
            char t[8];
            snprintf(t, sizeof t, "%d", G.colclue[x][i]);
            art_text(t, c0.x + 18, yy + i * 22.0f, 19, col_ink());
        }
    }
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            Rectangle r = cell(ctx, x, y);
            Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                             { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
            ink_fill(q, 4, G.mark[y][x] == 1 ? HATCH_CROSS : HATCH_NONE, 500 + y * N + x,
                     G.mark[y][x] == 1 ? (Color){ 172, 108, 168, 255 }
                                       : (Color){ 238, 231, 214, 255 });
            ink_rect(r, 1.8f, 0.6f, 560 + y * N + x, col_ink_soft());
            if (G.mark[y][x] == -1) {
                ink_line(r.x + 14, r.y + 14, r.x + r.width - 14, r.y + r.height - 14,
                         2.4f, 0.7f, 600 + y * N + x, col_ink_soft());
                ink_line(r.x + r.width - 14, r.y + 14, r.x + 14, r.y + r.height - 14,
                         2.4f, 0.7f, 640 + y * N + x, col_ink_soft());
            }
        }
    doodle(D_SPOOL, ctx->area.x + 140, ctx->area.y + 260, 40, 0.2f,
           (Color){ 172, 108, 168, 255 });
}

static const char *hint(pz_ctx *ctx, int tier)
{
    switch (tier) {
    case 1: return "A row clue of 5 fills the whole row. Start with the rows and "
                   "columns whose numbers leave no room to move.";
    case 2: return "Cross out what you have proven empty. Crosses are worth as "
                   "much as stitches: they are what turns the next row into a fact.";
    default: return "The pattern is symmetric left-to-right and top-to-bottom. "
                    "Solve one quarter and mirror it.";
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
            G.mark[y][x] = G.target[y][x] ? 1 : -1;
    return solved();
}

static const pz_def def = {
    .id = "thread_grid",
    .title = "Greta's Bunting Pattern",
    .clue_granted = "clue.greta_gossip",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
