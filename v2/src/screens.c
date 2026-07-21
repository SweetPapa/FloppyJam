/* ATTRACT / SEED SELECT / CALIBRATION / COUNT-IN / WIPE / SCORES (§2, §9). */
#include "game.h"
#include <time.h>
#include <stdlib.h>

static float s_attract_cycle = 0.0f;
static float s_calib_click_t = 0.0f;

/* ------------------------------------------------------------------ */
static void start_attract_music(void)
{
    audio_stop();
    audio_set_genome(&G.genome);
    audio_master_pitch(1.0f, 0.05f);
    audio_master_lp(20000.0f, 0.3f);
    audio_master_gain(0.75f, 0.4f);
    audio_metronome(0);
    audio_drum_mute(0);
    /* pad + sparse percussion, low intensity (§2.1) */
    for (int s = 0; s < STEM_COUNT; s++) audio_stem_gain(s, 0.0f, -1);
    audio_stem_gain(STEM_PAD, 1.0f, -1);
    audio_stem_gain(STEM_KICK, 0.35f, -1);
    audio_start((float)G.genome.bpm_base);
    clock_reset();
}

static void board_insert(int64_t score, int tier)
{
    ScoreRow row;
    memset(&row, 0, sizeof row);
    memcpy(row.initials, G.entry, 3);
    row.initials[3] = 0;
    row.score = (int32_t)g_clampi((int)score, 0, 2000000000);
    row.tier = (uint8_t)tier;
    row.seed = G.seed_code;

    int pos = G.board_count;
    for (int i = 0; i < G.board_count; i++)
        if (row.score > G.board[i].score) { pos = i; break; }
    if (pos >= LEADERBOARD_N) { G.entry_rank = -1; return; }

    for (int i = LEADERBOARD_N - 1; i > pos; i--) G.board[i] = G.board[i - 1];
    G.board[pos] = row;
    if (G.board_count < LEADERBOARD_N) G.board_count++;
    G.entry_rank = pos;
    persist_save_scores(G.board, G.board_count, G.party_seed);
}

static void set_active_seed(uint32_t code, int clear_board)
{
    G.seed_code = code;
    genome_init(&G.genome, code);
    draw_set_palette(G.genome.palette);
    audio_set_genome(&G.genome);
    if (clear_board) {
        G.party_seed = code;
        G.board_count = 0;
        memset(G.board, 0, sizeof G.board);
        persist_save_scores(G.board, 0, G.party_seed);
    }
}

