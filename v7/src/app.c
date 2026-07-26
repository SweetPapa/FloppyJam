/* app.c — the state machine, the input, and the one frame entry (§0, §10).
 *
 * The fixed-step accumulator lives here and nowhere else. The sim runs at
 * exactly 240 Hz whatever the display is doing; the renderer is handed the
 * leftover fraction and interpolates. Slow-motion is a multiplier on how much
 * time the accumulator is fed, never on the timestep — a variable timestep
 * would take determinism out with it.
 */
#include "app.h"
#include "ui.h"
#include "synth.h"
#include "rods.h"
#include "heat.h"

#include <string.h>

const char *const VB_BIND_NAMES[BIND_COUNT_] = {
    "UP", "DOWN", "FLICK", "PIN / BUNT", "CHIP", "ROD LEFT", "ROD RIGHT"
};

/* The ladder: ten personalities and then the one who runs the place. */
const VbRung VB_LADDER[VB_NRUNGS] = {
    { 0, 5,  "I. THE WALL"        },
    { 2, 5,  "II. THE METRONOME"  },
    { 6, 5,  "III. THE BANK CLERK"},
    { 1, 5,  "IV. THE GAMBLER"    },
    { 3, 7,  "V. THE SHOWOFF"     },
    { 7, 7,  "VI. THE CURVEBALL"  },
    { 5, 7,  "VII. THE FIRECRACKER"},
    { 4, 7,  "VIII. THE MIRROR"   },
    { 8, 10, "IX. THE COUNTERPUNCHER" },
    { 9, 10, "X. THE FINALE"      },
    { 9, 10, "THE LONG NIGHT"     },
};

/* Twenty authored setups. Each one poses a single question — and the answer is
 * always one of the five verbs, never a trick the game has not taught you. */
