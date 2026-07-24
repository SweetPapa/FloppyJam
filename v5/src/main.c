/* main.c — loop and state machine (TITLE/SELECT/PLAY/CARD/FINAL).
 *
 * The SURVEY -> STRIKE loop is the heartbeat: a player who knows what they
 * want gets from ball-at-rest to struck ball in well under three seconds,
 * and nothing in this file is allowed to put a menu inside that loop.
 */
#include "raylib.h"

#include "course.h"
#include "shot.h"
#include "camera.h"
#include "render.h"
#include "juice.h"
#include "synth.h"
#include "save.h"
#include "replay.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    ST_TITLE = 0, ST_SELECT, ST_HOWTO, ST_INTRO, ST_AIM, ST_RIDE,
    ST_RESULT, ST_CARD, ST_FINAL, ST_PAUSE, ST_OPTIONS, ST_REPLAY
} GameState;

#define RIDE_TICK_CAP 12000        /* 50 s, then we force the ball to rest */

typedef struct {
    BpWorld  w;
    BpShot   shot;
    BpCam    cam;
    BpHud    hud;
    BpSave   save;
    BpReplay rep;

    int   state, ret_state;
    int   menu_sel, pause_sel, opt_sel, select_sel;
    int   hole, from, to;
    int   strokes, restarts, total;
    int   scores[BP_NHOLES];
    int   seen_intro[BP_NHOLES];
    int   tips_left;

    float acc;                     /* physics accumulator                 */
    int   ride_ticks;
    int   rimming, rimmed;
    int   slowmo_armed;
    int   pending_ace;
    float result_t;
    float t;                       /* presentation clock (never sim)      */

    /* best-moment heuristics for the final card (Section 8) */
    float best_dist; int best_dist_hole;
    int   best_banks; int best_banks_hole;
    int   aces_this_round;

    /* ace replay */
    BpWorld rw;
    int   replay_shot;
    int   replay_ticks;
} Game;

static Game G;

/* ------------------------------------------------------------------ */

static float volf(int v) { return bp_clampf((float)v / 10.0f, 0.0f, 1.0f); }

static void push_volumes(void)
{
    bp_synth_volumes(volf(G.save.vol_master), volf(G.save.vol_music), volf(G.save.vol_sfx));
}

static V3 cup_pos(const BpWorld *w)
{
    int c = bp_course_cup(w);
    if (c < 0) return v3zero();
    return v3(w->pockets[c].x, w->pockets[c].y, w->pockets[c].z);
}

static float cup_dist(const BpWorld *w)
{
    V3 c = cup_pos(w);
    V3 d = v3sub(w->balls[0].p, c);
    return sqrtf(d.x * d.x + d.z * d.z);
}

/* ------------------------------------------------------------------ */
/* events -> sound and particles (Section 10)                          */