static uint32_t daily_seed(void)
{
    time_t now = time(NULL);
    struct tm lt;
#if defined(_WIN32)
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    return seed_from_date(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
}

/* ------------------------------------------------------------------ */
void screens_enter(AppState s)
{
    G.state = s;
    G.state_t = 0.0f;
    switch (s) {
    case ST_ATTRACT:
        start_attract_music();
        s_attract_cycle = 0.0f;
        draw_set_desat(0.0f);
        break;
    case ST_SEED:
        G.seed_menu_idx = 0;
        G.seed_entering = 0;
        G.seed_entry_pos = 0;
        G.seed_confirm_hold = 0.0f;
        memset(G.seed_entry, 0, sizeof G.seed_entry);
        break;
    case ST_CALIBRATE:
        G.calib_taps = 0;
        G.calib_result = clock_calibration_ms();
        audio_stop();
        audio_master_gain(0.9f, 0.2f);
        for (int i = 0; i < STEM_COUNT; i++) audio_stem_gain(i, 0.0f, -1);
        audio_stem_gain(STEM_PAD, 0.35f, -1);
        audio_metronome(1);
        audio_start(120.0f);
        clock_reset();
        break;
    case ST_COUNTIN:
        draw_set_desat(0.0f);
        G.first_phrase_done = 0;
        run_start();
        break;
    case ST_WIPE:
        G.wipe_t = 0.0f;
        break;
    case ST_TALLY:
        G.tally_t = 0.0f;
        G.tally_step = 0;
        G.tally_value = 0;
        break;
    case ST_SCORE_ENTRY:
        G.entry[0] = G.entry[1] = G.entry[2] = 'A';
        G.entry[3] = 0;
        G.entry_pos = 0;
        G.entry_rank = -1;
        break;
    case ST_LEADERBOARD:
        start_attract_music();
        break;
    default: break;
    }
}

/* ------------------------------------------------------------------ */
static int any_key_pressed(void)
{
    return GetKeyPressed() != 0;
}

static void update_attract(void)
{
    s_attract_cycle += G.dt;
    if (IsKeyDown(KEY_C)) {
        G.esc_hold += G.dt;
        if (G.esc_hold > 1.0f) { G.esc_hold = 0.0f; screens_enter(ST_CALIBRATE); return; }
    } else G.esc_hold = 0.0f;
    if (any_key_pressed()) screens_enter(ST_SEED);
}

static void draw_attract(void)
{
    const Palette *p = pal();
    char seed[SEED_LEN + 1];
    seed_to_string(G.seed_code, seed);

    harmonograph_draw(G.seed_code, G.state_t, 0.85f);

    gtext_center("THE GAUNTLET", SCREEN_W / 2, 60, 9, p->hot);
    gtext_center("A 90 SECOND RHYTHM GAUNTLET", SCREEN_W / 2, 150, 3, p->accent);

    int page = ((int)(s_attract_cycle / 6.0f)) % 2;
    if (page == 0 || G.board_count == 0) {
        gtext_center("PRESS ANY KEY", SCREEN_W / 2,
                     SCREEN_H - 190 + (int)(sinf(G.state_t * 3.0f) * 6.0f), 5, p->main);
        char buf[64];
        snprintf(buf, sizeof buf, "SEED %s   %s %s", seed,
                 genome_scale_name(&G.genome), heat_visual_name(0));
        gtext_center(buf, SCREEN_W / 2, SCREEN_H - 130, 3, p->dim);
        gtext_center("HOLD C TO CALIBRATE", SCREEN_W / 2, SCREEN_H - 90, 2, p->dim);
    } else {
        gtext_center("LOCAL LEADERBOARD", SCREEN_W / 2, 230, 4, p->accent);
        for (int i = 0; i < G.board_count; i++) {
            char buf[64];
            snprintf(buf, sizeof buf, "%2d  %-3s  %8d  T%d", i + 1,
                     G.board[i].initials, G.board[i].score, G.board[i].tier);
            gtext_center(buf, SCREEN_W / 2, 290 + i * 34, 3, i == 0 ? p->hot : p->main);
        }
    }
}

/* ------------------------------------------------------------------ */
static void update_seed(void)
{
    if (G.seed_entering) {
        int c = GetCharPressed();
        while (c > 0) {
            char ch = (char)((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
            if (strchr(SEED_ALPHABET, ch) && G.seed_entry_pos < SEED_LEN) {
                G.seed_entry[G.seed_entry_pos++] = ch;
                audio_sfx(SFX_UI, 0);
            }
            c = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && G.seed_entry_pos > 0)
            G.seed_entry[--G.seed_entry_pos] = 0;
        if (IsKeyPressed(KEY_ESCAPE)) G.seed_entering = 0;
        if (G.seed_entry_pos == SEED_LEN && IsKeyDown(KEY_ENTER)) {
            G.seed_confirm_hold += G.dt;
            if (G.seed_confirm_hold > 0.8f) {
                set_active_seed(seed_from_string(G.seed_entry), 1);
                screens_enter(ST_COUNTIN);
            }
        } else if (!IsKeyDown(KEY_ENTER)) G.seed_confirm_hold = 0.0f;
        return;
    }

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_J)) {
        G.seed_menu_idx = (G.seed_menu_idx + 1) % 3; audio_sfx(SFX_UI, 0);
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_F)) {
        G.seed_menu_idx = (G.seed_menu_idx + 2) % 3; audio_sfx(SFX_UI, 0);
    }
    if (IsKeyPressed(KEY_ESCAPE)) { screens_enter(ST_ATTRACT); return; }

    if (G.seed_menu_idx == 0) {
        if (IsKeyPressed(KEY_ENTER)) {
            set_active_seed(G.party_seed, 0);
            screens_enter(ST_COUNTIN);
        }
        G.seed_confirm_hold = 0.0f;
    } else if (G.seed_menu_idx == 1) {
        uint32_t d = daily_seed();
        if (d == G.party_seed) {
            if (IsKeyPressed(KEY_ENTER)) { set_active_seed(d, 0); screens_enter(ST_COUNTIN); }
        } else if (IsKeyDown(KEY_ENTER)) {
            G.seed_confirm_hold += G.dt;
            if (G.seed_confirm_hold > 0.8f) {
                set_active_seed(d, 1);
                screens_enter(ST_COUNTIN);
            }
        } else G.seed_confirm_hold = 0.0f;
    } else {
        if (IsKeyPressed(KEY_ENTER)) {
            G.seed_entering = 1;
            G.seed_entry_pos = 0;
            memset(G.seed_entry, 0, sizeof G.seed_entry);
        }
    }
}

