/* Palette, bitmap font, icons, primitives and the juice inventory (§9). */
#ifndef G_DRAW_H
#define G_DRAW_H

#include "common.h"
#include "raylib.h"

typedef struct { Color bg, dim, main, accent, hot; } Palette;

void  draw_init(void);
void  draw_shutdown(void);
void  draw_set_palette(int idx);
const Palette *pal(void);
/* desaturate everything toward `dim` — damage + WIPE states */
void  draw_set_desat(float amount);
Color pal_mix(Color c, float t);   /* t=0 normal, 1 fully dimmed */
Color cmix(Color a, Color b, float t);

/* --- frame structure ------------------------------------------------- */
void  draw_frame_begin(float dt);
void  draw_trail_begin(float fade);     /* playfield layer, leaves trails */
void  draw_trail_end(void);
void  draw_trail_blit(float rotation, float scale);
void  draw_frame_end(void);

/* --- text ------------------------------------------------------------ */
int   gtext_width(const char *s, int scale);
void  gtext(const char *s, int x, int y, int scale, Color c);
void  gtext_center(const char *s, int cx, int y, int scale, Color c);
void  gtext_shadow(const char *s, int x, int y, int scale, Color c);

/* --- icons ----------------------------------------------------------- */
#define ICON_COUNT 16
void  gicon(int icon_id, int x, int y, int scale, Color c);
void  gkeycap(const char *label, int x, int y, int scale, Color c, bool lit);

/* --- juice ----------------------------------------------------------- */
void  shake_add(float amount);
void  shake_update(float dt);
Vector2 shake_offset(void);

void  hitstop_add(float seconds);
bool  hitstop_active(void);
void  hitstop_update(float dt);

void  particles_burst(float x, float y, int n, Color c, float speed, float life);
void  particles_update(float dt);
void  particles_draw(void);
void  particles_clear(void);

void  pop_text(const char *s, float x, float y, int scale, Color c, float life);
void  pops_update(float dt);
void  pops_draw(void);
void  pops_clear(void);

/* persistent screen-edge cracks, one authored polyline set per HP lost */
void  draw_cracks(int count, float alpha, Color c);

/* harmonograph attract art (§9.3) */
void  harmonograph_draw(uint32_t seed_code, float t, float alpha);

/* misc primitives */
void  ring(float cx, float cy, float r, float thick, Color c);
void  draw_bar_meter(float x, float y, float w, float h, float v, Color fg, Color bg);

#endif
