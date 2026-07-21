/* THE GAUNTLET — state machine and frame loop (§2, §11). */
#include "game.h"
#include <stdlib.h>
#include <time.h>

static const int RAYKEYS[LANE_COUNT] = { KEY_F, KEY_G, KEY_H, KEY_J, KEY_SPACE };
/* MIRROR: F<->J, G<->H (§6.2) */
static const int MIRROR_MAP[LANE_COUNT] = { LANE_J, LANE_H, LANE_G, LANE_F, LANE_SPACE };

static void push_input(int logical, int raw, int down, uint64_t t)
{
    if (G.in.count >= MAX_INPUT_EVENTS) return;
    InputEvent *e = &G.in.e[G.in.count++];
    e->lane = (uint8_t)logical;
    e->raw = (uint8_t)raw;
    e->down = (uint8_t)down;
    e->t = t;
    if (G.run_active && !G.replay_playing && G.replay_count < REPLAY_MAX) {
        G.replay[G.replay_count].t = t;
        G.replay[G.replay_count].key = (uint8_t)raw;
        G.replay[G.replay_count].down = (uint8_t)down;
        G.replay_count++;
    }
}

/* §11.4 — replay: inputs were sample-timestamped and everything else derives
 * from the seed, so playing the log back through the same pipeline reproduces
 * the run exactly. Debugging superpower and party feature for ~40 lines. */
static void replay_poll(uint64_t now)
{
    G.in.count = 0;
    G.in.press_mask = 0;
    G.in.release_mask = 0;
    static uint32_t held;
    while (G.replay_cursor < G.replay_count && G.replay[G.replay_cursor].t <= now) {
        ReplayEvent *r = &G.replay[G.replay_cursor++];
        int phys = r->key % LANE_COUNT;
        int logical = G.mods.mirror ? MIRROR_MAP[phys] : phys;
        push_input(logical, phys, r->down, r->t);
        if (r->down) { held |= (1u << logical); G.in.press_mask |= (1u << logical); }
        else { held &= ~(1u << logical); G.in.release_mask |= (1u << logical); }
    }
    G.in.down_mask = held;
    if (G.replay_cursor >= G.replay_count && !G.run_active) G.replay_playing = 0;
}

static void input_poll(void)
{
    G.in.count = 0;
    G.in.press_mask = 0;
    G.in.release_mask = 0;
    G.in.down_mask = 0;

    double mapped = clock_map(G.host_time);
    uint64_t t = (mapped > 0.0) ? (uint64_t)mapped : 0u;

    for (int phys = 0; phys < LANE_COUNT; phys++) {
        int logical = G.mods.mirror ? MIRROR_MAP[phys] : phys;
        if (IsKeyDown(RAYKEYS[phys])) G.in.down_mask |= (1u << logical);
        if (IsKeyPressed(RAYKEYS[phys])) {
            G.in.press_mask |= (1u << logical);
            push_input(logical, phys, 1, t);
        }
        if (IsKeyReleased(RAYKEYS[phys])) {
            G.in.release_mask |= (1u << logical);
            push_input(logical, phys, 0, t);
        }
    }
}

/* ------------------------------------------------------------------ */
/* autoplay: a robot that plays the chart. Not a cheat — it is how the  */
/* pipeline gets soak-tested without three humans in the room.          */
/* ------------------------------------------------------------------ */
static int autoplay_key_for(const MicroState *m, int chart_lane)
{
    for (int l = 0; l < LANE_COUNT; l++)
        if (m->key_map[l] == (int8_t)chart_lane) return l;
    return chart_lane;
}

