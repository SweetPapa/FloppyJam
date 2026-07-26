/* palette.h — the colour law (§8, Pillar 4).
 *
 * "Stunning must stay readable." That is a law, so it lives in code that can
 * be tested rather than in a note somebody remembers. Everything the game
 * draws gets its colour from here, and the two functions at the bottom are the
 * law itself: the gameplay layer always wins contrast against the spectacle
 * layer, and no effect may flash faster or harder than the comfort caps allow.
 *
 * Raylib-free on purpose — tests/test_contrast.c samples the worst cases
 * headless, with no GPU and no window, and fails the build over them.
 */
#ifndef VB_PALETTE_H
#define VB_PALETTE_H

#include "core.h"

typedef struct { float r, g, b; } VbRGB;

static inline VbRGB rgb(float r, float g, float b) {
    VbRGB c; c.r = r; c.g = g; c.b = b; return c;
}

/* Cosmetic unlocks (§11). Every pair is chosen to stay separable under
 * deuteranopia and protanopia: the two sides never differ by hue alone, they
 * always differ in luminance too. */
#define VB_NPALETTES 6
extern const char *const VB_PALETTE_NAMES[VB_NPALETTES];
/* save.c validates against VB_NPALETTES_MAX without including this header */
typedef char vb_palette_count_agrees[(VB_NPALETTES == VB_NPALETTES_MAX) ? 1 : -1];

/* The gameplay layer. */
VbRGB vb_side_color(int palette, int side);
VbRGB vb_ball_color(int heat);          /* deep cyan -> white-gold (§4)      */
VbRGB vb_ball_outline(VbRGB under);     /* the readability guarantee         */

/* The spectacle layer. */
VbRGB vb_void_color(int palette, int heat);
VbRGB vb_table_color(int palette);
VbRGB vb_crowd_color(int palette, int heat);
VbRGB vb_line_color(int palette);

/* relative luminance, Rec. 709 */
float vb_lum(VbRGB c);
static inline float vb_contrast(VbRGB a, VbRGB b) {
    float d = vb_lum(a) - vb_lum(b);
    return d < 0 ? -d : d;
}

/* The law, clause 1 (§8): the ball keeps at least this much luminance delta
 * against whatever is underneath it. If a shader ever makes a save harder,
 * the shader loses — vb_spectacle_cap is how it loses. */
#define VB_CONTRAST_MIN 0.34f

/* Dims a spectacle colour until the gameplay colour above it clears the law.
 * Returns the colour the spectacle layer is actually allowed to be. */
VbRGB vb_spectacle_cap(VbRGB spectacle, VbRGB gameplay);

/* The law, clause 2 (§8 Comfort): photosensitivity caps. No effect may exceed
 * VB_FLASH_HZ transitions per second, nor VB_FLASH_DELTA of luminance swing.
 * `reduce_motion` kills shake, slow-mo and the radial pull but keeps every bit
 * of the colour language. */
#define VB_FLASH_HZ    3.0f
#define VB_FLASH_DELTA 0.42f

typedef struct {
    float last_lum;
    float since;        /* seconds since the last transition                */
    int   reduce_motion;
} VbFlashGuard;

void  vb_flash_reset(VbFlashGuard *g);
/* Feed it the luminance an effect wants this frame; it returns the luminance
 * the effect is allowed to use. */
float vb_flash_limit(VbFlashGuard *g, float want, float dt);

#endif /* VB_PALETTE_H */
