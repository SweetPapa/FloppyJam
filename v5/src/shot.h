/* shot.h — Section 4. Aim, power meter, english widget, aim guide.
 * The verb set never grows past these three things. */
#ifndef BP_SHOT_H
#define BP_SHOT_H

#include "physics.h"

typedef struct {
    float aim;              /* radians, 0 = +Z                            */
    float tx, ty;           /* tip offset, fractions of R                 */
    int   charging;
    float meter;            /* 0..1 displayed power                       */
    float meter_t;          /* seconds held                               */
    float last_power;
    BpGuideHit guide;

    /* cue animation: -1 .. 0 backswing, 0 .. 1 follow-through */
    float cue_pull;
    float strike_anim;      /* counts down after the strike               */
} BpShot;

void  bp_shot_reset(BpShot *s, int keep_spin);
/* Advance the power meter. Returns the power to strike with, or 0. */
float bp_shot_charge(BpShot *s, int held, float dt);
void  bp_shot_update_guide(BpShot *s, const BpWorld *w);
float bp_shot_power_curve(float p);

#endif