static void consume_events(void)
{
    int i;
    for (i = 0; i < G.w.nev; ++i) {
        const BpEvent *e = &G.w.ev[i];
        float m = e->mag;
        switch (e->kind) {
        case EV_WALL:
            bp_sfx(SFX_THUMP, bp_clampf(0.72f + m * 0.14f, 0.5f, 2.0f),
                   bp_clampf(0.16f + m * 0.16f, 0.0f, 0.9f));
            bp_burst(e->at, 3, (Color){ 220, 230, 245, 255 }, m * 0.30f, 0.20f, 6.0f);
            break;
        case EV_BUMPER: {
            int n = (e->b < 0) ? 0 : (e->b % 5);
            bp_sfx(SFX_BOING0 + n, bp_clampf(0.9f + m * 0.05f, 0.7f, 1.6f),
                   bp_clampf(0.30f + m * 0.10f, 0.0f, 0.95f));
            bp_shockwave(e->at, (Color){ 255, 170, 210, 220 }, 0.10f);
            bp_burst(e->at, 8, (Color){ 255, 150, 200, 255 }, m * 0.45f, 0.35f, 5.0f);
            G.cam.shake = bp_clampf(G.cam.shake + 0.25f, 0.0f, 1.0f);
            break;
        }
        case EV_BALLHIT:
            bp_sfx(SFX_CLACK, bp_clampf(0.85f + m * 0.10f, 0.6f, 2.0f),
                   bp_clampf(0.20f + m * 0.18f, 0.0f, 0.95f));
            bp_burst(e->at, 5, (Color){ 255, 255, 240, 255 }, m * 0.35f, 0.18f, 6.0f);
            if (m > 2.0f) bp_hitstop(0.035f);
            break;
        case EV_LAND:
            bp_sfx(SFX_LAND, bp_clampf(0.8f + m * 0.10f, 0.5f, 1.8f),
                   bp_clampf(0.10f + m * 0.14f, 0.0f, 0.8f));
            bp_burst(e->at, 6, (Color){ 210, 226, 200, 255 }, m * 0.25f, 0.30f, 5.0f);
            break;
        case EV_RIM:
            bp_sfx(SFX_RIM, bp_clampf(1.0f + m * 0.25f, 0.7f, 2.4f),
                   bp_clampf(0.14f + m * 0.30f, 0.0f, 0.7f));
            G.rimming = 1;
            break;
        case EV_KICK:
            bp_sfx(SFX_KICK, 1.0f, 0.55f);
            bp_burst(e->at, 12, (Color){ 255, 190, 110, 255 }, 1.6f, 0.35f, 4.0f);
            break;
        case EV_TARGET:
            bp_sfx(SFX_UNCAP, 1.25f, 0.6f);
            bp_burst(e->at, 20, (Color){ 140, 255, 200, 255 }, 1.8f, 0.6f, 3.0f);
            bp_toast("GATE OPEN", 1.8f);
            break;
        case EV_VOID:
            bp_sfx(SFX_SPLASH, 1.0f, 0.7f);
            bp_flash((Color){ 120, 60, 200, 255 }, 0.5f);
            bp_burst(e->at, 18, (Color){ 130, 180, 255, 255 }, 1.4f, 0.7f, 6.0f);
            break;
        case EV_POCKET: {
            int pk = e->b;
            if (pk < 0 || pk >= G.w.npockets) break;
            switch (G.w.pockets[pk].kind) {
            case PK_CUP:
                bp_sfx(SFX_SWALLOW, 1.0f, 0.9f);
                break;
            case PK_WARP:
                bp_sfx(SFX_WARP, 1.0f, 0.7f);
                G.cam.warp_blur = 1.0f;
                bp_burst(e->at, 16, (Color){ 120, 240, 255, 255 }, 1.4f, 0.5f, 2.0f);
                break;
            case PK_BONUS:
                if (e->a == 0) { bp_sfx(SFX_SCRATCH, 1.0f, 0.85f); }
                else {
                    bp_sfx(SFX_UNCAP, 1.0f, 0.8f);
                    bp_confetti(e->at, 26, 0.7f);
                    bp_toast("GOLD POCKET  -1 STROKE", 2.2f);
                }
                break;
            default:
                if (e->a == 0) bp_sfx(SFX_SCRATCH, 1.0f, 0.85f);
                else           bp_sfx(SFX_SWALLOW, 1.2f, 0.5f);
                break;
            }
            if (G.w.eight_potted && G.w.cup_sealed == 0 && G.hud.sealed) {
                bp_sfx(SFX_UNCAP, 1.0f, 0.9f);
                bp_toast("THE CUP IS OPEN", 2.4f);
                bp_confetti(cup_pos(&G.w), 30, 0.8f);
                G.hud.sealed = 0;
            }
            break;
        }
        default: break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* hole lifecycle                                                      */

static void start_hole(int index, int fresh)
{
    G.hole = index;
    bp_course_build(&G.w, index);
    bp_replay_begin(&G.rep, index);
    bp_render_hole_begin();
    bp_juice_reset();
    if (fresh) { G.strokes = 0; }
    G.ride_ticks = 0;
    G.rimming = G.rimmed = 0;
    G.pending_ace = 0;
    G.acc = 0.0f;
    bp_shot_reset(&G.shot, 0);            /* english resets each hole (4.4) */
    G.shot.aim = 0.0f;
    bp_cam_init(&G.cam, G.w.balls[0].p);
    G.hud.sealed = G.w.cup_sealed;
    bp_music_mood(index < 9 ? 1 : 2);

    if (!G.seen_intro[index]) {
        G.seen_intro[index] = 1;
        G.state = ST_INTRO;
        bp_cam_set_mode(&G.cam, CAM_FLYOVER, &G.w, G.w.balls[0].p, cup_pos(&G.w));
        G.cam.target = cup_pos(&G.w);
        G.cam.pos = v3add(G.cam.target, v3(0.0f, 2.0f, -2.6f));
    } else {
        G.state = ST_AIM;
        bp_cam_set_mode(&G.cam, CAM_SURVEY, &G.w, G.w.balls[0].p, cup_pos(&G.w));
    }
}

static void begin_round(int from, int to)
{
    int i;
    G.from = from; G.to = to;
    G.total = 0;
    G.restarts = 0;
    G.aces_this_round = 0;
    G.best_dist = 0.0f; G.best_dist_hole = -1;
    G.best_banks = 0; G.best_banks_hole = -1;
    G.tips_left = 5;
    for (i = 0; i < BP_NHOLES; ++i) { G.scores[i] = 0; G.seen_intro[i] = 0; }
    start_hole(from, 1);
}

static void finish_hole(void)
{
    int s = G.strokes;
    const BpHole *H = &BP_HOLES[G.hole];
    Color tint = (Color){ 255, 236, 180, 255 };
    if (s > BP_STROKE_CAP) s = BP_STROKE_CAP;
    G.scores[G.hole] = s;
    G.total += s;

    if (s == 1) { G.aces_this_round++; G.save.aces++; }
    if (G.save.best_hole[G.hole] == 0 || s < G.save.best_hole[G.hole])
        G.save.best_hole[G.hole] = (unsigned char)s;

    if (s - H->par <= -1) tint = (Color){ 140, 240, 180, 255 };
    else if (s - H->par >= 2) tint = (Color){ 255, 160, 130, 255 };

    bp_banner(bp_score_name(s, H->par), bp_score_flair(s, H->par), 2.6f, tint);
    bp_sfx(SFX_BANNER, 1.0f, 0.8f);
    bp_confetti(cup_pos(&G.w), s <= H->par ? 70 : 22, s < H->par ? 1.25f : 0.7f);

    /* first-bogey tip, at most once each and five in the whole round (15) */
    if (s > H->par && G.tips_left > 0 && H->tip &&
        !(G.save.tips_used & (1u << (G.hole & 7))) && G.hole < 8) {
        G.save.tips_used |= (unsigned char)(1u << (G.hole & 7));
        G.tips_left--;
        bp_toast(H->tip, 5.0f);
    }

    G.state = ST_RESULT;
    G.result_t = 0.0f;
    bp_save_store(&G.save);
}

static void advance_hole(void)
{
    if (G.hole >= G.to) {
        int total = G.total;
        if (G.from == 0 && G.to == 8) {
            if (!G.save.best_front || total < G.save.best_front)
                G.save.best_front = (unsigned short)total;
        } else if (G.from == 9 && G.to == 17) {
            if (!G.save.best_back || total < G.save.best_back)
                G.save.best_back = (unsigned short)total;
        } else if (G.from == 0 && G.to == 17) {
            if (!G.save.best_full || total < G.save.best_full)
                G.save.best_full = (unsigned short)total;
        }
        bp_save_store(&G.save);
        G.state = ST_FINAL;
    } else {
        start_hole(G.hole + 1, 1);
    }
}

/* ------------------------------------------------------------------ */
/* shot resolution                                                     */

static void resolve_shot(void)
{
    /* best-moment heuristics */
    if (G.w.holed) {
        if (G.w.dist_travelled > G.best_dist) {
            G.best_dist = G.w.dist_travelled;
            G.best_dist_hole = G.hole;
        }
        if (G.w.wall_hits > G.best_banks) {
            G.best_banks = G.w.wall_hits;
            G.best_banks_hole = G.hole;
        }
    }

    if (G.w.bonus_hits > 0) {
        int b = G.w.bonus_hits > 2 ? 2 : G.w.bonus_hits;
        G.strokes -= b;
        if (G.strokes < 0) G.strokes = 0;
    }

    if (G.w.holed) {
        finish_hole();
        if (G.strokes == 1) G.pending_ace = 1;
        return;
    }

    if (G.w.scratched) {
        G.strokes++;
        bp_respawn_cue(&G.w);
        bp_flash((Color){ 255, 60, 60, 255 }, 0.85f);
        bp_toast("SCRATCH!  +1 STROKE", 2.2f);
    } else if (G.rimming && !G.w.holed) {
        bp_sfx(SFX_SAD, 1.0f, 0.55f);
        bp_toast("RIMMED OUT", 1.8f);
    }

    if (G.strokes >= BP_STROKE_CAP) { G.strokes = BP_STROKE_CAP; finish_hole(); return; }

    bp_shot_reset(&G.shot, 1);            /* leaving english dialled is the trap */
    G.state = ST_AIM;
    bp_cam_set_mode(&G.cam, CAM_SURVEY, &G.w, G.w.balls[0].p, cup_pos(&G.w));
}

static void fire(float power)
{
    bp_shot_begin(&G.w);
    bp_strike(&G.w, G.shot.aim, power, G.shot.tx, G.shot.ty);
    bp_replay_push(&G.rep, G.shot.aim, power, G.shot.tx, G.shot.ty);
    G.strokes++;
    G.ride_ticks = 0;
    G.rimming = 0;
    G.acc = 0.0f;
    G.shot.strike_anim = 1.0f;
    G.shot.cue_pull = 0.0f;
    G.state = ST_RIDE;
    bp_cam_set_mode(&G.cam, CAM_RIDE, &G.w, G.w.balls[0].p, cup_pos(&G.w));

    bp_sfx(SFX_CRACK, bp_clampf(0.72f + power * 0.72f, 0.5f, 1.9f),
           bp_clampf(0.35f + power * 0.60f, 0.0f, 1.0f));
    bp_hitstop(0.033f);
    bp_shockwave(G.w.balls[0].p, (Color){ 255, 245, 220, 210 }, 0.09f);
    if (power > 0.60f) G.cam.shake = bp_clampf(power, 0.0f, 1.0f);
    bp_music_duck(0.65f);
}

/* run the remainder of the shot instantly; identical result (4.5) */
static void skip_to_rest(void)
{
    while (G.ride_ticks < RIDE_TICK_CAP && !bp_settled(&G.w) && !G.w.holed) {
        bp_step(&G.w);
        consume_events();
        G.ride_ticks++;
    }
}

/* ------------------------------------------------------------------ */
/* input                                                               */

static void aim_input(float dt)
{
    float fine = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 0.125f : 1.0f;
    float rate = 1.9f * fine;
    float p;

    if (IsKeyDown(KEY_A)) G.shot.aim -= rate * dt;
    if (IsKeyDown(KEY_D)) G.shot.aim += rate * dt;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 d = GetMouseDelta();
        G.shot.aim += d.x * 0.006f * fine;
    }
    /* camera orbit: right drag, or Q/E, wheel to zoom */
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 d = GetMouseDelta();
        bp_cam_orbit(&G.cam, d.x * 0.006f, -d.y * 0.004f);
    }
    if (IsKeyDown(KEY_Q)) bp_cam_orbit(&G.cam, -1.4f * dt, 0.0f);
    if (IsKeyDown(KEY_E)) bp_cam_orbit(&G.cam,  1.4f * dt, 0.0f);
    if (IsKeyDown(KEY_R) && !IsKeyDown(KEY_LEFT_CONTROL)) { /* reserved */ }
    {
        float wheel = GetMouseWheelMove();
        if (wheel > 0.5f) bp_cam_zoom(&G.cam, -1);
        if (wheel < -0.5f) bp_cam_zoom(&G.cam, 1);
        if (IsKeyPressed(KEY_Z)) bp_cam_zoom(&G.cam, -1);
        if (IsKeyPressed(KEY_X)) bp_cam_zoom(&G.cam, 1);
    }

    /* english on the cue-ball face */
    {
        float s = 1.35f * dt;
        if (IsKeyDown(KEY_UP))    G.shot.ty += s;
        if (IsKeyDown(KEY_DOWN))  G.shot.ty -= s;
        if (IsKeyDown(KEY_LEFT))  G.shot.tx -= s;
        if (IsKeyDown(KEY_RIGHT)) G.shot.tx += s;
        if (IsKeyPressed(KEY_C))  { G.shot.tx = G.shot.ty = 0.0f; bp_sfx(SFX_UI, 1.2f, 0.3f); }
        {
            float mag = sqrtf(G.shot.tx * G.shot.tx + G.shot.ty * G.shot.ty);
            if (mag > BP_TIP_MAX) {
                G.shot.tx *= BP_TIP_MAX / mag;
                G.shot.ty *= BP_TIP_MAX / mag;
            }
        }
    }

    p = bp_shot_charge(&G.shot, IsKeyDown(KEY_SPACE), dt);
    if (p > 0.0f) fire(p);
}

/* ------------------------------------------------------------------ */

static void sim_ride(float dt)
{
    float scale = bp_timescale();
    if (bp_hitstop_left() > 0.0f) return;

    G.acc += dt * scale;
    if (G.acc > 0.25f) G.acc = 0.25f;
    while (G.acc >= BP_DT) {
        G.acc -= BP_DT;
        bp_step(&G.w);
        consume_events();
        if (++G.ride_ticks >= RIDE_TICK_CAP) break;
        if (G.w.holed) break;
    }
}

static void update_play(float dt)
{
    const BpBall *cb = &G.w.balls[0];
    float sp = v3len(cb->v);

    switch (G.state) {
    case ST_INTRO:
        if (G.cam.t > 2.5f || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            G.state = ST_AIM;
            bp_cam_set_mode(&G.cam, CAM_SURVEY, &G.w, cb->p, cup_pos(&G.w));
        }
        break;

    case ST_AIM:
        aim_input(dt);
        bp_shot_update_guide(&G.shot, &G.w);
        bp_music_duck(0.0f);
        bp_roll(0.0f, 0);
        break;

    case ST_RIDE: {
        float cd;
        if (IsKeyPressed(KEY_R)) skip_to_rest();
        sim_ride(dt);
        cd = cup_dist(&G.w);
        /* the tension zoom (9.2) */
        if (cd < 6.0f * BP_R && sp < 1.2f && !G.w.holed)
            bp_cam_set_mode(&G.cam, CAM_CUP, &G.w, cb->p, cup_pos(&G.w));
        else if (G.cam.mode == CAM_CUP && (cd > 10.0f * BP_R || sp > 2.0f))
            bp_cam_set_mode(&G.cam, CAM_RIDE, &G.w, cb->p, cup_pos(&G.w));

        /* slow-mo when capture looks certain (10.7) */
        if (!G.slowmo_armed && cd < 3.2f * BP_R && sp < 0.9f && cb->state != BS_GONE) {
            G.slowmo_armed = 1;
            bp_slowmo(0.55f, 0.30f);
        }
        if (cd > 8.0f * BP_R) G.slowmo_armed = 0;

        bp_roll(cb->state == BS_GONE ? 0.0f : sp, cb->surf);
        bp_render_trail(&G.w, G.shot.tx, G.shot.ty, dt);

        if (G.w.holed || bp_settled(&G.w) || G.ride_ticks >= RIDE_TICK_CAP) {
            bp_roll(0.0f, 0);
            bp_music_duck(0.0f);
            resolve_shot();
        }
        break;
    }

    case ST_RESULT:
        G.result_t += dt;
        if (G.pending_ace && G.result_t > 1.6f) {
            G.pending_ace = 0;
            bp_replay_seek(&G.rep, &G.rw, G.rep.n - 1);
            bp_shot_begin(&G.rw);
            bp_strike(&G.rw, G.rep.shot[G.rep.n - 1].aim, G.rep.shot[G.rep.n - 1].power,
                      G.rep.shot[G.rep.n - 1].tx, G.rep.shot[G.rep.n - 1].ty);
            G.replay_ticks = 0;
            G.acc = 0.0f;
            G.state = ST_REPLAY;
            bp_cam_init(&G.cam, G.rw.balls[0].p);
            bp_cam_set_mode(&G.cam, CAM_CINEMA, &G.rw, G.rw.balls[0].p, cup_pos(&G.rw));
            bp_banner("ACE", "replaying that one", 3.2f, (Color){ 255, 226, 130, 255 });
            break;
        }
        if (G.result_t > 2.8f || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            bp_banner("", "", 0.0f, (Color){ 255, 255, 255, 255 });
            G.state = ST_CARD;
        }
        break;

    case ST_REPLAY:
        G.acc += dt * 0.55f;
        while (G.acc >= BP_DT && G.replay_ticks < RIDE_TICK_CAP) {
            G.acc -= BP_DT;
            bp_step(&G.rw);
            G.rw.nev = 0;
            G.replay_ticks++;
            if (G.rw.holed) break;
        }
        bp_render_update(&G.rw, dt);
        if (G.rw.holed || bp_settled(&G.rw) || IsKeyPressed(KEY_SPACE) ||
            IsKeyPressed(KEY_ENTER)) {
            G.state = ST_CARD;
        }
        break;

    case ST_CARD:
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_TAB))
            advance_hole();
        break;

    default: break;
    }
}

