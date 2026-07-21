/* Shared scaffolding for the 15 microgames (§5). */
#ifndef G_MICRO_COMMON_H
#define G_MICRO_COMMON_H

#include "game.h"

/* request a flavor for a phrase bar AND read back the authored pattern.
 * build_chart runs one full bar before the phrase plays, so the request always
 * lands before the sequencer reaches that bar. */
void mc_bar(const Genome *g, const PhraseCtx *c, int bar, BarFlavor f, BarPattern *out);
/* flavor NONE for every bar of the phrase — clears stale ring entries */
void mc_plain(const Genome *g, const PhraseCtx *c, int bar, BarPattern *out);

/* the standard hit/miss loop most games inherit.
 * strict_extra: 1 = a press with no note in range counts as a MISS, not just noise */
void mc_update_standard(MicroState *m, const InputEvents *in, const ClockSnap *clk,
                        int strict_extra);
void mc_expire(MicroState *m, const ClockSnap *clk);
/* the default success condition from §4.2 */
bool mc_judge_default(const MicroState *m);

/* standard playfield: N lanes with keycap labels and falling notes */
void mc_draw_standard(MicroState *m, int lanes, const int *lane_ids);

/* map a midi note to one of `lanes` lanes, stably */
int  mc_note_lane(int midi, int lanes);

extern const int LANES_SPACE[1];
extern const int LANES_FJ[2];
extern const int LANES_FGHJ[4];
extern const int LANES_ALL[5];
extern const char *LANE_LABEL[LANE_COUNT];

/* the roster, defined one game per struct in micro_a/b/c.c */
extern const Microgame MG_TAP, MG_UPBEAT, MG_ALTERNATE, MG_HOLD, MG_REST;
extern const Microgame MG_SNIPE, MG_DOUBLE, MG_STAIRS, MG_RAPID, MG_ECHO;
extern const Microgame MG_BLIND, MG_SWAP, MG_MEASURE, MG_CALLBACK, MG_FINALE;

#endif
