/* sim.h — Section 5, exactly. The frozen contract (§13).
 *
 * Pure and raylib-free: no rendering, no audio, no wall clock, no allocation.
 * A VbSim is a value — copy it, diff it, run a thousand of them. Same seed and
 * same input stream produce a bit-identical match, which is what buys replays
 * and ghosts for free and keeps the netcode door open (§15).
 */
#ifndef VB_SIM_H
#define VB_SIM_H

#include "core.h"

/* ---- rods ------------------------------------------------------------- */
/* Paddles on a rod are rigidly spaced and move together: sliding a rod slides
 * its whole wall of light, exactly like gripping a foosball bar. */
typedef struct {
    int   side;          /* 0 or 1 — who holds this bar                     */
    int   role;          /* VB_GK / VB_DEF / VB_ATK                         */
    int   n;             /* paddle count on the rod: 1, 2 or 3              */
    float x;             /* rod plane, table x                              */
    float spacing;       /* rigid spacing between paddles along y           */
    float half;          /* paddle half length along y (tilt-scaled)        */
    float travel;        /* |offset| limit                                  */

    float off;           /* rod offset along y                              */
    float vel;           /* lateral velocity — the source of english        */

    int   state;         /* VB_ROD_*                                        */
    int   timer;         /* ticks left in windup / recovery                 */
    int   charge;        /* ticks the flick button has been held            */
    int   kind;          /* the strike being wound up (VB_STRIKE_*)         */
    int   pad;           /* which paddle on the rod is committed            */
    int   pin_ball;      /* index of the pinned ball, -1 when free          */
    int   pin_ticks;     /* pin shot clock, counts up to VB_PIN_CLOCK       */
    float pin_aim;       /* aim wedge angle while pinned, radians           */
    float pin_face;      /* which side of the paddle the ball is held on    */
    int   catch_on;      /* the controller is holding PIN this tick         */
    int   bunt_on;       /* a tapped catch: this contact will be cushioned  */
    int   whiff;         /* cosmetic: ticks of "I committed and missed"     */
    int   flash;         /* cosmetic: ticks of contact glow                 */
} VbRod;

/* ---- ball ------------------------------------------------------------- */
typedef struct {
    V2    p, v;
    float z, vz;         /* chip hop; z == 0 means live on the glass        */
    V2    curve;         /* Magnus-style lateral acceleration, decaying     */
    int   curve_ticks;
    float radius;
    int   heat;          /* 0..VB_HEAT_MAX                                  */
    int   alive;
    int   mercy;         /* pre-launch shimmer ticks; frozen while > 0      */
    V2    mercy_v;       /* the velocity waiting behind the shimmer         */
    int   last_rod;      /* rod index of the last contact, -1 at serve      */
    int   last_side;     /* side of the last toucher, -1 at serve           */
    int   chip_pass;     /* rod index this hop is licensed to clear         */
    int   contacts;      /* paddle contacts this rally — the rally counter  */
} VbBall;

/* ---- per-side handicap (§6 Table Tilt) -------------------------------- */
/* Openly negotiated before the match and shown as a badge. A shark and their
 * grandparent set the tilt like golf strokes and then both actually try. */
typedef struct {
    float pad_scale;     /* paddle length multiplier                        */
    float rod_speed;     /* rod speed multiplier                            */
    float assist;        /* 0..1 angle-assist strength                      */
    int   heat_cap;      /* personal heat ceiling                           */
    float goal_scale;    /* goal mouth width multiplier against this side   */
} VbTilt;

/* ---- mutators (§6 PARTY only) ----------------------------------------- */
enum {
    VB_MUT_MULTIBALL   = 1 << 0,
    VB_MUT_BIG_BALL    = 1 << 1,
    VB_MUT_PEA_BALL    = 1 << 2,
    VB_MUT_MAGNET      = 1 << 3,
    VB_MUT_MIRROR      = 1 << 4,
    VB_MUT_SUDDEN_HEAT = 1 << 5,
    VB_MUT_MOVING_GOAL = 1 << 6,
    VB_MUT_GOLDEN_GOAL = 1 << 7
};

typedef struct {
    int      scheme[VB_NPLAYERS];   /* VB_SCHEME_GLIDE / VB_SCHEME_GRIP     */
    int      doubles;               /* 2v2: slot 1 and 3 are the ATK owners */
    int      swap[VB_NSIDES];       /* teammates traded roles between points */
    unsigned mutators;              /* always 0 under STANDARD              */
    VbTilt   tilt[VB_NSIDES];
} VbSimCfg;