/* ------------------------------------------------------------------ */

static void fill_hud(void)
{
    int i;
    G.hud.hole = G.hole;
    G.hud.par = BP_HOLES[G.hole].par;
    G.hud.strokes = G.strokes;
    G.hud.restarts = G.restarts;
    G.hud.name = BP_HOLES[G.hole].name;
    G.hud.brief = BP_HOLES[G.hole].brief;
    G.hud.from = G.from; G.hud.to = G.to;
    G.hud.power = G.shot.meter;
    G.hud.charging = G.shot.charging;
    G.hud.tx = G.shot.tx; G.hud.ty = G.shot.ty;
    G.hud.riding = (G.state == ST_RIDE);
    G.hud.sealed = G.w.cup_sealed;
    G.hud.aces = G.save.aces;
    G.hud.running = 0; G.hud.running_par = 0;
    for (i = G.from; i <= G.to; ++i) {
        G.hud.scores[i] = G.scores[i];
        if (G.scores[i] > 0) { G.hud.running += G.scores[i]; G.hud.running_par += BP_HOLES[i].par; }
    }
    for (i = 0; i < BP_NHOLES; ++i) {
        G.hud.scores[i] = G.scores[i];
        G.hud.bests[i] = G.save.best_hole[i];
    }
}

static void draw_final(int sw, int sh)
{
    char buf[160];
    int y = 96, par = 0, i;
    for (i = G.from; i <= G.to; ++i) par += BP_HOLES[i].par;
    ClearBackground((Color){ 9, 14, 24, 255 });
    bp_render_scorecard(&G.hud, sw, sh, 1);
    DrawRectangle(sw / 2 - 330, y + 424, 660, 122, (Color){ 10, 14, 24, 235 });
    DrawRectangleLines(sw / 2 - 330, y + 424, 660, 122, (Color){ 78, 96, 122, 255 });
    DrawText("BEST MOMENTS", sw / 2 - 310, y + 436, 20, (Color){ 246, 240, 210, 255 });
    if (G.best_dist_hole >= 0) {
        snprintf(buf, sizeof buf, "longest holed shot: %.1f m on hole %d",
                 G.best_dist, G.best_dist_hole + 1);
        DrawText(buf, sw / 2 - 310, y + 464, 16, (Color){ 190, 205, 225, 255 });
    }
    if (G.best_banks_hole >= 0) {
        snprintf(buf, sizeof buf, "most rails in one holed shot: %d on hole %d",
                 G.best_banks, G.best_banks_hole + 1);
        DrawText(buf, sw / 2 - 310, y + 486, 16, (Color){ 190, 205, 225, 255 });
    }
    snprintf(buf, sizeof buf, "holes in one this round: %d", G.aces_this_round);
    DrawText(buf, sw / 2 - 310, y + 508, 16,
             G.aces_this_round ? (Color){ 255, 226, 130, 255 } : (Color){ 140, 158, 185, 255 });
    (void)par;
}

