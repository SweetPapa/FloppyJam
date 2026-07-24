/* replay.h — 16 bytes per shot. Determinism makes replays free (S.14). */
#ifndef BP_REPLAY_H
#define BP_REPLAY_H

#include "course.h"

#define BP_MAX_SHOTS 16

typedef struct { float aim, power, tx, ty; } BpShotRec;

typedef struct {
    BpShotRec shot[BP_MAX_SHOTS];
    int n;
    int hole;
} BpReplay;

void bp_replay_begin(BpReplay *r, int hole);
void bp_replay_push(BpReplay *r, float aim, float power, float tx, float ty);

/* Rebuild the world and re-play shots [0, upto) so the caller can watch or
 * re-sim shot `upto`. Returns 1 if `upto` is a valid shot index. */
int  bp_replay_seek(const BpReplay *r, BpWorld *w, int upto);

#endif
