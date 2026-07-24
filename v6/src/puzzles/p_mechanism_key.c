/* Wick — the tower's word wheel (§5.1: word/cipher, language-simple).
 *
 * Wick talks mostly to the tower, and the tower answers in a lock: five brass
 * rings, each carrying a few letters. Turn them until the reading spells the
 * word cut into the lintel. The word is always a common one.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define NRING 5
#define RLET  4

typedef struct {
    char letter[NRING][RLET];
    int  pos[NRING];
    int  answer[NRING];
    const char *word;
    int  sel;
} Wheel;

static Wheel K;

static const char *WORDS[3] = { "LIGHT", "TOWER", "NIGHT" };

static bool solved(void)
{
    for (int i = 0; i < NRING; i++)
        if (K.letter[i][K.pos[i]] != K.word[i]) return false;
    return true;
}

static void init(pz_ctx *ctx)
{
    memset(&K, 0, sizeof K);
    K.word = WORDS[ctx->seed % 3u];
    K.sel = 0;
    unsigned r = ctx->seed | 1u;
    static const char DECOY[] = "ABCDEFGHIJKLMNOPRSTUVWY";
    for (int i = 0; i < NRING; i++) {
        r = r * 1664525u + 1013904223u;
        int slot = (int)((r >> 16) % RLET);
        K.answer[i] = slot;
        for (int k = 0; k < RLET; k++) {
            if (k == slot) { K.letter[i][k] = K.word[i]; continue; }
            char c;
            do {
                r = r * 1664525u + 1013904223u;
                c = DECOY[(r >> 16) % (sizeof DECOY - 1)];
            } while (c == K.word[i]);
            K.letter[i][k] = c;
        }
        r = r * 1664525u + 1013904223u;
        K.pos[i] = (int)((r >> 16) % RLET);
        if (K.pos[i] == slot) K.pos[i] = (slot + 1) % RLET;   /* never start solved */
    }
}

static Rectangle ring_rect(pz_ctx *ctx, int i, int k)
{
    float w = 120, h = 74;
    float ox = ctx->area.x + (ctx->area.width - NRING * (w + 16)) * 0.5f;
    return (Rectangle){ ox + i * (w + 16), ctx->area.y + 90 + k * (h + 8), w, h };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return PZ_RUNNING;
    Vector2 m = art_mouse();
    for (int i = 0; i < NRING; i++)
        for (int k = 0; k < RLET; k++) {
            if (!CheckCollisionPointRec(m, ring_rect(ctx, i, k))) continue;
            K.pos[i] = k;
            K.sel = i;
            sfx_play(SFX_TICK);
            if (solved()) return PZ_SOLVED;
            return PZ_RUNNING;
        }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("Five brass rings. Click the letter you want each one to show.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    char reading[NRING + 1];
    for (int i = 0; i < NRING; i++) reading[i] = K.letter[i][K.pos[i]];
    reading[NRING] = 0;
    float w = art_text_w(reading, 40);
    art_text(reading, ctx->area.x + (ctx->area.width - w) * 0.5f, ctx->area.y + 24, 40,
             col_ink());

    for (int i = 0; i < NRING; i++)
        for (int k = 0; k < RLET; k++) {
            Rectangle r = ring_rect(ctx, i, k);
            bool on = (K.pos[i] == k);
            Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                             { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
            ink_fill(q, 4, HATCH_NONE, 2800 + i * RLET + k,
                     on ? (Color){ 214, 176, 72, 255 } : (Color){ 232, 226, 210, 255 });
            ink_rect(r, on ? 2.8f : 1.6f, 0.7f, 2830 + i * RLET + k,
                     on ? col_ink() : col_ink_soft());
            char c[2] = { K.letter[i][k], 0 };
            float cw = art_text_w(c, 28);
            art_text(c, r.x + (r.width - cw) * 0.5f, r.y + 18, 28, col_ink());
        }

    doodle(D_KEY, ctx->area.x + 60, ctx->area.y + 60, 30, -0.3f,
           (Color){ 198, 168, 96, 255 });
    doodle(D_LOCK, ctx->area.x + ctx->area.width - 60, ctx->area.y + 60, 28, 0,
           (Color){ 150, 146, 140, 255 });
}

static const char *hint(pz_ctx *ctx, int tier)
{
    static char buf[128];
    switch (tier) {
    case 1: return "It is one ordinary word, five letters, and it is something "
                   "the tower does.";
    case 2:
        snprintf(buf, sizeof buf, "It begins with %c and ends with %c.",
                 K.word[0], K.word[NRING - 1]);
        return buf;
    default:
        snprintf(buf, sizeof buf, "The lintel says %s.", K.word);
        return buf;
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    for (int i = 0; i < NRING; i++)
        for (int k = 0; k < RLET; k++)
            if (K.letter[i][k] == K.word[i]) K.pos[i] = k;
    return solved();
}

static const pz_def def = {
    .id = "mechanism_key",
    .title = "Wick's Word Wheel",
    .clue_granted = "clue.stair_rotten",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
