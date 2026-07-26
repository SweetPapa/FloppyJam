/* ui.c — §10. Everything that is not the table.
 *
 * The menu never implies that one ruleset is the real game (Pillar 3): the
 * three doors are listed as three doors, in the same type, with the same
 * weight. STANDARD is not "normal" and PARTY is not "silly"; they are what
 * they are, and RALLY is not a training mode even though it is the best one.
 */
#include "ui.h"
#include "app.h"
#include "synth.h"
#include "rods.h"
#include "heat.h"

#include <stdio.h>
#include <string.h>

/* ---- small drawing helpers -------------------------------------------- */

static Color c_of(VbRGB c, float a) {
    Color o = { (unsigned char)(vb_clampf(c.r, 0, 1) * 255),
                (unsigned char)(vb_clampf(c.g, 0, 1) * 255),
                (unsigned char)(vb_clampf(c.b, 0, 1) * 255),
                (unsigned char)(vb_clampf(a, 0, 1) * 255) };
    return o;
}
static VbRGB mix_rgb(VbRGB a, VbRGB b, float t) {
    return rgb(vb_lerpf(a.r, b.r, t), vb_lerpf(a.g, b.g, t), vb_lerpf(a.b, b.b, t));
}
static const Color DIMTEXT = { 150, 158, 172, 255 };
static const Color TEXT    = { 232, 238, 248, 255 };
static const Color ACCENT  = { 255, 236, 170, 255 };

static void text_c(const char *s, int y, int size, Color col) {
    int w = MeasureText(s, size);
    DrawText(s, GetScreenWidth() / 2 - w / 2, y, size, col);
}
static void panel(float x, float y, float w, float h, float a) {
    DrawRectangleRounded((Rectangle){ x, y, w, h }, 0.06f, 8, (Color){ 8, 10, 16, (unsigned char)(a * 255) });
    DrawRectangleRoundedLines((Rectangle){ x, y, w, h }, 0.06f, 8,
                              (Color){ 60, 72, 96, (unsigned char)(a * 200) });
}

/* ---- menu plumbing ----------------------------------------------------- */

static int key_rep(int k1, int k2, int pad) {
    if (IsKeyPressed(k1) || IsKeyPressed(k2)) return 1;
    if (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, pad)) return 1;
    return 0;
}
static int nav_up(void)   { return key_rep(KEY_UP, KEY_W, GAMEPAD_BUTTON_LEFT_FACE_UP); }
static int nav_down(void) { return key_rep(KEY_DOWN, KEY_S, GAMEPAD_BUTTON_LEFT_FACE_DOWN); }
static int nav_left(void) { return key_rep(KEY_LEFT, KEY_A, GAMEPAD_BUTTON_LEFT_FACE_LEFT); }
static int nav_right(void){ return key_rep(KEY_RIGHT, KEY_D, GAMEPAD_BUTTON_LEFT_FACE_RIGHT); }
/* Confirm and cancel are gated by the guard app_go sets, so a single press
 * cannot walk through two screens. Movement is not gated — the cursor should
 * feel live the instant a screen appears. */
static float g_guard = 0.0f;

static int nav_ok(void) {
    if (g_guard > 0.0f) return 0;
    /* Both players' FLICK keys confirm, as well as the usual suspects: the
     * button you strike with is the button you will reach for on a menu, and
     * player one's is RSHIFT now that they are on the arrows. */
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)
        || IsKeyPressed(KEY_F) || IsKeyPressed(KEY_RIGHT_SHIFT)) return 1;
    if (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) return 1;
    return 0;
}
static int nav_back(void) {
    if (g_guard > 0.0f) return 0;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) return 1;
    if (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) return 1;
    return 0;
}

static int menu(VbApp *a, int n) {
    int moved = 0;
    if (nav_up())   { a->sel = (a->sel + n - 1) % n; moved = 1; }
    if (nav_down()) { a->sel = (a->sel + 1) % n;     moved = 1; }
    if (moved) vb_sfx(VB_SFX_UI, 1.0f, 0.4f);
    /* never move the cursor and act on it in the same frame: whatever you just
     * launched would not be the thing you were looking at */
    if (!moved && nav_ok()) { vb_sfx(VB_SFX_UI_BIG, 1.0f, 0.5f); return 1; }
    return 0;
}

static void draw_items(VbApp *a, const char *const *items, const char *const *subs,
                       int n, int top, int size) {
    for (int i = 0; i < n; i++) {
        int y = top + i * (size + 20);
        int on = (i == a->sel);
        char line[128];
        snprintf(line, sizeof line, "%s%s", on ? "> " : "  ", items[i]);
        text_c(line, y, size, on ? ACCENT : TEXT);
        if (subs && subs[i] && on) text_c(subs[i], y + size + 2, 16, DIMTEXT);
    }
}

/* ---- screens: input ---------------------------------------------------- */

static const char *const MENU_ITEMS[] = {
    "VERSUS", "RALLY", "GAUNTLET", "TRICKSHOT", "OPTIONS", "QUIT"
};
static const char *const MENU_SUBS[] = {
    "1v1 or 2v2 at one table. Pick a ruleset, then settle it.",
    "Co-op keep-up. No goals, no pressure, one long volley.",
    "Ten opponents and a finale. Beat them for the look of the place.",
    "Twenty posed shots. Medals for doing it in fewer tries.",
    "Sound, colour, comfort, and every key you press.",
    NULL
};

static const char *const SETUP_ITEMS[] = {
    "RULESET", "PLAYERS", "P1 SCHEME", "P2 SCHEME", "POINTS", "MUTATORS",
    "TABLE TILT", "START"
};
/* the two extra rows doubles adds: each player picks their own scheme (§3.3) */
static const char *const SETUP_ITEMS_D[] = {
    "RULESET", "PLAYERS", "P1 SCHEME  (GK+DEF)", "P1B SCHEME  (ATK)",
    "P2 SCHEME  (GK+DEF)", "P2B SCHEME  (ATK)", "POINTS", "MUTATORS",
    "TABLE TILT", "START"
};

/* maps a row on the setup screen to what it edits, so the two layouts do not
 * each need their own switch */
