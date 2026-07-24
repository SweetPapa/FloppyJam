/* Bartleby Shelf — the shifted spine (§5.1: word/cipher, language-simple).
 *
 * The festival ledgers were catalogued by a librarian who thought letter
 * games were funny. Turn the dial until the spine reads like English, then
 * say so. No typing, no vocabulary walls: the answer is a common word.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

typedef struct {
    const char *plain;
    int   shift;            /* the true shift */
    int   dial;             /* what the player has turned to */
} Cipher;

static Cipher C;

static const char *PLAINS[3] = {
    "LANTERN NIGHT LEDGER",
    "THE PRISM DRINKS LIGHT",
    "EVERY LAMP ON THE GREEN",
};

static bool solved(void) { return C.dial == C.shift; }

static void init(pz_ctx *ctx)
{
    memset(&C, 0, sizeof C);
    C.plain = PLAINS[ctx->seed % 3u];
    C.shift = 3 + (int)((ctx->seed >> 4) % 20u);       /* never 0, never 26 */
    C.dial = 0;
}

/* the shelf shows plaintext rotated by (shift - dial): dial == shift reads true */
static void shown(char *out, int cap)
{
    int k = ((C.shift - C.dial) % 26 + 26) % 26;
    int i = 0;
    for (const char *p = C.plain; *p && i < cap - 1; p++, i++)
        out[i] = (*p >= 'A' && *p <= 'Z') ? (char)('A' + ((*p - 'A' + k) % 26)) : *p;
    out[i] = 0;
}

static Rectangle R_LEFT(pz_ctx *c)  { return (Rectangle){ c->area.x + 180, c->area.y + 250, 90, 66 }; }
static Rectangle R_RIGHT(pz_ctx *c) { return (Rectangle){ c->area.x + 660, c->area.y + 250, 90, 66 }; }
static Rectangle R_SAY(pz_ctx *c)   { return (Rectangle){ c->area.x + 330, c->area.y + 340, 280, 62 }; }

static pz_status update(pz_ctx *ctx, float dt)
{
    if (pz_button_clicked(R_LEFT(ctx), true))  { C.dial = (C.dial + 25) % 26; }
    if (pz_button_clicked(R_RIGHT(ctx), true)) { C.dial = (C.dial + 1) % 26; }
    if (IsKeyPressed(KEY_LEFT))  C.dial = (C.dial + 25) % 26;
    if (IsKeyPressed(KEY_RIGHT)) C.dial = (C.dial + 1) % 26;

    if (pz_button_clicked(R_SAY(ctx), true)) {
        if (solved()) return PZ_SOLVED;
        pz_attempt_failed(ctx);
    }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("Turn the dial until the spine says something. Then tell Bartleby.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    Rectangle spine = { ctx->area.x + 120, ctx->area.y + 100, ctx->area.width - 240, 110 };
    Vector2 q[4] = { { spine.x, spine.y }, { spine.x + spine.width, spine.y },
                     { spine.x + spine.width, spine.y + spine.height },
                     { spine.x, spine.y + spine.height } };
    ink_fill(q, 4, HATCH_NONE, 1600, (Color){ 122, 96, 132, 255 });
    ink_rect(spine, 3.0f, 1.0f, 1601, col_ink());

    char text[64];
    shown(text, sizeof text);
    float w = art_text_w(text, 28);
    art_text(text, spine.x + (spine.width - w) * 0.5f, spine.y + 38, 28,
             (Color){ 244, 238, 220, 255 });

    pz_button(R_LEFT(ctx), "<", true, 1610);
    pz_button(R_RIGHT(ctx), ">", true, 1612);
    char d[32];
    snprintf(d, sizeof d, "dial: %d", C.dial);
    float dw = art_text_w(d, 22);
    art_text(d, ctx->area.x + 465 - dw * 0.5f, ctx->area.y + 266, 22, col_ink());
    pz_button(R_SAY(ctx), "That is what it says", true, 1614);

    doodle(D_BOOK, ctx->area.x + 60, ctx->area.y + 330, 40, -0.15f,
           (Color){ 122, 96, 132, 255 });
}

static const char *hint(pz_ctx *ctx, int tier)
{
    static char buf[160];
    switch (tier) {
    case 1: return "Every letter has moved along the alphabet by the same step. "
                   "Find the step and the whole spine falls out at once.";
    case 2: {
        char t[64];
        shown(t, sizeof t);
        snprintf(buf, sizeof buf, "The first word has %d letters and the whole "
                 "thing is about the festival. Try making the first letter an L, "
                 "a T or an E.", (int)strcspn(t, " "));
        return buf;
    }
    default:
        snprintf(buf, sizeof buf, "The dial wants to be on %d.", C.shift);
        return buf;
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    for (int i = 0; i < 26; i++) {          /* the honest brute force, 26 turns */
        if (solved()) return true;
        C.dial = (C.dial + 1) % 26;
    }
    return solved();
}

static const pz_def def = {
    .id = "cipher_shelf",
    .title = "Bartleby's Catalogue",
    .clue_granted = "clue.festival_lapsed",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
