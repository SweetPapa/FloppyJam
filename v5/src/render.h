/* render.h — Section 12. Flat-shaded low-poly, everything procedural. */
#ifndef BP_RENDER_H
#define BP_RENDER_H

#include "course.h"
#include "shot.h"
#include "camera.h"
#include "raylib.h"

typedef struct {
    Color felt, felt2, rim, rail, railTop, sky, horizon, accent, ink;
} BpPalette;

BpPalette bp_palette(int hole);

typedef struct {
    int   hole;                 /* 0-based                                */
    int   par, strokes, restarts;
    int   running, running_par;
    int   from, to;             /* selected range, inclusive 0-based      */
    int   sealed, bonus_left, aces;
    int   scores[BP_NHOLES];    /* 0 = not played                         */
    int   bests[BP_NHOLES];
    float power; int charging;
    float tx, ty;
    int   riding;
    float ball_speed;
    const char *name, *brief;
} BpHud;

void bp_render_init(void);
void bp_render_hole_begin(void);
void bp_render_update(const BpWorld *w, float dt);
void bp_render_trail(const BpWorld *w, float tx, float ty, float dt);

void bp_render_world(const BpWorld *w, int hole, const BpShot *sh,
                     int show_guide, int show_cue, float t);
void bp_render_hud(const BpWorld *w, const BpHud *h, int sw, int sh);
void bp_render_scorecard(const BpHud *h, int sw, int sh, int final_page);
void bp_render_title(int sw, int sh, int sel, float t, const BpHud *h);
void bp_render_select(int sw, int sh, int sel, float t);
void bp_render_pause(int sw, int sh, int sel, const BpHud *h);
void bp_render_options(int sw, int sh, int sel, int vm, int vmu, int vs, int fs);

/* golf/pool flavour text for a hole result */
const char *bp_score_name(int strokes, int par);
const char *bp_score_flair(int strokes, int par);

#endif