enum { SR_RULES = 0, SR_PLAYERS, SR_S0, SR_S1, SR_S2, SR_S3, SR_POINTS,
       SR_MUT, SR_TILT, SR_START };

static void setup_rows(const VbApp *a, int *rows, int *n) {
    int k = 0;
    rows[k++] = SR_RULES;
    rows[k++] = SR_PLAYERS;
    rows[k++] = SR_S0;
    if (a->doubles) rows[k++] = SR_S1;
    rows[k++] = SR_S2;
    if (a->doubles) rows[k++] = SR_S3;
    rows[k++] = SR_POINTS;
    rows[k++] = SR_MUT;
    rows[k++] = SR_TILT;
    rows[k++] = SR_START;
    *n = k;
}

static void setup_input(VbApp *a) {
    int rows[10], n;
    setup_rows(a, rows, &n);
    if (a->sel >= n) a->sel = n - 1;
    if (nav_up())   { a->sel = (a->sel + n - 1) % n; vb_sfx(VB_SFX_UI, 1, 0.4f); }
    if (nav_down()) { a->sel = (a->sel + 1) % n;     vb_sfx(VB_SFX_UI, 1, 0.4f); }
    int d = nav_right() - nav_left();
    if (d) vb_sfx(VB_SFX_UI, 1.1f, 0.4f);

    switch (rows[a->sel]) {
        case SR_RULES:
            if (d) {
                a->ruleset = (a->ruleset == VB_RULES_STANDARD)
                           ? VB_RULES_PARTY : VB_RULES_STANDARD;
                /* the lock is not a UI convention — it is enforced in
                 * rules.c — but the menu should not pretend otherwise */
                if (a->ruleset == VB_RULES_STANDARD) a->mutators = 0;
            }
            break;
        case SR_PLAYERS:
            /* three ways to fill the table, one axis to pick between them */
            if (d) {
                int mode = a->ai_on[1] ? 0 : (a->doubles ? 2 : 1);
                mode = (mode + d + 3) % 3;
                a->ai_on[1] = (mode == 0);
                a->doubles  = (mode == 2);
            }
            break;
        case SR_S0: if (d) a->save.scheme[0] = !a->save.scheme[0]; break;
        case SR_S1: if (d) a->save.scheme[1] = !a->save.scheme[1]; break;
        case SR_S2: if (d) a->save.scheme[2] = !a->save.scheme[2]; break;
        case SR_S3: if (d) a->save.scheme[3] = !a->save.scheme[3]; break;
        /* The one case here that does not fit on its line. Kept on three lines
         * rather than crammed onto one: with the ternary wrapped, a trailing
         * `break` lines up under the ternary's second arm instead of under the
         * `if`, and Apple clang rejects that as -Wmisleading-indentation, which
         * is -Werror here. The Linux GCC does not, so it only ever failed the
         * macOS runner. */
        case SR_POINTS:
            if (d) a->save.target = a->save.target == 5 ? 7
                                  : a->save.target == 7 ? 10 : 5;
            break;
        case SR_MUT:
            if (nav_ok() && a->ruleset == VB_RULES_PARTY) { app_go(a, ST_TILT); a->sub = 1; }
            break;
        case SR_TILT: if (nav_ok()) { app_go(a, ST_TILT); a->sub = 0; } break;
        case SR_START:
            if (nav_ok()) {
                a->mode = VB_MODE_VERSUS;
                if (a->ai_on[1]) {
                    vb_ai_init(&a->ai[1], (int)(a->blink * 7.0f) % VB_NPERSONAS,
                               1, 2, 424242u);
                    app_go(a, ST_BANTER);
                } else app_start_match(a);
            }
            break;
        default: break;
    }
    if (nav_back()) app_go(a, ST_MENU);
}

/* Table Tilt: openly negotiated, before the match, in front of both players.
 * The screen is framed as sportsmanship because that is what it is (§6). */
static void tilt_input(VbApp *a) {
    int rows = a->sub ? 8 : 10;   /* sub == 1: the mutator page             */
    if (nav_up())   a->sel = (a->sel + rows - 1) % rows;
    if (nav_down()) a->sel = (a->sel + 1) % rows;
    int d = nav_right() - nav_left();

    if (a->sub) {
        static const unsigned BITS[8] = {
            VB_MUT_MULTIBALL, VB_MUT_BIG_BALL, VB_MUT_PEA_BALL, VB_MUT_MAGNET,
            VB_MUT_MIRROR, VB_MUT_SUDDEN_HEAT, VB_MUT_MOVING_GOAL,
            VB_MUT_GOLDEN_GOAL
        };
        if (nav_ok() || d) a->mutators ^= BITS[a->sel];
        if (a->ruleset == VB_RULES_STANDARD) a->mutators = 0;
    } else if (d) {
        int side = a->sel / 5;
        VbTilt *t = &a->tilt[side];
        switch (a->sel % 5) {
            case 0: t->pad_scale  = vb_clampf(t->pad_scale + 0.05f * (float)d, 0.70f, 1.40f); break;
            case 1: t->rod_speed  = vb_clampf(t->rod_speed + 0.05f * (float)d, 0.70f, 1.40f); break;
            case 2: a->save.assist[side] = vb_clampf(a->save.assist[side] + 0.10f * (float)d, 0.0f, 1.0f); break;
            case 3: t->heat_cap   = vb_clampi(t->heat_cap + d, 0, 12); break;
            case 4: t->goal_scale = vb_clampf(t->goal_scale + 0.05f * (float)d, 0.80f, 1.50f); break;
        }
    }
    if (nav_back()) app_go(a, ST_SETUP);
}