static void autoplay_fill(uint64_t now)
{
    if (G.card_active && G.card_picked < 0 && G.card_t > 0.15f) {
        push_input(LANE_F, LANE_F, 1, now);
        push_input(LANE_F, LANE_F, 0, now);
        return;
    }
    MicroState *m = &G.micro;
    if (!m->active) return;

    for (int i = 0; i < m->chart.count; i++) {
        Note *n = &m->chart.n[i];
        if (!n->resolved || n->judged || n->kind == NOTE_MARK) continue;
        int lane = autoplay_key_for(m, n->lane);
        if (n->kind == NOTE_HOLD) {
            if (!n->holding && now >= n->t) push_input(lane, lane, 1, now);
            else if (n->holding && now >= n->t_end) push_input(lane, lane, 0, now);
        } else if (now >= n->t) {
            push_input(lane, lane, 1, now);
            push_input(lane, lane, 0, now);
            if (m->game_id == 6) {          /* DOUBLE wants both hands */
                push_input(LANE_J, LANE_J, 1, now);
                push_input(LANE_J, LANE_J, 0, now);
            }
        }
    }

    if (m->game_id == 8) {                  /* RAPID: alternate every frame */
        int lane = (m->ix[1] == 1) ? LANE_J : LANE_F;
        push_input(lane, lane, 1, now);
        push_input(lane, lane, 0, now);
    }
    if (m->game_id == 12) {                 /* MEASURE: hold four beats */
        double target = (double)G.clk.samples_per_beat * 4.0;
        if (m->ix[0] == 0 && m->chart.bar_known[0] && now >= m->chart.bar_start[0])
            push_input(LANE_SPACE, LANE_SPACE, 1, now);
        else if (m->ix[0] == 1 && (double)now - (double)m->ux[0] >= target)
            push_input(LANE_SPACE, LANE_SPACE, 0, now);
    }
}

#ifdef TUNING
static void tuning_keys(void)
{
    if (IsKeyPressed(KEY_F1)) G.tuning_window_scale -= 0.05f;
    if (IsKeyPressed(KEY_F2)) G.tuning_window_scale += 0.05f;
    G.tuning_window_scale = g_clampf(G.tuning_window_scale, 0.3f, 3.0f);
    if (IsKeyPressed(KEY_F3)) G.tuning_bpm_offset -= 5.0f;
    if (IsKeyPressed(KEY_F4)) G.tuning_bpm_offset += 5.0f;
    if (IsKeyPressed(KEY_F3) || IsKeyPressed(KEY_F4))
        audio_set_bpm(heat_bpm(G.tier, G.genome.bpm_base) + G.mods.bpm_bonus
                      + G.tuning_bpm_offset, G.clk.bar + 2);
    if (IsKeyPressed(KEY_F5)) G.tuning_overlay = !G.tuning_overlay;
    if (IsKeyPressed(KEY_F6)) {
        G.phrase_index += HEAT_PHRASES_PER_TIER;
        G.tier = G.phrase_index / HEAT_PHRASES_PER_TIER;
        for (int s = 0; s < STEM_COUNT; s++)
            audio_stem_gain(s, heat_stem_active(G.tier, s) ? 1.0f : 0.0f, -1);
    }
    if (IsKeyPressed(KEY_F7) && G.run_active) G.next_seg_kind = 1;
    if (IsKeyPressed(KEY_F8)) G.tuning_infinite_hp = !G.tuning_infinite_hp;
    if (IsKeyPressed(KEY_F9)) {
        uint32_t s = seed_hash32((uint64_t)G.seed_code * 6364136223846793005ULL + 1);
        game_init(s);
    }
    if (IsKeyPressed(KEY_F10)) {
        static int solo = -1;
        solo = (solo + 1) % (STEM_COUNT + 1);
        for (int s = 0; s < STEM_COUNT; s++)
            audio_stem_gain(s, (solo == STEM_COUNT || solo == s)
                            ? (heat_stem_active(G.tier, s) ? 1.0f : 0.0f) : 0.0f, -1);
    }
}
#endif

static int playfield_state(void)
{
    return G.state == ST_COUNTIN || G.state == ST_RUN
        || G.state == ST_WIPE || G.state == ST_TALLY;
}

