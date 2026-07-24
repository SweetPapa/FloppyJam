#include "replay.h"

void bp_replay_begin(BpReplay *r, int hole)
{
    r->n = 0;
    r->hole = hole;
}

void bp_replay_push(BpReplay *r, float aim, float power, float tx, float ty)
{
    BpShotRec *s;
    if (r->n >= BP_MAX_SHOTS) return;
    s = &r->shot[r->n++];
    s->aim = aim; s->power = power; s->tx = tx; s->ty = ty;
}

int bp_replay_seek(const BpReplay *r, BpWorld *w, int upto)
{
    int i, t;
    if (upto < 0 || upto > r->n) return 0;
    bp_course_build(w, r->hole);
    for (i = 0; i < upto; ++i) {
        bp_shot_begin(w);
        bp_strike(w, r->shot[i].aim, r->shot[i].power, r->shot[i].tx, r->shot[i].ty);
        for (t = 0; t < 20000 && !bp_settled(w); ++t) {
            bp_step(w);
            if (w->holed) break;
        }
        if (w->scratched && !w->holed) bp_respawn_cue(w);
    }
    return upto < r->n;
}
