/* Heat tiers (§7): the arrangement IS the spectator's progress bar. */
#ifndef G_HEAT_H
#define G_HEAT_H

#include "common.h"

#define HEAT_PHRASES_PER_TIER 4
#define HEAT_MAX_TIER 12

float heat_bpm(int tier, int bpm_base);
float heat_window_scale(int tier);
/* 1 if `stem` should be audible at `tier` */
int   heat_stem_active(int tier, int stem);
const char *heat_visual_name(int tier);
/* 0..1 visual intensity used all over draw.c */
float heat_intensity(int tier);

#endif