#define R6(a,b,c,d,e,f) { a, b, c, d, e, f }
const VbTrickshot VB_TRICKSHOTS[VB_NTRICK] = {
 { "FIRST BLOOD",     "Flick it straight in.",
   -0.20f,  0.00f,  0.90f,  0.00f, 2, R6( 0.00f, 0.00f, 0.10f, 0.00f, 0.15f,-0.14f), VB_SCHEME_GLIDE, 1, 3 },
 { "OFF THE WALL",    "Bank it. The side walls are live.",
   -0.30f, -0.35f,  1.10f, -0.55f, 3, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.10f), VB_SCHEME_GLIDE, 2, 4 },
 { "THE GAP",         "There is one lane open. Find it.",
   -0.10f,  0.20f,  1.20f,  0.00f, 4, R6( 0.00f, 0.10f,-0.11f, 0.00f, 0.19f, 0.12f), VB_SCHEME_GLIDE, 2, 5 },
 { "THREAD IT",       "Past two bars, one line.",
    0.00f, -0.10f,  1.30f,  0.10f, 5, R6( 0.00f, 0.00f, 0.06f, 0.00f,-0.12f, 0.05f), VB_SCHEME_GLIDE, 3, 6 },
 { "HOT BALL",        "Step nine. Mind the breath.",
   -0.40f,  0.00f,  2.20f,  0.00f, 9, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.08f,-0.06f), VB_SCHEME_GLIDE, 2, 5 },
 { "THE CATCH",       "Pin it dead, then look up.",
   -0.10f,  0.05f,  1.40f,  0.00f, 5, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.14f, 0.13f), VB_SCHEME_GRIP,  2, 5 },
 { "WALK THE LINE",   "Pin, drag two lanes, snap.",
   -0.10f, -0.30f,  1.30f,  0.00f, 5, R6( 0.00f, 0.00f, 0.00f, 0.00f,-0.15f,-0.13f), VB_SCHEME_GRIP,  3, 6 },
 { "TOP SHELF",       "Snap it into the far corner.",
    0.00f,  0.10f,  1.20f, -0.10f, 6, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.06f), VB_SCHEME_GRIP,  3, 6 },
 { "OVER THE TOP",    "Chip the bar in front of you.",
   -0.05f,  0.00f,  1.10f,  0.00f, 4, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.00f,-0.10f), VB_SCHEME_GRIP,  3, 7 },
 { "THROUGH BALL",    "Chip past the defence, collect, finish.",
   -0.35f, -0.15f,  1.20f,  0.10f, 4, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.00f), VB_SCHEME_GRIP,  4, 8 },
 { "COLD HANDS",      "Bunt the heat out, then place it.",
   -0.20f,  0.00f,  3.10f,  0.00f,12, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.10f, 0.10f), VB_SCHEME_GRIP,  3, 7 },
 { "THE CURVE",       "Sweep the bar. Bend it round the goalie.",
   -0.10f,  0.25f,  1.30f, -0.10f, 6, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f), VB_SCHEME_GRIP,  4, 8 },
 { "DOUBLE BANK",     "Two walls, then in.",
   -0.30f,  0.40f,  1.40f,  0.70f, 6, R6( 0.00f, 0.00f, 0.00f, 0.00f,-0.19f, 0.14f), VB_SCHEME_GRIP,  4, 9 },
 { "THE ROCKET",      "Charge it. Everything gives.",
   -0.45f,  0.00f,  0.80f,  0.00f, 7, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.19f, 0.00f), VB_SCHEME_GRIP,  3, 7 },
 { "BEHIND THE BAR",  "Your attack rod is already past them. Use it.",
    0.15f, -0.20f,  0.60f,  0.20f, 3, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.12f), VB_SCHEME_GRIP,  3, 6 },
 { "TIGHT ANGLE",     "From the wall, across the face.",
   -0.20f,  0.55f,  1.10f, -0.20f, 5, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.12f,-0.12f), VB_SCHEME_GRIP,  4, 8 },
 { "PIN AND WAIT",    "Two seconds is a long time. Let them move first.",
   -0.10f,  0.00f,  1.20f,  0.00f, 5, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f), VB_SCHEME_GRIP,  4, 8 },
 { "THE SCORCHER",    "Step nine or better, or it does not count.",
   -0.50f,  0.10f,  2.60f, -0.10f,10, R6( 0.00f, 0.00f, 0.00f, 0.00f, 0.06f, 0.08f), VB_SCHEME_GRIP,  4, 9 },
 { "NO WAY THROUGH",  "There is. There always is.",
   -0.25f,  0.00f,  1.30f,  0.00f, 6, R6( 0.00f, 0.10f,-0.11f, 0.00f, 0.19f, 0.00f), VB_SCHEME_GRIP,  5, 10 },
 { "THE LONG ONE",    "Your own goalie to theirs. One touch.",
   -0.86f,  0.00f,  0.50f,  0.00f, 8, R6( 0.00f, 0.19f, 0.11f, 0.00f,-0.19f, 0.11f), VB_SCHEME_GRIP,  5, 12 },
};

/* ---- input ------------------------------------------------------------- */

static const int DEFAULT_BINDS[VB_NPLAYERS][BIND_COUNT_] = {
    { KEY_W, KEY_S, KEY_F, KEY_G, KEY_R, KEY_Q, KEY_E },
    { KEY_T, KEY_G, KEY_V, KEY_B, KEY_C, KEY_Z, KEY_X },
    { KEY_UP, KEY_DOWN, KEY_RIGHT_SHIFT, KEY_SLASH, KEY_PERIOD, KEY_COMMA, KEY_M },
    { KEY_I, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_U, KEY_O },
};

static void binds_default(VbSave *s) {
    for (int p = 0; p < VB_NPLAYERS; p++)
        for (int k = 0; k < BIND_COUNT_; k++)
            s->binds[p][k] = DEFAULT_BINDS[p][k];
}

/* One player's input for this frame. Gamepads are preferred and keyboard is a
 * peer, not a fallback — any mix, at the same table (§3). */