static void draw_howto(int sw, int sh)
{
    static const char *L[] = {
        "It looks like mini golf. You are shooting pool.",
        "",
        "AIM      drag with the left mouse button, or A / D.  Hold SHIFT for fine aim.",
        "POWER    hold SPACE.  The meter climbs, then falls, then climbs. Let go.",
        "ENGLISH  arrow keys move the strike point on the cue-ball face (bottom left).",
        "         UP is follow, DOWN is draw, LEFT and RIGHT are side english.  C centres it.",
        "CAMERA   drag with the right mouse button or Q / E.  Wheel or Z / X to zoom.",
        "RIDE     R skips the ball straight to rest.  The result is identical.",
        "         TAB shows the card.  ESC pauses.",
        "",
        "The dotted guide shows geometry only: first contact and one centre-ball bounce.",
        "It never accounts for spin. Learning what english does is the game.",
        "",
        "SAFE ROUTE   every hole can be parred with centre ball and a steady hand.",
        "HERO LINE    every hole also hides a shorter line that needs a bank, a draw,",
        "             running english or a combo -- and that can cost you if you miss.",
        "",
        "Gold pockets pay one stroke back for an OBJECT ball. Your own ball in one is",
        "still a scratch. Greed cuts both ways.",
    };
    int i;
    ClearBackground((Color){ 9, 14, 24, 255 });
    DrawText("HOW TO PLAY", sw / 2 - 130, 54, 38, (Color){ 255, 206, 84, 255 });
    for (i = 0; i < (int)(sizeof(L) / sizeof(L[0])); ++i)
        DrawText(L[i], 90, 124 + i * 26, 18, (Color){ 196, 210, 230, 255 });
    DrawText("ESC / ENTER  back", 90, sh - 44, 17, (Color){ 120, 138, 165, 255 });
    (void)sw;
}

