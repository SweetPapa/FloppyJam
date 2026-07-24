/* Capt. Maribel Sorge — rope routing (§5.1: routing/flow).
 *
 * "A rope that skips a cleat is a rope that lets go." Run one continuous line
 * from the bollard to the ferry, through every cleat, never crossing itself.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define GW 5
#define GH 4
#define NCELL (GW * GH)

typedef struct {
    int  path[NCELL];
    int  n;
    int  start, end;
    unsigned cleats;        /* bitmask of cells that must be visited */
    bool done;
} Rope;

static Rope R;

static int cell_of(int x, int y) { return y * GW + x; }
static int cx_of(int c) { return c % GW; }
static int cy_of(int c) { return c / GW; }

static bool adjacent(int a, int b)
{
    int dx = cx_of(a) - cx_of(b), dy = cy_of(a) - cy_of(b);
    return (dx * dx + dy * dy) == 1;
}

static bool in_path(int c)
{
    for (int i = 0; i < R.n; i++) if (R.path[i] == c) return true;
    return false;
}

static bool solved(void)
{
    if (R.n < 2 || R.path[R.n - 1] != R.end) return false;
    for (int c = 0; c < NCELL; c++)
        if ((R.cleats >> c) & 1u) { if (!in_path(c)) return false; }
    return true;
}

static void reset(void)
{
    R.n = 1;
    R.path[0] = R.start;
    R.done = false;
}

static void init(pz_ctx *ctx)
{
    memset(&R, 0, sizeof R);
    R.start = cell_of(0, 0);
    R.end = cell_of(0, GH - 1);
    /* the cleats: more of them the deeper into the game you meet this rope */
    R.cleats = (1u << cell_of(2, 0)) | (1u << cell_of(4, 1)) | (1u << cell_of(1, 2));
    if (ctx->difficulty >= 1) R.cleats |= (1u << cell_of(3, 2));
    if (ctx->difficulty >= 2) R.cleats |= (1u << cell_of(0, 3));
    reset();
}

static Rectangle cell_rect(pz_ctx *ctx, int c)
{
    float w = ctx->area.width / (GW + 1.0f);
    float h = ctx->area.height / (GH + 0.6f);
    float s = fminf(w, h);
    float ox = ctx->area.x + (ctx->area.width - s * GW) * 0.5f;
    float oy = ctx->area.y + (ctx->area.height - s * GH) * 0.5f;
    return (Rectangle){ ox + cx_of(c) * s + 8, oy + cy_of(c) * s + 8, s - 16, s - 16 };
}

static Vector2 cell_mid(pz_ctx *ctx, int c)
{
    Rectangle r = cell_rect(ctx, c);
    return (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();
    for (int c = 0; c < NCELL; c++) {
        if (!CheckCollisionPointRec(m, cell_rect(ctx, c))) continue;

        if (R.n > 1 && R.path[R.n - 1] == c) { R.n--; sfx_play(SFX_TICK); return PZ_RUNNING; }
        if (in_path(c)) { sfx_play(SFX_CLUNK); return PZ_RUNNING; }
        if (!adjacent(R.path[R.n - 1], c)) { sfx_play(SFX_CLUNK); return PZ_RUNNING; }

        R.path[R.n++] = c;
        sfx_play(SFX_CHIME);
        if (c == R.end) {
            if (solved()) return PZ_SOLVED;
            pz_attempt_failed(ctx);          /* reached the ferry, missed a cleat */
        }
        return PZ_RUNNING;
    }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("Run the line from the bollard to the ferry, through every cleat.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    for (int c = 0; c < NCELL; c++) {
        Rectangle r = cell_rect(ctx, c);
        bool cleat = (R.cleats >> c) & 1u;
        bool have = in_path(c);
        Vector2 mid = { r.x + r.width * 0.5f, r.y + r.height * 0.5f };

        ink_circle(mid.x, mid.y, r.width * 0.30f, 2.2f, 0.7f, 100 + c,
                   have ? col_accent_b() : col_ink_soft());
        if (cleat)
            doodle(D_ANCHOR, mid.x, mid.y, r.width * 0.20f, 0,
                   have ? col_cool() : col_accent_a());
        if (c == R.start) doodle(D_LOCK, mid.x, mid.y, r.width * 0.18f, 0, col_ink());
        if (c == R.end)   doodle(D_WAVE, mid.x, mid.y, r.width * 0.22f, 0, col_sea());
    }

    if (R.n > 1) {
        Vector2 pts[NCELL];
        for (int i = 0; i < R.n; i++) pts[i] = cell_mid(ctx, R.path[i]);
        ink_stroke(pts, R.n, 7.0f, 1.6f, 777, (Color){ 190, 150, 96, 255 });
    }
    int need = 0, got = 0;
    for (int c = 0; c < NCELL; c++)
        if ((R.cleats >> c) & 1u) { need++; if (in_path(c)) got++; }
    char b[64];
    snprintf(b, sizeof b, "cleats: %d of %d   (click the last knot to undo)", got, need);
    art_text(b, ctx->area.x, ctx->area.y + ctx->area.height - 4, 15, col_ink_soft());
}

static const char *hint(pz_ctx *ctx, int tier)
{
    switch (tier) {
    case 1: return "The rope only steps to a neighbour — never diagonally, never twice.";
    case 2: return "Serve the far cleats first. Sweep along one row, drop down, "
                   "sweep back: a boustrophedon, as Bartleby would insist on calling it.";
    default: return "Sweep the top row rightwards, drop down, sweep back left, "
                    "drop down, sweep right, drop down, sweep left to the ferry.";
    }
}

/* §10.5: drive it home with no window, no mouse, no luck. */
static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    for (int y = 0; y < GH; y++) {
        if (y & 1) for (int x = GW - 1; x >= 0; x--) {
            int c = cell_of(x, y);
            if (c != R.path[0]) R.path[R.n++] = c;
        }
        else for (int x = 0; x < GW; x++) {
            int c = cell_of(x, y);
            if (c != R.path[0]) R.path[R.n++] = c;
        }
    }
    return solved();
}

static const pz_def def = {
    .id = "rope_knot",
    .title = "Maribel's Mooring",
    .clue_granted = "clue.ferry_log",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