static VbInput read_input(const VbApp *a, int slot) {
    VbInput in;
    in.axis = 0.0f;
    in.btn = 0;
    const VbSave *s = &a->save;

    /* Slots are 0,1 for side 0 and 2,3 for side 1, but pads are numbered from
     * zero as they are plugged in. In singles only slots 0 and 2 are live, so
     * the second player is holding the SECOND pad, not the third. */
    int pad = a->doubles ? slot : slot / 2;
    if (IsGamepadAvailable(pad)) {
        float ax = GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_Y);
        if (vb_absf(ax) < 0.16f) ax = 0.0f;          /* a real dead zone    */
        in.axis += ax;
        if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_UP))   in.axis -= 1.0f;
        if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) in.axis += 1.0f;
        if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) in.btn |= VB_BTN_FLICK;
        if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) in.btn |= VB_BTN_PIN;
        if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_UP))   in.btn |= VB_BTN_CHIP;
        if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_TRIGGER_1))  in.btn |= VB_BTN_PREV;
        if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) in.btn |= VB_BTN_NEXT;
    }

    const int *b = s->binds[slot];
    if (IsKeyDown(b[BIND_UP]))    in.axis -= 1.0f;
    if (IsKeyDown(b[BIND_DOWN]))  in.axis += 1.0f;
    if (IsKeyDown(b[BIND_FLICK])) in.btn |= VB_BTN_FLICK;
    if (IsKeyDown(b[BIND_PIN]))   in.btn |= VB_BTN_PIN;
    if (IsKeyDown(b[BIND_CHIP]))  in.btn |= VB_BTN_CHIP;
    if (IsKeyDown(b[BIND_PREV]))  in.btn |= VB_BTN_PREV;
    if (IsKeyDown(b[BIND_NEXT]))  in.btn |= VB_BTN_NEXT;

    if (s->invert[slot]) in.axis = -in.axis;
    in.axis = vb_clampf(in.axis, -1.0f, 1.0f);
    /* Quantise on capture, exactly as the replay stores it, so a rally and its
     * replay are fed byte-identical sticks (§5.1). */
    in.axis = (float)(int)(in.axis * 127.0f) / 127.0f;
    return in;
}

/* ---- audio from events ------------------------------------------------- */

static void speak(VbApp *a, const VbSim *s) {
    for (int i = 0; i < s->nev; i++) {
        const VbEvent *e = &s->ev[i];
        float sp = e->f;
        float pitch = vb_clampf(0.75f + sp * 0.20f, 0.5f, 2.2f);
        /* every impact is placed where it happened on the table */
        float pan = vb_clampf(e->at.x / VB_TABLE_HX, -1.0f, 1.0f);
        switch (e->type) {
            case VB_EV_HIT:
                if (e->i == VB_STRIKE_IDLE)      vb_sfx_at(VB_SFX_BUNT, pitch * 1.1f, 0.35f, pan);
                else if (e->i == VB_STRIKE_BUNT) vb_sfx_at(VB_SFX_BUNT, pitch, 0.6f, pan);
                else if (e->i == VB_STRIKE_CHARGED) vb_sfx_at(VB_SFX_CHARGED, pitch, 0.9f, pan);
                else if (e->i == VB_STRIKE_CHIP) vb_sfx_at(VB_SFX_CHIP, pitch, 0.75f, pan);
                else                             vb_sfx_at(VB_SFX_FLICK, pitch, 0.8f, pan);
                break;
            case VB_EV_SNAP:  vb_sfx_at(VB_SFX_SNAP, pitch, 1.0f, pan); break;
            case VB_EV_CHIP:  vb_sfx_at(VB_SFX_CHIP, pitch, 0.8f, pan); break;
            case VB_EV_LAND:  vb_sfx_at(VB_SFX_LAND, 1.0f, 0.5f, pan); break;
            case VB_EV_WALL:  vb_sfx_at(VB_SFX_WALL, pitch, 0.45f, pan); break;
            case VB_EV_PIN:   vb_sfx_at(VB_SFX_PIN, 1.0f, 0.7f, pan); break;
            case VB_EV_WHIFF: vb_sfx_at(VB_SFX_WHIFF, 1.0f, 0.4f, pan); break;
            case VB_EV_SAVE:  vb_sfx_at(VB_SFX_SAVE, 1.0f, 0.7f, pan); break;
            case VB_EV_MERCY: vb_sfx_at(VB_SFX_MERCY, 1.0f, 0.5f, pan); break;
            /* the referee and the detonation are not events on the table —
             * they are the building talking, so they stay centred */
            case VB_EV_STALL: vb_sfx(VB_SFX_REF, 1.0f, 0.8f); break;
            case VB_EV_GOAL:
                vb_sfx(vb_heat_scorcher(e->i) ? VB_SFX_SCORCHER : VB_SFX_GOAL,
                       1.0f, 1.0f);
                break;
            default: break;
        }
    }
}