static void options_input(VbApp *a) {
    int n = 9;
    if (nav_up())   a->sel = (a->sel + n - 1) % n;
    if (nav_down()) a->sel = (a->sel + 1) % n;
    int d = nav_right() - nav_left();
    VbSave *s = &a->save;
    switch (a->sel) {
        case 0: s->master = vb_clampf(s->master + 0.05f * (float)d, 0, 1); break;
        case 1: s->music  = vb_clampf(s->music  + 0.05f * (float)d, 0, 1); break;
        case 2: s->sfx    = vb_clampf(s->sfx    + 0.05f * (float)d, 0, 1); break;
        case 3: s->crowd  = vb_clampf(s->crowd  + 0.05f * (float)d, 0, 1); break;
        case 4:
            if (d) {   /* only walk onto palettes that have been unlocked */
                for (int i = 0; i < VB_NPALETTES; i++) {
                    s->palette = (s->palette + d + VB_NPALETTES) % VB_NPALETTES;
                    if (vb_save_palette_unlocked(s, s->palette)) break;
                }
            }
            break;
        case 5: if (d) s->reduce_motion = !s->reduce_motion; break;
        case 6: if (d) s->assist[0] = vb_clampf(s->assist[0] + 0.1f * (float)d, 0, 1); break;
        case 7: if (d) s->invert[0] = !s->invert[0]; break;
        case 8: if (nav_ok()) { app_go(a, ST_BINDS); a->rebind = -1; } break;
        default: break;
    }
    if (d || a->sel <= 3) vb_synth_volumes(s->master, s->music, s->sfx, s->crowd);
    if (d) vb_fx_settings(&a->fx, s->palette, s->reduce_motion);
    if (nav_back()) { vb_save_write(s); app_go(a, a->prev_state == ST_PAUSE ? ST_PAUSE : ST_MENU); }
}

static void binds_input(VbApp *a) {
    int n = BIND_COUNT_ * 2;      /* two local players on the keyboard      */
    if (a->rebind >= 0) {
        int k = GetKeyPressed();
        if (k) {
            int slot = (a->rebind / BIND_COUNT_) ? 2 : 0;
            a->save.binds[slot][a->rebind % BIND_COUNT_] = k;
            a->rebind = -1;
            vb_sfx(VB_SFX_UI_BIG, 1.0f, 0.5f);
        }
        return;
    }
    if (nav_up())   a->sel = (a->sel + n - 1) % n;
    if (nav_down()) a->sel = (a->sel + 1) % n;
    if (nav_ok())   a->rebind = a->sel;
    if (IsKeyPressed(KEY_R)) {
        for (int p = 0; p < VB_NPLAYERS; p++)
            for (int k = 0; k < BIND_COUNT_; k++)
                a->save.binds[p][k] = 0;
        a->save.binds[0][0] = 0;
        vb_save_write(&a->save);
    }
    if (nav_back()) { vb_save_write(&a->save); app_go(a, ST_OPTIONS); }
}

static void ladder_input(VbApp *a) {
    if (nav_up())   a->rung = vb_clampi(a->rung - 1, 0, VB_NRUNGS - 1);
    if (nav_down()) a->rung = vb_clampi(a->rung + 1, 0, VB_NRUNGS - 1);
    int d = nav_right() - nav_left();
    if (d) a->tier = vb_clampi(a->tier + d, 0, 2);
    /* you may attempt the next rung after the one you have cleared */
    int open = a->save.rung_cleared[a->tier] + 1;
    if (a->rung > open) a->rung = open;
    if (nav_ok()) {
        a->mode = VB_MODE_GAUNTLET;
        a->ruleset = VB_RULES_STANDARD;
        a->ai_on[1] = 1;
        a->mutators = 0;
        app_go(a, ST_BANTER);
    }
    if (nav_back()) app_go(a, ST_MENU);
}

static void shots_input(VbApp *a) {
    if (nav_up())   a->shot = (a->shot + VB_NTRICK - 1) % VB_NTRICK;
    if (nav_down()) a->shot = (a->shot + 1) % VB_NTRICK;
    if (nav_ok()) {
        a->mode = VB_MODE_TRICKSHOT;
        a->ai_on[0] = a->ai_on[1] = 0;
        a->attempts = 0;
        app_setup_trickshot(a);
        app_go(a, ST_PLAY);
    }
    if (nav_back()) app_go(a, ST_MENU);
}

void vb_ui_frame(VbApp *a, float dt) {
    if (a->input_guard > 0.0f) a->input_guard -= dt;
    g_guard = a->input_guard;

    switch (a->state) {
        case ST_TITLE:
            if (nav_ok() || IsKeyPressed(KEY_ENTER)) app_go(a, ST_MENU);
            break;

        case ST_MENU:
            if (menu(a, 6)) {
                switch (a->sel) {
                    case 0: a->mode = VB_MODE_VERSUS; a->ai_on[1] = 1;
                            app_go(a, ST_SETUP); break;
                    case 1: a->mode = VB_MODE_RALLY; a->ruleset = VB_RULES_CO_OP;
                            a->ai_on[0] = a->ai_on[1] = 0; a->mutators = 0;
                            app_start_match(a); break;
                    case 2: app_go(a, ST_LADDER); break;
                    case 3: app_go(a, ST_SHOTS); break;
                    case 4: app_go(a, ST_OPTIONS); break;
                    case 5: a->running = 0; break;
                }
            }
            if (nav_back()) app_go(a, ST_TITLE);
            break;

        case ST_SETUP:   setup_input(a); break;
        case ST_TILT:    tilt_input(a); break;
        case ST_OPTIONS: options_input(a); break;
        case ST_BINDS:   binds_input(a); break;
        case ST_LADDER:  ladder_input(a); break;
        case ST_SHOTS:   shots_input(a); break;

        case ST_BANTER:
            if (nav_ok()) app_start_match(a);
            if (nav_back()) app_go(a, a->mode == VB_MODE_GAUNTLET ? ST_LADDER : ST_SETUP);
            break;

        case ST_PAUSE:
            if (menu(a, 4)) {
                switch (a->sel) {
                    case 0: app_go(a, ST_PLAY); break;
                    case 1: app_start_match(a); break;
                    case 2: app_go(a, ST_OPTIONS); a->prev_state = ST_PAUSE; break;
                    case 3: app_go(a, ST_MENU); break;
                }
            }
            if (nav_back()) app_go(a, ST_PLAY);
            break;

        case ST_CARD:
            /* instant rematch is one button from the victory screen (§10) */
            if (nav_ok()) app_start_match(a);
            if (nav_back()) app_go(a, ST_MENU);
            break;

        default: break;
    }
    (void)dt;
}

/* ---- screens: draw ----------------------------------------------------- */

