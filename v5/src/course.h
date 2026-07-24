/* course.h — hole data tables and world construction (Section 6).
 *
 * A hole is pure data: floor pads carrying a surface + height plane, wall
 * segments, posts, pockets, movers and ball spawns. bp_course_build turns
 * one into a BpWorld. Nothing in here is randomised.
 */
#ifndef BP_COURSE_H
#define BP_COURSE_H

#include "physics.h"

#define BP_NHOLES 18

/* A wall segment in the XZ plane; becomes a yaw-oriented box. */
typedef struct {
    float x0, z0, x1, z1;
    float y;                /* base height                                */
    float h;                /* height above base (0 => default rail)       */
    unsigned char kind;     /* BpBoxKind                                   */
    unsigned char gate;     /* gate id+1                                   */
    unsigned char mover;    /* mover index+1                               */
} BpWallDef;

typedef struct {
    float x, z;
    unsigned char kind;     /* BpBallKind                                  */
    unsigned char color;    /* palette slot 0..7                           */
    unsigned char num;
} BpBallDef;

typedef struct {
    const char *name;
    const char *brief;      /* the one gimmick, shown on the intro card    */
    const char *tip;        /* first-bogey hint (Section 15)               */
    unsigned char par;
    unsigned char sealed;   /* rack hole: cup starts capped (7.4)          */
    float tee_x, tee_z;
    float kill_y;

    const BpPad     *pads;    unsigned char npads;
    const BpWallDef *walls;   unsigned char nwalls;
    const BpPost    *posts;   unsigned char nposts;
    const BpPocket  *pockets; unsigned char npockets;
    const BpMover   *movers;  unsigned char nmovers;
    const BpBallDef *balls;   unsigned char nballs;
} BpHole;

extern const BpHole BP_HOLES[BP_NHOLES];

/* Build hole `index` (0-based) into w, ready to play. */
void bp_course_build(BpWorld *w, int index);
/* Index of the PK_CUP pocket, or -1. */
int  bp_course_cup(const BpWorld *w);
/* Playfield bounds, for the flyover camera and the minimap. */
void bp_course_bounds(const BpWorld *w, V3 *lo, V3 *hi);

#endif /* BP_COURSE_H */