/* ---- setup ------------------------------------------------------------- */

void app_go(VbApp *a, int state) {
    a->prev_state = a->state;
    a->state = state;
    a->sel = 0;
    a->blink = 0.0f;
    /* A screen that has only just appeared does not answer to the button that
     * brought it here. Without this, one press walks straight through the
     * victory card into the menu and out the other side into whatever was
     * under the cursor — and the player never sees their own stat card.
     * The victory screen gets longer, because it has something to read. */
    a->input_guard = (state == ST_CARD) ? 0.85f : 0.22f;
}

static VbSimCfg build_sim_cfg(VbApp *a) {
    VbSimCfg c;
    memset(&c, 0, sizeof c);
    for (int i = 0; i < VB_NPLAYERS; i++) c.scheme[i] = a->save.scheme[i];
    c.doubles = a->doubles;
    c.mutators = a->mutators;
    for (int t = 0; t < VB_NSIDES; t++) {
        c.tilt[t] = a->tilt[t];
        if (c.tilt[t].pad_scale <= 0.01f) c.tilt[t].pad_scale = 1.0f;
        if (c.tilt[t].rod_speed <= 0.01f) c.tilt[t].rod_speed = 1.0f;
        if (c.tilt[t].goal_scale <= 0.01f) c.tilt[t].goal_scale = 1.0f;
        c.tilt[t].assist = a->save.assist[t];
    }
    return c;
}

void app_start_match(VbApp *a) {
    VbSimCfg sc = build_sim_cfg(a);
    VbMatchCfg mc;
    mc.mode = a->mode;
    mc.ruleset = a->ruleset;
    mc.target = a->save.target;
    mc.golden = (a->mutators & VB_MUT_GOLDEN_GOAL) ? 1 : 0;

    if (a->mode == VB_MODE_GAUNTLET) {
        const VbRung *r = &VB_LADDER[vb_clampi(a->rung, 0, VB_NRUNGS - 1)];
        mc.target = r->target;
        a->ai_on[1] = 1;
        vb_ai_init(&a->ai[1], r->persona, a->tier, 2, a->seed ^ 0x9E3779B9u);
    } else if (a->mode == VB_MODE_RALLY) {
        mc.ruleset = VB_RULES_CO_OP;
    }

    vb_match_init(&a->match, &mc, &sc, a->seed ? a->seed : 20260725u);
    vb_replay_reset(&a->replay);
    vb_replay_begin(&a->replay, &a->match.sim);
    a->acc = 0.0;
    a->attempts = 0;
    a->card_replay = 0;
    a->card_t = 0.0f;
    app_go(a, ST_PLAY);
}

/* Trickshots pose the table and then get out of the way. */
void app_setup_trickshot(VbApp *a) {
    const VbTrickshot *t = &VB_TRICKSHOTS[vb_clampi(a->shot, 0, VB_NTRICK - 1)];
    VbSimCfg sc = build_sim_cfg(a);
    for (int i = 0; i < VB_NPLAYERS; i++) sc.scheme[i] = t->scheme;
    VbMatchCfg mc = { VB_MODE_TRICKSHOT, VB_RULES_STANDARD, 1, 0 };
    vb_match_init(&a->match, &mc, &sc, 7u + (unsigned)a->shot);
    vb_sim_serve(&a->match.sim, 0);
    VbBall *b = &a->match.sim.balls[0];
    b->alive = 1;
    b->p = v2(t->bx, t->by);
    b->v = v2(t->bvx, t->bvy);
    b->heat = t->heat;
    for (int i = 0; i < VB_NRODS; i++)
        a->match.sim.rods[i].off = vb_clampf(t->off[i], -a->match.sim.rods[i].travel,
                                              a->match.sim.rods[i].travel);
    a->match.phase = VB_PH_LIVE;
    a->acc = 0.0;
}

/* ---- the attract table ------------------------------------------------- */

