/* MagLava — C/Raylib port for FloppyJam.
 *
 * A top-down vertical climbing game with magnetic-tether "color swing"
 * movement, faithfully ported from the original TypeScript/Phaser game.
 * Gameplay math runs in the original 2D game-space (x in [0,540], y in
 * [0,5000], where DECREASING y is "up"/progress). The renderer projects
 * that plane into a 3D shaft. See sim.c for the authoritative simulation.
 *
 * This header is the shared contract. sim.c and levels_gen.h are pure C
 * (no raylib) so the core game logic can be unit-tested headlessly.
 */
#ifndef MAGLAVA_H
#define MAGLAVA_H

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* World / gameplay constants (mirrors src/constants/gameplay.ts)      */
/* ------------------------------------------------------------------ */
#define WORLD_WIDTH        540.0f
#define WORLD_HEIGHT       860.0f
#define LEVEL_HEIGHT       5000.0f
#define LEGACY_WIDTH       1280.0f
#define LEGACY_SCALE_X     (WORLD_WIDTH / LEGACY_WIDTH) /* 0.421875 */

#define PLAYER_SIZE            20.0f
#define PLAYER_GRAVITY         400.0f
#define PLAYER_MAX_FALL_SPEED  600.0f
#define PLAYER_SWING_MAX_SPEED 500.0f
#define ATTACHMENT_RADIUS      30.0f
#define DETECTION_RANGE        400.0f

/* Arcade-easy swing preset (the shipped default feel). */
#define ARC_STEER        0.10f
#define ARC_INIT_MULT    0.85f
#define ARC_SPEED_BOOST  1.3f
#define ARC_MIN_SPEED    230.0f

/* Lava */
#define LAVA_START_OFFSET   500.0f  /* lava starts this far below player start */
#define LAVA_WARNING_DIST   200.0f
#define LAVA_DESTROY_BUFFER 50.0f
#define BACKTRACK_THRESHOLD 50.0f
#define BACKTRACK_MULT      1.5f
#define BACKTRACK_TIME      3.0f    /* seconds */

/* Camera */
#define CAM_LERP_SPEED   0.08f
#define CAM_PLAYER_OFF_Y 0.6f

/* Scoring */
#define SCORE_LANDING       10
#define SCORE_CHECKPOINT    100
#define SCORE_LEVEL_COMPLETE 500
#define SCORE_NO_DEATH      1000
#define COMBO_MAX           10
#define COMBO_TIMEOUT       3.0f    /* seconds */

/* Timings (seconds) */
#define RESPAWN_DELAY   1.0f
#define IMMUNITY_TIME   10.0f
#define RACE_LOST_DELAY 1.5f

/* Obstacle constants */
#define SWEEPER_DEF_LENGTH 80.0f
#define SWEEPER_DEF_SPEED  150.0f
#define SWEEPER_WIDTH      12.0f
#define SWEEPER_HITPAD     4.0f    /* half-thickness = (WIDTH+PAD)/2 = 8 */
#define PULSE_MIN_R        30.0f
#define PULSE_MAX_R        100.0f
#define PULSE_DEF_SPEED    35.0f
#define PULSE_THICK        8.0f
#define LASER_THICK        6.0f
#define LASER_DEF_ON       1200.0f /* ms */
#define LASER_DEF_OFF      5000.0f /* ms */
#define LASER_WARNING      1000.0f /* ms */
#define ROAMER_SIZE        22.0f
#define ROAMER_DEF_SPEED   50.0f
#define ROAMER_TURN_RATE   1.5f
#define MINE_SIZE          18.0f
#define MINE_FORCE_RADIUS  120.0f
#define MINE_FORCE_STRENGTH 180.0f

/* Anomaly level ids (campaign 1..25) */
#define LEVEL_FLASHLIGHT  12
#define LEVEL_AIRACER     18
#define LEVEL_ROTCAM      25
#define AIRACER_SPEED     160.0f
#define ROTCAM_SPEED      12.0f   /* deg/sec */
#define ROTCAM_MAX        35.0f   /* deg */
#define FLASHLIGHT_RADIUS 260.0f  /* clear radius in game-space px */

#define LEVEL_COUNT 25

/* ------------------------------------------------------------------ */
/* Colors                                                              */
/* ------------------------------------------------------------------ */
typedef enum { COL_RED = 0, COL_BLUE = 1, COL_YELLOW = 2, COL_GREEN = 3 } MagColor;

/* Obstacle types */
typedef enum {
    OB_SWEEPER = 0, OB_PULSE = 1, OB_LASER = 2, OB_ROAMER = 3, OB_MINE = 4
} ObType;

/* ------------------------------------------------------------------ */
/* Static level definition data (generated into levels_gen.h)          */
/* Coordinates are already in final 540-space (legacy X pre-scaled).   */
/* ------------------------------------------------------------------ */
typedef struct { float x, y; unsigned char color; } MagnetDef;
typedef struct { float y; int respawn; } CheckpointDef; /* respawn = magnet index */

