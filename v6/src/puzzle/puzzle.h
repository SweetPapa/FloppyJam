/* puzzle — the plugin host and the plugin contract (API.md §7.4).
 *
 * One agent per puzzle, zero collisions: a mini-game is exactly one file in
 * src/puzzles/ that registers itself. There is no registry to merge.
 *
 * The host owns the standard frame — paper panel, title, hint tiers, the skip
 * flow, attempt counting, granting the clue — so a puzzle file contains only
 * the puzzle. Puzzles draw through artkit (visual consistency for free) and
 * never touch flags (§7.4).
 */
#ifndef HD_PUZZLE_H
#define HD_PUZZLE_H

#include "raylib.h"
#include <stdbool.h>

typedef enum { PZ_RUNNING = 0, PZ_SOLVED, PZ_EXITED } pz_status;

typedef struct {
    unsigned  seed;        /* derived from the save, so re-entry is identical */
    int       difficulty;  /* 0..2, ramps within a chapter and across the game */
    Rectangle area;        /* the play area the host hands over */
    int       attempts;    /* honest attempts so far; 3 unlocks the skip */
    int       hint_tier;   /* 0..3, spent from feathers by the host */
    float     t;           /* seconds since init, for the puzzle's own idle art */
} pz_ctx;

typedef struct {
    const char *id;                       /* "tart_tiling"      */
    const char *title;                    /* shown in the frame */
    const char *clue_granted;             /* clue token on solve, may be NULL  */
    void      (*init)(pz_ctx *);
    pz_status (*update)(pz_ctx *, float dt);
    void      (*draw)(pz_ctx *);
    const char *(*hint)(pz_ctx *, int tier);      /* tiers 1..3 */
    void      (*shutdown)(pz_ctx *);
    /* §10.5 gate: drive the puzzle to solved with no window and no input.
     * Returns true if the puzzle can actually be finished. */
    bool      (*solve_replay)(pz_ctx *);
} pz_def;

void pz_register(const pz_def *def);

/* self-registration; the Makefile globs the files, nothing indexes them */
#define PZ_REGISTER(sym)                                                       \
    __attribute__((constructor)) static void pz_ctor_##sym(void)               \
    { pz_register(&sym); }

/* --- host ---------------------------------------------------------------- */
int           puzzle_count(void);
const pz_def *puzzle_at(int i);
const pz_def *puzzle_find(const char *id);

bool        puzzle_start(const char *id, int difficulty, unsigned seed);
pz_status   puzzle_update(float dt);
void        puzzle_draw(void);
const char *puzzle_current_id(void);
const char *puzzle_clue(void);
bool        puzzle_was_skipped(void);
/* spend a feather on the next hint tier; the capture harness uses it too */
void        puzzle_spend_hint(void);

/* puzzles call this when the player commits a wrong answer; three honest
 * attempts unlock the skip, and nothing is ever taken away (§1.2.1) */
void pz_attempt_failed(pz_ctx *ctx);

/* small shared widgets so every mini-game looks like the same game */
void pz_button(Rectangle r, const char *label, bool enabled, int seed);
bool pz_button_clicked(Rectangle r, bool enabled);

#endif