static void draw_seed(void)
{
    const Palette *p = pal();
    char party[SEED_LEN + 1], daily[SEED_LEN + 1];
    seed_to_string(G.party_seed, party);
    seed_to_string(daily_seed(), daily);

    harmonograph_draw(G.seed_code, G.state_t * 0.4f, 0.25f);
    gtext_center("SEED SELECT", SCREEN_W / 2, 70, 7, p->hot);

    const char *labels[3] = { "PARTY SEED", "DAILY SEED", "ENTER SEED" };
    const char *vals[3] = { party, daily, G.seed_entry_pos ? G.seed_entry : "------" };
    for (int i = 0; i < 3; i++) {
        int sel = (i == G.seed_menu_idx);
        Color c = sel ? p->hot : p->main;
        int y = 240 + i * 90;
        if (sel) DrawRectangle(220, y - 14, SCREEN_W - 440, 66, cmix(p->bg, p->dim, 0.8f));
        gtext(labels[i], 260, y, 5, c);
        gtext(vals[i], SCREEN_W - 260 - gtext_width(vals[i], 5), y, 5, c);
    }

    if (G.seed_entering) {
        gtext_center("TYPE 6 CHARACTERS  A-Z 0-9", SCREEN_W / 2, SCREEN_H - 190, 3, p->accent);
    }
    if (G.seed_confirm_hold > 0.0f) {
        gtext_center("THIS CLEARS THE LEADERBOARD", SCREEN_W / 2, SCREEN_H - 150, 4, p->hot);
        gtext_center("HOLD ENTER TO CONFIRM", SCREEN_W / 2, SCREEN_H - 110, 4, p->hot);
        draw_bar_meter(SCREEN_W / 2 - 200, SCREEN_H - 70, 400, 14,
                       G.seed_confirm_hold / 0.8f, p->hot, p->dim);
    } else {
        gtext_center("UP/DOWN CHOOSE    ENTER CONFIRM", SCREEN_W / 2, SCREEN_H - 90, 3, p->dim);
    }
}

