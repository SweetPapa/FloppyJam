/* test_contrast.c — the readability probe (Pillar 4, §8) and the comfort caps.
 *
 * Pillar 4 says the gameplay layer always wins contrast against the spectacle
 * layer, and that a shader which makes a save harder loses. That is checkable,
 * so it is checked here rather than left to somebody's eye on the day: every
 * palette, every heat step, every spectacle surface the ball can be drawn over
 * — worst case, not average case. Headless; no GPU, no window.
 */
#include "palette.h"
#include <stdio.h>

static int fails = 0, checks = 0;

#define CHECK(cond, ...) do {                               \
    checks++;                                               \
    if (!(cond)) {                                          \
        fails++;                                            \
        printf("  FAIL %s:%d  ", __FILE__, __LINE__);       \
        printf(__VA_ARGS__); printf("\n");                  \
    }                                                       \
} while (0)

/* every surface the gameplay layer can find itself drawn over */
static VbRGB spectacle_at(int pal, int heat, int which) {
    switch (which) {
        case 0: return vb_void_color(pal, heat);
        case 1: return vb_table_color(pal);
        case 2: return vb_crowd_color(pal, heat);
        case 3: return vb_line_color(pal);
        /* the worst cases the effects can actually produce: a goal detonation
         * flooding the void with the scorer's colour, and the white-out at the
         * peak of a scorcher */
        case 4: return vb_side_color(pal, 0);
        case 5: return vb_side_color(pal, 1);
        case 6: return rgb(1.0f, 1.0f, 1.0f);
        default: return rgb(0.0f, 0.0f, 0.0f);
    }
}
static const char *SPECT_NAME[] = {
    "void", "table glass", "crowd bokeh", "lane etching",
    "goal flood A", "goal flood B", "scorcher white-out"
};

static void test_ball_readable(void) {
    printf("\nthe ball against every spectacle surface (Pillar 4)\n");
    float worst = 1e9f;
    const char *worst_where = "";
    int worst_heat = 0;

    for (int pal = 0; pal < VB_NPALETTES; pal++) {
        for (int heat = 0; heat <= VB_HEAT_MAX; heat++) {
            VbRGB ball = vb_ball_color(heat);
            for (int w = 0; w < 7; w++) {
                VbRGB raw = spectacle_at(pal, heat, w);
                VbRGB under = vb_spectacle_cap(raw, ball);
                VbRGB ring = vb_ball_outline(under);

                /* the guarantee is on the ball's outline against whatever is
                 * under it — that ring is what your eye tracks at speed */
                float d = vb_contrast(ring, under);
                if (d < worst) { worst = d; worst_where = SPECT_NAME[w]; worst_heat = heat; }
                CHECK(d >= VB_CONTRAST_MIN,
                      "palette %s, heat %d, over %s: delta %.3f under the %.2f law",
                      VB_PALETTE_NAMES[pal], heat, SPECT_NAME[w], d, VB_CONTRAST_MIN);
            }
        }
    }
    printf("  worst case over %d palettes x %d heat steps x 7 surfaces:\n",
           VB_NPALETTES, VB_HEAT_MAX + 1);
    printf("  delta %.3f (law %.2f) at heat %d over the %s\n",
           worst, VB_CONTRAST_MIN, worst_heat, worst_where);
}

static void test_sides_separable(void) {
    printf("\nthe two sides stay separable (§8, colourblind-safe pairs)\n");
    for (int pal = 0; pal < VB_NPALETTES; pal++) {
        VbRGB a = vb_side_color(pal, 0), b = vb_side_color(pal, 1);
        /* they must differ in LUMINANCE, not only in hue: hue alone is not a
         * signal every player receives */
        float d = vb_contrast(a, b);
        CHECK(d >= 0.12f, "palette %s: the two sides differ by only %.3f luminance",
              VB_PALETTE_NAMES[pal], d);
        /* and neither may be a red/green pair, which is the one axis that
         * disappears for the most people */
        int a_red = (a.r > 0.55f && a.g < 0.45f && a.b < 0.45f);
        int b_grn = (b.g > 0.55f && b.r < 0.45f && b.b < 0.45f);
        int a_grn = (a.g > 0.55f && a.r < 0.45f && a.b < 0.45f);
        int b_red = (b.r > 0.55f && b.g < 0.45f && b.b < 0.45f);
        CHECK(!((a_red && b_grn) || (a_grn && b_red)),
              "palette %s is a red/green pair", VB_PALETTE_NAMES[pal]);
        printf("  %-10s luminance delta %.3f\n", VB_PALETTE_NAMES[pal], d);
    }
}

static void test_spectacle_yields(void) {
    printf("\nwhen they conflict, the shader loses (Pillar 4)\n");
    /* a spectacle deliberately painted the exact colour of the ball */
    for (int heat = 0; heat <= VB_HEAT_MAX; heat++) {
        VbRGB ball = vb_ball_color(heat);
        VbRGB capped = vb_spectacle_cap(ball, ball);
        CHECK(vb_lum(capped) < vb_lum(ball) || vb_lum(ball) < 0.02f,
              "heat %d: a spectacle matching the ball was not dimmed", heat);
    }
    /* and it yields by dimming, never by shifting hue into something else */
    VbRGB bright = rgb(1.0f, 0.9f, 0.4f);
    VbRGB capped = vb_spectacle_cap(bright, rgb(1.0f, 0.95f, 0.5f));
    CHECK(capped.r <= bright.r && capped.g <= bright.g && capped.b <= bright.b,
          "the spectacle brightened instead of yielding");
}

static void test_comfort_caps(void) {
    printf("\nphotosensitivity caps (§8 Comfort)\n");
    VbFlashGuard g;
    vb_flash_reset(&g);
    g.reduce_motion = 0;

    /* a goal detonation trying to strobe black-to-white every frame at 120 Hz */
    float dt = 1.0f / 120.0f;
    int over_delta = 0, transitions = 0;
    float prev = 0.0f;
    for (int i = 0; i < 1200; i++) {
        float want = (i & 1) ? 1.0f : 0.0f;
        float got = vb_flash_limit(&g, want, dt);
        float d = got - prev;
        if (d < 0) d = -d;
        if (d > VB_FLASH_DELTA + 1e-4f) over_delta++;
        if (d > 0.10f) transitions++;
        prev = got;
    }
    float secs = 1200.0f * dt;
    float hz = (float)transitions / secs;
    CHECK(over_delta == 0, "%d frames swung past the %.2f luminance cap",
          over_delta, VB_FLASH_DELTA);
    CHECK(hz <= VB_FLASH_HZ + 0.5f,
          "a strobing effect got through at %.1f Hz against a %.1f Hz cap",
          hz, VB_FLASH_HZ);
    printf("  a 60 Hz black/white strobe comes out at %.1f Hz, max swing %.2f\n",
           hz, VB_FLASH_DELTA);

    /* an effect that is not strobing must pass through essentially untouched */
    vb_flash_reset(&g);
    float slow = 0.0f, err = 0.0f;
    for (int i = 0; i < 240; i++) {
        float want = (float)i / 240.0f;
        slow = vb_flash_limit(&g, want, dt);
        float e = want - slow;
        if (e < 0) e = -e;
        if (e > err) err = e;
    }
    CHECK(err < 0.05f, "a gentle ramp was distorted by %.3f", err);
    printf("  a one-second fade passes through within %.3f\n", err);
}

int main(void) {
    printf("VOLLEYBAR — contrast probe\n");
    test_ball_readable();
    test_sides_separable();
    test_spectacle_yields();
    test_comfort_caps();
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
