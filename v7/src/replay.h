/* replay.h — Section 5.1 and §10. Free, because the sim is deterministic.
 *
 * Nothing is recorded except the sim state a rally began in and the per-tick
 * inputs after it. Playback restores the snapshot and replays the buttons, so
 * a replay is not an approximation of the rally — it IS the rally, run again.
 *
 * Raylib-free.
 */
#ifndef VB_REPLAY_H
#define VB_REPLAY_H

#include "sim.h"

#define VB_RALLY_TICKS  (VB_TICK_HZ * 20)   /* a 20 s rally is a long one   */
#define VB_REPLAY_SLOTS 4

typedef struct { signed char axis; unsigned char btn; } VbFrame;

typedef struct {
    VbSim   start;                                  /* the rally's tick 0   */
    VbFrame in[VB_RALLY_TICKS][VB_NPLAYERS];
    int     len;
    int     used;
    int     contacts, top_heat, scorcher, scored_by;
} VbRally;

typedef struct {
    VbRally slot[VB_REPLAY_SLOTS];
    int     cur;              /* slot currently being written               */
    int     recording;
    int     playing, play_t, play_slot;
    VbSim   play_sim;
} VbReplay;

void vb_replay_reset(VbReplay *r);
void vb_replay_begin(VbReplay *r, const VbSim *s);
void vb_replay_record(VbReplay *r, const VbInput in[VB_NPLAYERS]);
void vb_replay_end(VbReplay *r, int contacts, int top_heat, int scorcher,
                   int scored_by);
/* The rally worth showing: contacts first, then heat, then the scorcher. */
int  vb_replay_best(const VbReplay *r);
void vb_replay_play(VbReplay *r, int slot);
/* Advances the playback sim one tick. Returns 0 when the rally is over. */
int  vb_replay_step(VbReplay *r);

#endif /* VB_REPLAY_H */
