/* core.h — BREAK PAR shared scalar/vector math and tuning constants.
 *
 * Deliberately raylib-free: physics.c must build against this header alone so
 * the headless test harness (make test) links without a window system.
 */
#ifndef BP_CORE_H
#define BP_CORE_H

#include <math.h>
#include <stddef.h>

/* ---- vector -------------------------------------------------------- */

typedef struct { float x, y, z; } V3;

static inline V3 v3(float x, float y, float z) { V3 r; r.x=x; r.y=y; r.z=z; return r; }
static inline V3 v3zero(void) { return v3(0,0,0); }
static inline V3 v3add(V3 a, V3 b) { return v3(a.x+b.x, a.y+b.y, a.z+b.z); }
static inline V3 v3sub(V3 a, V3 b) { return v3(a.x-b.x, a.y-b.y, a.z-b.z); }
static inline V3 v3mul(V3 a, float s) { return v3(a.x*s, a.y*s, a.z*s); }
static inline float v3dot(V3 a, V3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline V3 v3cross(V3 a, V3 b) {
    return v3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}
static inline float v3len(V3 a) { return sqrtf(v3dot(a,a)); }
static inline float v3len2(V3 a) { return v3dot(a,a); }
static inline V3 v3norm(V3 a) {
    float l = v3len(a);
    return (l > 1e-9f) ? v3mul(a, 1.0f/l) : v3zero();
}
static inline V3 v3lerp(V3 a, V3 b, float t) { return v3add(a, v3mul(v3sub(b,a), t)); }
/* component of a perpendicular to unit n */
static inline V3 v3tan(V3 a, V3 n) { return v3sub(a, v3mul(n, v3dot(a,n))); }

static inline float bp_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline int bp_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float bp_lerpf(float a, float b, float t) { return a + (b-a)*t; }
/* shortest signed angular difference, radians */
static inline float bp_angdiff(float a, float b) {
    float d = a - b;
    while (d >  3.14159265f) d -= 6.28318531f;
    while (d < -3.14159265f) d += 6.28318531f;
    return d;
}

#define BP_PI  3.14159265358979f
#define BP_TAU 6.28318530717959f
#define BP_DEG (BP_PI/180.0f)

/* ---- physical constants (Section 5) -------------------------------- */

#define BP_DT        (1.0f/240.0f)   /* fixed physics timestep            */
#define BP_R         0.0286f         /* ball radius, m                    */
#define BP_MASS      0.17f           /* ball mass, kg (impulses use /m)   */
#define BP_G         9.81f           /* gravity, m/s^2                    */

/* Cloth/turf friction. Section 16 gate 2 retuned these upward from the
 * pool-table values in the spec: BREAK PAR holes are 10-16 m long, so a
 * 50%-power shot has to stop in ~7 m, not the ~60 m real pool cloth gives. */
#define BP_MU_SLIDE  0.30f
#define BP_MU_ROLL   0.090f
#define BP_MU_SPIN   0.050f

#define BP_SLIP_EPS  0.005f          /* |u| below this => rolling         */
#define BP_REST_V    0.045f          /* speed below this may snap to rest */

#define BP_E_WALL    0.85f
#define BP_MU_WALL   0.34f
#define BP_E_BALL    0.95f
#define BP_E_FLOOR   0.45f
#define BP_E_BUMPER  1.40f
#define BP_BUMPER_KICK 0.75f         /* extra outward m/s from a bumper   */

#define BP_VMAX      9.0f            /* cue-ball launch speed at 100%     */
#define BP_POW_EXP   1.35f
#define BP_POW_MIN   0.04f
#define BP_K_SPIN    5.0f            /* gate-3 tune: 2.5 is real pool; hotter reads better */
#define BP_TIP_MAX   0.70f           /* max strike offset, fraction of R  */

#define BP_CUP_R     (2.2f*BP_R)     /* cup mouth radius                  */
#define BP_CUP_DEPTH (2.4f*BP_R)
#define BP_POCKET_R  (3.4f*BP_R)     /* scratch/bonus/warp pockets        */
#define BP_E_RIM     0.55f

#define BP_STROKE_CAP 8

#endif /* BP_CORE_H */
