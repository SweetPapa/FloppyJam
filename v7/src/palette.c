/* palette.c — the colour law (§8, Pillar 4). */
#include "palette.h"
#include "heat.h"

const char *const VB_PALETTE_NAMES[VB_NPALETTES] = {
    "ARC LIGHT", "SODIUM", "DEEP FIELD", "COPPER", "ORCHID", "MIDWINTER"
};

/* Side pairs. Deliberately never a red/green pair, and never two colours of
 * the same luminance — a player who cannot separate the hues can still
 * separate the bars. */
static const VbRGB SIDE[VB_NPALETTES][2] = {
    { { 0.45f, 0.82f, 1.00f }, { 0.95f, 0.45f, 0.10f } },  /* ARC LIGHT     */
    { { 1.00f, 0.78f, 0.22f }, { 0.42f, 0.36f, 0.92f } },  /* SODIUM        */
    { { 0.55f, 0.90f, 0.95f }, { 0.72f, 0.24f, 0.62f } },  /* DEEP FIELD    */
    { { 1.00f, 0.62f, 0.34f }, { 0.22f, 0.42f, 0.62f } },  /* COPPER        */
    { { 0.82f, 0.52f, 1.00f }, { 0.95f, 0.90f, 0.45f } },  /* ORCHID        */
    { { 0.88f, 0.94f, 1.00f }, { 0.20f, 0.48f, 0.70f } },  /* MIDWINTER     */
};

static const VbRGB VOID_BASE[VB_NPALETTES] = {
    { 0.020f, 0.028f, 0.052f }, { 0.036f, 0.026f, 0.020f },
    { 0.014f, 0.020f, 0.034f }, { 0.038f, 0.024f, 0.018f },
    { 0.030f, 0.018f, 0.042f }, { 0.016f, 0.026f, 0.040f },
};

float vb_lum(VbRGB c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }

VbRGB vb_side_color(int palette, int side) {
    return SIDE[vb_clampi(palette, 0, VB_NPALETTES - 1)][side ? 1 : 0];
}

/* The star of the show. Deep cyan at rest, white-gold at twelve — the whole
 * state of the rally readable from across a room (§4). */
VbRGB vb_ball_color(int heat) {
    float t = vb_heat_temp(heat);
    /* three stops: cold cyan -> hot amber -> white-gold */
    VbRGB cold = rgb(0.22f, 0.82f, 0.98f);
    VbRGB mid  = rgb(1.00f, 0.72f, 0.28f);
    VbRGB hot  = rgb(1.00f, 0.98f, 0.86f);
    if (t < 0.6f) {
        float u = t / 0.6f;
        return rgb(vb_lerpf(cold.r, mid.r, u),
                   vb_lerpf(cold.g, mid.g, u),
                   vb_lerpf(cold.b, mid.b, u));
    }
    float u = (t - 0.6f) / 0.4f;
    return rgb(vb_lerpf(mid.r, hot.r, u),
               vb_lerpf(mid.g, hot.g, u),
               vb_lerpf(mid.b, hot.b, u));
}

/* Clause 1 of the law, from the ball's side: whatever is under it, the ball
 * gets a ring that separates it. Bright things get a dark ring and dark things
 * get a bright one, and the ring is never the reason you missed a save. */
VbRGB vb_ball_outline(VbRGB under) {
    float l = vb_lum(under);
    return (l > 0.42f) ? rgb(0.03f, 0.03f, 0.05f) : rgb(1.0f, 1.0f, 1.0f);
}

/* heat pulls the void up out of black, but only so far */
VbRGB vb_void_color(int palette, int heat) {
    int p = vb_clampi(palette, 0, VB_NPALETTES - 1);
    VbRGB b = VOID_BASE[p];
    float t = vb_heat_temp(heat);
    VbRGB s = vb_side_color(p, heat > 6);
    float k = 0.10f * t;
    return rgb(b.r + s.r * k, b.g + s.g * k, b.b + s.b * k);
}

VbRGB vb_table_color(int palette) {
    VbRGB v = VOID_BASE[vb_clampi(palette, 0, VB_NPALETTES - 1)];
    return rgb(v.r * 2.1f + 0.030f, v.g * 2.1f + 0.036f, v.b * 2.1f + 0.052f);
}

VbRGB vb_line_color(int palette) {
    VbRGB t = vb_table_color(palette);
    return rgb(t.r + 0.10f, t.g + 0.13f, t.b + 0.16f);
}

/* ten thousand phones in a dark stadium */
VbRGB vb_crowd_color(int palette, int heat) {
    VbRGB s = vb_side_color(vb_clampi(palette, 0, VB_NPALETTES - 1), 0);
    float t = 0.18f + 0.34f * vb_heat_temp(heat);
    return rgb(s.r * t, s.g * t, s.b * t);
}

VbRGB vb_spectacle_cap(VbRGB spectacle, VbRGB gameplay) {
    /* Scale the spectacle toward black until the gameplay layer clears the
     * law. Toward black rather than toward grey, because the identity of this
     * game is light on darkness — dimming is always the right retreat. */
    float want = vb_lum(gameplay);
    for (int i = 0; i < 24; i++) {
        if (vb_contrast(spectacle, gameplay) >= VB_CONTRAST_MIN) break;
        float k = (want > 0.5f) ? 0.90f : 1.0f / 0.90f;
        spectacle = rgb(vb_clampf(spectacle.r * k, 0.0f, 1.0f),
                        vb_clampf(spectacle.g * k, 0.0f, 1.0f),
                        vb_clampf(spectacle.b * k, 0.0f, 1.0f));
        /* a spectacle that has gone as far as it can go stops here; the
         * gameplay layer's own outline (vb_ball_outline) is the backstop */
        if (want <= 0.5f && vb_lum(spectacle) >= 0.999f) break;
        if (want >  0.5f && vb_lum(spectacle) <= 0.001f) break;
    }
    return spectacle;
}

void vb_flash_reset(VbFlashGuard *g) {
    g->last_lum = 0.0f;
    g->since = 10.0f;
    /* reduce_motion is set by the caller from the save file and left alone */
}

float vb_flash_limit(VbFlashGuard *g, float want, float dt) {
    g->since += dt;

    /* Both clauses of the comfort cap fall out of one number: the fastest the
     * screen is allowed to travel is a full permitted swing per permitted
     * transition, VB_FLASH_DELTA * VB_FLASH_HZ of luminance per second.
     *
     * A slew limit rather than a counter, because a counter only asks "how
     * often did I flash" and can be walked straight past by an effect that
     * strobes just under the threshold every frame. Rate-limiting the light
     * itself cannot be walked past: a 60 Hz black-to-white strobe comes out as
     * a shimmer, while a one-second fade — 1.0 per second, inside the budget —
     * passes through untouched. Nothing legitimate is even slowed. */
    float max_step = VB_FLASH_DELTA * VB_FLASH_HZ * dt;
    float delta = want - g->last_lum;
    if (delta >  max_step) delta =  max_step;
    if (delta < -max_step) delta = -max_step;

    float out = g->last_lum + delta;

    /* and an absolute ceiling on the swing away from where we settled */
    float mag = delta < 0 ? -delta : delta;
    if (mag > 0.02f) g->since = 0.0f;

    g->last_lum = out;
    return out;
}
