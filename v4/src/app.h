/* app.h — raylib-side shared state for the presentation layer
 * (render.c, ui.c, audio.c, main.c). Keeps maglava.h free of raylib. */
#ifndef APP_H
#define APP_H

#include "raylib.h"
#include "maglava.h"

/* World scale: game-space pixels -> 3D world units. */
#define WS 0.02f

typedef enum {
    SCR_TITLE = 0,
    SCR_SELECT,
    SCR_PLAYING,
    SCR_PAUSED,
    SCR_COMPLETE
} Screen;

typedef enum {
    SFX_SWING = 0, SFX_LAND, SFX_LAND_BACK, SFX_CHECKPOINT,
    SFX_DEATH, SFX_COMPLETE, SFX_UI, SFX_WARN, SFX_COUNT
} SfxId;

typedef struct {
    float x, y, z;      /* world-space */
    float vx, vy, vz;
    float life, max_life;
    float size;
    Color color;
    int active;
} Particle;

#define MAX_PARTICLES 512

/* floating score / status text anchored in game-space */
typedef struct {
    float gx, gy;
    float life, max_life;
    char text[16];
    Color color;
    int active;
} Popup;

#define MAX_POPUPS 16

/* player motion trail sample */
typedef struct { float x, y, z; int used; } TrailPt;
#define TRAIL_LEN 26

typedef struct {
    Screen screen;
    Screen prev_screen;
    GameSim sim;
    SaveData save;
    int level_id;       /* currently selected / playing */
    int select_cursor;  /* level-select highlighted index (0-based) */

    Camera3D cam;
    float cam_y;        /* smoothed follow target Y (world) */
    float cam_x;        /* smoothed horizontal follow (world) */
    float cam_dist;     /* smoothed dolly distance */
    float cam_shake;    /* shake magnitude, decays */
    float lava_flash;   /* backtrack flash 0..1 */

    Particle parts[MAX_PARTICLES];
    int part_head;
    Popup popups[MAX_POPUPS];
    int popup_head;
    TrailPt trail[TRAIL_LEN];
    int trail_head;
    float trail_timer;

    /* juice / feel */
    float time_scale;    /* <1 = slow motion (death drama) */
    float score_shown;   /* HUD score counts up smoothly */
    float intro_t;       /* level-intro card timer (counts up) */
    float fade;          /* 1 = black, fades to 0 on level start/respawn */
    float death_flash;   /* 0..1 red full-screen flash */
    float land_pop;      /* 0..1 pop scale on the magnet just landed on */
    int   land_idx;      /* which magnet is popping */
    float warn_timer;    /* spacing for lava warning beeps */
    int   prev_state;    /* detects respawn transitions */

    /* generated textures */
    Texture2D tex_glow;    /* soft radial glow sprite */
    Texture2D tex_flash;   /* flashlight dark-with-hole overlay */
    Texture2D tex_spark;   /* small particle */

    /* audio */
    int audio_ok;
    Sound sfx[SFX_COUNT];

    float t;            /* wall time accumulator (visual anims) */
    float accum;        /* fixed-timestep accumulator */
    float complete_t;   /* timer on complete screen */
    int muted;
} App;

/* Color table for magnet colors (main / glow). */
Color mag_color(MagColor c, int glow);

/* render.c */
void render_init(App *a);
void render_shutdown(App *a);
void render_world(App *a);              /* draws the 3D scene */
void render_spawn_burst(App *a, float gx, float gy, Color col, int n, float spd);
void render_push_popup(App *a, float gx, float gy, const char *text, Color c);
void render_update_fx(App *a, float dt); /* particles, trail, popups */
void render_reset_fx(App *a);
Vector3 game_to_world(float gx, float gy, float gz);

/* audio.c */
void audio_init(App *a);
void audio_shutdown(App *a);
void audio_play(App *a, SfxId id);
void audio_play_pitched(App *a, SfxId id, float pitch);

/* ui.c */
void ui_hud(App *a);
void ui_title(App *a);
void ui_select(App *a);
void ui_pause(App *a);
void ui_complete(App *a);

#endif /* APP_H */
