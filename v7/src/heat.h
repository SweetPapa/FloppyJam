/* heat.h — Section 4. The rally engine.
 *
 * Every paddle contact raises the ball one step of heat. Heat is the single
 * number the whole game reads from: speed, colour, trail length, crowd volume,
 * music layers, and the mercy beat that keeps a step-12 exchange reactable
 * instead of random.
 *
 * Raylib-free on purpose — the tests run this headless.
 */
#ifndef VB_HEAT_H
#define VB_HEAT_H

#include "core.h"

/* speed(h) = V0 * 1.11^h, clamped at the cap (§5.4) */
float vb_heat_speed(int heat);
/* One step up, honouring a personal heat cap from Table Tilt (0 = no tilt). */
int   vb_heat_up(int heat, int cap);
/* Bunts and pins cool the ball by one (§4). */
int   vb_heat_down(int heat);
/* The mercy beat: at step 8+ a struck ball holds a 120 ms shimmer before it
 * flies. This is what makes maximum heat a crescendo and not a coin flip. */
int   vb_heat_mercy(int heat);
/* 0..1 along the temperature spectrum: deep cyan -> white-gold. */
float vb_heat_temp(int heat);
/* Music layers in at heat 3/6/9/12 (§9): returns 0..4. */
int   vb_heat_layers(int heat);
/* A goal at step 9+ is a SCORCHER: bigger detonation, tracked on the stat
 * card, and worth exactly the same one point in STANDARD. */
static inline int vb_heat_scorcher(int heat) { return heat >= VB_SCORCHER; }

#endif /* VB_HEAT_H */