static void draw_heat_meter(VbApp *a, const VbMatch *m) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    int heat = 0;
    for (int i = 0; i < m->sim.nballs; i++)
        if (m->sim.balls[i].alive && m->sim.balls[i].heat > heat)
            heat = m->sim.balls[i].heat;

    /* The 90-second test lives here: labelled, the colour of the ball, and it
     * says out loud what it is doing to the ball. */
    int bw = w / 3, bx = w / 2 - bw / 2, by = h - 62;
    DrawText("HEAT", bx, by - 20, 16, DIMTEXT);
    char sp[48];
    snprintf(sp, sizeof sp, "BALL SPEED  x%.2f", (double)(vb_heat_speed(heat) / VB_V0));
    DrawText(sp, bx + bw - MeasureText(sp, 16), by - 20, 16, DIMTEXT);

    int seg = bw / (VB_HEAT_MAX + 1);
    float pulse = a->fx.heat_pulse;
    for (int i = 0; i <= VB_HEAT_MAX; i++) {
        VbRGB c = vb_ball_color(i);
        int on = (i <= heat);
        int newest = (on && i == heat);
        /* the segment that just lit grows and flares for a moment: the meter
         * should tell you it moved without you having to be watching it */
        float grow = (newest ? pulse : 0.0f) * 5.0f;
        Rectangle r = { (float)(bx + i * seg + 1) - grow, (float)by - grow,
                        (float)(seg - 2) + grow * 2.0f, 14.0f + grow * 2.0f };
        DrawRectangleRec(r, on ? c_of(c, 0.95f) : (Color){ 34, 38, 48, 255 });
        if (newest && pulse > 0.0f) {
            DrawRectangleLinesEx(r, 1.5f, c_of(rgb(1, 1, 1), pulse * 0.9f));
            BeginBlendMode(BLEND_ADDITIVE);
            DrawRectangleRec(r, c_of(c, pulse * 0.45f));
            EndBlendMode();
        }
        if (on && i >= VB_MERCY_HEAT)
            DrawRectangleLinesEx(r, 1.0f, c_of(rgb(1, 1, 1), 0.7f));
    }
    if (heat >= VB_MERCY_HEAT)
        DrawText("MERCY BEAT", bx, by + 18, 14, c_of(vb_ball_color(heat), 0.9f));
    if (heat >= VB_SCORCHER) {
        const char *s = "SCORCHER RANGE";
        DrawText(s, bx + bw - MeasureText(s, 14), by + 18, 14, ACCENT);
    }
}

static void tilt_badges(VbApp *a, int side, int x, int y) {
    const VbTilt *t = &a->tilt[side];
    char b[96];
    int n = 0;
    b[0] = 0;
    if (t->pad_scale  != 1.0f) n += snprintf(b + n, sizeof b - (size_t)n, "PAD %+d%% ", (int)((t->pad_scale - 1) * 100));
    if (t->rod_speed  != 1.0f) n += snprintf(b + n, sizeof b - (size_t)n, "SPD %+d%% ", (int)((t->rod_speed - 1) * 100));
    if (t->goal_scale != 1.0f) n += snprintf(b + n, sizeof b - (size_t)n, "GOAL %+d%% ", (int)((t->goal_scale - 1) * 100));
    if (t->heat_cap)           n += snprintf(b + n, sizeof b - (size_t)n, "CAP %d ", t->heat_cap);
    if (n > 0) DrawText(b, x, y, 14, ACCENT);
}

