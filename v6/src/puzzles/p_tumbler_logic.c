/* Petra Ward — tumbler logic (§5.1: logic/deduction-lite).
 *
 * Petra will not pick a lock for you, but she will teach you to read one.
 * Five pins, each at a height from 1 to 5, and five true statements about
 * them. Exactly one arrangement fits.
 */
#include "puzzle/puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define NPIN 5

typedef struct {
    int  pin[NPIN];             /* 1..5, all distinct */
    int  answer[NPIN];
    int  sel;
} Lock;

static Lock L;

/* Statements are generated from the answer so they are always true and always
 * sufficient — no puzzle in this game is ever unfair by construction. */
static char g_clue[NPIN][96];

static bool solved(void)
{
    for (int i = 0; i < NPIN; i++) if (L.pin[i] != L.answer[i]) return false;
    return true;
}

static void init(pz_ctx *ctx)
{
    memset(&L, 0, sizeof L);
    int a[NPIN] = { 1, 2, 3, 4, 5 };
    unsigned r = ctx->seed | 1u;
    for (int i = NPIN - 1; i > 0; i--) {
        r = r * 1664525u + 1013904223u;
        int j = (int)(r >> 16) % (i + 1);
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
    memcpy(L.answer, a, sizeof a);
    for (int i = 0; i < NPIN; i++) L.pin[i] = 1;
    L.sel = 0;

    static const char *NAME[NPIN] = { "first", "second", "third", "fourth", "fifth" };
    int order[NPIN];
    for(int i=0;i<NPIN;i++)order[L.answer[i]-1]=i;
    for(int i=0;i<NPIN-1;i++)
        snprintf(g_clue[i],96,"The %s pin sits lower than the %s.",NAME[order[i]],NAME[order[i+1]]);
    snprintf(g_clue[NPIN-1],96,"Use heights 1 to 5 exactly once each.");
}

static Rectangle pin_rect(pz_ctx *ctx, int i)
{
    return (Rectangle){ ctx->area.x + 520 + i * 82.0f, ctx->area.y + 60, 68, 300 };
}

static pz_status update(pz_ctx *ctx, float dt)
{
    Vector2 m = art_mouse();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < NPIN; i++) {
            if (!CheckCollisionPointRec(m, pin_rect(ctx, i))) continue;
            L.sel = i;
            /* click high in the column to raise, low to lower */
            Rectangle r = pin_rect(ctx, i);
            int step = (m.y < r.y + r.height * 0.5f) ? 1 : -1;
            L.pin[i] += step;
            if (L.pin[i] > NPIN) L.pin[i] = 1;
            if (L.pin[i] < 1) L.pin[i] = NPIN;
            sfx_play(SFX_TICK);
            if (solved()) return PZ_SOLVED;
            return PZ_RUNNING;
        }
    }
    if (IsKeyPressed(KEY_LEFT) && L.sel > 0) L.sel--;
    if (IsKeyPressed(KEY_RIGHT) && L.sel < NPIN - 1) L.sel++;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        L.pin[L.sel] += IsKeyPressed(KEY_UP) ? 1 : -1;
        if (L.pin[L.sel] > NPIN) L.pin[L.sel] = 1;
        if (L.pin[L.sel] < 1) L.pin[L.sel] = NPIN;
        if (solved()) return PZ_SOLVED;
    }
    return PZ_RUNNING;
}

static void draw(pz_ctx *ctx)
{
    art_text("Click the top of a pin to raise it, the bottom to lower it.",
             ctx->area.x, ctx->area.y - 26, 16, col_ink_soft());

    for (int i = 0; i < NPIN; i++)
        art_text_wrap(g_clue[i], ctx->area.x + 16, ctx->area.y + 40 + i * 60.0f, 440,
                      16, col_ink(), -1);

    for (int i = 0; i < NPIN; i++) {
        Rectangle r = pin_rect(ctx, i);
        ink_rect(r, 2.0f, 0.7f, 1700 + i, col_ink_soft());
        float unit = r.height / (NPIN + 1.0f);
        float h = unit * L.pin[i];
        Rectangle p = { r.x + 12, r.y + r.height - h, r.width - 24, h };
        Vector2 q[4] = { { p.x, p.y }, { p.x + p.width, p.y },
                         { p.x + p.width, p.y + p.height }, { p.x, p.y + p.height } };
        ink_fill(q, 4, HATCH_NONE, 1720 + i,
                 i == L.sel ? (Color){ 198, 168, 96, 255 } : (Color){ 168, 164, 156, 255 });
        ink_rect(p, 2.0f, 0.6f, 1740 + i, col_ink());
        char n[8];
        snprintf(n, sizeof n, "%d", L.pin[i]);
        art_text(n, r.x + r.width * 0.5f - 6, r.y + r.height + 8, 17, col_ink());
    }
    doodle(D_LOCK, ctx->area.x + 460, ctx->area.y + 380, 34, 0,
           (Color){ 130, 126, 120, 255 });
}

static const char *hint(pz_ctx *ctx, int tier)
{
    static char buf[128];
    switch (tier) {
    case 1: return "The five pins use every height from 1 to 5 exactly once. "
                   "No two pins share a height.";
    case 2: return "Find the pin that is never higher than another. That is 1. "
                   "Follow the comparisons upward to place 2, 3, 4 and 5.";
    default:
        snprintf(buf, sizeof buf, "The heights, in order: %d %d %d %d %d.",
                 L.answer[0], L.answer[1], L.answer[2], L.answer[3], L.answer[4]);
        return buf;
    }
}

static bool solve_replay(pz_ctx *ctx)
{
    init(ctx);
    int solutions=0;
    for(int a=1;a<=5;a++)for(int b=1;b<=5;b++)for(int c=1;c<=5;c++)for(int d=1;d<=5;d++)for(int e=1;e<=5;e++) {
        int p[5]={a,b,c,d,e};bool valid=true;
        for(int i=0;i<5;i++)for(int j=i+1;j<5;j++)if(p[i]==p[j])valid=false;
        for(int i=0;i<5;i++)for(int j=0;j<5;j++)if(L.answer[i]+1==L.answer[j] && p[i]>=p[j])valid=false;
        if(valid){solutions++;memcpy(L.pin,p,sizeof p);if(!solved())return false;}
    }
    return solutions==1 && solved();
}

static const pz_def def = {
    .id = "tumbler_logic",
    .title = "Petra's Teaching Lock",
    .clue_granted = "clue.picks_borrowed",
    .init = init,
    .update = update,
    .draw = draw,
    .hint = hint,
    .solve_replay = solve_replay,
};
PZ_REGISTER(def)
