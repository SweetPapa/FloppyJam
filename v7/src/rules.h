/* rules.h — Section 6. Rulesets, serve, clocks, scoring, mutators.
 *
 * The ruleset lock is a promise to competitive players: STANDARD cannot be
 * given a mutator by any path through this API. vb_match_init strips them.
 *
 * Raylib-free.
 */
#ifndef VB_RULES_H
#define VB_RULES_H

#include "sim.h"

enum { VB_MODE_VERSUS = 0, VB_MODE_RALLY, VB_MODE_GAUNTLET, VB_MODE_TRICKSHOT };
enum { VB_RULES_STANDARD = 0, VB_RULES_PARTY, VB_RULES_CO_OP };

/* match phases — between points is 1.2 s, max, because this game respects the
 * run-it-back reflex (§10) */
enum { VB_PH_SERVE = 0, VB_PH_LIVE, VB_PH_GOAL, VB_PH_OVER };

typedef struct {
    int longest_rally;   /* contacts in the best volley                     */
    int top_heat;
    int saves;
    int scorchers;
    int pins;
    int chips;
    int goals;
    int banks;
} VbStats;

typedef struct {
    int mode;            /* VB_MODE_*                                       */
    int ruleset;         /* VB_RULES_*                                      */
    int target;          /* first to 5 / 7 / 10                             */
    int golden;          /* Golden Goal finale (PARTY)                      */
} VbMatchCfg;

typedef struct {
    VbSim      sim;
    VbMatchCfg cfg;

    int   score[VB_NSIDES];
    int   phase, phase_ticks;
    int   serve_to;          /* loser serves (§5.5)                         */
    int   winner;            /* -1 until somebody takes it                  */
    int   match_point;       /* the key lifts a half-step for this (§9)     */

    int   rally;             /* contacts in the live rally                  */
    int   rally_heat;        /* peak heat this rally                        */
    int   streak;            /* RALLY mode: the shared keep-up streak       */
    int   best_streak;

    int   stall_warned;
    int   last_goal_heat;
    int   last_goal_side;
    int   scorcher_crack;    /* a cracked-glass rally follows a scorcher    */

    VbStats stats[VB_NSIDES];
} VbMatch;

void vb_match_init(VbMatch *m, const VbMatchCfg *mc, const VbSimCfg *sc,
                   unsigned seed);
/* One fixed tick of the whole match: clocks, sim, scoring, serve. */
void vb_match_step(VbMatch *m, const VbInput in[VB_NPLAYERS]);
/* True while the sim is actually running (not between points). */
int  vb_match_live(const VbMatch *m);
/* Points needed by the leader — used by the HUD and the music key lift. */
int  vb_match_is_match_point(const VbMatch *m);

#endif /* VB_RULES_H */