static void draw_hud(VbApp *a) {
    const VbMatch *m = &a->match;
    int w = GetScreenWidth();

    if (m->cfg.mode == VB_MODE_TRICKSHOT) {
        const VbTrickshot *t = &VB_TRICKSHOTS[a->shot];
        text_c(t->name, 18, 26, TEXT);
        text_c(t->hint, 48, 16, DIMTEXT);
        char at[64];
        snprintf(at, sizeof at, "ATTEMPT %d   GOLD IN %d   SILVER IN %d",
                 a->attempts + 1, t->gold, t->silver);
        text_c(at, 70, 15, DIMTEXT);
        return;
    }

    if (m->cfg.mode == VB_MODE_RALLY) {
        char s[64];
        snprintf(s, sizeof s, "%d", m->streak);
        text_c(s, 26, 54, c_of(vb_ball_color(m->rally_heat), 1.0f));
        text_c("TOUCHES, TOGETHER", 84, 15, DIMTEXT);
        snprintf(s, sizeof s, "BEST  %d", m->best_streak > a->save.best_streak
                                        ? m->best_streak : a->save.best_streak);
        text_c(s, 104, 15, DIMTEXT);
        draw_heat_meter(a, m);
        return;
    }

    /* the score, in each side's own colour, either side of the centre */
    char l[16], r[16];
    snprintf(l, sizeof l, "%d", m->score[0]);
    snprintf(r, sizeof r, "%d", m->score[1]);
    DrawText(l, w / 2 - 96 - MeasureText(l, 48), 22, 48,
             c_of(vb_side_color(a->save.palette, 0), 1.0f));
    DrawText(r, w / 2 + 96, 22, 48, c_of(vb_side_color(a->save.palette, 1), 1.0f));
    char tgt[32];
    snprintf(tgt, sizeof tgt, "FIRST TO %d", m->cfg.target);
    text_c(tgt, 40, 15, DIMTEXT);

    tilt_badges(a, 0, 24, 24);
    {
        const VbTilt *t = &a->tilt[1];
        if (t->pad_scale != 1.0f || t->rod_speed != 1.0f
            || t->goal_scale != 1.0f || t->heat_cap)
            tilt_badges(a, 1, w - 250, 24);
    }

    if (vb_match_is_match_point(m) && m->winner < 0) {
        /* it breathes, because it is the only line on screen that changes
         * what the next twenty seconds are worth */
        float k = 0.72f + 0.28f * sinf(a->blink * 4.2f);
        text_c("MATCH POINT", 84, 20, c_of(rgb(1.0f, 0.92f, 0.66f), k));
    }

    /* Pillar 2: the rally is the show. So the counter grows and warms as the
     * exchange goes on, and a long one is legible from across the room. */
    {
        char rally[48];
        snprintf(rally, sizeof rally, "RALLY  %d", m->rally);
        float k = vb_clampf((float)m->rally / 24.0f, 0.0f, 1.0f);
        int size = 16 + (int)(10.0f * k);
        Color col = c_of(mix_rgb(rgb(0.59f, 0.62f, 0.67f),
                                 vb_ball_color(m->rally_heat), k), 0.65f + 0.35f * k);
        DrawText(rally, 24, GetScreenHeight() - 20 - size, size, col);
        if (m->rally >= 15)
            DrawText("KEEP IT ALIVE", 24, GetScreenHeight() - 20 - size - 18, 13,
                     c_of(vb_ball_color(m->rally_heat), 0.45f + 0.35f * k));
    }

    /* The referee's warning shows late on purpose: it is the pulse just before
     * the heat-0 pop, not a nag at anyone working the ball in their own half. */
    if (m->sim.ctrl_ticks > (VB_STALL_TICKS * 7) / 10 && m->phase == VB_PH_LIVE) {
        float k = (float)m->sim.ctrl_ticks / (float)VB_STALL_TICKS;
        text_c("MOVE IT", 132, 18, c_of(rgb(1.0f, 0.8f, 0.3f), 0.35f + 0.65f * k));
    }
    if (m->phase == VB_PH_SERVE) text_c("SERVE", GetScreenHeight() / 2 - 60, 20, DIMTEXT);

    /* The 1.2 s between points used to be dead air. It is the only moment in
     * the match with nothing to react to, so it is the right place to say what
     * just happened and who has the ball next — and to let a SCORCHER be
     * called by name, since the stat card tracks it anyway (§4). */
    if (m->phase == VB_PH_GOAL && m->last_goal_side >= 0) {
        int scorch = vb_heat_scorcher(m->last_goal_heat);
        VbRGB sc = vb_side_color(a->save.palette, m->last_goal_side);
        float k = vb_clampf((float)m->phase_ticks / (float)VB_TICKS(1.2f), 0, 1);
        int y = GetScreenHeight() / 2 - 96;
        /* it arrives at size and settles, rather than just appearing */
        int size = scorch ? 62 : 46;
        size += (int)(14.0f * k * k);
        const char *what = scorch ? "SCORCHER" : "GOAL";
        int tw = MeasureText(what, size);
        DrawText(what, GetScreenWidth() / 2 - tw / 2, y, size,
                 c_of(scorch ? vb_ball_color(m->last_goal_heat) : sc, 0.55f + 0.45f * k));
        char sub[64];
        snprintf(sub, sizeof sub, "PLAYER %d  ·  STEP %d  ·  PLAYER %d SERVES",
                 m->last_goal_side + 1, m->last_goal_heat, m->serve_to + 1);
        text_c(sub, y + size + 8, 16, DIMTEXT);
    }

    /* doubles: who is holding what, and the offer to trade */
    if (m->sim.cfg.doubles) {
        for (int side = 0; side < VB_NSIDES; side++) {
            int sw = m->sim.cfg.swap[side];
            char s[64];
            snprintf(s, sizeof s, "P%d %s  ·  P%dB %s", side + 1,
                     sw ? "ATK" : "GK+DEF", side + 1, sw ? "GK+DEF" : "ATK");
            int x = side ? w - 24 - MeasureText(s, 14) : 24;
            DrawText(s, x, GetScreenHeight() - 56, 14,
                     c_of(vb_side_color(a->save.palette, side), 0.9f));
        }
        if (m->phase != VB_PH_LIVE)
            text_c("CHIP TO SWAP ROLES", GetScreenHeight() / 2 - 34, 16, ACCENT);
    }

    draw_heat_meter(a, m);
}

static void draw_title(VbApp *a) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    vb_fx_draw_sim(&a->fx, &a->attract.sim, a->alpha, 1);
    DrawRectangle(0, 0, w, h, (Color){ 0, 0, 0, 110 });

    int size = w / 12;
    const char *t = "VOLLEYBAR";
    int tw = MeasureText(t, size);
    /* the logo sits on its own pane of glass */
    panel((float)(w / 2 - tw / 2 - 40), (float)(h / 3 - 26), (float)(tw + 80),
          (float)(size + 52), 0.55f);
    DrawText(t, w / 2 - tw / 2, h / 3 - 8, size, TEXT);
    text_c("SIX RODS. ONE COMET. NO SPINNING.", h / 3 + size + 6, 18, DIMTEXT);

    if (a->save.longest_rally > 0) {
        char s[80];
        snprintf(s, sizeof s, "LONGEST RALLY EVER   %d", a->save.longest_rally);
        text_c(s, h - 132, 20, ACCENT);
    }
    if (((int)(a->blink * 2.0f)) & 1)
        text_c("PRESS START", h - 92, 22, TEXT);
    text_c(a->graybox ? "GRAY-BOX BUILD" : "", h - 30, 14, DIMTEXT);
}

static void draw_setup(VbApp *a) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    text_c("SET THE TABLE", 60, 34, TEXT);
    text_c("Then settle it.", 100, 16, DIMTEXT);

    int rows[10], n;
    setup_rows(a, rows, &n);
    const char *const *names = a->doubles ? SETUP_ITEMS_D : SETUP_ITEMS;

    int top = 150;
    for (int i = 0; i < n; i++) {
        int y = top + i * 32;
        int on = (i == a->sel);
        int row = rows[i];
        char v[48];
        int dim = 0;
        switch (row) {
            case SR_RULES:
                snprintf(v, sizeof v, "%s", a->ruleset == VB_RULES_STANDARD
                         ? "STANDARD  (locked, no mutators)" : "PARTY  (mutators, Tilt on)");
                break;
            case SR_PLAYERS:
                snprintf(v, sizeof v, "%s", a->ai_on[1] ? "1P vs AI"
                         : a->doubles ? "2v2 DOUBLES" : "1v1 LOCAL");
                break;
            case SR_S0: case SR_S1: case SR_S2: case SR_S3: {
                int slot = row - SR_S0;
                snprintf(v, sizeof v, "%s", a->save.scheme[slot] ? "GRIP" : "GLIDE");
                break;
            }
            case SR_POINTS: snprintf(v, sizeof v, "FIRST TO %d", a->save.target); break;
            case SR_MUT:
                snprintf(v, sizeof v, "%s", a->ruleset == VB_RULES_PARTY
                         ? "CHOOSE >" : "STANDARD IS LOCKED");
                dim = (a->ruleset == VB_RULES_STANDARD);
                break;
            case SR_TILT: snprintf(v, sizeof v, "NEGOTIATE >"); break;
            default: v[0] = 0; break;
        }
        DrawText(names[i], w / 2 - 280, y, 20, on ? ACCENT : TEXT);
        DrawText(v, w / 2 + 20, y, 20, on ? ACCENT : (dim ? DIMTEXT : TEXT));
    }
    text_c("LEFT / RIGHT to change   ENTER to choose   ESC to go back", h - 48, 15, DIMTEXT);
}