/* ------------------------------------------------------------------ */
static int cmp_float(const void *a, const void *b)
{
    float fa = *(const float *)a, fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

static void update_calibrate(void)
{
    s_calib_click_t = G.clk.beat_phase;
    if (IsKeyPressed(KEY_SPACE) && G.calib_taps < 16) {
        double spb = G.clk.samples_per_beat;
        double t = clock_map_raw(G.host_time);
        double phase = fmod(t - 0.0, spb);
        if (phase > spb * 0.5) phase -= spb;
        G.calib_offsets[G.calib_taps++] = samples_to_ms(phase);
        audio_sfx(SFX_GOOD, 0);
    }
    if (G.state_t > 10.0f || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
        if (G.calib_taps >= 4) {
            float tmp[16];
            memcpy(tmp, G.calib_offsets, sizeof(float) * (size_t)G.calib_taps);
            qsort(tmp, (size_t)G.calib_taps, sizeof(float), cmp_float);
            float median = tmp[G.calib_taps / 2];
            clock_set_calibration_ms(median);
            persist_save_config(median);
        }
        audio_metronome(0);
        screens_enter(ST_ATTRACT);
    }
}

static void draw_calibrate(void)
{
    const Palette *p = pal();
    gtext_center("CALIBRATION", SCREEN_W / 2, 80, 7, p->hot);
    gtext_center("TAP SPACE ON EVERY CLICK", SCREEN_W / 2, 180, 4, p->main);

    float u = s_calib_click_t;
    ring(SCREEN_W / 2.0f, SCREEN_H / 2.0f, 40.0f + 220.0f * u, 8.0f,
         cmix(p->hot, p->bg, u));
    ring(SCREEN_W / 2.0f, SCREEN_H / 2.0f, 260.0f, 3.0f, p->dim);

    char buf[64];
    snprintf(buf, sizeof buf, "TAPS %d/16", G.calib_taps);
    gtext_center(buf, SCREEN_W / 2, SCREEN_H - 220, 4, p->accent);
    snprintf(buf, sizeof buf, "CURRENT OFFSET %+0.1f MS", (double)clock_calibration_ms());
    gtext_center(buf, SCREEN_W / 2, SCREEN_H - 170, 3, p->main);
    gtext_center("ENTER TO FINISH", SCREEN_W / 2, SCREEN_H - 110, 3, p->dim);
    draw_bar_meter(SCREEN_W / 2 - 300, SCREEN_H - 70, 600, 12,
                   g_clampf(G.state_t / 10.0f, 0, 1), p->main, p->dim);
}

/* ------------------------------------------------------------------ */
static void draw_countin(void)
{
    const Palette *p = pal();
    int beats_left = 8 - (G.clk.bar * BEATS_PER_BAR + (int)(G.clk.bar_phase * BEATS_PER_BAR));
    if (beats_left < 0) beats_left = 0;
    char buf[8];
    snprintf(buf, sizeof buf, "%d", (beats_left % 4) + 1);
    float pulse = 1.0f - G.clk.beat_phase;
    gtext_center(buf, SCREEN_W / 2, (int)pf_center_y() - 60, 10 + (int)(pulse * 4.0f), p->hot);
    gtext_center("READY", SCREEN_W / 2, (int)pf_center_y() + 80, 6, p->accent);
    ring(pf_center_x(), pf_center_y(), 120.0f + 200.0f * G.clk.beat_phase, 6.0f,
         cmix(p->hot, p->bg, G.clk.beat_phase));
}

/* ------------------------------------------------------------------ */
static void update_wipe(void)
{
    G.wipe_t += G.dt;
    draw_set_desat(g_clampf(G.wipe_t * 3.0f, 0.0f, 0.85f));
    if (G.wipe_t > 1.5f) screens_enter(ST_TALLY);   /* crowd laugh window */
}

static void draw_wipe(void)
{
    const Palette *p = pal();
    Color veil = p->bg;
    veil.a = (unsigned char)(g_clampf(G.wipe_t * 2.0f, 0.0f, 0.65f) * 255.0f);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, veil);

    float t = g_ease_out(G.wipe_t / 0.28f);
    int scale = (int)g_lerpf(28.0f, 12.0f, t);
    if (scale < 12) scale = 12;
    gtext_center("WIPED", SCREEN_W / 2, (int)(pf_center_y() - scale * 4), scale, p->hot);
    draw_cracks(3, 0.9f, p->hot);
}

/* ------------------------------------------------------------------ */
static void update_tally(void)
{
    G.tally_t += G.dt;
    /* base -> each modifier multiplier stacks in, Balatro-style, ~2 s max */
    int steps = 1 + G.mods.chip_count;
    int want = g_clampi((int)(G.tally_t / (2.0f / (float)(steps > 0 ? steps : 1))), 0, steps);
    if (want != G.tally_step) {
        G.tally_step = want;
        audio_sfx(SFX_CARD, 72.0f + 3.0f * (float)want);
    }
    G.tally_value = (int64_t)((double)G.score
                  * g_clampf((float)G.tally_step / (float)(steps ? steps : 1), 0.0f, 1.0f));
    if (G.tally_t > 2.2f || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        G.tally_value = G.score;
        screens_enter(ST_SCORE_ENTRY);
    }
}