/* ------------------------------------------------------------------ */

static void menu_move(int *sel, int count)
{
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { *sel = (*sel + 1) % count; bp_sfx(SFX_UI, 1.0f, 0.4f); }
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) { *sel = (*sel + count - 1) % count; bp_sfx(SFX_UI, 1.0f, 0.4f); }
}

/* --tour walks every hole, plays a shot on each and screenshots it. It is a
 * smoke test for the renderer, not a game mode; nothing else reads it. */
static int tour = 0, tour_frame = 0, tour_hole = 0;

/* Play the whole card unattended: aim at the cup, guess a power from the
 * distance, jitter a little so nothing gets wedged. Verifies the strike ->
 * ride -> hole-out -> card loop as well as the renderer. */
static unsigned int tour_rs = 12345u;
static float tour_rnd(void)
{
    tour_rs ^= tour_rs << 13; tour_rs ^= tour_rs >> 17; tour_rs ^= tour_rs << 5;
    return ((float)(tour_rs & 0xffffu) / 32768.0f) - 1.0f;
}

static int tour_shot_wait = 0, tour_shots = 0, tour_grab = 0;

static void tour_step(void)
{
    char name[64];
    ++tour_frame;
    if (tour_frame < 5) return;
    if (tour_frame == 5) {
        TakeScreenshot("tour_title.png");
        begin_round(0, 17);
        tour_hole = 0;
        return;
    }

    if (G.state == ST_INTRO) {
        G.state = ST_AIM;
        bp_cam_set_mode(&G.cam, CAM_SURVEY, &G.w, G.w.balls[0].p, cup_pos(&G.w));
        tour_shot_wait = 4;
        return;
    }

    if (G.state == ST_AIM) {
        if (tour_shot_wait > 0) { --tour_shot_wait; return; }
        if (!tour_grab) {
            bp_shot_update_guide(&G.shot, &G.w);
            snprintf(name, sizeof name, "tour_%02d_aim.png", G.hole + 1);
            TakeScreenshot(name);
            tour_grab = 1;
            return;
        }
        {
            V3 target = cup_pos(&G.w);
            V3 d;
            float dist, v, p;
            if (G.w.cup_sealed) {           /* rack hole: shoot at the eight */
                int i;
                for (i = 0; i < G.w.nballs; ++i)
                    if (G.w.balls[i].kind == BALL_EIGHT && G.w.balls[i].state != BS_GONE)
                        target = G.w.balls[i].p;
            }
            d = v3sub(target, G.w.balls[0].p);
            dist = sqrtf(d.x * d.x + d.z * d.z);
            G.shot.aim = atan2f(d.x, d.z) + tour_rnd() * 0.06f;
            v = sqrtf(2.0f * BP_MU_ROLL * BP_G * (dist * 1.15f + 0.2f));
            p = powf(bp_clampf(v / BP_VMAX, 0.05f, 1.0f), 1.0f / BP_POW_EXP);
            p = bp_clampf(p + tour_rnd() * 0.05f, 0.08f, 1.0f);
            fire(p);
            ++tour_shots;
        }
        return;
    }

    if (G.state == ST_RESULT) {
        if (tour_grab != 2) {
            snprintf(name, sizeof name, "tour_%02d_result.png", G.hole + 1);
            TakeScreenshot(name);
            tour_grab = 2;
        }
        G.result_t += 0.2f;
        return;
    }

    if (G.state == ST_REPLAY) { G.acc += 0.05f; return; }

    if (G.state == ST_CARD) {
        if (tour_grab != 3) {
            snprintf(name, sizeof name, "tour_%02d_card.png", G.hole + 1);
            TakeScreenshot(name);
            tour_grab = 3;
            return;
        }
        tour_grab = 0;
        tour_shot_wait = 2;
        advance_hole();
        return;
    }

    /* TakeScreenshot grabs the frame that was already presented, so give the
     * final page a couple of frames to actually get drawn first. */
    if (G.state == ST_FINAL) {
        if (++tour_shot_wait > 3) { TakeScreenshot("tour_final.png"); tour = 2; }
    }
}

