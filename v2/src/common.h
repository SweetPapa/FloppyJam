/* THE GAUNTLET - common types shared across every module.
 * Nothing here owns time except the sequencer; see clock.h. */
#ifndef G_COMMON_H
#define G_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define SAMPLE_RATE   48000
#define STEPS_PER_BAR 16
#define BEATS_PER_BAR 4
#define STEPS_PER_BEAT 4

#define SCREEN_W 1280
#define SCREEN_H 720
#define BANNER_H 120
#define STATUS_H 64

/* ---- stems -------------------------------------------------------- */
typedef enum {
    STEM_PAD = 0,
    STEM_KICK,
    STEM_HAT,
    STEM_BASS,
    STEM_PLUCK,
    STEM_BELL,
    STEM_SNARE,
    STEM_COUNT
} Stem;

/* ---- input lanes --------------------------------------------------- */
enum { LANE_F = 0, LANE_G, LANE_H, LANE_J, LANE_SPACE, LANE_COUNT };

#define KEYMASK_F     (1u<<0)
#define KEYMASK_G     (1u<<1)
#define KEYMASK_H     (1u<<2)
#define KEYMASK_J     (1u<<3)
#define KEYMASK_SPACE (1u<<4)
#define KEYMASK_FJ    (KEYMASK_F|KEYMASK_J)
#define KEYMASK_FGHJ  (KEYMASK_F|KEYMASK_G|KEYMASK_H|KEYMASK_J)
#define KEYMASK_ALL   (KEYMASK_FGHJ|KEYMASK_SPACE)

/* ---- judgments ----------------------------------------------------- */
typedef enum { J_NONE = 0, J_PERFECT, J_GOOD, J_MISS } Judgment;

#define SCORE_PERFECT 100
#define SCORE_GOOD    50
#define WINDOW_PERFECT_MS 45.0f
#define WINDOW_GOOD_MS    90.0f

/* ---- small helpers -------------------------------------------------- */
static inline float g_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline int g_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float g_lerpf(float a, float b, float t) { return a + (b - a) * t; }
/* cubic ease-out: everything eases, nothing teleports (§9.2) */
static inline float g_ease_out(float t) {
    t = g_clampf(t, 0.0f, 1.0f);
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}
static inline float g_approach(float cur, float target, float rate, float dt) {
    float k = 1.0f - expf(-rate * dt);
    return cur + (target - cur) * k;
}

#endif /* G_COMMON_H */
