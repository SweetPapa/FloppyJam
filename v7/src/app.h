/* app.h — the state machine and the single frame entry (§0, §10).
 *
 * app_frame() is the only per-frame function in the game, and it exists from
 * day one so the web build is never a port: on desktop main.c drives it from a
 * while loop, on web emscripten_set_main_loop drives it. Same code, same sim,
 * single-threaded either way.
 */
#ifndef VB_APP_H
#define VB_APP_H

#include "rules.h"
#include "ai.h"
#include "replay.h"
#include "save.h"
#include "fx.h"

enum {
    ST_TITLE = 0,   /* attract mode: two AIs rally behind the logo         */
    ST_MENU,
    ST_SETUP,       /* scheme select, then mutators and Tilt if they apply */
    ST_TILT,
    ST_BANTER,      /* who you are about to play, and what they say        */
    ST_PLAY,
    ST_PAUSE,
    ST_CARD,        /* stat card + the best rally, replayed               */
    ST_OPTIONS,
    ST_BINDS,
    ST_LADDER,      /* the Gauntlet                                        */
    ST_SHOTS,       /* the Trickshot list                                  */
    ST_COUNT_
};

/* keyboard bindings, per player (§3: every action is remappable) */
enum { BIND_UP = 0, BIND_DOWN, BIND_FLICK, BIND_PIN, BIND_CHIP,
       BIND_PREV, BIND_NEXT, BIND_COUNT_ };
extern const char *const VB_BIND_NAMES[BIND_COUNT_];

#define VB_NTRICK VB_NTRICKSHOTS

typedef struct {
    const char *name;
    const char *hint;
    float bx, by, bvx, bvy;
    int   heat;
    float off[VB_NRODS];
    int   scheme;              /* the scheme the shot is posed for         */
    int   gold, silver;        /* attempts for each medal                  */
} VbTrickshot;
extern const VbTrickshot VB_TRICKSHOTS[VB_NTRICK];

typedef struct {
    int   persona;
    int   target;              /* first to N on this rung                  */
    const char *rung;
} VbRung;
extern const VbRung VB_LADDER[VB_NRUNGS];

typedef struct VbApp {
    int      state, prev_state;
    int      running;
    VbSave   save;
    VbFx     fx;
    VbMatch  match;
    VbReplay replay;
    VbAI     ai[VB_NSIDES];
    int      ai_on[VB_NSIDES];

    /* what is being set up / played */
    int   mode, ruleset, doubles;
    unsigned mutators;
    VbTilt tilt[VB_NSIDES];
    int   tier, rung, shot;
    int   attempts;
    unsigned swap_btn[VB_NSIDES];   /* edge detect for the doubles role swap */
    unsigned seed;

    /* menus */
    int   sel, sub, rebind;
    float blink;
    float input_guard;   /* seconds a fresh screen ignores confirm/cancel   */

    /* the loop */
    double acc;
    float  alpha;
    int    started;            /* the first input gesture has happened     */
    int    graybox;

    /* attract mode runs a real match, because it is the best trailer we
     * could possibly have and it costs nothing (§10) */
    VbMatch attract;
    VbAI    attract_ai[2];

    /* card */
    int   card_replay;
    float card_t;
    int   last_longest, last_top_heat;
} VbApp;

void app_init(VbApp *a, int graybox);
void app_frame(VbApp *a);       /* one frame: input, sim, audio, draw       */
void app_shutdown(VbApp *a);

/* used by ui.c */
void app_go(VbApp *a, int state);
void app_start_match(VbApp *a);
void app_setup_trickshot(VbApp *a);
/* Both sides on the AI, straight into a live match: what --demo runs, and
 * what a screenshot of "the game" should actually show. */
void app_start_demo(VbApp *a);

#endif /* VB_APP_H */