static void draw_tilt(VbApp *a) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    if (a->sub) {
        text_c("MUTATORS", 60, 34, TEXT);
        text_c("PARTY only. STANDARD never sees any of this.", 100, 16, DIMTEXT);
        static const char *const NAMES[8] = {
            "MULTIBALL", "BIG BALL", "PEA BALL", "MAGNET PADDLES",
            "MIRROR MATCH", "SUDDEN HEAT", "MOVING GOALS", "GOLDEN GOAL"
        };
        static const unsigned BITS[8] = {
            VB_MUT_MULTIBALL, VB_MUT_BIG_BALL, VB_MUT_PEA_BALL, VB_MUT_MAGNET,
            VB_MUT_MIRROR, VB_MUT_SUDDEN_HEAT, VB_MUT_MOVING_GOAL, VB_MUT_GOLDEN_GOAL
        };
        for (int i = 0; i < 8; i++) {
            int y = 170 + i * 32, on = (i == a->sel);
            DrawText(NAMES[i], w / 2 - 200, y, 20, on ? ACCENT : TEXT);
            DrawText((a->mutators & BITS[i]) ? "ON" : "OFF", w / 2 + 120, y, 20,
                     (a->mutators & BITS[i]) ? ACCENT : DIMTEXT);
        }
        text_c("ESC to go back", h - 48, 15, DIMTEXT);
        return;
    }

    text_c("TABLE TILT", 56, 34, TEXT);
    text_c("Set the table, then settle it.", 94, 17, ACCENT);
    text_c("Handicaps are agreed out loud, like golf strokes, and both of you", 122, 15, DIMTEXT);
    text_c("still play to win. They show as badges on the versus screen.", 142, 15, DIMTEXT);

    static const char *const ROWS[5] = {
        "PADDLE SIZE", "ROD SPEED", "AIM ASSIST", "HEAT CAP", "GOAL WIDTH AGAINST"
    };
    for (int side = 0; side < 2; side++) {
        int x = side ? w / 2 + 40 : w / 2 - 400;
        char hd[32];
        snprintf(hd, sizeof hd, "PLAYER %d", side + 1);
        DrawText(hd, x, 190, 22, c_of(vb_side_color(a->save.palette, side), 1.0f));
        for (int i = 0; i < 5; i++) {
            int idx = side * 5 + i, y = 230 + i * 34, on = (idx == a->sel);
            DrawText(ROWS[i], x, y, 17, on ? ACCENT : TEXT);
            char v[32];
            const VbTilt *t = &a->tilt[side];
            switch (i) {
                case 0: snprintf(v, sizeof v, "%d%%", (int)(t->pad_scale * 100)); break;
                case 1: snprintf(v, sizeof v, "%d%%", (int)(t->rod_speed * 100)); break;
                case 2: snprintf(v, sizeof v, "%d%%", (int)(a->save.assist[side] * 100)); break;
                case 3: snprintf(v, sizeof v, "%s", t->heat_cap ? TextFormat("%d", t->heat_cap) : "NONE"); break;
                default: snprintf(v, sizeof v, "%d%%", (int)(t->goal_scale * 100)); break;
            }
            DrawText(v, x + 280, y, 17, on ? ACCENT : DIMTEXT);
        }
    }
    text_c("ESC to go back", h - 48, 15, DIMTEXT);
}

static void draw_banter(VbApp *a) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    const VbPersona *p = &VB_PERSONAS[a->ai[1].persona];
    VbRGB sig = rgb((float)p->col[0] / 255.0f, (float)p->col[1] / 255.0f,
                    (float)p->col[2] / 255.0f);
    panel((float)(w / 2 - 340), (float)(h / 2 - 150), 680.0f, 300.0f, 0.75f);
    text_c(p->name, h / 2 - 118, 44, c_of(sig, 1.0f));
    text_c(p->style, h / 2 - 62, 18, DIMTEXT);
    char q[160];
    snprintf(q, sizeof q, "\"%s\"", p->banter[0]);
    text_c(q, h / 2 - 10, 20, TEXT);
    snprintf(q, sizeof q, "\"%s\"", p->banter[1]);
    text_c(q, h / 2 + 22, 20, TEXT);
    if (a->mode == VB_MODE_GAUNTLET) {
        char r[64];
        snprintf(r, sizeof r, "%s   ·   %s",
                 VB_LADDER[a->rung].rung,
                 a->tier == 0 ? "STEADY" : a->tier == 1 ? "SHARP" : "MERCILESS");
        text_c(r, h / 2 + 70, 16, ACCENT);
    }
    if (((int)(a->blink * 2.0f)) & 1) text_c("PRESS START", h / 2 + 108, 20, TEXT);
}