int main(int argc, char **argv)
{
    int windowed = 0, autoplay = 0, idle = 0, force_game = -1;
    float soak_seconds = 0.0f;
    uint32_t forced_seed = 0;
    int have_forced_seed = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--windowed") == 0 || strcmp(argv[i], "-w") == 0) windowed = 1;
        else if (strcmp(argv[i], "--autoplay") == 0) { autoplay = 1; windowed = 1; }
        else if (strcmp(argv[i], "--idle") == 0) { idle = 1; windowed = 1; }
        else if (strcmp(argv[i], "--soak") == 0 && i + 1 < argc) {
            soak_seconds = (float)atof(argv[++i]);
            windowed = 1;
        } else if (strcmp(argv[i], "--force-game") == 0 && i + 1 < argc) {
            force_game = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            forced_seed = seed_from_string(argv[++i]);
            have_forced_seed = 1;
        }
    }

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(SCREEN_W, SCREEN_H, "THE GAUNTLET");
    SetExitKey(KEY_NULL);
    if (!windowed) {
        int mon = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(mon), GetMonitorHeight(mon));
        ToggleFullscreen();
    }

    audio_init();
    draw_init();

    memset(&G, 0, sizeof G);
    G.tuning_window_scale = 1.0f;
    G.mods.wild_hidden = -1;

    /* first boot generates the party seed; after that it lives in the scores file */
    G.party_seed = seed_hash32((uint64_t)time(NULL) ^ 0x5DEECE66DULL);
    persist_load_scores(G.board, &G.board_count, &G.party_seed);
    float calib = 0.0f;
    persist_load_config(&calib);
    clock_set_calibration_ms(calib);
    if (G.board_count == 0)
        persist_save_scores(G.board, 0, G.party_seed);

    if (have_forced_seed) G.party_seed = forced_seed;
    G.autoplay = autoplay;
    G.force_game = force_game;
    int headless_run = autoplay || idle;
    game_init(G.party_seed);
    screens_enter(headless_run ? ST_COUNTIN : ST_BOOT);

    double soak_start = GetTime();
    int last_report = -1;
    while (!WindowShouldClose() && !G.quit) {
        G.dt = GetFrameTime();
        if (G.dt > 0.1f) G.dt = 0.1f;
        G.host_time = GetTime();

        audio_snapshot(&G.clk);
        clock_frame(&G.clk, G.host_time);
        if (G.replay_playing) {
            double mp = clock_map(G.host_time);
            replay_poll((mp > 0.0) ? (uint64_t)mp : 0u);
            if (IsKeyPressed(KEY_ESCAPE)) { G.replay_playing = 0; run_abort(); }
        } else {
            input_poll();
        }
        if (headless_run) {
            double mapped = clock_map(G.host_time);
            if (G.autoplay) autoplay_fill((mapped > 0.0) ? (uint64_t)mapped : 0u);
            if (G.state == ST_LEADERBOARD || G.state == ST_SCORE_ENTRY
                || G.state == ST_TALLY) {
                if (G.state == ST_SCORE_ENTRY && G.state_t > 0.4f)
                    screens_submit_score();
                else if (G.state == ST_LEADERBOARD && G.state_t > 1.0f)
                    screens_enter(ST_COUNTIN);
            }
            int sec = (int)(GetTime() - soak_start);
            if (sec != last_report) {
                last_report = sec;
                printf("t=%3ds state=%d bar=%3d phrase=%2d tier=%d hp=%d "
                       "score=%lld mult=%.2f game=%s\n",
                       sec, (int)G.state, G.clk.bar, G.phrase_index, G.tier, G.hp,
                       (long long)G.score, (double)cards_mult(),
                       G.micro.def ? G.micro.def->name : "-");
                fflush(stdout);
            }
            if (soak_seconds > 0.0f && GetTime() - soak_start > (double)soak_seconds)
                G.quit = 1;
        }

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
#ifdef TUNING
        tuning_keys();
#endif
        /* ESC held 1 s aborts the run. No pause — party momentum (§4.3). */
        if (G.run_active) {
            if (IsKeyDown(KEY_ESCAPE)) {
                G.esc_hold += G.dt;
                if (G.esc_hold >= 1.0f) { G.esc_hold = 0.0f; run_abort(); }
            } else G.esc_hold = 0.0f;
        } else if (G.state == ST_ATTRACT && IsKeyPressed(KEY_ESCAPE)) {
            G.quit = 1;
        }

        screens_update();
        if (G.state == ST_COUNTIN || G.state == ST_RUN) run_update();

        float vdt = hitstop_active() ? G.dt * 0.15f : G.dt;
        draw_frame_begin(vdt);
        draw_set_desat(G.state == ST_WIPE || G.state == ST_TALLY ? 0.6f : G.desat);

        if (playfield_state()) {
            draw_trail_begin(0.08f);     /* previous frame at 92% (§9.2) */
            run_draw_playfield();
            draw_trail_end();
        }

        BeginDrawing();
        ClearBackground(pal()->bg);
        if (playfield_state()) draw_trail_blit(G.spin_angle, 1.0f);
        screens_draw();
        if (G.state == ST_COUNTIN || G.state == ST_RUN || G.state == ST_WIPE)
            run_draw_hud();
        EndDrawing();
        draw_frame_end();
    }

    persist_save_scores(G.board, G.board_count, G.party_seed);
    draw_shutdown();
    audio_shutdown();
    CloseWindow();
    return 0;
}