static void draw_tally(void)
{
    const Palette *p = pal();
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){p->bg.r, p->bg.g, p->bg.b, 220});
    gtext_center("WIPED", SCREEN_W / 2, 70, 10, p->hot);

    char buf[64];
    snprintf(buf, sizeof buf, "%lld", (long long)G.tally_value);
    gtext_center(buf, SCREEN_W / 2, 240, 10, p->accent);

    int cx = SCREEN_W / 2 - (G.mods.chip_count * 100) / 2;
    for (int i = 0; i < G.mods.chip_count; i++) {
        int id = G.mods.chip[i];
        Color c = (i < G.tally_step) ? (CARDS[id].safety ? p->accent : p->hot) : p->dim;
        char m[16];
        snprintf(m, sizeof m, "X%.2f", (double)CARDS[id].mult);
        gtext(CARDS[id].name, cx + i * 100, 400, 2, c);
        gtext(m, cx + i * 100, 430, 2, c);
    }
    snprintf(buf, sizeof buf, "TIER %d REACHED   %d PHRASES   BEST FLOW %d",
             G.tier, G.phrase_index, G.best_flow);
    gtext_center(buf, SCREEN_W / 2, 520, 3, p->main);
    gtext_center("PRESS ENTER", SCREEN_W / 2, SCREEN_H - 90, 3, p->dim);
}