static void draw_card(VbApp *a) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    const VbMatch *m = &a->match;

    /* the best rally of the match, playing behind the numbers */
    if (a->card_replay >= 0 && a->replay.playing)
        vb_fx_draw_sim(&a->fx, &a->replay.play_sim, 0.0f, 1);
    else
        vb_fx_draw_sim(&a->fx, &m->sim, 0.0f, 1);
    DrawRectangle(0, 0, w, h, (Color){ 0, 0, 0, 140 });

    if (m->cfg.mode == VB_MODE_RALLY) {
        text_c("RALLY", 70, 32, TEXT);
    } else if (m->winner >= 0) {
        char t[64];
        snprintf(t, sizeof t, "PLAYER %d TAKES IT", m->winner + 1);
        text_c(t, 60, 40, c_of(vb_side_color(a->save.palette, m->winner), 1.0f));
        char s[32];
        snprintf(s, sizeof s, "%d  -  %d", m->score[0], m->score[1]);
        text_c(s, 106, 30, TEXT);
    }

    static const char *const ROWS[6] = {
        "LONGEST RALLY", "TOP HEAT", "SAVES", "SCORCHERS", "PINS LANDED", "CHIPS"
    };
    int top = 180;
    for (int i = 0; i < 6; i++) {
        int y = top + i * 30;
        DrawText(ROWS[i], w / 2 - 240, y, 19, DIMTEXT);
        int va = 0, vb = 0;
        switch (i) {
            case 0: va = m->stats[0].longest_rally; vb = m->stats[1].longest_rally; break;
            case 1: va = m->stats[0].top_heat;      vb = m->stats[1].top_heat; break;
            case 2: va = m->stats[0].saves;         vb = m->stats[1].saves; break;
            case 3: va = m->stats[0].scorchers;     vb = m->stats[1].scorchers; break;
            case 4: va = m->stats[0].pins;          vb = m->stats[1].pins; break;
            default: va = m->stats[0].chips;        vb = m->stats[1].chips; break;
        }
        char sa[16], sb[16];
        snprintf(sa, sizeof sa, "%d", va);
        snprintf(sb, sizeof sb, "%d", vb);
        DrawText(sa, w / 2 + 60 - MeasureText(sa, 19), y, 19,
                 c_of(vb_side_color(a->save.palette, 0), 1.0f));
        DrawText(sb, w / 2 + 160, y, 19, c_of(vb_side_color(a->save.palette, 1), 1.0f));
    }

    if (a->card_replay >= 0)
        text_c("BEST RALLY OF THE MATCH", top + 200, 16, ACCENT);
    if (a->save.longest_rally == a->last_longest && a->last_longest > 0)
        text_c("A NEW PERSONAL BEST", top + 224, 16, ACCENT);
    text_c("ENTER to run it back    ESC for the menu", h - 56, 18, TEXT);
}

static void draw_ladder(VbApp *a) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    text_c("THE GAUNTLET", 50, 34, TEXT);
    const char *tn = a->tier == 0 ? "STEADY" : a->tier == 1 ? "SHARP" : "MERCILESS";
    char sub[96];
    snprintf(sub, sizeof sub, "< %s >    they react in %d ms", tn, 320 - a->tier * 70);
    text_c(sub, 90, 17, ACCENT);
    text_c("Difficulty changes how fast they see and how well they aim. Never the physics.",
           114, 14, DIMTEXT);

    int open = a->save.rung_cleared[a->tier] + 1;
    for (int i = 0; i < VB_NRUNGS; i++) {
        int y = 160 + i * 30;
        int locked = (i > open);
        const VbPersona *p = &VB_PERSONAS[VB_LADDER[i].persona];
        VbRGB sig = rgb((float)p->col[0] / 255.0f, (float)p->col[1] / 255.0f,
                        (float)p->col[2] / 255.0f);
        Color col = locked ? (Color){ 70, 74, 84, 255 }
                  : (i == a->rung ? ACCENT : c_of(sig, 1.0f));
        char line[128];
        snprintf(line, sizeof line, "%s%-24s %s", i == a->rung ? "> " : "  ",
                 VB_LADDER[i].rung, locked ? "LOCKED" : p->name);
        DrawText(line, w / 2 - 260, y, 19, col);
        if (i <= a->save.rung_cleared[a->tier])
            DrawText("CLEARED", w / 2 + 220, y, 15, DIMTEXT);
    }
    text_c("UP / DOWN to pick    LEFT / RIGHT for difficulty    ENTER to play", h - 48, 15, DIMTEXT);
}

static void draw_shots(VbApp *a) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    text_c("TRICKSHOT", 44, 34, TEXT);
    text_c("Twenty posed shots. Fewer attempts, better medal.", 82, 16, DIMTEXT);
    int gold = 0, done = 0;
    for (int i = 0; i < VB_NTRICK; i++) {
        if (a->save.medal[i] >= 0) done++;
        if (a->save.medal[i] == 2) gold++;
    }
    char hd[64];
    snprintf(hd, sizeof hd, "%d / %d CLEARED   %d GOLD", done, VB_NTRICK, gold);
    text_c(hd, 106, 16, ACCENT);

    int top = 140, cols = 2, per = VB_NTRICK / cols;
    for (int i = 0; i < VB_NTRICK; i++) {
        int c = i / per, r = i % per;
        int x = w / 2 - 420 + c * 430, y = top + r * 28;
        int on = (i == a->shot);
        const char *med = a->save.medal[i] == 2 ? "GOLD"
                        : a->save.medal[i] == 1 ? "SILVER"
                        : a->save.medal[i] == 0 ? "BRONZE" : "-";
        char line[96];
        snprintf(line, sizeof line, "%s%2d  %-16s", on ? ">" : " ", i + 1,
                 VB_TRICKSHOTS[i].name);
        DrawText(line, x, y, 18, on ? ACCENT : TEXT);
        DrawText(med, x + 330, y, 15, a->save.medal[i] == 2 ? ACCENT : DIMTEXT);
    }
    DrawText(VB_TRICKSHOTS[a->shot].hint, w / 2 - 420, h - 76, 17, TEXT);
    text_c("ENTER to attempt    ESC for the menu", h - 44, 15, DIMTEXT);
}