typedef struct {
    unsigned char type;
    float x, y;
    float length;    /* sweeper arm length (unscaled, matches original) */
    float ex, ey;    /* laser endpoint (pre-computed, scaled) */
    float speed;     /* sweeper deg/s (signed), roamer/pulse px/s */
    float on_ms, off_ms; /* laser timing */
    float init_angle;    /* roamer initial heading (deg), from id hash */
    unsigned char polarity; /* mine: 0 blue, 1 red */
} ObstacleDef;

typedef struct {
    const char *name;
    float par_time;   /* seconds */
    float lava_speed; /* px/sec */
    float lava_accel;
    unsigned int bg_color; /* 0x00RRGGBB */
    int n_mag; const MagnetDef *mag;
    int n_cp;  const CheckpointDef *cp;
    int n_ob;  const ObstacleDef *ob;
} LevelDef;

extern const LevelDef LEVELS[LEVEL_COUNT];
extern const char *const LEVEL_LABELS[LEVEL_COUNT]; /* e.g. "1-1", "5" */

/* ------------------------------------------------------------------ */
/* Runtime simulation state                                            */
/* ------------------------------------------------------------------ */
typedef enum {
    PS_ATTACHED = 0, PS_SWINGING = 1, PS_FALLING = 2, PS_DEAD = 3
} PlayerState;

typedef struct {
    /* live obstacle state (indexed parallel to LevelDef.ob) */
    unsigned char type;
    float x, y;          /* current position (roamer moves) */
    float angle;         /* sweeper/roamer heading (deg) */
    float radius;        /* pulse ring current radius */
    int expanding;       /* pulse ring direction */
    float timer;         /* laser state timer (ms) */
    int lstate;          /* laser: 0 off,1 warning,2 on */
    float vx, vy;        /* roamer velocity */
    float target_angle;  /* roamer steering target (deg) */
    int alive;           /* consumed by lava? */
    unsigned char polarity;
} Obstacle;

typedef struct {
    int active;
    float x, y;          /* game-space */
    int target_idx;      /* index into magnets */
    float finish_y;
    int finished;
} AIRacer;

typedef struct {
    int level_id;              /* campaign 1..25 */
    const LevelDef *lv;

    /* player */
    PlayerState state;
    float px, py;              /* player position (game-space) */
    float vx, vy;              /* velocity */
    int attached_idx;          /* magnet index, or -1 */
    int target_idx;            /* swing target magnet index, or -1 */
    MagColor color;
    float immune;              /* seconds remaining */
    float bob_t;               /* attached bob phase */

    /* magnets: alive flag (consumed by lava / recreated on respawn) */
    int n_mag;
    int mag_alive[64];

    /* obstacles */
    int n_ob;
    Obstacle ob[64];

    /* lava */
    float lava_y;
    float normal_lava_speed;
    float lava_speed;          /* current effective */
    int lava_stopped;          /* level complete */
    float backtrack_timer;     /* seconds remaining of 1.5x */
    int backtrack_active;

    /* progress / scoring */
    int score;
    int combo;
    float combo_timer;
    int deaths;
    float elapsed;             /* seconds since start */
    float highest_y;           /* lowest y reached (highest point) */
    float last_attach_y;
    int cur_cp;                /* index of current checkpoint, or -1 */
    float cp_lava_y;           /* lava snapshot at last checkpoint */

    /* flow */
    float respawn_timer;       /* >0 = dying, counts down to respawn */
    int game_over;             /* level complete */
    int won;

    /* anomalies */
    AIRacer ai;
    int race_lost;
    float race_lost_timer;
    float rot_cam;             /* current rotation deg (level 25) */
    int rot_dir;

    /* event flags for one frame (consumed by audio/fx layer) */
    int ev_attach;             /* landed on a magnet */
    int ev_attach_forward;     /* landed going up */
    int ev_checkpoint;
    int ev_swing;              /* started a swing */
    int ev_death;
    int ev_complete;
    unsigned int rng;          /* deterministic per-sim RNG */
} GameSim;

/* Sim API (sim.c) — pure, no raylib. */
void  sim_init(GameSim *g, int level_id);
/* color_pressed: 0..3 for a color just-pressed this frame, or -1 for none. */
void  sim_update(GameSim *g, float dt, int color_pressed);
float sim_height(const GameSim *g); /* climbed height for HUD */
int   sim_stars(const GameSim *g);  /* 1..3 once complete */
/* Nearest magnet of color within detection range excluding a magnet index.
 * Returns index or -1. Exposed for the target reticle / AI. */
int   sim_find_magnet(const GameSim *g, float x, float y, MagColor c, int exclude);

/* Save data (save.c) — pure stdio. */
typedef struct { unsigned char stars[LEVEL_COUNT]; int unlocked; } SaveData;
void save_load(SaveData *s);
void save_store(const SaveData *s);
void save_record(SaveData *s, int level_id, int stars);

#endif /* MAGLAVA_H */