/* ------------------------------------------------------------------ */
static void update_score_entry(void)
{
    if (IsKeyPressed(KEY_LEFT)) {
        G.entry_pos = (G.entry_pos + 2) % 3;
        audio_sfx(SFX_UI, 0);
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        G.entry_pos = (G.entry_pos + 1) % 3;
        audio_sfx(SFX_UI, 0);
    }
    if (IsKeyPressed(KEY_UP)) {
        G.entry[G.entry_pos] = (char)(G.entry[G.entry_pos] == 'Z' ? 'A' : G.entry[G.entry_pos] + 1);
        audio_sfx(SFX_CLICK, 0);
    }
    if (IsKeyPressed(KEY_DOWN)) {
        G.entry[G.entry_pos] = (char)(G.entry[G.entry_pos] == 'A' ? 'Z' : G.entry[G.entry_pos] - 1);
        audio_sfx(SFX_CLICK, 0);
    }
    int c = GetCharPressed();
    while (c > 0) {
        char ch = (char)((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
        if (ch >= 'A' && ch <= 'Z') {
            G.entry[G.entry_pos] = ch;
            if (G.entry_pos < 2) G.entry_pos++;
        }
        c = GetCharPressed();
    }
    if (IsKeyPressed(KEY_ENTER)) {
        board_insert(G.score, G.tier);
        screens_enter(ST_LEADERBOARD);
    }
}

static void draw_score_entry(void)
{
    const Palette *p = pal();
    gtext_center("ENTER YOUR INITIALS", SCREEN_W / 2, 120, 5, p->accent);
    char buf[32];
    snprintf(buf, sizeof buf, "%lld", (long long)G.score);
    gtext_center(buf, SCREEN_W / 2, 200, 8, p->hot);

    for (int i = 0; i < 3; i++) {
        char ch[2] = { G.entry[i], 0 };
        int x = SCREEN_W / 2 - 150 + i * 110;
        int sel = (i == G.entry_pos);
        Color c = sel ? p->hot : p->main;
        gtext(ch, x, 350, 10, c);
        if (sel) {
            DrawRectangle(x - 6, 350 + 88, 72, 8, p->hot);
            gtext_center("^", x + 30, 320 - 20, 4, p->hot);
        }
    }
    gtext_center("LEFT RIGHT MOVE   UP DOWN LETTER   ENTER OK",
                 SCREEN_W / 2, SCREEN_H - 120, 3, p->dim);
}

void screens_submit_score(void)
{
    board_insert(G.score, G.tier);
    screens_enter(ST_LEADERBOARD);
}

/* ------------------------------------------------------------------ */
static void update_leaderboard(void)
{
    if (IsKeyPressed(KEY_R) && G.replay_count > 0) {
        int saved = G.replay_count;
        screens_enter(ST_COUNTIN);      /* same seed, same inputs, same run */
        G.replay_count = saved;
        G.replay_playing = 1;
        G.replay_cursor = 0;
        return;
    }
    if (G.state_t > 0.6f && any_key_pressed()) screens_enter(ST_COUNTIN);
    if (G.state_t > 25.0f) screens_enter(ST_ATTRACT);
}

static void draw_leaderboard(void)
{
    const Palette *p = pal();
    char seed[SEED_LEN + 1];
    seed_to_string(G.seed_code, seed);

    harmonograph_draw(G.seed_code, G.state_t * 0.3f, 0.2f);
    gtext_center("LEADERBOARD", SCREEN_W / 2, 50, 7, p->hot);
    char buf[64];
    snprintf(buf, sizeof buf, "SEED %s", seed);
    gtext_center(buf, SCREEN_W / 2, 130, 3, p->dim);

    for (int i = 0; i < G.board_count; i++) {
        int hi = (i == G.entry_rank);
        Color c = hi ? p->hot : p->main;
        if (hi) DrawRectangle(300, 180 + i * 40 - 6, SCREEN_W - 600, 38,
                              cmix(p->bg, p->dim, 0.9f));
        snprintf(buf, sizeof buf, "%2d  %-3s  %8d  TIER %d", i + 1,
                 G.board[i].initials, G.board[i].score, G.board[i].tier);
        gtext_center(buf, SCREEN_W / 2, 180 + i * 40, 3, c);
    }
    if (G.board_count == 0)
        gtext_center("NO SCORES YET", SCREEN_W / 2, 300, 4, p->dim);

    float blink = (sinf(G.state_t * 4.0f) > 0.0f) ? 1.0f : 0.35f;
    Color c = p->hot;
    c.a = (unsigned char)(blink * 255.0f);
    gtext_center("PASS THE KEYBOARD", SCREEN_W / 2, SCREEN_H - 150, 6, c);
    gtext_center("ANY KEY = NEXT PLAYER, SAME SEED", SCREEN_W / 2, SCREEN_H - 90, 3, p->dim);
    if (G.replay_count > 0)
        gtext_center("R = REPLAY THE WIPE", SCREEN_W / 2, SCREEN_H - 55, 2, p->dim);
}

/* ------------------------------------------------------------------ */
void screens_update(void)
{
    G.state_t += G.dt;
    switch (G.state) {
    case ST_BOOT:
        if (G.state_t > 0.4f) screens_enter(ST_ATTRACT);
        break;
    case ST_ATTRACT:   update_attract(); break;
    case ST_SEED:      update_seed(); break;
    case ST_CALIBRATE: update_calibrate(); break;
    case ST_COUNTIN:
        if (G.clk.running && G.clk.bar >= 2) screens_enter(ST_RUN);
        break;
    case ST_RUN:       break;
    case ST_WIPE:      update_wipe(); break;
    case ST_TALLY:     update_tally(); break;
    case ST_SCORE_ENTRY: update_score_entry(); break;
    case ST_LEADERBOARD: update_leaderboard(); break;
    default: break;
    }
}

void screens_draw(void)
{
    switch (G.state) {
    case ST_BOOT: {
        const Palette *p = pal();
        gtext_center("THE GAUNTLET", SCREEN_W / 2, SCREEN_H / 2 - 40, 8, p->main);
        break;
    }
    case ST_ATTRACT:     draw_attract(); break;
    case ST_SEED:        draw_seed(); break;
    case ST_CALIBRATE:   draw_calibrate(); break;
    case ST_COUNTIN:     draw_countin(); break;
    case ST_RUN:         break;
    case ST_WIPE:        draw_wipe(); break;
    case ST_TALLY:       draw_tally(); break;
    case ST_SCORE_ENTRY: draw_score_entry(); break;
    case ST_LEADERBOARD: draw_leaderboard(); break;
    default: break;
    }
}
