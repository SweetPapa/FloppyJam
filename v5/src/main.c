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
#include "scenery.h"
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

#define RIDE_TICK_CAP BP_RIDE_TICKS   /* shared with the preview (core.h) */

typedef struct {
    BpWorld  w;
    BpShot   shot;
    BpCam    cam;
    BpHud    hud;
    BpSave   save;
    BpReplay rep;

    int   state, ret_state;    /* ret_state: where OPTIONS returns to        */
    int   pause_from;          /* the play state PAUSE / the card overlay resumes to */
    int   card_overlay;        /* TAB scorecard over play, vs the between-holes card  */
    int   menu_sel, pause_sel, opt_sel, select_sel;
    int   hole, from, to;
    int   strokes, restarts, total;
    int   scores[BP_NHOLES];
    int   seen_intro[BP_NHOLES];
    int   tips_left;

    float acc;                     /* physics accumulator                 */
    int   ride_ticks;
    int   rimming, rimmed;
    int   banks_said;              /* highest rail count already called out */
    int   slowmo_armed;
    int   pending_ace;
    float result_t;
    float pow_flash, pow_value;    /* strike readout by the meter         */

    /* trajectory preview (optional assist) */
    BpPreview preview;
    BpPlanner planner;
    int   preview_dirty;
    float preview_settle;
    float last_aim, last_tx, last_ty;
    float plan_power;
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

/* What the player is actually trying to reach: the cup, or the 8-ball while a
 * rack hole is still sealed (7.4). Used by the auto-focus / auto-aim keys. */
static V3 objective_pos(const BpWorld *w)
{
    if (w->cup_sealed) {
        int i;
        for (i = 0; i < w->nballs; ++i)
            if (w->balls[i].kind == BALL_EIGHT && w->balls[i].state != BS_GONE)
                return w->balls[i].p;
    }
    return cup_pos(w);
}

/* Swing the camera behind the ball looking toward the objective and zoom out
 * far enough to see the whole line — the "show me the hole" key. */
static void focus_camera_on_objective(void)
{
    V3 ball = G.w.balls[0].p, obj = objective_pos(&G.w);
    V3 d = v3sub(obj, ball);
    float dist = sqrtf(d.x * d.x + d.z * d.z);
    G.cam.yaw = atan2f(d.x, d.z) + BP_PI;    /* forward_yaw looks along d */
    G.cam.pitch = 52.0f * BP_DEG;            /* high enough to clear rails */
    /* zoom out with distance so the far cup stays in shot */
    G.cam.zoom = (dist > 8.0f) ? 3 : (dist > 4.0f) ? 2 : 1;
}

/* Point the cue at the objective, and bring the camera round to match. */
static void aim_at_objective(void)
{
    V3 ball = G.w.balls[0].p, obj = objective_pos(&G.w);
    V3 d = v3sub(obj, ball);
    if (d.x * d.x + d.z * d.z > 1e-6f) G.shot.aim = atan2f(d.x, d.z);
    focus_camera_on_objective();
}

/* The menu backdrop is a live hero shot of hole 1: wide, low and slowly
 * turning, framed so the skyline sits across the top half. It deliberately
 * ignores bp_cam_update — the play camera hugs the ball and ducks behind
 * rails, and neither is what you want a title screen doing. */
static void menu_camera(float dt)
{
    V3 lo, hi, c, dir;
    float p, y;
    bp_course_bounds(&G.w, &lo, &hi);
    c = v3(0.5f * (lo.x + hi.x), hi.y + 0.30f, 0.5f * (lo.z + hi.z));
    G.cam.yaw += 0.075f * dt;
    G.cam.pitch = 15.0f * BP_DEG;
    G.cam.mode = CAM_SURVEY;
    G.cam.t += dt;
    G.cam.target = c;
    G.cam.eye_dist = 0.55f * ((hi.x - lo.x) + (hi.z - lo.z)) + 5.0f;
    p = G.cam.pitch; y = G.cam.yaw;
    dir = v3(sinf(y) * cosf(p), sinf(p), cosf(y) * cosf(p));
    G.cam.pos = v3add(c, v3mul(dir, G.cam.eye_dist));
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
            /* A bank is a decision that paid off, so say so from the third one
             * on. The count is the sim's, not a guess. */
            if (e->a == 0 && G.w.wall_hits >= 3 && G.w.wall_hits != G.banks_said) {
                char b[16];
                G.banks_said = G.w.wall_hits;
                snprintf(b, sizeof b, "%d RAILS", G.w.wall_hits);
                bp_popup(e->at, b, (Color){ 150, 220, 255, 255 }, 0);
                bp_sfx(SFX_CHIME, 1.3f + 0.06f * (float)G.w.wall_hits, 0.22f);
                bp_sparkle(e->at, 5, (Color){ 170, 226, 255, 255 }, 0.05f, 0.4f);
            }
            break;
        case EV_BUMPER: {
            int n = (e->b < 0) ? 0 : (e->b % 5);
            bp_sfx(SFX_BOING0 + n, bp_clampf(0.9f + m * 0.05f, 0.7f, 1.6f),
                   bp_clampf(0.30f + m * 0.10f, 0.0f, 0.95f));
            bp_shockwave(e->at, (Color){ 255, 170, 210, 220 }, 0.10f);
            bp_burst(e->at, 8, (Color){ 255, 150, 200, 255 }, m * 0.45f, 0.35f, 5.0f);
            bp_sparkle(e->at, 6, (Color){ 255, 190, 226, 255 }, 0.06f, 0.45f);
            bp_edge_pulse((Color){ 255, 110, 180, 255 },
                          bp_clampf(0.18f + m * 0.07f, 0.0f, 0.55f));
            G.cam.shake = bp_clampf(G.cam.shake + 0.25f, 0.0f, 1.0f);
            break;
        }
        case EV_BALLHIT:
            bp_sfx(SFX_CLACK, bp_clampf(0.85f + m * 0.10f, 0.6f, 2.0f),
                   bp_clampf(0.20f + m * 0.18f, 0.0f, 0.95f));
            bp_burst(e->at, 5, (Color){ 255, 255, 240, 255 }, m * 0.35f, 0.18f, 6.0f);
            bp_sparkle(e->at, 3, (Color){ 255, 250, 220, 255 }, 0.04f, 0.30f);
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
            bp_sfx(SFX_CHIME, 1.0f, 0.55f);
            bp_burst(e->at, 20, (Color){ 140, 255, 200, 255 }, 1.8f, 0.6f, 3.0f);
            bp_sparkle(e->at, 16, (Color){ 170, 255, 220, 255 }, 0.10f, 0.8f);
            bp_edge_pulse((Color){ 120, 255, 190, 255 }, 0.5f);
            bp_popup(e->at, "GATE OPEN", (Color){ 150, 255, 205, 255 }, 1);
            bp_toast("GATE OPEN", 1.8f);
            break;
        case EV_VOID:
            bp_sfx(SFX_SPLASH, 1.0f, 0.7f);
            bp_flash((Color){ 120, 60, 200, 255 }, 0.22f);
            bp_edge_pulse((Color){ 140, 80, 230, 255 }, 0.9f);
            bp_burst(e->at, 18, (Color){ 130, 180, 255, 255 }, 1.4f, 0.7f, 6.0f);
            break;
        case EV_POCKET: {
            int pk = e->b;
            if (pk < 0 || pk >= G.w.npockets) break;
            switch (G.w.pockets[pk].kind) {
            case PK_CUP:
                bp_sfx(SFX_SWALLOW, 1.0f, 0.9f);
                bp_sparkle(e->at, 22, (Color){ 255, 240, 190, 255 }, 0.10f, 0.9f);
                bp_edge_pulse((Color){ 255, 226, 150, 255 }, 0.8f);
                break;
            case PK_WARP:
                bp_sfx(SFX_WARP, 1.0f, 0.7f);
                G.cam.warp_blur = 1.0f;
                bp_burst(e->at, 16, (Color){ 120, 240, 255, 255 }, 1.4f, 0.5f, 2.0f);
                bp_sparkle(e->at, 14, (Color){ 150, 245, 255, 255 }, 0.09f, 0.7f);
                bp_edge_pulse((Color){ 110, 235, 255, 255 }, 0.7f);
                break;
            case PK_BONUS:
                if (e->a == 0) { bp_sfx(SFX_SCRATCH, 1.0f, 0.85f); }
                else {
                    bp_sfx(SFX_COIN, 1.0f, 0.85f);
                    bp_sfx(SFX_UNCAP, 1.0f, 0.55f);
                    bp_confetti(e->at, 26, 0.7f);
                    bp_sparkle(e->at, 18, (Color){ 255, 216, 110, 255 }, 0.10f, 0.9f);
                    bp_popup(e->at, "-1 STROKE", (Color){ 255, 222, 120, 255 }, 1);
                    bp_edge_pulse((Color){ 255, 208, 74, 255 }, 0.75f);
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
                bp_sfx(SFX_CHIME, 0.8f, 0.6f);
                bp_toast("THE CUP IS OPEN", 2.4f);
                bp_confetti(cup_pos(&G.w), 30, 0.8f);
                bp_sparkle(cup_pos(&G.w), 24, (Color){ 180, 255, 220, 255 }, 0.14f, 1.1f);
                bp_popup(cup_pos(&G.w), "OPEN", (Color){ 150, 255, 205, 255 }, 1);
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
    bp_scenery_build(&G.w, index);
    bp_juice_reset();
    if (fresh) { G.strokes = 0; }
    G.ride_ticks = 0;
    G.rimming = G.rimmed = 0;
    G.banks_said = 0;
    G.pending_ace = 0;
    G.acc = 0.0f;
    bp_shot_reset(&G.shot, 0);            /* english resets each hole (4.4) */
    G.shot.aim = 0.0f;
    bp_cam_init(&G.cam, G.w.balls[0].p);
    G.hud.sealed = G.w.cup_sealed;
    G.pow_flash = 0.0f;
    G.card_overlay = 0;
    G.preview.valid = 0;
    G.preview_dirty = 1;
    G.preview_settle = 0.0f;
    G.plan_power = 0.5f;
    G.planner.done = 1;
    G.last_aim = G.shot.aim + 1.0f;      /* force a first recompute */
    /* A settled breeze per hole: deterministic from the index so a hole always
     * feels the same, and cosmetic only so it can never affect a shot. */
    bp_wind_set((float)(((unsigned)index * 2654435761u) % 628u) * 0.01f,
                0.25f + 0.65f * (float)(((unsigned)index * 40503u) % 97u) / 96.0f);
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

    /* The room reacts in proportion. Par gets a horn stab; under par gets the
     * stab plus applause; an ace brings the house down. Over par gets the
     * banner and nothing else, which is its own kind of feedback. */
    {
        int d = s - H->par;
        if (d <= 0) {
            bp_sfx(SFX_STAB, d <= -2 ? 1.18f : 1.0f, d < 0 ? 0.85f : 0.6f);
            bp_popup(cup_pos(&G.w), bp_score_name(s, H->par), tint, 1);
            bp_sparkle(cup_pos(&G.w), d < 0 ? 34 : 18, tint, 0.16f, 1.2f);
            bp_edge_pulse(tint, d < 0 ? 0.9f : 0.5f);
        }
        if (d < 0 || s == 1)
            bp_sfx(SFX_CROWD, s == 1 ? 1.0f : 1.12f, s == 1 ? 0.85f : 0.55f);
    }

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
        /* the punishment reads at the edge of the frame, not across the table:
         * a full-screen red wash hid the very geometry you need to see to
         * work out what you just did wrong */
        bp_flash((Color){ 255, 60, 60, 255 }, 0.30f);
        bp_edge_pulse((Color){ 255, 60, 60, 255 }, 1.0f);
        bp_toast("SCRATCH!  +1 STROKE", 2.2f);
    } else if (G.rimming && !G.w.holed) {
        bp_sfx(SFX_SAD, 1.0f, 0.55f);
        bp_toast("RIMMED OUT", 1.8f);
    }

    if (G.strokes >= BP_STROKE_CAP) { G.strokes = BP_STROKE_CAP; finish_hole(); return; }

    bp_shot_reset(&G.shot, 1);            /* leaving english dialled is the trap */
    G.preview.valid = 0;
    G.preview_dirty = 1;
    G.preview_settle = 0.0f;
    G.planner.done = 1;
    G.last_aim = G.shot.aim + 1.0f;
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
    G.banks_said = 0;
    G.acc = 0.0f;
    G.shot.strike_anim = 1.0f;
    G.shot.cue_pull = 0.0f;
    G.state = ST_RIDE;
    bp_cam_set_mode(&G.cam, CAM_RIDE, &G.w, G.w.balls[0].p, cup_pos(&G.w));

    /* Strike feedback scales continuously with power so you can feel how hard
     * you hit it with your eyes shut (16.1). Brightness, hitstop, shockwave
     * size, dust, camera kick and the sub-thump layer all track it. */
    bp_sfx(SFX_CRACK, bp_clampf(0.72f + power * 0.72f, 0.5f, 1.9f),
           bp_clampf(0.35f + power * 0.60f, 0.0f, 1.0f));
    if (power > 0.42f)                       /* body under the crack */
        bp_sfx(SFX_THUMP, bp_clampf(0.42f + power * 0.22f, 0.35f, 0.9f),
               bp_clampf((power - 0.42f) * 0.85f, 0.0f, 0.55f));
    /* One crisp frame of hitstop, tops. The old 18-66 ms froze the ball for
     * up to four frames after release, which read as the strike being laggy
     * and disconnected from the key. The ball now leaves on the next frame. */
    bp_hitstop(power > 0.5f ? 0.016f : 0.0f);
    bp_shockwave(G.w.balls[0].p, (Color){ 255, 245, 220, 210 }, 0.05f + power * 0.11f);
    bp_burst(G.w.balls[0].p, 3 + (int)(power * 14.0f),
             (Color){ 236, 240, 228, 255 }, 0.35f + power * 1.9f, 0.22f + power * 0.22f, 5.5f);
    G.cam.shake = bp_clampf(G.cam.shake + power * power * 1.15f, 0.0f, 1.0f);
    if (power > 0.78f) bp_flash((Color){ 255, 248, 226, 255 }, (power - 0.78f) * 1.4f);

    /* on-screen readout by the power meter: a word plus the number */
    G.pow_flash = 1.0f;
    G.pow_value = power;
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

/* Aiming is detented: every DETENT of rotation lands with an audible and
 * visible click, so a player can count clicks to a line instead of eyeballing
 * a continuous slider. Fine aim subdivides the same detent.
 *
 * Sizes chosen for precision at range: a 0.60 deg detent 14 m from the cup is
 * ~15 cm of aim at the target (was 26 cm), and fine at 0.10 deg is ~2.5 cm —
 * tight enough to thread a gap or split a ball on the far side of the hole. */
#define AIM_DETENT      (0.60f * BP_DEG)
#define AIM_DETENT_FINE (0.10f * BP_DEG)

static float aim_click_acc = 0.0f;

static void aim_click(float delta, int fine)
{
    float step = fine ? AIM_DETENT_FINE : AIM_DETENT;
    G.shot.aim += delta;
    aim_click_acc += fabsf(delta);
    while (aim_click_acc >= step) {
        aim_click_acc -= step;
        bp_sfx(SFX_TICK, fine ? 1.55f : 1.0f, fine ? 0.10f : 0.16f);
        G.shot.aim_tick = 1.0f;
    }
}

/* Camera: mouse drag, plus Q/E swing and Z/X zoom on the keyboard. Vertical
 * framing is handled by mouse drag and the auto-focus keys, freeing W/S for
 * english. */
static void camera_input(float dt)
{
    float wheel;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
        IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 d = GetMouseDelta();
        bp_cam_orbit(&G.cam, d.x * 0.005f, -d.y * 0.0035f);
    }
    if (IsKeyDown(KEY_Q)) bp_cam_orbit(&G.cam, -1.5f * dt, 0.0f);
    if (IsKeyDown(KEY_E)) bp_cam_orbit(&G.cam,  1.5f * dt, 0.0f);
    wheel = GetMouseWheelMove();
    if (wheel > 0.5f)  { bp_cam_zoom(&G.cam, -1); bp_sfx(SFX_TICK, 1.3f, 0.16f); }
    if (wheel < -0.5f) { bp_cam_zoom(&G.cam,  1); bp_sfx(SFX_TICK, 1.1f, 0.16f); }
    if (IsKeyPressed(KEY_Z)) { bp_cam_zoom(&G.cam, -1); bp_sfx(SFX_TICK, 1.3f, 0.16f); }
    if (IsKeyPressed(KEY_X)) { bp_cam_zoom(&G.cam,  1); bp_sfx(SFX_TICK, 1.1f, 0.16f); }
    /* V lines the camera up behind the current aim — the "get me square" key */
    if (IsKeyPressed(KEY_V)) {
        G.cam.yaw = G.shot.aim + BP_PI;
        bp_sfx(SFX_UI, 1.3f, 0.35f);
    }
}

static void aim_input(float dt)
{
    int fine = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    /* 0.35 rad/s coarse is ~20 deg/s: unhurried, so the finer detents don't
     * make a full swing take forever. Fine aim is ~7x slower again (4.1). */
    float rate = fine ? 0.050f : 0.35f;
    float step = fine ? AIM_DETENT_FINE : AIM_DETENT;
    /* invert just flips which arrow turns the cue which way, for players who
     * read left/right the other round. `left` is the sign applied to LEFT. */
    float left = G.save.invert_aim ? 1.0f : -1.0f;
    float p;

    /* AIM is on the arrow keys: LEFT / RIGHT rotate the cue, detented. */
    if (IsKeyDown(KEY_LEFT))  aim_click(left * rate * dt, fine);
    if (IsKeyDown(KEY_RIGHT)) aim_click(-left * rate * dt, fine);
    if (IsKeyPressed(KEY_LEFT))  aim_click(left * step, fine);
    if (IsKeyPressed(KEY_RIGHT)) aim_click(-left * step, fine);

    /* UP  focuses the camera on the hole and zooms out for a clear line.
     * DOWN also swings the cue round to point straight at it. */
    if (IsKeyPressed(KEY_UP))   { focus_camera_on_objective(); bp_sfx(SFX_UI, 1.2f, 0.4f); }
    if (IsKeyPressed(KEY_DOWN)) { aim_at_objective();          bp_sfx(SFX_UI, 1.4f, 0.4f); }

    camera_input(dt);

    /* english on the cue-ball face: A/D move the strike point left and right,
     * W/S move it up (follow) and down (draw). */
    {
        float s = 1.35f * dt;
        if (IsKeyDown(KEY_W)) G.shot.ty += s;   /* follow */
        if (IsKeyDown(KEY_S)) G.shot.ty -= s;   /* draw   */
        if (IsKeyDown(KEY_A)) G.shot.tx -= s;   /* left   */
        if (IsKeyDown(KEY_D)) G.shot.tx += s;   /* right  */
        if (IsKeyPressed(KEY_C))  { G.shot.tx = G.shot.ty = 0.0f; bp_sfx(SFX_UI, 1.2f, 0.3f); }
        {
            float mag = sqrtf(G.shot.tx * G.shot.tx + G.shot.ty * G.shot.ty);
            if (mag > BP_TIP_MAX) {
                G.shot.tx *= BP_TIP_MAX / mag;
                G.shot.ty *= BP_TIP_MAX / mag;
            }
        }
    }

    if (IsKeyPressed(KEY_P)) {
        G.save.preview = (unsigned char)!G.save.preview;
        G.preview_dirty = 1;
        G.preview_settle = 1.0f;
        G.preview.valid = 0;
        bp_sfx(SFX_UI, G.save.preview ? 1.5f : 0.9f, 0.5f);
        bp_toast(G.save.preview ? "TRAJECTORY PREVIEW ON"
                                : "TRAJECTORY PREVIEW OFF", 1.8f);
        bp_save_store(&G.save);
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

/* The preview is exact, not estimated: it replays the shot on a copy of the
 * world with the real solver. Recompute policy keeps it cheap —
 *   charging   : every frame, at the power you would release at right now;
 *   aim moving : every frame, at the last planned power, so the line tracks;
 *   aim still  : once, a ~20-sim search for the power that finishes nearest
 *                the objective. That is the "hit it optimally" line. */
static void update_preview(float dt)
{
    int changed;
    if (!G.save.preview) { G.preview.valid = 0; return; }

    changed = (G.shot.aim != G.last_aim) || (G.shot.tx != G.last_tx) ||
              (G.shot.ty != G.last_ty);
    if (changed) {
        G.last_aim = G.shot.aim; G.last_tx = G.shot.tx; G.last_ty = G.shot.ty;
        G.preview_settle = 0.0f;
        G.preview_dirty = 1;
    } else {
        G.preview_settle += dt;
    }

    if (G.shot.charging) {
        /* live: the exact path for the power you would release at right now */
        bp_predict(&G.w, G.shot.aim, G.shot.meter, G.shot.tx, G.shot.ty, &G.preview);
        G.preview_dirty = 1;          /* re-plan once the meter is released */
        G.planner.done = 1;
    } else if (changed || !G.preview.valid) {
        /* the aim is moving: keep the line glued to it at the planned power */
        bp_predict(&G.w, G.shot.aim, G.plan_power, G.shot.tx, G.shot.ty, &G.preview);
    } else if (G.preview_dirty && G.preview_settle > 0.18f) {
        /* the aim has settled: hunt for the best power a few candidates per
         * frame so the search never costs a dropped frame */
        if (G.planner.done || G.planner.aim != G.shot.aim)
            bp_plan_begin(&G.planner, G.shot.aim, G.shot.tx, G.shot.ty);
        if (bp_plan_step(&G.w, &G.planner, 3, &G.preview)) {
            G.plan_power = G.planner.best_p;
            G.preview_dirty = 0;
        }
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
        update_preview(dt);
        bp_music_duck(0.0f);
        bp_roll(0.0f, 0);
        bp_speedlines(0.0f);
        break;

    case ST_RIDE: {
        float cd;
        {   /* let the player reframe while the ball is out there */
            float wheel = GetMouseWheelMove();
            if (wheel > 0.5f)  bp_cam_zoom(&G.cam, -1);
            if (wheel < -0.5f) bp_cam_zoom(&G.cam,  1);
            if (IsKeyPressed(KEY_Z)) bp_cam_zoom(&G.cam, -1);
            if (IsKeyPressed(KEY_X)) bp_cam_zoom(&G.cam,  1);
        }
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
        /* streaks only once the ball is genuinely quick, so a lag putt does
         * not get dressed up as a rocket */
        bp_speedlines(bp_clampf((sp - 2.6f) * 0.18f, 0.0f, 1.0f));

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
            G.card_overlay = 0;
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
        if (G.card_overlay) {
            /* the TAB scorecard popped over a shot: dismiss it, don't advance */
            if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE) ||
                IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                G.card_overlay = 0;
                G.state = G.pause_from;
                G.shot.charge_lock = 1;
            }
        } else if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
                   IsKeyPressed(KEY_TAB)) {
            advance_hole();
        }
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
    G.hud.pow_flash = G.pow_flash;
    G.hud.pow_value = G.pow_value;
    G.hud.aim_tick = G.shot.aim_tick;
    G.hud.pow_value = G.pow_value;
    G.hud.aim_tick = G.shot.aim_tick;
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
    DrawRectangle(0, 0, sw, sh, (Color){ 6, 7, 17, 160 });
    bp_render_scorecard(&G.hud, sw, sh, 1);
    bp_render_panel(sw / 2 - 330, y + 424, 660, 122);
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
        "AIM      LEFT / RIGHT arrows.  Every click is one detent; count clicks to a line.",
        "         Tap for one click, hold to sweep.  SHIFT for fine detents.",
        "         OPTIONS can swap LEFT / RIGHT if you read it the other way.",
        "         UP focuses the camera on the hole; DOWN also aims the cue straight at it.",
        "POWER    hold SPACE.  The meter climbs, then falls, then climbs. Let go.",
        "ENGLISH  A / D move the strike point left and right on the cue-ball face,",
        "         W / S move it up (follow) and down (draw).  C centres it.",
        "CAMERA   the mouse is camera only -- drag any button to orbit, wheel to zoom.",
        "         Q / E swing round, Z / X zoom, V squares up behind your aim.",
        "RIDE     R skips the ball straight to rest.  The result is identical.",
        "         TAB shows the card.  ESC pauses.",
        "PREVIEW  P draws the full predicted path of the shot (also in OPTIONS).",
        "         Off by default.  It is exact, not a guess -- the game replays",
        "         your shot on a copy of the world to draw it.",
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
    DrawRectangle(0, 0, sw, sh, (Color){ 6, 7, 17, 160 });
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
        G.save.preview = 1;          /* the tour exercises the assist too */
        begin_round(0, 17);
        tour_hole = 0;
        return;
    }

    if (G.state == ST_INTRO) {
        G.state = ST_AIM;
        bp_cam_set_mode(&G.cam, CAM_SURVEY, &G.w, G.w.balls[0].p, cup_pos(&G.w));
        tour_shot_wait = 22;         /* long enough for the optimal-power search */
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
    bp_scenery_build(&G.w, 0);
    bp_cam_init(&G.cam, G.w.balls[0].p);
    bp_music_mood(3);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;
        G.t += dt;

        bp_music_update();
        bp_juice_update(dt);
        bp_scenery_update(dt);
        /* decay the transient readouts, and let the breeze blow while playing */
        if (G.pow_flash > 0.0f) G.pow_flash = bp_clampf(G.pow_flash - dt * 1.15f, 0.0f, 1.0f);
        if (G.shot.aim_tick > 0.0f)
            G.shot.aim_tick = bp_clampf(G.shot.aim_tick - dt * 7.0f, 0.0f, 1.0f);
        if (G.state >= ST_INTRO && G.state <= ST_RESULT)
            bp_wind_motes(G.cam.target, dt);

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
            menu_camera(dt);
            break;

        case ST_SELECT:
            menu_move(&G.select_sel, 3);
            menu_camera(dt);
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
            menu_camera(dt);
            break;

        case ST_FINAL:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
                G.state = ST_TITLE;
                bp_course_build(&G.w, 0);
                bp_scenery_build(&G.w, 0);
                bp_cam_init(&G.cam, G.w.balls[0].p);
                bp_music_mood(3);
            }
            menu_camera(dt);
            break;

        case ST_PAUSE:
            menu_move(&G.pause_sel, 4);
            /* resume to the state we paused FROM, not ret_state — ret_state is
             * clobbered to ST_PAUSE if OPTIONS was opened from here, which is
             * what used to leave RESUME stuck in the pause menu. */
            if (IsKeyPressed(KEY_ESCAPE)) {
                G.state = G.pause_from; G.shot.charge_lock = 1; bp_music_duck(0.0f);
            }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                bp_sfx(SFX_UI, 1.4f, 0.5f);
                if (G.pause_sel == 0) { G.state = G.pause_from; G.shot.charge_lock = 1; }
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
                    bp_scenery_build(&G.w, 0);
                    bp_cam_init(&G.cam, G.w.balls[0].p);
                }
            }
            break;

        case ST_OPTIONS: {
            int *v = NULL;
            menu_move(&G.opt_sel, 7);
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
            } else if (G.opt_sel == 4) {
                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
                    IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                    G.save.preview = (unsigned char)!G.save.preview;
                    G.preview_dirty = 1;
                    G.preview.valid = 0;
                    bp_sfx(SFX_UI, 1.2f, 0.5f);
                }
            } else if (G.opt_sel == 5) {
                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
                    IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                    G.save.invert_aim = (unsigned char)!G.save.invert_aim;
                    bp_sfx(SFX_UI, 1.2f, 0.5f);
                }
            }
            if (IsKeyPressed(KEY_ESCAPE) ||
                (G.opt_sel == 6 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))) {
                bp_save_store(&G.save);
                G.state = G.ret_state ? G.ret_state : ST_TITLE;
            }
            break;
        }

        default:  /* the playing states */
            if (IsKeyPressed(KEY_ESCAPE)) {
                G.pause_from = G.state;
                G.state = ST_PAUSE;
                G.pause_sel = 0;
                bp_music_duck(0.4f);
                break;
            }
            if (IsKeyPressed(KEY_TAB) && (G.state == ST_AIM || G.state == ST_RIDE)) {
                G.pause_from = G.state;
                G.card_overlay = 1;
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
            /* Every screen in the game sits in front of the same live city —
             * title, round select, how-to and the final card included. The
             * alternative was one flat black page in the middle of a neon
             * round, which read as a different game. The pages that are not
             * mid-round show hole 1; the final card keeps the hole you just
             * finished, since that is the table still standing there. */
            int page = (G.state == ST_TITLE || G.state == ST_SELECT ||
                        G.state == ST_HOWTO || G.state == ST_FINAL);
            int hole = (page && G.state != ST_FINAL) ? 0 : G.hole;
            BpPalette pal = bp_palette(hole);
            const BpWorld *world = (G.state == ST_REPLAY) ? &G.rw : &G.w;
            Camera3D cam = bp_cam_raylib(&G.cam);
            int show_cue = (G.state == ST_AIM);
            /* With the full preview on, the short geometry guide is just a
             * second line saying less, so it steps aside. */
            int show_guide = show_cue && !(G.save.preview && G.preview.valid);
            ClearBackground(pal.sky);
            BeginMode3D(cam);
            bp_scenery_draw(&G.cam, &pal, G.t);
            bp_render_world(world, hole, &G.shot, show_guide, show_cue, G.t);
            if (G.state == ST_AIM && G.save.preview)
                bp_render_preview(world, &G.preview, G.t);
            EndMode3D();
            bp_scenery_draw_overlay(sw, sh, &pal, G.t);
            if (!page) bp_juice_draw_popups(cam, sw, sh);
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
                                  G.save.vol_sfx, G.save.fullscreen, G.save.preview,
                                  G.save.invert_aim);
            } else {
                bp_render_hud(world, &G.hud, sw, sh);
                if (G.state == ST_AIM && G.save.preview)
                    bp_render_preview_readout(&G.preview, sw, sh, G.shot.charging);
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
