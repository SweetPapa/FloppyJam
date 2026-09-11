#include "puzzle.h"
#include "art/artkit.h"
#include "audio/synth.h"
#include "flags/flags.h"
#include "content/content.h"
#include "save/save.h"
#include "scene/scene.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define PZ_MAX 32

static const pz_def *g_reg[PZ_MAX];
static int           g_nreg;

void pz_register(const pz_def *def)
{
    if (g_nreg < PZ_MAX && def && def->id) g_reg[g_nreg++] = def;
}

int puzzle_count(void) { return g_nreg; }
const pz_def *puzzle_at(int i) { return (i >= 0 && i < g_nreg) ? g_reg[i] : NULL; }

const pz_def *puzzle_find(const char *id)
{
    for (int i = 0; i < g_nreg; i++)
        if (strcmp(g_reg[i]->id, id) == 0) return g_reg[i];
    return NULL;
}

/* ------------------------------------------------------------------- host */
static const pz_def *g_cur;
static pz_ctx        g_ctx;
static const char   *g_hint_text;
static bool          g_skipped;
static float         g_solve_flash;
static bool          g_solved;
static bool          g_hint_open, g_offer_help;
static Rectangle R_NOTE_CLOSE = {800,478,210,46};
static Rectangle R_REVIEW = {674,658,296,36};

static Rectangle R_HINT  = { 150, 596, 240, 60 };
static Rectangle R_SKIP  = { 410, 596, 240, 60 };
static Rectangle R_LEAVE = { 1000, 596, 180, 60 };

const char *puzzle_current_id(void) { return g_cur ? g_cur->id : ""; }
const char *puzzle_clue(void) { return (g_cur && g_cur->clue_granted) ? g_cur->clue_granted : ""; }
bool puzzle_was_skipped(void) { return g_skipped; }

bool puzzle_start(const char *id, int difficulty, unsigned seed)
{
    const pz_def *d = puzzle_find(id);
    if (!d) return false;
    g_cur = d;
    memset(&g_ctx, 0, sizeof g_ctx);
    g_ctx.seed = seed ? seed : 0x9e3779b9u;
    g_ctx.difficulty = difficulty;
    g_ctx.area = (Rectangle){ 170, 168, 940, 392 };
    g_hint_text = NULL;
    g_hint_open=false;g_offer_help=false;
    g_skipped = false;
    g_solved = false;
    g_solve_flash = 0;
    if (d->init) d->init(&g_ctx);
    char key[48];snprintf(key,sizeof key,"phint.%s",id);
    g_ctx.hint_tier=flag_get(key);if(g_ctx.hint_tier>3)g_ctx.hint_tier=3;
    if(g_ctx.hint_tier>0 && d->hint)g_hint_text=d->hint(&g_ctx,g_ctx.hint_tier);
    music_mood(MOOD_PUZZLE);
    return true;
}

void pz_attempt_failed(pz_ctx *ctx)
{
    ctx->attempts++;
    sfx_play(SFX_CLUNK);
}

void pz_button(Rectangle r, const char *label, bool enabled, int seed)
{
    bool hot = enabled && art_hover(r);
    Color fill = !enabled ? (Color){221,215,202,255}
               : hot ? (Color){226,201,156,255} : (Color){239,228,207,255};
    DrawRectangleRounded(r,0.08f,4,fill);
    ink_rect(r, 2.2f, 0.8f, seed + 1, enabled ? col_ink() : col_ink_soft());
    /* a label that outgrows its button steps down rather than spilling out */
    float sz = art_text_size_for(label, r.width - 18, 18);
    float w = art_text_w(label, sz);
    art_text(label, r.x + (r.width - w) * 0.5f,
             r.y + r.height * 0.5f - sz * text_scale() * 0.62f, sz,
             enabled ? col_ink() : col_ink_soft());
}

bool pz_button_clicked(Rectangle r, bool enabled)
{
    if (!enabled) return false;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return false;
    if (!art_hover(r)) return false;
    sfx_play(SFX_CLICK);
    return true;
}

void puzzle_spend_hint(void)
{
    if(!g_cur)return;
    if(g_ctx.hint_tier>=3){g_hint_open=true;return;}
    if(g_ctx.hint_tier>0 && flag_get("feathers")<=0) {
        g_hint_text=ui_str("hint.nofeathers");g_hint_open=true;return;
    }
    if(g_ctx.hint_tier>0)flag_add("feathers",-1);
    g_ctx.hint_tier++;sfx_play(SFX_SPARKLE);
    g_hint_text=g_cur->hint?g_cur->hint(&g_ctx,g_ctx.hint_tier):ui_str("hint.none");
    g_hint_open=true;
    char key[48];snprintf(key,sizeof key,"phint.%s",g_cur->id);flag_set(key,g_ctx.hint_tier);
    save_autosave(scene_current());
}

