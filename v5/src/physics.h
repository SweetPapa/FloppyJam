/* physics.h — Section 5, the ground truth.
 *
 * One deterministic simulation. No wall-clock, no randomness, no scripted
 * outcomes. Everything else in the game observes this module; nothing here
 * knows that rendering exists.
 */
#ifndef BP_PHYSICS_H
#define BP_PHYSICS_H

#include "core.h"

/* ---- limits -------------------------------------------------------- */
#define BP_MAX_PADS    72
#define BP_MAX_BOXES   96
#define BP_MAX_POSTS   40
#define BP_MAX_POCKETS 14
#define BP_MAX_BALLS   12
#define BP_MAX_MOVERS  10
#define BP_MAX_GATES    4
#define BP_MAX_EVENTS  48

/* ---- surfaces (Section 6.3) ---------------------------------------- */
typedef enum {
    SURF_FELT = 0,   /* the default green                                */
    SURF_ROUGH,      /* golf rough: kills hero overshoots                */
    SURF_ICE,        /* touch-shot terror                                */
    SURF_SAND,       /* speed-capped trap                                */
    SURF_KICK,       /* kicker pad: sets speed along an arrow            */
    SURF_COUNT
} BpSurf;

typedef struct { float mu_slide, mu_roll, mu_spin, cap; } BpSurfDef;
extern const BpSurfDef BP_SURF[SURF_COUNT];

/* ---- geometry ------------------------------------------------------ */

/* Rail bitmask on a pad edge. */
#define PAD_W 1  /* -X */
#define PAD_E 2  /* +X */
#define PAD_N 4  /* -Z */
#define PAD_S 8  /* +Z */

/* Floor pad: XZ rectangle carrying a linear height plane.
 * h(x,z) = y0 + sx*(x-x0) + sz*(z-z0)   */
typedef struct {
    float x0, z0, x1, z1;
    float y0, sx, sz;
    unsigned char surf;
    unsigned char rails;   /* PAD_* bits: auto-generate cushion boxes    */
    unsigned char mover;   /* mover index+1 (tilt platforms), 0 = static */
    float ka;              /* kicker arrow, radians (SURF_KICK only)     */
    float ks;              /* kicker speed, m/s                          */
} BpPad;

typedef enum { BOX_RAIL = 0, BOX_BUMPER_WALL, BOX_GATE, BOX_CAP } BpBoxKind;

/* Oriented (yaw-only) box: rails, gates, sweeper bars, cup caps. */
typedef struct {
    float cx, cy, cz;
    float hx, hy, hz;
    float yaw;
    unsigned char kind;
    unsigned char gate;    /* gate id+1: solid only while gate is shut   */
    unsigned char mover;   /* mover index+1                              */
} BpBox;

typedef enum { POST_RAIL = 0, POST_BUMPER, POST_TARGET } BpPostKind;

typedef struct {
    float cx, cy, cz;      /* cy = base                                  */
    float r, h;
    unsigned char kind;
    unsigned char target;  /* gate id+1 this post opens when struck      */
    unsigned char mover;
    unsigned char lit;     /* runtime: target already struck             */
} BpPost;

typedef enum { PK_CUP = 0, PK_SCRATCH, PK_BONUS, PK_WARP } BpPocketKind;

typedef struct {
    float x, z, y;         /* y = rim plane                              */
    unsigned char kind;
    signed char link;      /* warp partner pocket index                  */
} BpPocket;

typedef enum { BALL_CUE = 0, BALL_OBJ, BALL_EIGHT } BpBallKind;
typedef enum { BS_REST = 0, BS_ROLL, BS_AIR, BS_GONE } BpBallState;

typedef struct {
    V3 p, v, w;            /* position, velocity, angular velocity       */
    V3 home;               /* spawn (object balls respawn here)          */
    V3 prev;               /* rest position before the current shot      */
    unsigned char kind;
    unsigned char state;
    unsigned char color;   /* palette slot                               */
    unsigned char num;     /* printed number, 0 = cue                    */
    unsigned char grounded;
    unsigned char surf;    /* surface currently under the ball           */
    unsigned char respawn; /* voided: restore to home at next shot start */
    short cool;            /* ticks of pocket immunity after a warp       */
    float spinvis;         /* accumulated roll angle, cosmetic           */
    V3 axis;               /* cosmetic roll axis                         */
} BpBall;

/* ---- movers (Section 6.4): pose is a pure function of shot ticks ---- */
typedef enum {
    MOV_ORBIT = 0,   /* p = c + r*(cos,sin)(w*t + ph)                    */
    MOV_SPIN,        /* yaw = w*t + ph                                   */
    MOV_SHUTTLE,     /* p = c + dir * amp * tri(w*t + ph)                */
    MOV_TILT         /* pad slope = amp * sin(w*t + ph)                  */
} BpMoverKind;

