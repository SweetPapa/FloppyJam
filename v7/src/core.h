/* core.h — VOLLEYBAR shared math, table geometry and tuning constants.
 *
 * Deliberately raylib-free. sim.c, rods.c, heat.c, rules.c, ai.c and replay.c
 * build against this header alone, so the headless suites (make test) link
 * without a window system or an audio device.
 *
 * Units: the table is 2.0 long (x) by 1.2 wide (y), centred on the origin.
 * Speeds are table units per second. One "ball width" is 2*VB_BALL_R.
 */
#ifndef VB_CORE_H
#define VB_CORE_H

#include <math.h>
#include <stddef.h>

/* ---- vector ---------------------------------------------------------- */

typedef struct { float x, y; } V2;

static inline V2 v2(float x, float y) { V2 r; r.x = x; r.y = y; return r; }
static inline V2 v2zero(void) { return v2(0, 0); }
static inline V2 v2add(V2 a, V2 b) { return v2(a.x + b.x, a.y + b.y); }
static inline V2 v2sub(V2 a, V2 b) { return v2(a.x - b.x, a.y - b.y); }
static inline V2 v2mul(V2 a, float s) { return v2(a.x * s, a.y * s); }
static inline float v2dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }
static inline float v2cross(V2 a, V2 b) { return a.x * b.y - a.y * b.x; }
static inline float v2len2(V2 a) { return v2dot(a, a); }
static inline float v2len(V2 a) { return sqrtf(v2dot(a, a)); }
static inline V2 v2perp(V2 a) { return v2(-a.y, a.x); }
static inline V2 v2norm(V2 a) {
    float l = v2len(a);
    return (l > 1e-9f) ? v2mul(a, 1.0f / l) : v2zero();
}
static inline V2 v2lerp(V2 a, V2 b, float t) { return v2add(a, v2mul(v2sub(b, a), t)); }

static inline float vb_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline int vb_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float vb_lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float vb_absf(float v) { return v < 0 ? -v : v; }
static inline float vb_signf(float v) { return v < 0 ? -1.0f : 1.0f; }
static inline float vb_maxf(float a, float b) { return a > b ? a : b; }
static inline float vb_minf(float a, float b) { return a < b ? a : b; }

#define VB_PI  3.14159265358979f
#define VB_TAU 6.28318530717959f
#define VB_DEG (VB_PI / 180.0f)

/* ---- the tick (§5.1) -------------------------------------------------- */
/* Everything in the sim counts ticks. No wall clock ever reaches sim code:
 * that is the whole of determinism, and replays/ghosts/netcode fall out of it
 * for free. Presentation code may use seconds; it may not feed them back. */

#define VB_TICK_HZ   240
#define VB_DT        (1.0f / (float)VB_TICK_HZ)
/* ticks from a duration in seconds, rounded — spelled out so tuning numbers in
 * this header stay readable as seconds instead of as tick counts. */
#define VB_TICKS(sec) ((int)((sec) * (float)VB_TICK_HZ + 0.5f))

/* ---- table (§2) ------------------------------------------------------- */

#define VB_TABLE_HX   1.00f            /* half length, long axis            */
#define VB_TABLE_HY   0.60f            /* half width                        */
#define VB_BALL_R     0.022f           /* ball radius                       */
#define VB_BALL_W     (2.0f * VB_BALL_R)
/* goal mouth ~22% of table width, centred on each short end */
#define VB_GOAL_HALF  (0.22f * (2.0f * VB_TABLE_HY) * 0.5f)   /* 0.132 */
/* Pillar-2 law: no rod may ever seal the table. Every rod, at every legal
 * offset, must leave an open lane of at least this many ball widths.
 * tests/test_sim.c asserts it by sweeping every rod across its full travel. */
#define VB_OPEN_LANE  (1.25f * VB_BALL_W)

#define VB_NRODS      6
#define VB_MAXPAD     3
#define VB_MAXBALLS   3
#define VB_NSIDES     2
#define VB_NPLAYERS   4                /* 2 per side in doubles             */

/* rod roles, left goal to right goal (§2) */
enum { VB_GK = 0, VB_DEF, VB_ATK };

/* Rod plane positions. The interleave is the strategic engine: your ATK rod
 * lives past their DEF rod, so a lazy clear feeds their forwards. */
#define VB_ROD_GK_X   0.855f
#define VB_ROD_DEF_X  0.560f
#define VB_ROD_ATK_X  0.235f

/* paddle geometry per role */
#define VB_PAD_HALF_GK   0.100f        /* < goal half (0.132) - open lane   */
#define VB_PAD_HALF_DEF  0.095f
#define VB_PAD_HALF_ATK  0.080f
#define VB_PAD_HALF_X    0.016f        /* paddle half thickness along x     */

/* rod travel limits (half-range of the rod offset) */
#define VB_TRAVEL_GK   0.150f
#define VB_TRAVEL_DEF  0.190f
#define VB_TRAVEL_ATK  0.115f

#define VB_ROD_SPEED   1.55f           /* table units/s at full stick       */
#define VB_ROD_ACCEL   14.0f           /* units/s^2 — rods have mass        */

/* ---- ball & strikes (§5.2, §5.3, §5.4) -------------------------------- */

#define VB_REST_WALL   0.94f
#define VB_REST_IDLE   0.65f           /* un-flicked paddle: soft bunt      */
#define VB_MIN_SPEED   0.55f           /* the floor at heat 0               */
#define VB_FLOOR_K     0.45f           /* ...and it rides the heat ladder   */
#define VB_SEP_SPEED   0.16f           /* a touched ball always leaves      */
#define VB_NUDGE       2.60f           /* units/s^2 back up to the floor    */

