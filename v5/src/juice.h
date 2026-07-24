/* juice.h — Section 10. Particles, hitstop, slow-mo, banners, tips. */
#ifndef BP_JUICE_H
#define BP_JUICE_H

#include "core.h"
#include "raylib.h"

#define BP_MAX_PARTICLES 512

void  bp_juice_reset(void);
void  bp_juice_update(float dt);

void  bp_burst(V3 at, int count, Color c, float speed, float life, float gravity);
void  bp_confetti(V3 at, int count, float power);
void  bp_shockwave(V3 at, Color c, float r);
void  bp_flash(Color c, float amount);

void  bp_hitstop(float seconds);
float bp_hitstop_left(void);
void  bp_slowmo(float seconds, float scale);
float bp_timescale(void);          /* 1.0 normally, < 1 during slow-mo   */

void  bp_banner(const char *big, const char *small, float hold, Color tint);
void  bp_toast(const char *text, float hold);

void  bp_juice_draw_world(void);   /* call inside BeginMode3D            */
void  bp_juice_draw_hud(int sw, int sh);

#endif