static void draw_options(VbApp *a) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    const VbSave *s = &a->save;
    text_c("OPTIONS", 60, 34, TEXT);
    static const char *const NAMES[9] = {
        "MASTER", "MUSIC", "SFX", "CROWD", "PALETTE", "REDUCE MOTION",
        "GLIDE AIM ASSIST", "INVERT P1 STICK", "KEYS"
    };
    for (int i = 0; i < 9; i++) {
        int y = 150 + i * 34, on = (i == a->sel);
        DrawText(NAMES[i], w / 2 - 260, y, 20, on ? ACCENT : TEXT);
        char v[48];
        switch (i) {
            case 0: snprintf(v, sizeof v, "%d%%", (int)(s->master * 100)); break;
            case 1: snprintf(v, sizeof v, "%d%%", (int)(s->music * 100)); break;
            case 2: snprintf(v, sizeof v, "%d%%", (int)(s->sfx * 100)); break;
            case 3: snprintf(v, sizeof v, "%d%%", (int)(s->crowd * 100)); break;
            case 4: snprintf(v, sizeof v, "%s", VB_PALETTE_NAMES[s->palette]); break;
            case 5: snprintf(v, sizeof v, "%s", s->reduce_motion ? "ON" : "OFF"); break;
            case 6: snprintf(v, sizeof v, "%d%%", (int)(s->assist[0] * 100)); break;
            case 7: snprintf(v, sizeof v, "%s", s->invert[0] ? "ON" : "OFF"); break;
            default: snprintf(v, sizeof v, "REBIND >"); break;
        }
        DrawText(v, w / 2 + 100, y, 20, on ? ACCENT : DIMTEXT);
    }
    if (a->sel == 5)
        text_c("Kills shake, impact freeze, slow motion and the radial pull. "
               "Keeps every colour.", h - 76, 15, DIMTEXT);
    text_c("ESC to go back", h - 48, 15, DIMTEXT);
}

/* A rebinding screen that prints "265" is not a rebinding screen. Letters and
 * digits are their own ASCII codes in raylib, so only the odd ones need a
 * table — and anything genuinely unknown still shows its number rather than
 * a blank. */
static const char *key_name(int k) {
    static char buf[12];
    static const struct { int k; const char *n; } NAMED[] = {
        { KEY_UP, "UP" }, { KEY_DOWN, "DOWN" }, { KEY_LEFT, "LEFT" },
        { KEY_RIGHT, "RIGHT" }, { KEY_SPACE, "SPACE" }, { KEY_ENTER, "ENTER" },
        { KEY_TAB, "TAB" }, { KEY_LEFT_SHIFT, "LSHIFT" },
        { KEY_RIGHT_SHIFT, "RSHIFT" }, { KEY_LEFT_CONTROL, "LCTRL" },
        { KEY_RIGHT_CONTROL, "RCTRL" }, { KEY_LEFT_ALT, "LALT" },
        { KEY_RIGHT_ALT, "RALT" }, { KEY_SLASH, "/" }, { KEY_PERIOD, "." },
        { KEY_COMMA, "," }, { KEY_SEMICOLON, ";" }, { KEY_APOSTROPHE, "'" },
        { KEY_MINUS, "-" }, { KEY_EQUAL, "=" }, { KEY_GRAVE, "`" },
        { KEY_BACKSLASH, "\\" }, { KEY_LEFT_BRACKET, "[" },
        { KEY_RIGHT_BRACKET, "]" }, { KEY_BACKSPACE, "BKSP" },
    };
    if (k == 0) return "--";
    for (size_t i = 0; i < sizeof NAMED / sizeof NAMED[0]; i++)
        if (NAMED[i].k == k) return NAMED[i].n;
    if ((k >= KEY_A && k <= KEY_Z) || (k >= KEY_ZERO && k <= KEY_NINE)) {
        buf[0] = (char)k; buf[1] = 0;
        return buf;
    }
    snprintf(buf, sizeof buf, "#%d", k);
    return buf;
}

static void draw_binds(VbApp *a) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    text_c("KEYS", 54, 32, TEXT);
    text_c("Every action is remappable. Gamepads work at the same time.", 92, 15, DIMTEXT);
    for (int p = 0; p < 2; p++) {
        int slot = p ? 2 : 0;
        int x = w / 2 - 340 + p * 360;
        char hd[24];
        snprintf(hd, sizeof hd, "PLAYER %d", p + 1);
        DrawText(hd, x, 150, 22, c_of(vb_side_color(a->save.palette, p), 1.0f));
        for (int k = 0; k < BIND_COUNT_; k++) {
            int idx = p * BIND_COUNT_ + k, y = 190 + k * 30;
            int on = (idx == a->sel);
            DrawText(VB_BIND_NAMES[k], x, y, 18, on ? ACCENT : TEXT);
            const char *v = (a->rebind == idx) ? "PRESS A KEY"
                          : key_name(a->save.binds[slot][k]);
            DrawText(v, x + 200, y, 18, on ? ACCENT : DIMTEXT);
        }
    }
    text_c("ENTER to rebind    R to reset every key    ESC to go back", h - 48, 15, DIMTEXT);
}

static void draw_pause(VbApp *a) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    vb_fx_draw(&a->fx, &a->match, a->alpha);
    DrawRectangle(0, 0, w, h, (Color){ 0, 0, 0, 165 });
    text_c("PAUSED", h / 2 - 130, 40, TEXT);
    static const char *const ITEMS[4] = { "RESUME", "RUN IT BACK", "OPTIONS", "QUIT TO MENU" };
    draw_items(a, ITEMS, NULL, 4, h / 2 - 60, 24);
}

void vb_ui_draw(VbApp *a) {
    switch (a->state) {
        case ST_TITLE: draw_title(a); return;
        case ST_PLAY:
            vb_fx_draw(&a->fx, &a->match, a->alpha);
            draw_hud(a);
            return;
        case ST_PAUSE: draw_pause(a); return;
        case ST_CARD:  draw_card(a); return;
        default: break;
    }

    /* every other screen sits over the attract table, dimmed right down */
    vb_fx_draw_sim(&a->fx, &a->attract.sim, a->alpha, 1);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){ 0, 0, 0, 200 });

    switch (a->state) {
        case ST_MENU: {
            int h = GetScreenHeight();
            text_c("VOLLEYBAR", 60, 46, TEXT);
            draw_items(a, MENU_ITEMS, MENU_SUBS, 6, 150, 26);
            text_c("Casual and competitive are two doors into the same room.",
                   h - 52, 15, DIMTEXT);
            break;
        }
        case ST_SETUP:   draw_setup(a); break;
        case ST_TILT:    draw_tilt(a); break;
        case ST_BANTER:  draw_banter(a); break;
        case ST_OPTIONS: draw_options(a); break;
        case ST_BINDS:   draw_binds(a); break;
        case ST_LADDER:  draw_ladder(a); break;
        case ST_SHOTS:   draw_shots(a); break;
        default: break;
    }
}
