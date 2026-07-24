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
    SFX_DEATH, SFX_COMPLETE, SFX_UI, SFX_COUNT
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

typedef struct {
    Screen screen;
    Screen prev_screen;
    GameSim sim;
    SaveData save;
    int level_id;       /* currently selected / playing */
    int select_cursor;  /* level-select highlighted index (0-based) */

    Camera3D cam;
    float cam_y;        /* smoothed follow target Y (world) */
    float cam_shake;    /* shake magnitude, decays */
    float lava_flash;   /* backtrack flash 0..1 */

    Particle parts[MAX_PARTICLES];
    int part_head;

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
void render_update_particles(App *a, float dt);
Vector3 game_to_world(float gx, float gy, float gz);

/* audio.c */
void audio_init(App *a);
void audio_shutdown(App *a);
void audio_play(App *a, SfxId id);

/* ui.c */
void ui_hud(App *a);
void ui_title(App *a);
void ui_select(App *a);
void ui_pause(App *a);
void ui_complete(App *a);

#endif /* APP_H */