int main(int argc, char **argv)
{
    int sw, sh;
    int a;

    for (a = 1; a < argc; ++a)
        if (strcmp(argv[a], "--tour") == 0) tour = 1;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "BREAK PAR");
    SetTargetFPS(60);
    SetExitKey(0);

    memset(&G, 0, sizeof(G));
    bp_save_load(&G.save);
    bp_synth_init();
    push_volumes();
    bp_render_init();
    bp_juice_reset();
    if (G.save.fullscreen && !IsWindowFullscreen()) ToggleFullscreen();

    G.state = ST_TITLE;
    G.from = 0; G.to = 17;
    bp_course_build(&G.w, 0);
    bp_cam_init(&G.cam, G.w.balls[0].p);
    bp_music_mood(1);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;
        G.t += dt;

        bp_music_update();
        bp_juice_update(dt);

        if (tour) {
            tour_step();
            if (tour == 2) break;
            if (G.state == ST_RIDE) skip_to_rest();   /* keep the tour brisk */
        }

        /* ---- update ---- */
        switch (G.state) {
        case ST_TITLE:
            menu_move(&G.menu_sel, 4);
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                bp_sfx(SFX_UI, 1.4f, 0.5f);
                if (G.menu_sel == 0) { G.state = ST_SELECT; G.select_sel = 2; }
                else if (G.menu_sel == 1) { G.ret_state = ST_TITLE; G.state = ST_OPTIONS; G.opt_sel = 0; }
                else if (G.menu_sel == 2) { G.state = ST_HOWTO; }
                else break;
            }
            if (G.menu_sel == 3 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) goto done;
            /* idle orbit behind the tee of hole 1 */
            bp_cam_orbit(&G.cam, 0.10f * dt, 0.0f);
            bp_cam_update(&G.cam, &G.w, G.w.balls[0].p, v3zero(), 0.0f, dt);
            break;

        case ST_SELECT:
            menu_move(&G.select_sel, 3);
            if (IsKeyPressed(KEY_ESCAPE)) { G.state = ST_TITLE; }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                bp_sfx(SFX_UI, 1.4f, 0.5f);
                if (G.select_sel == 0) begin_round(0, 8);
                else if (G.select_sel == 1) begin_round(9, 17);
                else begin_round(0, 17);
            }
            break;

        case ST_HOWTO:
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                G.state = ST_TITLE;
            break;

        case ST_FINAL:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
                G.state = ST_TITLE;
                bp_course_build(&G.w, 0);
                bp_cam_init(&G.cam, G.w.balls[0].p);
                bp_music_mood(1);
            }
            break;

        case ST_PAUSE:
            menu_move(&G.pause_sel, 4);
            if (IsKeyPressed(KEY_ESCAPE)) { G.state = G.ret_state; bp_music_duck(0.0f); }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                bp_sfx(SFX_UI, 1.4f, 0.5f);
                if (G.pause_sel == 0) { G.state = G.ret_state; }
                else if (G.pause_sel == 1) {
                    /* restarting zeroes this hole's strokes; the card flags it */
                    G.restarts++;
                    G.seen_intro[G.hole] = 1;
                    start_hole(G.hole, 1);
                } else if (G.pause_sel == 2) {
                    G.ret_state = ST_PAUSE; G.state = ST_OPTIONS; G.opt_sel = 0;
                } else {
                    G.state = ST_TITLE;
                    bp_course_build(&G.w, 0);
                    bp_cam_init(&G.cam, G.w.balls[0].p);
                }
            }
            break;

        case ST_OPTIONS: {
            int *v = NULL;
            menu_move(&G.opt_sel, 5);
            if (G.opt_sel == 0 || G.opt_sel == 1 || G.opt_sel == 2) {
                static int tmp;
                tmp = (G.opt_sel == 0) ? G.save.vol_master
                    : (G.opt_sel == 1) ? G.save.vol_music : G.save.vol_sfx;
                v = &tmp;
                if (IsKeyPressed(KEY_LEFT) && *v > 0) (*v)--;
                if (IsKeyPressed(KEY_RIGHT) && *v < 10) (*v)++;
                if (G.opt_sel == 0) G.save.vol_master = (unsigned char)*v;
                if (G.opt_sel == 1) G.save.vol_music = (unsigned char)*v;
                if (G.opt_sel == 2) G.save.vol_sfx = (unsigned char)*v;
                push_volumes();
            } else if (G.opt_sel == 3) {
                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
                    IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                    G.save.fullscreen = (unsigned char)!G.save.fullscreen;
                    ToggleFullscreen();
                    bp_sfx(SFX_UI, 1.2f, 0.5f);
                }
            }
            if (IsKeyPressed(KEY_ESCAPE) ||
                (G.opt_sel == 4 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))) {
                bp_save_store(&G.save);
                G.state = G.ret_state ? G.ret_state : ST_TITLE;
            }
            break;
        }

        default:  /* the playing states */
            if (IsKeyPressed(KEY_ESCAPE)) {
                G.ret_state = G.state;
                G.state = ST_PAUSE;
                G.pause_sel = 0;
                bp_music_duck(0.4f);
                break;
            }
            if (IsKeyPressed(KEY_TAB) && (G.state == ST_AIM || G.state == ST_RIDE)) {
                G.ret_state = G.state;
                G.state = ST_CARD;
                break;
            }
            update_play(dt);
            if (G.state != ST_REPLAY) {
                bp_render_update(&G.w, dt);
                bp_cam_update(&G.cam, &G.w, G.w.balls[0].p, G.w.balls[0].v,
                              cup_dist(&G.w), dt);
            } else {
                bp_cam_update(&G.cam, &G.rw, G.rw.balls[0].p, G.rw.balls[0].v, 0.0f, dt);
            }
            if (G.shot.strike_anim > 0.0f)
                G.shot.strike_anim = bp_clampf(G.shot.strike_anim - dt * 6.0f, 0.0f, 1.0f);
            break;
        }

        fill_hud();

        /* ---- draw ---- */
        sw = GetScreenWidth();
        sh = GetScreenHeight();
        BeginDrawing();
        {
            BpPalette pal = bp_palette(G.hole);
            if (G.state == ST_TITLE) {
                ClearBackground(pal.sky);
            } else if (G.state == ST_HOWTO || G.state == ST_FINAL) {
                ClearBackground((Color){ 9, 14, 24, 255 });
            } else {
                Color deep = { (unsigned char)(pal.sky.r * 0.45f),
                               (unsigned char)(pal.sky.g * 0.45f),
                               (unsigned char)(pal.sky.b * 0.55f), 255 };
                ClearBackground(pal.sky);
                DrawRectangleGradientV(0, 0, sw, sh / 2, deep, pal.sky);
                DrawRectangleGradientV(0, sh / 2, sw, sh - sh / 2, pal.sky, pal.horizon);
            }
        }

        if (G.state == ST_TITLE) {
            bp_render_title(sw, sh, G.menu_sel, G.t, &G.hud);
        } else if (G.state == ST_SELECT) {
            bp_render_select(sw, sh, G.select_sel, G.t);
        } else if (G.state == ST_HOWTO) {
            draw_howto(sw, sh);
        } else if (G.state == ST_FINAL) {
            draw_final(sw, sh);
        } else {
            const BpWorld *world = (G.state == ST_REPLAY) ? &G.rw : &G.w;
            Camera3D cam = bp_cam_raylib(&G.cam);
            int show_cue = (G.state == ST_AIM);
            BeginMode3D(cam);
            bp_render_world(world, G.hole, &G.shot, show_cue, show_cue, G.t);
            EndMode3D();

            if (G.cam.warp_blur > 0.01f) {
                DrawRectangle(0, 0, sw, sh,
                              (Color){ 120, 240, 255, (unsigned char)(G.cam.warp_blur * 90.0f) });
            }

            if (G.state == ST_CARD) {
                bp_render_scorecard(&G.hud, sw, sh, 0);
            } else if (G.state == ST_PAUSE) {
                bp_render_hud(world, &G.hud, sw, sh);
                bp_render_pause(sw, sh, G.pause_sel, &G.hud);
            } else if (G.state == ST_OPTIONS) {
                bp_render_options(sw, sh, G.opt_sel, G.save.vol_master, G.save.vol_music,
                                  G.save.vol_sfx, G.save.fullscreen);
            } else {
                bp_render_hud(world, &G.hud, sw, sh);
                if (G.state == ST_INTRO) {
                    const char *s = BP_HOLES[G.hole].brief;
                    int w = MeasureText(s, 24);
                    DrawRectangle(sw / 2 - w / 2 - 18, sh - 140, w + 36, 40,
                                  (Color){ 8, 12, 20, 200 });
                    DrawText(s, sw / 2 - w / 2, sh - 132, 24, (Color){ 236, 242, 252, 255 });
                    DrawText("SPACE  skip", sw / 2 - 40, sh - 92, 16,
                             (Color){ 140, 158, 185, 255 });
                }
            }
            bp_juice_draw_hud(sw, sh);
        }

        EndDrawing();
    }

done:
    bp_save_store(&G.save);
    bp_synth_shutdown();
    CloseWindow();
    return 0;
}
