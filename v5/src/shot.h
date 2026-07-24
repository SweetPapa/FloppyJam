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
    float aim_tick;         /* 0..1 pulse on each aim detent              */
    float cue_pull;
    float strike_anim;      /* counts down after the strike               */
    /* SPACE must be released once before a new charge can begin. Set on
     * resume / hole start so a menu SPACE never leaks into a shot.         */
    int   charge_lock;
} BpShot;

void  bp_shot_reset(BpShot *s, int keep_spin);
/* Advance the power meter. Returns the power to strike with, or 0. */
float bp_shot_charge(BpShot *s, int held, float dt);
void  bp_shot_update_guide(BpShot *s, const BpWorld *w);
float bp_shot_power_curve(float p);

/* ---- trajectory preview (optional assist) --------------------------
 *
 * The simulation is deterministic, so a "prediction" is not an estimate: we
 * copy the world, play the shot on the copy with the real bp_strike/bp_step,
 * and record where the ball went. The line is EXACT for the power shown —
 * the only error left is your own release timing on the meter.
 *
 * Off by default. The spec (4.3, 15) argues hard for a geometry-only guide,
 * and that stays the shipped default; this is opt-in for players who want it.
 */
#define BP_PREVIEW_MAX     384   /* samples on the cue-ball path         */
#define BP_PREVIEW_OBJ     3     /* object balls whose path is drawn     */
/* hit[] low bits are the contact marker (1 wall, 2 bumper, 3 ball). The high
 * bit means "the ball teleported to here" (a warp pocket) — the renderer must
 * NOT draw a segment into a break point, or a long straight line jumps clean
 * across the hole and the preview looks wonky on warp holes. */
#define BP_PV_MARK  0x0f
#define BP_PV_BREAK 0x80
/* A gap between consecutive samples larger than this is a teleport, not a
 * roll: a warp pocket, or the ball plunging off the world. A full-speed ball
 * at the coarsest decimated sampling still moves under 2 m in one step, while
 * a warp jumps many metres, so this threshold cleanly separates a genuine
 * (if coarse) path from a discontinuity that must not be drawn. */
#define BP_PV_JUMP  3.0f
#define BP_PREVIEW_OBJ_MAX 64

typedef struct {
    V3            pt[BP_PREVIEW_MAX];
    unsigned char hit[BP_PREVIEW_MAX];   /* 1 wall, 2 bumper, 3 ball     */
    int           n;

    struct {
        V3  pt[BP_PREVIEW_OBJ_MAX];
        int n;
        unsigned char color;
    } obj[BP_PREVIEW_OBJ];
    int nobj;

    int   holed;        /* the cue ball finds the cup                    */
    int   scratched;    /* void, water or a scratch pocket               */
    int   bonus;        /* an object ball drops in a gold pocket         */
    float rest_dist;    /* how far from the cup the cue ball settles     */
    float power;        /* the power this path was computed for          */
    int   valid;
} BpPreview;

/* Play `power` out on a scratch copy of the world and record the path. */
void  bp_predict(const BpWorld *w, float aim, float power, float tx, float ty,
                 BpPreview *out);

/* Searching every power for the best one costs ~8 ms on a busy hole, which
 * is a visible hitch if you do it in a single frame. So the planner is
 * incremental: the caller spends a few candidates per frame and the preview
 * shows the best found so far while it converges. */
typedef struct {
    float aim, tx, ty;
    int   phase;          /* 0 coarse sweep, 1 local refine, 2 finished    */
    int   i;
    float best_p, best_score;
    int   done;
} BpPlanner;

void bp_plan_begin(BpPlanner *pl, float aim, float tx, float ty);
/* Spend `budget` candidate sims. Returns 1 once the plan is final. */
int  bp_plan_step(const BpWorld *w, BpPlanner *pl, int budget, BpPreview *out);

#endif