static void attract_reset(VbApp *a) {
    VbSimCfg sc;
    memset(&sc, 0, sizeof sc);
    for (int i = 0; i < VB_NPLAYERS; i++) sc.scheme[i] = VB_SCHEME_GRIP;
    for (int t = 0; t < VB_NSIDES; t++) {
        sc.tilt[t].pad_scale = 1.0f; sc.tilt[t].rod_speed = 1.0f;
        sc.tilt[t].assist = 1.0f; sc.tilt[t].goal_scale = 1.0f;
    }
    VbMatchCfg mc = { VB_MODE_VERSUS, VB_RULES_STANDARD, 10, 0 };
    static unsigned n = 1u;
    vb_match_init(&a->attract, &mc, &sc, 0xA772AC7u + (n += 7919u));
    vb_ai_init(&a->attract_ai[0], (int)(n % VB_NPERSONAS), 2, 0, n * 3u + 1u);
    vb_ai_init(&a->attract_ai[1], (int)((n / 3u) % VB_NPERSONAS), 2, 2, n * 5u + 2u);
}

void app_start_demo(VbApp *a) {
    a->mode = VB_MODE_VERSUS;
    a->ruleset = VB_RULES_STANDARD;
    /* short, so --demo reaches the victory screen and its rally replay in a
     * sitting — the whole point is to exercise the full match arc */
    a->save.target = 3;
    a->ai_on[0] = a->ai_on[1] = 1;
    vb_ai_init(&a->ai[0], 3, 2, 0, 1234u);
    vb_ai_init(&a->ai[1], 7, 2, 2, 5678u);
    a->seed = 991u;
    app_start_match(a);
}

/* ---- init / shutdown --------------------------------------------------- */

void app_init(VbApp *a, int graybox) {
    memset(a, 0, sizeof *a);
    a->running = 1;
    a->graybox = graybox;

    vb_save_load(&a->save);
    if (a->save.binds[0][0] == 0) binds_default(&a->save);

    vb_fx_init(&a->fx, graybox);
    vb_fx_settings(&a->fx, a->save.palette, a->save.reduce_motion);
    vb_synth_init();
    vb_synth_volumes(a->save.master, a->save.music, a->save.sfx, a->save.crowd);

    a->mode = VB_MODE_VERSUS;
    a->ruleset = VB_RULES_STANDARD;
    a->seed = 20260725u;
    for (int t = 0; t < VB_NSIDES; t++) {
        a->tilt[t].pad_scale = 1.0f;
        a->tilt[t].rod_speed = 1.0f;
        a->tilt[t].goal_scale = 1.0f;
        a->tilt[t].assist = a->save.assist[t];
        a->tilt[t].heat_cap = 0;
    }
    attract_reset(a);
    app_go(a, ST_TITLE);
}

void app_shutdown(VbApp *a) {
    vb_save_write(&a->save);
    vb_fx_shutdown(&a->fx);
    vb_synth_shutdown();
}

/* ---- the frame --------------------------------------------------------- */

static void step_match(VbApp *a, VbMatch *m, VbInput in[VB_NPLAYERS], int record) {
    vb_fx_snapshot(&a->fx, &m->sim);
    vb_match_step(m, in);
    if (record) vb_replay_record(&a->replay, in);
    vb_fx_events(&a->fx, &m->sim);
    speak(a, &m->sim);
}

/* Closes the recording. The next rally is anchored when it actually goes live,
 * not here — see run_play. Recording the 1.2 s pause between points and then
 * feeding it back through vb_sim_step would replay a parked ball as a moving
 * one, and the replay would be a different rally to the one you just played. */
static void end_rally(VbApp *a) {
    VbMatch *m = &a->match;
    vb_replay_end(&a->replay, m->rally, m->rally_heat,
                  vb_heat_scorcher(m->last_goal_heat), m->last_goal_side);
}

