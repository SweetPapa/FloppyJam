/* fx.h — §8. The world, drawn.
 *
 * Everything on screen that is not a menu comes out of here: the glass, the
 * bars, the comet, the trails, the detonations, the crowd. Two rules govern
 * the whole file:
 *
 *   Pillar 4 — the gameplay layer always wins contrast. Every spectacle colour
 *   passes through vb_spectacle_cap before it is drawn, and the ball carries an
 *   outline computed against whatever ended up underneath it.
 *
 *   DoD 6 — gray-box is retained forever. vb_fx_init(fx, 1) refuses to load a
 *   single shader and draws flat rectangles, which is the build the game was
 *   proven fun in before any of this existed.
 */
#ifndef VB_FX_H
#define VB_FX_H

#include "raylib.h"
#include "sim.h"
#include "rules.h"
#include "palette.h"

typedef struct { float ox, oy, sc; } VbView;

#define VB_MAXPART 640
#define VB_MAXWAVE 8
#define VB_TRAIL   40

typedef struct {
    V2    p, v;
    float life, max, size;
    VbRGB col;
    int   kind;
} VbParticle;

typedef struct {
    V2    at;
    float t, dur, amp;
    VbRGB col;
    int   big;
} VbWave;

typedef struct {
    int   palette;
    int   reduce_motion;
    int   graybox;
    int   shaders_ok;

    Shader sh_table, sh_ball, sh_crowd, sh_ripple;
    int    has_table, has_ball, has_crowd, has_ripple;
    RenderTexture2D target;
    int    tw, th;

    VbParticle part[VB_MAXPART];
    int        npart;
    VbWave     wave[VB_MAXWAVE];

    V2    trail[VB_MAXBALLS][VB_TRAIL];
    int   trail_n[VB_MAXBALLS];
    int   trail_head[VB_MAXBALLS];

    float shake, shake_t;
    float slowmo;              /* seconds of 0.4x left after a goal        */
    float hitstop;             /* the frames a heavy strike hangs on       */
    float gasp;                /* the crowd holding its breath on a save   */
    float flood;               /* the scorer's colour over the void        */
    int   flood_side;
    V2    crack_at;            /* where a scorcher went in, so the glass
                                * fractures from the mouth it broke        */
    float time;                /* presentation clock; never reaches the sim */

    /* the meter has to be readable at a glance (§16.2), so it says so out
     * loud the instant it climbs */
    float heat_pulse;
    int   heat_seen;

    VbFlashGuard guard;

    /* render interpolation (§5.1: the sim is 240 Hz, the screen is not) */
    V2    prev_p[VB_MAXBALLS];
    float prev_off[VB_NRODS];
    int   have_prev;
} VbFx;

void   vb_fx_init(VbFx *fx, int graybox);
void   vb_fx_shutdown(VbFx *fx);
void   vb_fx_settings(VbFx *fx, int palette, int reduce_motion);

/* Call once per frame BEFORE stepping the sim: this is the "from" end of the
 * render interpolation. */
void   vb_fx_snapshot(VbFx *fx, const VbSim *s);
/* Drain the sim's events into particles, waves, shake and crowd noise. */
void   vb_fx_events(VbFx *fx, const VbSim *s);
void   vb_fx_update(VbFx *fx, float dt);
/* 1.0 normally, 0 while a heavy strike hangs, 0.4 during the slow-mo after a
 * goal, and always 1.0 under reduce-motion. Presentation only: it scales how
 * much time reaches the accumulator, never the timestep itself. */
float  vb_fx_timescale(const VbFx *fx);

VbView vb_fx_view(int w, int h);
Vector2 vb_fx_screen(VbView v, V2 p);

/* The whole world. `alpha` is the 0..1 position between the last two ticks. */
void   vb_fx_draw(VbFx *fx, const VbMatch *m, float alpha);
/* The attract-mode table behind the title, drawn from a bare sim. */
void   vb_fx_draw_sim(VbFx *fx, const VbSim *s, float alpha, int dim);

#endif /* VB_FX_H */
