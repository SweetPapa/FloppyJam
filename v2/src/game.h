/* The hub: app state, run state, and the microgame framework (§5.1). */
#ifndef G_GAME_H
#define G_GAME_H

#include "common.h"
#include "rng.h"
#include "sequencer.h"
#include "audio.h"
#include "clock.h"
#include "chart.h"
#include "cards.h"
#include "heat.h"
#include "draw.h"

/* ---- input ---------------------------------------------------------- */
typedef struct {
    uint8_t  lane;   /* logical lane, after MIRROR / SWAP remapping */
    uint8_t  raw;    /* physical lane */
    uint8_t  down;   /* 1 = press, 0 = release */
    uint64_t t;      /* sample time */
} InputEvent;

#define MAX_INPUT_EVENTS 64

typedef struct {
    InputEvent e[MAX_INPUT_EVENTS];
    int      count;
    uint32_t down_mask;      /* logical lanes currently held */
    uint32_t press_mask;     /* pressed this frame */
    uint32_t release_mask;
} InputEvents;

/* ---- phrase context passed to build_chart --------------------------- */
typedef struct {
    int base_bar;      /* absolute bar index of the phrase's first bar */
    int phrase_index;
    int tier;
    int dense;         /* DENSE card active */
    uint32_t seed_code;
} PhraseCtx;

typedef struct MicroState MicroState;
typedef struct Game Game;

typedef struct {
    const char *name;
    const char *prompt;
    uint8_t keys;
    uint8_t icon_id;
    uint8_t min_tier;
    void (*build_chart)(Chart *, const Genome *, const PhraseCtx *);
    void (*update)(MicroState *, const InputEvents *, const ClockSnap *);
    void (*draw)(MicroState *);
    bool (*judge_phrase)(const MicroState *);
} Microgame;

struct MicroState {
    const Microgame *def;
    Chart       chart;
    PhraseStats stats;
    PhraseCtx   ctx;
    Game       *g;
    int         game_id;

    uint64_t start_sample, end_sample;
    int      active, finished, hard_fail, extra;
    int      seq_pos;                 /* STAIRS/ECHO/CALLBACK cursor */
    int8_t   key_map[LANE_COUNT];     /* SWAP remap, logical -> chart lane */
    float    lane_flash[LANE_COUNT];
    float    fx[4];
    int      ix[4];
    uint64_t ux[4];
    uint8_t  slot[64];                /* RAPID 16th slots */
};

/* ---- modifiers ------------------------------------------------------ */
typedef struct {
    uint8_t owned[CARD_COUNT];
    int     chip[8];       /* draft order, for the chip row */
    int     chip_count;
    float   mult;

    int mirror, deaf, tight, fog, dense, spin, strobe, metronome;
    int breather;             /* skip the next card pick */
    int allin;                /* armed for the next phrase */
    int allin_active;         /* applies to the phrase running now */
    int wild_hidden;          /* card id hidden by WILD, -1 = none */
    int wild_reveal_phrase;
    float bpm_bonus;          /* TURBO accumulations */
} Mods;

/* ---- app state ------------------------------------------------------ */
typedef enum {
    ST_BOOT = 0, ST_ATTRACT, ST_SEED, ST_CALIBRATE,
    ST_COUNTIN, ST_RUN, ST_WIPE, ST_TALLY, ST_SCORE_ENTRY, ST_LEADERBOARD
} AppState;

typedef struct {
    char     initials[4];
    int32_t  score;
    uint8_t  tier;
    uint32_t seed;
} ScoreRow;

#define LEADERBOARD_N 10
#define REPLAY_MAX 2048

typedef struct { uint64_t t; uint8_t key; uint8_t down; } ReplayEvent;

struct Game {
    AppState state;
    float    state_t;
    float    dt;
    double   host_time;
    ClockSnap clk;
    InputEvents in;

    uint32_t party_seed;
    uint32_t seed_code;
    Genome   genome;

    Rng rng_sched, rng_cards, rng_cos;