pz_status puzzle_update(float dt)
{
    if (!g_cur) return PZ_EXITED;
    g_ctx.t += dt;

    if (g_solved) {
        g_solve_flash -= dt;
        if (g_solve_flash <= 0) return PZ_SOLVED;
        return PZ_RUNNING;
    }

    if(g_hint_open || g_offer_help) {
        if(IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) ||
           pz_button_clicked(R_NOTE_CLOSE,true)) {
            if(g_offer_help && !IsKeyPressed(KEY_ESCAPE)) {
                g_skipped=true;g_solved=true;g_solve_flash=.9f;sfx_play(SFX_SOLVE);
            }
            g_hint_open=false;g_offer_help=false;
        }
        return PZ_RUNNING; /* modal: clicking the note never moves a puzzle piece */
    }
    if (IsKeyPressed(KEY_ESCAPE) || pz_button_clicked(R_LEAVE,true)) return PZ_EXITED;
    if (pz_button_clicked(R_HINT,g_ctx.hint_tier==0 || (flag_get("feathers")>0 && g_ctx.hint_tier<3))) {
        puzzle_spend_hint();return PZ_RUNNING;
    }
    if(pz_button_clicked(R_REVIEW,g_hint_text!=NULL)) {g_hint_open=true;return PZ_RUNNING;}
    if(pz_button_clicked(R_SKIP,true)) {g_offer_help=true;return PZ_RUNNING;}

    pz_status s = g_cur->update ? g_cur->update(&g_ctx, dt) : PZ_RUNNING;
    if (s == PZ_SOLVED) {
        g_solved = true;
        g_solve_flash = 1.3f;
        sfx_play(SFX_SOLVE);
        return PZ_RUNNING;
    }
    return s;
}

void puzzle_draw(void)
{
    if (!g_cur) return;
    paper_grain(0.5f);

    Rectangle frame = { 140, 70, 1000, 500 };
    paper_panel(frame, 5.0f, 6161);
    art_text(g_cur->title ? g_cur->title : g_cur->id, frame.x + 26, frame.y + 20, 23,
             col_ink());
    ink_line(frame.x + 24, frame.y + 58, frame.x + frame.width - 24, frame.y + 58,
             2.0f, 1.2f, 6162, col_ink_soft());

    if (g_cur->draw) g_cur->draw(&g_ctx);

    char hb[64];
    snprintf(hb,sizeof hb,g_ctx.hint_tier==0?"First hint: free":"Next hint (%d feathers)",flag_get("feathers"));
    pz_button(R_HINT,hb,g_ctx.hint_tier==0 || (flag_get("feathers")>0 && g_ctx.hint_tier<3),6170);
    pz_button(R_SKIP,"Work together",true,6172);
    pz_button(R_LEAVE,ui_str("pz.leave"),true,6174);
    if(g_hint_text)pz_button(R_REVIEW,"Read your last hint",true,6176);

    const char *msg;
    char buf[256];
    if (g_solved) {
        snprintf(buf, sizeof buf, "%s",
                 g_skipped ? ui_str("pz.skipped") : ui_str("pz.solved"));
        msg = buf;
    } else msg = "Take your time. Hints start free; help is always available.";
    art_text_fit(msg, (Rectangle){ 668, 594, 322, 52 }, 15,
                 g_solved ? col_cool() : col_ink());

    if((g_hint_open || g_offer_help) && !g_solved) {
        DrawRectangle(0,0,VW,VH,(Color){24,22,28,125});
        Rectangle note={236,260,808,290};paper_panel(note,3,6180);
        doodle(D_FEATHER,274,302,18,0,col_ink());
        art_text(g_offer_help?"A little help from a friend":"A note from Pip",310,284,25,col_ink());
        art_text_fit(g_offer_help?"Your neighbour will finish this puzzle with you. You will still receive the evidence and continue the story. There is no cost. Escape closes this note if you want another try.":g_hint_text,
            (Rectangle){274,338,732,118},21,col_ink());
        pz_button(R_NOTE_CLOSE,g_offer_help?"Finish together":"Back to puzzle",true,6184);
    }
    vignette(0.5f);
}