/* ---- events ----------------------------------------------------------- */
/* The sim's only outward voice. fx.c and synth.c read these; nothing writes
 * back. Cleared at the top of every step. */
enum {
    VB_EV_HIT = 0,       /* a: rod  b: ball  f: outgoing speed  i: kind     */
    VB_EV_WALL,          /* f: impact speed                                 */
    VB_EV_GOAL,          /* a: scoring side  i: heat at the goal            */
    VB_EV_PIN,           /* a: rod                                          */
    VB_EV_SNAP,          /* a: rod  f: speed                                */
    VB_EV_CHIP,          /* a: rod  b: ball                                 */
    VB_EV_WHIFF,         /* a: rod                                          */
    VB_EV_SAVE,          /* a: rod — a goal-bound ball turned away          */
    VB_EV_STALL,         /* referee pulse                                   */
    VB_EV_MERCY,         /* a heat>=8 strike held for its breath            */
    VB_EV_LAND,          /* a chip touching down  b: ball                   */
    VB_EV_COUNT_
};

typedef struct {
    int   type, a, b, i;
    float f;
    V2    at;
} VbEvent;

#define VB_MAXEV 48

/* ---- the sim ---------------------------------------------------------- */
typedef struct {
    VbSimCfg cfg;
    VbRod    rods[VB_NRODS];
    VbBall   balls[VB_MAXBALLS];
    int      nballs;

    unsigned tick;                  /* the only clock in the building       */
    unsigned rng;                   /* cosmetic only (§5.1)                 */

    int      active_rod[VB_NPLAYERS]; /* GRIP: the rod under the stick      */
    int      handoff_lock[VB_NPLAYERS]; /* ticks a manual override sticks   */
    unsigned prev_btn[VB_NPLAYERS];
    int      hold_flick[VB_NPLAYERS]; /* ticks FLICK has been down          */
    int      hold_pin[VB_NPLAYERS];

    /* possession bookkeeping for the anti-stall referee (§5.5) */
    int      ctrl_side;             /* -1 when nobody is sitting on it      */
    int      ctrl_ticks;
    int      serve_prot;            /* protected-possession ticks left      */
    int      serve_side;

    float    goal_y[VB_NSIDES];     /* centre of each mouth (Moving Goals)  */
    float    goal_half[VB_NSIDES];  /* tilt- and mutator-scaled             */

    VbEvent  ev[VB_MAXEV];
    int      nev;

    /* last goal, latched for rules.c */
    int      goal_side;             /* -1 none this tick                    */
    int      goal_heat;
} VbSim;

/* ---- API -------------------------------------------------------------- */

void vb_sim_init(VbSim *s, const VbSimCfg *cfg, unsigned seed);
/* Park the ball on side's goalie for a serve; clears heat and curve. */
void vb_sim_serve(VbSim *s, int side);
/* Add a live ball for Multiball; returns its index or -1 when full. */
int  vb_sim_spawn(VbSim *s, V2 p, V2 v);
/* One fixed tick. in[] is indexed by player slot (0,1 = side 0). */
void vb_sim_step(VbSim *s, const VbInput in[VB_NPLAYERS]);
/* The anti-stall pop: heat to 0, ball toward the middle. rules.c calls it. */
void vb_sim_pop_center(VbSim *s);

/* geometry helpers, shared with rods.c, ai.c, fx.c and the tests */
float vb_pad_y(const VbRod *r, int i);          /* centre of paddle i       */
/* The paddle's COLLISION span, which is not always its drawn span: a gap
 * narrower than the ball between the outermost paddle and the side wall is
 * closed, because such a gap is not a lane the ball can use — it is a slot the
 * ball gets wedged in and buzzes forever. Everything wide enough to pass
 * through stays exactly as drawn. */
void  vb_pad_span(const VbRod *r, int i, float ball_r, float *lo, float *hi);
int   vb_rod_of(int side, int role);            /* rod index                */
int   vb_rod_owner_slot(const VbSim *s, int rod); /* which player slot      */
/* Widest open lane on a rod at its current offset, in table units. The
 * Pillar-2 law says this never drops below VB_OPEN_LANE. */
float vb_rod_open_lane(const VbRod *r);
/* Nearest live ball to a rod plane, or -1. */
int   vb_nearest_ball(const VbSim *s, int rod);

void  vb_ev(VbSim *s, int type, int a, int b, int i, float f, V2 at);

#endif /* VB_SIM_H */
