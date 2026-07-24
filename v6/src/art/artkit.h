/* artkit — living ink on paper (CANON §2.2, API.md §8).
 *
 * Everything visible in HUEDUNIT is drawn from this vocabulary. There are no
 * image files anywhere in the repo or the build; a scene is a list of strokes.
 *
 * The palette is the game's progress bar. Content authors pick honest colours;
 * palette_stage() decides how much of each colour the player has earned back.
 */
#ifndef HD_ARTKIT_H
#define HD_ARTKIT_H

#include "raylib.h"
#include <stdbool.h>

/* virtual canvas — everything draws in these units, letterboxed to the window */
#define VW 1280
#define VH 720

void art_init(void);
void art_shutdown(void);
void art_begin_frame(float dt);
void art_end_frame(void);

/* mouse in virtual canvas units — the whole game is letterboxed, so no
 * system outside artkit ever thinks about window size */
Vector2 art_mouse(void);
bool    art_hover(Rectangle r);

/* camera pan for cutscenes and wide scenes */
void art_camera(float x, float y);
/* screen shake; a no-op when reduce-motion is on (§5.4) */
void art_shake(float amount, float duration);
/* full-screen fade, driven by cutscenes and scene transitions */
void  art_fade_set(float amount);
float art_fade_get(void);

/* stable pseudo-noise: same inputs, same wobble, every frame. Nothing in the
 * world shimmers unless something asks it to. */
float art_noise(int seed, int i);

/* --- palette (§8.2) ------------------------------------------------------- */
enum { HUE_BASE = 0, HUE_BLUE, HUE_YELLOW, HUE_RED, HUE_GREEN, HUE_VIOLET, HUE_GOLD };

void  palette_set_stage(int n);       /* 0 gray .. 6 full spectrum + gold */
int   palette_stage(void);
/* the showpiece: a hue physically flooding outward from a point (§8.2) */
void  palette_bloom(float x, float y, int stage);
bool  palette_blooming(void);
float palette_band(int hue);          /* 0..1 how present that hue is */

/* resolve an authored colour through the current palette state at a position */
Color pal_at(Color c, float x, float y);
Color pal(Color c);                   /* uses the last primitive's centroid */

/* semantic slots — the colours the whole game shares */
Color col_ink(void);
Color col_ink_soft(void);
Color col_paper(void);
Color col_paper_dark(void);
Color col_accent_a(void);
Color col_accent_b(void);
Color col_sky(void);
Color col_sea(void);
Color col_warm(void);
Color col_cool(void);

/* --- ink vocabulary (§8.1) ------------------------------------------------ */
enum { HATCH_NONE = 0, HATCH_SOLID, HATCH_DIAG, HATCH_CROSS, HATCH_DOT, HATCH_VERT };

void ink_stroke(const Vector2 *pts, int n, float w, float wobble, int seed, Color c);
void ink_line(float x1, float y1, float x2, float y2, float w, float wobble,
              int seed, Color c);
void ink_rect(Rectangle r, float w, float wobble, int seed, Color c);
void ink_circle(float x, float y, float r, float w, float wobble, int seed, Color c);
void ink_fill(const Vector2 *pts, int n, int hatch, int seed, Color c);
void ink_blob(float x, float y, float r, float squash, int seed, Color c);

void paper_panel(Rectangle r, float torn, int seed);
void paper_grain(float intensity);
void vignette(float amt);
void wind_lines(float t, float amt, Color c);

/* --- parametric doodles (§8.1) -------------------------------------------- */
enum {
    D_LANTERN = 0, D_FISH, D_GEAR, D_TEACUP, D_FEATHER, D_KEY, D_PRISM,
    D_BIRD, D_FLOWER, D_BREAD, D_BOOK, D_CLOCK, D_SPOOL, D_BEE, D_STAR,
    D_WAVE, D_HOUSE, D_TREE, D_CLOUD, D_ENVELOPE, D_ANCHOR, D_LOCK,
    D_COUNT
};
void doodle(int id, float x, float y, float scale, float rot, Color c);
const char *doodle_name(int id);

/* --- text ----------------------------------------------------------------- */
float art_text(const char *s, float x, float y, float size, Color c);
float art_text_w(const char *s, float size);
/* wraps to `wide`, returns height drawn. reveal < 0 draws everything;
 * otherwise only the first `reveal` characters (dialogue typewriter). */
float art_text_wrap(const char *s, float x, float y, float wide, float size,
                    Color c, int reveal);

/* --- paper puppets (§8.1, §8.3) ------------------------------------------- */
enum { POSE_STAND = 0, POSE_WALK, POSE_SIT, POSE_POINT, POSE_BUSY };
enum { EM_NEUTRAL = 0, EM_HAPPY, EM_SAD, EM_WORRIED, EM_SHIFTY, EM_LAUGH, EM_MOVED,
       EM_COUNT };

int         npc_by_name(const char *name);     /* -1 if unknown */
const char *npc_display(int id);
const char *npc_key(int id);
int         npc_count(void);
int         npc_emote_by_name(const char *name);

void npc_draw(int id, float x, float y, float scale, int pose, int emote, float t);
void npc_bust(int id, Rectangle r, int emote, float t);

/* the sparkle every interactable wears when the highlight setting is on */
void sparkle(float x, float y, float r, float t);

#endif
