#include "heat.h"
#include "sequencer.h"

/* Starting tuning table from §7. Exposed to the TUNING build via F3/F4/F6. */
static const float TIER_BPM[6]    = { 112, 120, 128, 138, 148, 158 };
static const float TIER_WINDOW[6] = { 1.00f, 1.00f, 0.95f, 0.90f, 0.85f, 0.78f };

float heat_bpm(int tier, int bpm_base)
{
    tier = g_clampi(tier, 0, HEAT_MAX_TIER);
    float base = (tier < 6) ? TIER_BPM[tier] : (TIER_BPM[5] + 8.0f * (float)(tier - 5));
    /* the genome shifts the whole curve so every seed has its own tempo feel */
    return base + ((float)bpm_base - 112.0f);
}

float heat_window_scale(int tier)
{
    tier = g_clampi(tier, 0, HEAT_MAX_TIER);
    float w = (tier < 6) ? TIER_WINDOW[tier] : (0.72f - 0.04f * (float)(tier - 6));
    return (w < 0.60f) ? 0.60f : w;   /* floor at 60% of base (§4.2) */
}

int heat_stem_active(int tier, int stem)
{
    switch (stem) {
    case STEM_PAD:   return 1;
    case STEM_KICK:  return 1;
    case STEM_HAT:   return tier >= 1;
    case STEM_BASS:  return tier >= 2;
    case STEM_PLUCK: return tier >= 3;
    case STEM_BELL:  return tier >= 4;
    case STEM_SNARE: return tier >= 5;
    default: return 0;
    }
}

const char *heat_visual_name(int tier)
{
    static const char *N[7] = { "COLD", "WARM", "HOT", "BURNING", "BLOOM", "INFERNO", "MELTDOWN" };
    return N[g_clampi(tier, 0, 6)];
}

float heat_intensity(int tier)
{
    return g_clampf((float)tier / 6.0f, 0.0f, 1.4f);
}