static void finish_match(VbApp *a) {
    VbMatch *m = &a->match;
    int longest = m->stats[0].longest_rally > m->stats[1].longest_rally
                ? m->stats[0].longest_rally : m->stats[1].longest_rally;
    a->last_longest = longest;
    a->last_top_heat = m->stats[0].top_heat > m->stats[1].top_heat
                     ? m->stats[0].top_heat : m->stats[1].top_heat;
    vb_save_record(&a->save, longest,
                   m->stats[0].goals + m->stats[1].goals,
                   m->stats[0].scorchers + m->stats[1].scorchers,
                   m->stats[0].pins + m->stats[1].pins,
                   m->stats[0].chips + m->stats[1].chips,
                   m->stats[0].saves + m->stats[1].saves);
    if (m->best_streak > a->save.best_streak) a->save.best_streak = m->best_streak;

    /* Gauntlet progress and the cosmetic unlock that comes with it (§15: all
     * unlocks are cosmetic, so this can only ever hand out a palette). */
    if (a->mode == VB_MODE_GAUNTLET && m->winner == 0) {
        if (a->rung > a->save.rung_cleared[a->tier])
            a->save.rung_cleared[a->tier] = a->rung;
        int pal = 1 + a->rung / 2;
        if (pal < VB_NPALETTES) a->save.palettes |= (1u << pal);
    }
    vb_save_write(&a->save);

    int best = vb_replay_best(&a->replay);
    a->card_replay = best;
    if (best >= 0) vb_replay_play(&a->replay, best);
    a->card_t = 0.0f;
    app_go(a, ST_CARD);
}

static void trickshot_result(VbApp *a, int made) {
    const VbTrickshot *t = &VB_TRICKSHOTS[a->shot];
    a->attempts++;
    if (made) {
        int m = (a->attempts <= t->gold) ? 2 : (a->attempts <= t->silver) ? 1 : 0;
        if (m > a->save.medal[a->shot]) a->save.medal[a->shot] = (signed char)m;
        vb_save_write(&a->save);
        vb_sfx(VB_SFX_UI_BIG, 1.0f, 0.9f);
        app_go(a, ST_SHOTS);
    } else {
        app_setup_trickshot(a);
    }
}

static void run_play(VbApp *a, float dt) {
    VbMatch *m = &a->match;
    float ts = vb_fx_timescale(&a->fx);
    a->acc += (double)(dt * ts);
    /* A spiral guard: if the machine cannot keep up we drop sim time rather
     * than fall further behind every frame. The sim never sees a longer tick. */
    if (a->acc > 0.25) a->acc = 0.25;

    VbInput in[VB_NPLAYERS];
    while (a->acc >= (double)VB_DT) {
        memset(in, 0, sizeof in);
        int slots = a->doubles ? 4 : 2;
        for (int q = 0; q < slots; q++) {
            int slot = a->doubles ? q : q * 2;
            if (a->ai_on[slot / 2]) continue;
            in[slot] = read_input(a, slot);
        }
        for (int side = 0; side < VB_NSIDES; side++)
            if (a->ai_on[side]) {
                in[side * 2] = vb_ai_think(&a->ai[side], &m->sim);
                /* the Mirror learns from what you actually do */
                vb_ai_observe(&a->ai[side], &in[(1 - side) * 2]);
            }

        /* Doubles partners may trade roles between points, and only between
         * points — swapping mid-rally would be a teleport (§3.3). */
        if (a->doubles && m->phase != VB_PH_LIVE) {
            for (int side = 0; side < VB_NSIDES; side++) {
                unsigned now = in[side * 2].btn | in[side * 2 + 1].btn;
                unsigned before = a->swap_btn[side];
                if ((now & VB_BTN_CHIP) && !(before & VB_BTN_CHIP)) {
                    m->sim.cfg.swap[side] = !m->sim.cfg.swap[side];
                    vb_sfx(VB_SFX_UI_BIG, 1.0f, 0.6f);
                }
                a->swap_btn[side] = now;
            }
        }

        int was_goal = m->score[0] + m->score[1];
        int was_phase = m->phase;
        /* record live ticks only; the anchor below is the state those ticks
         * start from, so playback reproduces the rally exactly */
        step_match(a, m, in, was_phase == VB_PH_LIVE);
        a->acc -= (double)VB_DT;

        if (was_phase == VB_PH_SERVE && m->phase == VB_PH_LIVE)
            vb_replay_begin(&a->replay, &m->sim);

        if (m->cfg.mode == VB_MODE_TRICKSHOT) {
            if (m->sim.goal_side == 0) { trickshot_result(a, 1); return; }
            if (m->phase != VB_PH_LIVE || m->sim.tick > (unsigned)(VB_TICK_HZ * 14)) {
                trickshot_result(a, 0);
                return;
            }
            continue;
        }

        if (m->score[0] + m->score[1] != was_goal) end_rally(a);
        else if (was_phase == VB_PH_LIVE && m->phase != VB_PH_LIVE) end_rally(a);

        if (m->winner >= 0) { finish_match(a); return; }
    }
    a->alpha = (float)(a->acc / (double)VB_DT);
}

