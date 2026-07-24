/* Dr. Poppy Bloom — the mixing ratio (§5.1: mechanical/simulation).
 *
 * "Low spirits are not an illness, but they are a fact, and facts take tea."
 * Match the recipe exactly: spoons of each herb in the right proportion. No
 * arithmetic beyond counting (§5.1: no maths beyond arithmetic).
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"
#include "save/save.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define NHERB 3

typedef struct {
    int  want[NHERB];
    int  have[NHERB];
    float swirl;
} Pot;

static Pot M;

static const char *HERB[NHERB] = { "chamomile", "lemon balm", "sea rose" };
static const Color HERBC[NHERB] = {
    { 226, 200, 108, 255 }, { 150, 190, 120, 255 }, { 210, 130, 148, 255 }
};

static bool solved(void)
{
    for (int i = 0; i < NHERB; i++) if (M.have[i] != M.want[i]) return false;
    return true;
}

static void init(pz_ctx *ctx)
{
    memset(&M, 0, sizeof M);
    unsigned r = ctx->seed | 1u;
    for (int i = 0; i < NHERB; i++) {
        r = r * 1664525u + 1013904223u;
        M.want[i] = 1 + (int)((r >> 16) % (3u + (unsigned)ctx->difficulty));
    }
}

static Rectangle jar(pz_ctx *ctx, int i)
{
    return (Rectangle){ ctx->area.x + 90 + i * 230.0f, ctx->area.y + 120, 170, 210 };
}
static Rectangle R_POUR(pz_ctx *c) { return (Rectangle){ c->area.x + 640, c->area.y + 350, 230, 60 }; }
static Rectangle R_TIP(pz_ctx *c)  { return (Rectangle){ c->area.x + 640, c->area.y + 350 + 70, 230, 60 }; }

static pz_status update(pz_ctx *ctx, float dt)
{
    M.swirl += dt;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 m = art_mouse();
        for (int i = 0; i < NHERB; i++) {
            if (!CheckCollisionPointRec(m, jar(ctx, i))) continue;
            M.have[i]++;
            sfx_play(SFX_TICK);
            return PZ_RUNNING;
        }
    }
    if (pz_button_clicked(R_TIP(ctx), true)) {
        memset(M.have, 0, sizeof M.have);
        return PZ_RUNNING;
    }
    if (pz_button_clicked(R_POUR(ctx), true)) {
        if (solved()) return PZ_SOLVED;
        pz_attempt_failed(ctx);
    }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("Poppy's recipe, exactly. Click a jar for a spoonful.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    char recipe[128] = "Recipe: ";
    for (int i = 0; i < NHERB; i++) {
        char t[48];
        snprintf(t, sizeof t, "%d %s%s", M.want[i], HERB[i], i < NHERB - 1 ? ", " : "");
        strncat(recipe, t, sizeof recipe - strlen(recipe) - 1);
    }
    art_text(recipe, ctx->area.x + 20, ctx->area.y + 50, 18, col_ink());

    for (int i = 0; i < NHERB; i++) {
        Rectangle r = jar(ctx, i);
        Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                         { r.x + r.width - 12, r.y + r.height }, { r.x + 12, r.y + r.height } };
        ink_fill(q, 4, HATCH_NONE, 2200 + i, (Color){ 238, 232, 216, 255 });
        ink_stroke((Vector2[]){ q[0], q[1], q[2], q[3], q[0] }, 5, 2.6f, 0.9f,
                   2210 + i, col_ink());
        doodle(D_FLOWER, r.x + r.width * 0.5f, r.y + 70, 30, 0, HERBC[i]);
        float w = art_text_w(HERB[i], 16);
        art_text(HERB[i], r.x + (r.width - w) * 0.5f, r.y + r.height - 60, 16, col_ink());
        char n[24];
        snprintf(n, sizeof n, "%d in the pot", M.have[i]);
        float w2 = art_text_w(n, 14);
        art_text(n, r.x + (r.width - w2) * 0.5f, r.y + r.height - 34, 14,
                 M.have[i] == M.want[i] ? col_cool() : col_ink_soft());
    }

    /* the pot: colour is the honest mixture, so a wrong pot looks wrong */
    int total = M.have[0] + M.have[1] + M.have[2];
    Color brew = { 214, 206, 188, 255 };
    if (total) {
        float r = 0, g = 0, b = 0;
        for (int i = 0; i < NHERB; i++) {
            r += HERBC[i].r * (float)M.have[i];
            g += HERBC[i].g * (float)M.have[i];
            b += HERBC[i].b * (float)M.have[i];
        }
        brew = (Color){ (unsigned char)(r / total), (unsigned char)(g / total),
                        (unsigned char)(b / total), 255 };
    }
    float cx = ctx->area.x + 755, cy = ctx->area.y + 200;
    ink_blob(cx, cy, 86, 0.72f, 2260, brew);
    ink_circle(cx, cy, 88, 3.0f, 1.0f, 2261, col_ink());
    doodle(D_TEACUP, cx, cy + 130, 34, 0, col_ink_soft());
    if (!settings()->reduce_motion)
        for (int i = 0; i < 3; i++)
            doodle(D_STAR, cx + sinf(M.swirl * 1.4f + i * 2.1f) * 40,
                   cy - 100 - fmodf(M.swirl * 22 + i * 40, 90.0f), 7, 0, col_ink_soft());

    pz_button(R_POUR(ctx), "Pour it out for her", true, 2270);
    pz_button(R_TIP(ctx), "Tip the pot and start again", true, 2272);
}

static const char *hint(pz_ctx *ctx, int tier)
{
    static char buf[128];
    switch (tier) {
    case 1: return "Count what is already in the pot before you add more. The "
                   "number under each jar is the truth.";
    case 2: return "If you overshoot, tip the pot. Starting again costs nothing "
                   "at all — Poppy has a great deal of tea.";
    default:
        snprintf(buf, sizeof buf, "It wants %d, %d and %d, in that order.",
                 M.want[0], M.want[1], M.want[2]);
        return buf;
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    for (int i = 0; i < NHERB; i++) M.have[i] = M.want[i];
    return solved();
}

static const pz_def def = {
    .id = "mix_ratio",
    .title = "Poppy's Low-Spirits Tea",
    .clue_granted = "clue.town_lowspirits",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
