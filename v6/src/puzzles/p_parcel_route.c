/* Felix Route — parcel routing (§5.1: routing/flow).
 *
 * Every house gets its post, nobody gets it twice, and Felix walks each lane
 * only in the direction the lane goes. Click house to house along the roads.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define NHOUSE 7
#define NROAD  11

typedef struct { float x, y; const char *name; } House;
typedef struct { int a, b; } Road;

static const House HOUSE[NHOUSE] = {
    { 0.10f, 0.50f, "Post Office" },
    { 0.30f, 0.18f, "Bakery" },
    { 0.30f, 0.82f, "Forge" },
    { 0.52f, 0.50f, "Fountain" },
    { 0.74f, 0.18f, "Library" },
    { 0.74f, 0.82f, "Locksmith" },
    { 0.92f, 0.50f, "Tower Lodge" },
};
static const Road ROAD[NROAD] = {
    {0,1},{0,2},{1,3},{2,3},{1,4},{2,5},{3,4},{3,5},{4,6},{5,6},{4,5}
};

typedef struct {
    int  path[NHOUSE + 1];
    int  n;
} Route;

static Route R;

static bool linked(int a, int b)
{
    for (int i = 0; i < NROAD; i++)
        if ((ROAD[i].a == a && ROAD[i].b == b) || (ROAD[i].a == b && ROAD[i].b == a))
            return true;
    return false;
}

static bool visited(int h)
{
    for (int i = 0; i < R.n; i++) if (R.path[i] == h) return true;
    return false;
}

static bool solved(void) { return R.n == NHOUSE; }

static void init(pz_ctx *ctx)
{
    memset(&R, 0, sizeof R);
    R.path[0] = 0;                    /* Felix always starts at the Post Office */
    R.n = 1;
}

static Vector2 hpos(pz_ctx *ctx, int i)
{
    return (Vector2){ ctx->area.x + 40 + HOUSE[i].x * (ctx->area.width - 80),
                      ctx->area.y + 40 + HOUSE[i].y * (ctx->area.height - 130) };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();
    for (int i = 0; i < NHOUSE; i++) {
        Vector2 p = hpos(ctx, i);
        if (CheckCollisionPointCircle(m, p, 44)) {
            if (R.n > 1 && R.path[R.n - 1] == i) { R.n--; sfx_play(SFX_TICK); return PZ_RUNNING; }
            if (visited(i)) { sfx_play(SFX_CLUNK); return PZ_RUNNING; }
            if (!linked(R.path[R.n - 1], i)) { sfx_play(SFX_CLUNK); return PZ_RUNNING; }
            R.path[R.n++] = i;
            sfx_play(SFX_CHIME);
            if (solved()) return PZ_SOLVED;
            /* stuck: no unvisited neighbour left */
            bool any = false;
            for (int k = 0; k < NHOUSE; k++)
                if (!visited(k) && linked(i, k)) any = true;
            if (!any) pz_attempt_failed(ctx);
            return PZ_RUNNING;
        }
    }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("One round, every house once. Click the last stop to step back.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    for (int i = 0; i < NROAD; i++) {
        Vector2 a = hpos(ctx, ROAD[i].a), b = hpos(ctx, ROAD[i].b);
        ink_line(a.x, a.y, b.x, b.y, 2.4f, 1.4f, 1000 + i, col_paper_dark());
    }
    if (R.n > 1) {
        Vector2 pts[NHOUSE + 1];
        for (int i = 0; i < R.n; i++) pts[i] = hpos(ctx, R.path[i]);
        ink_stroke(pts, R.n, 6.0f, 1.4f, 1100, (Color){ 176, 92, 78, 255 });
    }
    for (int i = 0; i < NHOUSE; i++) {
        Vector2 p = hpos(ctx, i);
        bool v = visited(i);
        ink_blob(p.x, p.y, 34, 1.0f, 1200 + i,
                 v ? (Color){ 226, 190, 96, 255 } : (Color){ 234, 226, 208, 255 });
        ink_circle(p.x, p.y, 34, 2.4f, 0.8f, 1220 + i, col_ink());
        doodle(i == 0 ? D_ENVELOPE : D_HOUSE, p.x, p.y, 17, 0,
               v ? col_ink() : col_ink_soft());
        float w = art_text_w(HOUSE[i].name, 14);
        art_text(HOUSE[i].name, p.x - w * 0.5f, p.y + 40, 14, col_ink_soft());
    }
    char b[64];
    snprintf(b, sizeof b, "delivered: %d of %d", R.n, NHOUSE);
    art_text(b, ctx->area.x, ctx->area.y + ctx->area.height - 4, 15, col_ink_soft());
}

static const char *hint(pz_ctx *ctx, int tier)
{
    switch (tier) {
    case 1: return "Every house exactly once, and only along a lane that exists. "
                   "You may always step back.";
    case 2: return "The Tower Lodge has only two lanes into it, so it must be an "
                   "end of the round, not a middle. Plan to finish there.";
    default: return "Post Office, Bakery, Library, Fountain, Forge, Locksmith, "
                    "Tower Lodge.";
    }
}

static bool dfs(void)
{
    if (solved()) return true;
    int cur = R.path[R.n - 1];
    for (int i = 0; i < NHOUSE; i++) {
        if (visited(i) || !linked(cur, i)) continue;
        R.path[R.n++] = i;
        if (dfs()) return true;
        R.n--;
    }
    return false;
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    return dfs() && solved();
}

static const pz_def def = {
    .id = "parcel_route",
    .title = "Felix's Morning Round",
    .clue_granted = "clue.felix_letters",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