void app_frame(VbApp *a) {
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.1f;
    a->blink += dt;

    /* On web the audio device cannot open until the player has touched
     * something, so the first gesture starts it (§0). */
    if (!a->started && (GetKeyPressed() || IsMouseButtonPressed(0)
                        || IsGamepadAvailable(0))) {
        a->started = 1;
        vb_synth_gesture();
        vb_synth_volumes(a->save.master, a->save.music, a->save.sfx, a->save.crowd);
    }

    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

    /* the sim, wherever it is running this frame */
    switch (a->state) {
        case ST_PLAY:
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) {
                app_go(a, ST_PAUSE);
                break;
            }
            run_play(a, dt);
            break;

        case ST_CARD:
            a->card_t += dt;
            /* the best rally of the match, replayed from the input log —
             * free, because the sim is deterministic (§5.1) */
            if (a->card_replay >= 0 && a->card_t > 1.2f) {
                a->acc += (double)dt;
                while (a->acc >= (double)VB_DT) {
                    if (!vb_replay_step(&a->replay))
                        vb_replay_play(&a->replay, a->card_replay);
                    a->acc -= (double)VB_DT;
                }
            }
            break;

        case ST_TITLE: {
            /* two AIs rallying behind the logo glass */
            a->acc += (double)dt;
            if (a->acc > 0.25) a->acc = 0.25;
            VbInput in[VB_NPLAYERS];
            while (a->acc >= (double)VB_DT) {
                memset(in, 0, sizeof in);
                in[0] = vb_ai_think(&a->attract_ai[0], &a->attract.sim);
                in[2] = vb_ai_think(&a->attract_ai[1], &a->attract.sim);
                vb_fx_snapshot(&a->fx, &a->attract.sim);
                vb_match_step(&a->attract, in);
                vb_fx_events(&a->fx, &a->attract.sim);
                a->acc -= (double)VB_DT;
            }
            a->alpha = (float)(a->acc / (double)VB_DT);
            if (a->attract.winner >= 0) attract_reset(a);
            break;
        }
        default: break;
    }

    /* controller-disconnect auto-pause (§10) */
    if (a->state == ST_PLAY && !a->ai_on[1] && a->started) {
        static int had_pad = 0;
        int has = IsGamepadAvailable(0);
        if (had_pad && !has) app_go(a, ST_PAUSE);
        had_pad = has;
    }

    vb_fx_update(&a->fx, dt);

    /* the bed follows the ball that is actually in play */
    {
        const VbMatch *m = (a->state == ST_TITLE) ? &a->attract : &a->match;
        int heat = 0;
        for (int i = 0; i < m->sim.nballs; i++)
            if (m->sim.balls[i].alive && m->sim.balls[i].heat > heat)
                heat = m->sim.balls[i].heat;
        /* The crowd tracks the heat, and leans in a little further when the
         * match is on the line — a room reacts to what is at stake, not only
         * to how fast the ball is moving. The music already lifts a half-step
         * for the same moment (§9); this is the other half of it. */
        int mp = vb_match_is_match_point(m);
        float bed = vb_heat_temp(heat);
        if (mp && m->winner < 0) bed = vb_clampf(bed * 1.10f + 0.14f, 0.0f, 1.0f);
        vb_synth_crowd(bed, a->fx.gasp);
        vb_synth_music(vb_heat_layers(heat), mp, m->cfg.mode == VB_MODE_RALLY);
    }
    vb_synth_update();

    /* menus, HUD, and every screen that is not the table */
    vb_ui_frame(a, dt);

    BeginDrawing();
    vb_ui_draw(a);
    EndDrawing();

    if (WindowShouldClose()) a->running = 0;
}