typedef struct {
    unsigned char kind;
    float cx, cy, cz;      /* pivot / centre                             */
    float amp;             /* radius / travel / slope amplitude          */
    float rate;            /* rad per second                             */
    float phase;
    float dx, dz;          /* shuttle direction (unit)                   */
} BpMover;

/* ---- events (consumed by juice + audio, never by the sim) ---------- */
typedef enum {
    EV_WALL = 0, EV_BUMPER, EV_BALLHIT, EV_LAND, EV_RIM,
    EV_POCKET, EV_KICK, EV_TARGET, EV_REST, EV_VOID, EV_SURFACE
} BpEventKind;

typedef struct {
    unsigned char kind;
    unsigned char a;       /* ball index                                 */
    signed char b;          /* other ball / pocket index / surface id     */
    float mag;             /* impact speed etc.                          */
    V3 at;
} BpEvent;

/* ---- world --------------------------------------------------------- */
typedef struct {
    BpPad     pads[BP_MAX_PADS];       int npads;
    BpBox     boxes[BP_MAX_BOXES];     int nboxes;
    BpPost    posts[BP_MAX_POSTS];     int nposts;
    BpPocket  pockets[BP_MAX_POCKETS]; int npockets;
    BpMover   movers[BP_MAX_MOVERS];   int nmovers;
    BpBall    balls[BP_MAX_BALLS];     int nballs;

    unsigned char gate_open[BP_MAX_GATES];
    unsigned char cup_sealed;          /* rack holes (7.4)               */
    float kill_y;                      /* below this = void              */

    int   tick;                        /* ticks since shot start         */
    float ftick;                       /* fractional tick inside substep */
    float min_thick;                   /* thinnest wall, drives substeps */

    BpEvent ev[BP_MAX_EVENTS]; int nev;

    /* per-shot outcome flags, cleared by bp_shot_begin */
    int   holed;            /* cue ball captured by the cup              */
    int   scratched;        /* cue ball in a scratch/bonus pocket, or void*/
    int   bonus_hits;       /* object balls potted into bonus pockets     */
    int   eight_potted;
    int   wall_hits;        /* banks taken by the cue ball this shot      */
    float dist_travelled;   /* cue ball path length this shot             */
} BpWorld;

/* ---- API ----------------------------------------------------------- */
void  bp_world_reset(BpWorld *w);
void  bp_world_finalize(BpWorld *w);          /* builds rails, min_thick */

/* Snapshot rest positions and clear per-shot state. Call before a strike. */
void  bp_shot_begin(BpWorld *w);
/* Apply the cue impulse. aim in radians (0 = +Z), power 0..1,
 * tip offset (tx,ty) in fractions of R, |offset| <= BP_TIP_MAX. */
void  bp_strike(BpWorld *w, float aim, float power, float tx, float ty);

void  bp_step(BpWorld *w);                    /* one BP_DT tick          */
/* Put the cue ball back where it was before the shot (5.8 penalty flow). */
void  bp_respawn_cue(BpWorld *w);
int   bp_settled(const BpWorld *w);           /* all balls at rest       */

/* Geometry queries shared with the aim guide and renderer. */
float bp_ground_h(const BpWorld *w, float x, float z, float bally, int *pad_out);
void  bp_mover_pose(const BpWorld *w, int mover, float tick, V3 *off, float *yaw);
V3    bp_pad_normal(const BpPad *p);
/* Live (tilt-aware) accessors the renderer needs. */
float bp_pad_height(const BpWorld *w, int pad, float x, float z);
V3    bp_pad_normal_at(const BpWorld *w, int pad);
void  bp_box_pose(const BpWorld *w, int i, V3 *c, float *yaw);
int   bp_box_visible(const BpWorld *w, int i);
void  bp_post_pose(const BpWorld *w, int i, V3 *c);
int   bp_pocket_at(const BpWorld *w, float x, float z, float y);

/* Cast the aim guide: returns distance to first contact, fills hit info.
 * kind: 0 none, 1 wall/post, 2 ball, 3 pocket/cup, 4 ledge. */
typedef struct {
    int kind;
    V3 point;
    V3 normal;      /* surface normal at contact (walls)                 */
    V3 rebound;     /* centre-ball rebound direction                     */
    int ball;       /* object ball index for kind==2                     */
    V3 objdir;      /* object ball departure direction                   */
    float dist;
} BpGuideHit;
void bp_guide(const BpWorld *w, int ball, float aim, BpGuideHit *out);

#endif /* BP_PHYSICS_H */
