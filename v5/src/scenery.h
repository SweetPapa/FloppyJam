/* scenery.h — the world outside the rails.
 *
 * BREAK PAR is played on a table dropped into the middle of a neon city at
 * two in the morning. None of this is an asset: the skyline, the plaza, the
 * ferris wheel and the stars are all seeded off the hole index and rebuilt
 * on the fly, so eighteen holes get eighteen different night views for a few
 * hundred bytes of table.
 *
 * Everything here is decoration. Nothing in this file is ever read by
 * physics.c, and none of it can move a ball.
 */
#ifndef BP_SCENERY_H
#define BP_SCENERY_H

#include "render.h"
#include "camera.h"

/* Reseed the skyline for a hole. Cheap enough to call on every hole start. */
void bp_scenery_build(const BpWorld *w, int hole);

/* Animate signs, beacons, the wheel and the blimp. Presentation clock only. */
void bp_scenery_update(float dt);

/* The backdrop: sky dome, stars, moon, plaza, skyline, funfair.
 * Call first inside BeginMode3D so the course draws over it. */
void bp_scenery_draw(const BpCam *cam, const BpPalette *pal, float t);

/* Set dressing that hugs the course: festoon lights, lamp posts, the cup
 * beacon. Call after the course so the glows blend over the felt. */
void bp_scenery_draw_near(const BpWorld *w, const BpPalette *pal, float t);

/* A screen-space wash of city light, drawn under the HUD: horizon bloom,
 * a soft vignette and the drifting haze that sells the depth. */
void bp_scenery_draw_overlay(int sw, int sh, const BpPalette *pal, float t);

#endif /* BP_SCENERY_H */