#define VB_V0          0.95f           /* heat 0: crosses the table ~2.1s   */
#define VB_HEAT_BASE   1.11f           /* speed(h) = V0 * 1.11^h            */
#define VB_HEAT_MAX    12
#define VB_MERCY_HEAT  8               /* mercy beat arms from here up      */
#define VB_MERCY_TICKS VB_TICKS(0.120f)
#define VB_SCORCHER    9               /* goals at this heat or above       */

#define VB_ENGLISH_K   0.42f           /* curve per unit of rod lateral vel */
#define VB_CURVE_TICKS VB_TICKS(0.50f) /* Magnus decay window               */

#define VB_CHARGE_MAX  VB_TICKS(0.60f)
#define VB_CHARGE_SPD  1.30f
#define VB_CHARGE_CRV  1.80f
#define VB_SNAP_SPD    1.60f
#define VB_BUNT_SPD    0.55f

/* windup / recovery, in ticks (spec quotes them at 60 Hz; converted here) */
#define VB_WU_FLICK    VB_TICKS(0.050f)
#define VB_RC_FLICK    VB_TICKS(0.130f)
#define VB_WU_CHARGED  VB_TICKS(0.133f)
#define VB_RC_CHARGED  VB_TICKS(0.267f)
#define VB_WU_SNAP     VB_TICKS(0.067f)
#define VB_RC_SNAP     VB_TICKS(0.333f)

#define VB_PIN_CLOCK   VB_TICKS(2.00f) /* hard cap on a pin (§5.5)          */
#define VB_PIN_DRAG    1.05f           /* walking the line, units/s         */
#define VB_STALL_TICKS VB_TICKS(4.00f) /* referee pulse then heat-0 pop     */
#define VB_SERVE_PROT  VB_TICKS(1.50f) /* protected possession after a goal */

/* chip: a ballistic hop that clears exactly one rod plane */
#define VB_CHIP_APEX   0.062f
#define VB_CHIP_GRAV   2.60f
#define VB_CHIP_CLEAR  0.030f          /* z above which paddles miss        */
#define VB_CHIP_RANGE  2.5f            /* rod gaps travelled                */

/* GLIDE angle assist (§3.1) — a visible setting, default on for GLIDE */
#define VB_ASSIST_BEND (18.0f * VB_DEG)

/* ---- strike kinds ----------------------------------------------------- */
enum {
    VB_STRIKE_NONE = 0,
    VB_STRIKE_BUNT,
    VB_STRIKE_FLICK,
    VB_STRIKE_CHARGED,
    VB_STRIKE_SNAP,
    VB_STRIKE_CHIP,
    VB_STRIKE_IDLE          /* passive contact, no button                  */
};

/* ---- rod machine states ----------------------------------------------- */
enum {
    VB_ROD_IDLE = 0,
    VB_ROD_WINDUP,    /* committed; the lane behind it is genuinely open    */
    VB_ROD_STRIKE,    /* the live window — contact here is a powered hit    */
    VB_ROD_RECOVER,   /* the price of committing                           */
    VB_ROD_CHARGING,  /* holding for pace and curve                        */
    VB_ROD_PINNED     /* the ball is trapped dead under the paddle         */
};
#define VB_STRIKE_WINDOW VB_TICKS(0.070f)  /* how long a swing stays live  */
#define VB_TAP_TICKS     VB_TICKS(0.120f)  /* tap vs hold, for PIN/BUNT    */
#define VB_BUNT_WINDOW   VB_TICKS(0.200f)  /* a tapped catch cushions      */
#define VB_CHARGE_MIN    VB_TICKS(0.150f)  /* below this a hold is a tap   */

/* ---- control schemes (§3) --------------------------------------------- */
enum { VB_SCHEME_GLIDE = 0, VB_SCHEME_GRIP };

/* Palette count, mirrored here so save.c can validate a file without dragging
 * the renderer's header in. palette.h static-asserts the two agree. */
#define VB_NPALETTES_MAX 6

/* ---- input ------------------------------------------------------------ */
/* One of these per player per tick. Recorded verbatim (§5.1) — this struct is
 * the replay format, so nothing derived and nothing float-fragile goes in it
 * beyond the single axis. */
enum {
    VB_BTN_FLICK = 1 << 0,
    VB_BTN_PIN   = 1 << 1,
    VB_BTN_CHIP  = 1 << 2,   /* modifier: flick + chip = lofted strike      */
    VB_BTN_PREV  = 1 << 3,   /* shoulder: force active rod left            */
    VB_BTN_NEXT  = 1 << 4    /* shoulder: force active rod right           */
};

typedef struct {
    float    axis;   /* -1..1, quantised on capture so replays stay exact   */
    unsigned btn;
} VbInput;

/* one cosmetic-only PRNG (§5.1) — never consulted by anything that decides
 * where the ball goes. */
static inline unsigned vb_rand(unsigned *s) {
    *s = (*s * 1664525u) + 1013904223u;
    return (*s >> 8) & 0xFFFFFFu;
}
static inline float vb_randf(unsigned *s) {
    return (float)vb_rand(s) / 16777216.0f;
}
static inline float vb_rands(unsigned *s) { return vb_randf(s) * 2.0f - 1.0f; }

#endif /* VB_CORE_H */
