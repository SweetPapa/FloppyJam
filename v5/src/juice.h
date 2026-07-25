/* juice.h — Section 10. Particles, hitstop, slow-mo, banners, tips. */
#ifndef BP_JUICE_H
#define BP_JUICE_H

#include "core.h"
#include "raylib.h"

#define BP_MAX_PARTICLES 512
#define BP_MAX_POPUPS     12

void  bp_juice_reset(void);
void  bp_juice_update(float dt);

void  bp_burst(V3 at, int count, Color c, float speed, float life, float gravity);
void  bp_confetti(V3 at, int count, float power);
void  bp_shockwave(V3 at, Color c, float r);
void  bp_flash(Color c, float amount);

/* Hard twinkling flecks — the neon counterpart to bp_burst, for anything
 * that should read as "that was worth something". */
void  bp_sparkle(V3 at, int count, Color c, float spread, float life);

/* A number or word that pops out of the table where the thing happened and
 * floats off. World-anchored, so it stays glued to the spot as the camera
 * moves; drawn in the HUD pass because text is 2D. */
void  bp_popup(V3 at, const char *text, Color c, int big);

/* Coloured bloom around the edge of the frame. Reads as a light in the room
 * changing rather than the whole screen being repainted, which is what the
 * full-frame bp_flash does — use this for the frequent events. */
void  bp_edge_pulse(Color c, float amount);

/* Radial streaks from the centre of the frame, 0..1. Driven by ball speed. */
void  bp_speedlines(float amount);

/* Ambient breeze. Cosmetic ONLY: it drifts motes and leans the flag, and is
 * deliberately never read by physics.c, so determinism and every tuning gate
 * stay intact and the wind never lies to the player about the shot. */
void  bp_wind_set(float angle, float strength);
float bp_wind_angle(void);
float bp_wind_strength(void);
void  bp_wind_motes(V3 centre, float dt);

void  bp_hitstop(float seconds);
float bp_hitstop_left(void);
void  bp_slowmo(float seconds, float scale);
float bp_timescale(void);          /* 1.0 normally, < 1 during slow-mo   */

void  bp_banner(const char *big, const char *small, float hold, Color tint);
void  bp_toast(const char *text, float hold);

void  bp_juice_draw_world(void);   /* call inside BeginMode3D            */
void  bp_juice_draw_hud(int sw, int sh);
/* popups need the camera to project; call after EndMode3D */
void  bp_juice_draw_popups(Camera3D cam, int sw, int sh);

#endif