    /* --- run --------------------------------------------------------- */
    int64_t score;
    int64_t display_score;
    int     hp, tier, phrase_index;
    int     run_active;
    int     bars_seen;
    int     next_seg_bar;
    int     next_seg_kind;      /* 0 = phrase, 1 = card bar */
    int     phrases_since_card;
    int     first_phrase_done;
    int     last_game_id;
    int     flow;               /* consecutive PERFECTs */
    int     best_flow;
    float   flow_meter;
    int     phrase_scores_shown;

    MicroState micro;       /* the phrase currently playing */
    MicroState pending;     /* the phrase telegraphed for the next bar */
    int        pending_valid;

    Mods mods;

    /* card pick */
    int   card_active;
    int   card_prepared;
    int   card_choice[2];
    int   card_picked;
    int   card_bar;
    uint64_t card_end_sample;
    float card_t;

    /* presentation */
    float shake_hint;
    float desat;
    float spin_angle;
    float strobe_v;
    float banner_t;
    float wipe_t;
    float tally_t;
    int   tally_step;
    int64_t tally_value;

    /* seed select */
    int   seed_menu_idx;
    char  seed_entry[SEED_LEN + 1];
    int   seed_entry_pos;
    int   seed_entering;
    float seed_confirm_hold;

    /* score entry */
    char  entry[4];
    int   entry_pos;
    int   entry_rank;

    /* leaderboard */
    ScoreRow board[LEADERBOARD_N];
    int      board_count;

    /* calibration */
    int   calib_taps;
    float calib_offsets[16];
    float calib_result;

    /* replay (§11.4) */
    ReplayEvent replay[REPLAY_MAX];
    int         replay_count;
    int         replay_playing;
    int         replay_cursor;

    /* tuning build */
    int   tuning_overlay;
    int   tuning_infinite_hp;
    float tuning_window_scale;
    float tuning_bpm_offset;
    float last_offset_ms;

    float esc_hold;
    int   quit;
    int   autoplay;      /* --autoplay: robot player, for soak tests and tuning */
    int   force_game;    /* --force-game N: pin the scheduler, -1 = off */
};

extern Game G;

/* ---- microgame roster ----------------------------------------------- */
#define MICROGAME_COUNT 15
extern const Microgame *MICROGAMES[MICROGAME_COUNT];

/* ---- run engine (game.c) -------------------------------------------- */
void game_init(uint32_t seed_code);
void run_start(void);
void run_update(void);
void run_draw_playfield(void);
void run_draw_hud(void);
void run_on_bar(const BarInfo *bi);
void run_abort(void);

float run_window_scale(void);
void  micro_feedback(MicroState *m, Judgment j, int note_index, float x, float y);
void  micro_flash_lane(MicroState *m, int lane);
int   micro_chord_tone(const MicroState *m, int index);

/* playfield geometry helpers shared by every microgame */
float pf_center_x(void);
float pf_center_y(void);
float pf_top(void);
float pf_bottom(void);
float pf_lane_x(int lane, int lanes);
/* how far into its approach a note is: 0 = one beat away, 1 = due now */
float pf_note_progress(const MicroState *m, const Note *n, uint64_t now);
bool  pf_note_visible(const MicroState *m, const Note *n, uint64_t now);
void  pf_draw_note(const MicroState *m, const Note *n, uint64_t now, int lane, int lanes);

/* ---- cards (cards logic lives in game.c) ---------------------------- */
void cards_begin_pick(int bar);
void cards_update(void);
void cards_draw(void);
void cards_apply(int card_id, int announce);
float cards_mult(void);

/* ---- screens.c ------------------------------------------------------ */
void screens_update(void);
void screens_draw(void);
void screens_enter(AppState s);
void screens_submit_score(void);

/* ---- persist.c ------------------------------------------------------ */
void persist_load_scores(ScoreRow *rows, int *count, uint32_t *party_seed);
void persist_save_scores(const ScoreRow *rows, int count, uint32_t party_seed);
void persist_load_config(float *calib_ms);
void persist_save_config(float calib_ms);

#endif
