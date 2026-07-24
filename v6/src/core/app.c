#include "app.h"
#include "content/content.h"
#include "flags/flags.h"
#include "save/save.h"
#include "art/artkit.h"
#include "audio/synth.h"
#include "scene/scene.h"
#include "dlg/dlg.h"
#include "board/board.h"
#include "cut/cut.h"
#include "puzzle/puzzle.h"
#include "journal/journal.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define START_SCENE "p_gate"

typedef enum {
    A_TITLE = 0, A_NAME, A_RECAP, A_TOWN, A_DLG, A_PUZZLE, A_BOARD, A_CUT,
    A_JOURNAL, A_SETTINGS, A_QUIT
} AppState;

static AppState g_state = A_TITLE;
static AppState g_after_cut = A_TOWN;
static AppState g_after_puzzle = A_TOWN;
static bool     g_resume_dlg;
static float    g_time;
static char     g_recap[512];
static char     g_name[24];
static int      g_menu_hover = -1;
static bool     g_quit;

bool app_wants_quit(void) { return g_quit; }

/* --------------------------------------------------------------- helpers */
static unsigned hash_str(const char *s)
{
    unsigned h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static int difficulty_now(void)
{
    int s = palette_stage();
    return s >= 4 ? 2 : (s >= 2 ? 1 : 0);
}

static void enter_scene(const char *id)
{
    if (!scene_load(id)) return;
    sfx_play(SFX_DOOR);
    save_autosave(id);                 /* autosave on every scene change (§5.4) */
    g_state = A_TOWN;
}

static void start_puzzle(const char *id)
{
    unsigned seed = hash_str(id) ^ (unsigned)flag_get("save_seed");
    if (puzzle_start(id, difficulty_now(), seed)) g_state = A_PUZZLE;
    else if (g_resume_dlg) { dlg_resume(); g_state = A_DLG; }
    else g_state = A_TOWN;
}

static void start_cut(const char *id, AppState back)
{
    g_after_cut = back;
    if (cut_play(id)) g_state = A_CUT;
    else g_state = back;
}

static void begin_new_game(void)
{
    flags_reset();
    palette_set_stage(0);
    flag_set("save_seed", (int)(hash_str(g_name[0] ? g_name : "Quill") & 0x7fffffff));
    flag_set("feathers", 2);           /* two on the house: generosity, §5.2 */
    flag_set("stage", 0);
    snprintf(settings()->detective, sizeof settings()->detective, "%s",
             g_name[0] ? g_name : "Quill");
    settings_store();
    enter_scene(START_SCENE);
}

/* ==========================================================================
 * title
 * ========================================================================== */
static const char *MENU[4] = { "New Case", "Continue", "Settings", "Leave Town" };

static void title_draw(void)
{
    /* the gray town, breathing under paper grain (W3-A4) */
    int keep = palette_stage();
    palette_set_stage(0);
    scene_draw_backdrop("square", g_time * 0.3f);
    palette_set_stage(keep);

    DrawRectangle(0, 0, VW, VH, (Color){ 240, 234, 220, 120 });
    wind_lines(g_time, 0.7f, col_ink_soft());

    Rectangle plate = { 300, 90, VW - 600, 190 };
    paper_panel(plate, 7.0f, 11111);
    /* the title keeps clear of the prism and the magpie at every text size */
    float ts = art_text_size_for("HUEDUNIT", plate.width - 260, 60);
    float w = art_text_w("HUEDUNIT", ts);
    art_text("HUEDUNIT", plate.x + (plate.width - w) * 0.5f,
             plate.y + 40 + (60 - ts) * 0.5f, ts, col_ink());
    const char *sub = ui_str("title.sub");
    float ss = art_text_size_for(sub, plate.width - 80, 18);
    float w2 = art_text_w(sub, ss);
    art_text(sub, plate.x + (plate.width - w2) * 0.5f, plate.y + 126, ss,
             col_ink_soft());
    doodle(D_PRISM, plate.x + 76, plate.y + 96, 36, -0.2f, (Color){ 168, 140, 210, 255 });
    doodle(D_BIRD, plate.x + plate.width - 78, plate.y + 96, 34, 0.1f, col_ink());

    for (int i = 0; i < 4; i++) {
        Rectangle r = { VW * 0.5f - 170, 340 + i * 74.0f, 340, 58 };
        bool enabled = (i != 1) || save_exists(0);
        pz_button(r, MENU[i], enabled, 12000 + i);
    }
    art_text(ui_str("title.foot"), 40, VH - 40, 13, col_ink_soft());
    paper_grain(0.6f);
    vignette(0.5f);
}

static void title_update(void)
{
    for (int i = 0; i < 4; i++) {
        Rectangle r = { VW * 0.5f - 170, 340 + i * 74.0f, 340, 58 };
        bool enabled = (i != 1) || save_exists(0);
        if (!pz_button_clicked(r, enabled)) continue;
        switch (i) {
        case 0: g_name[0] = 0; g_state = A_NAME; break;
        case 1:
            if (save_read(0)) {
                save_build_recap(g_recap, sizeof g_recap);
                g_state = A_RECAP;
            }
            break;
        case 2: g_state = A_SETTINGS; break;
        default: g_quit = true; break;
        }
    }
    g_menu_hover = -1;
}

/* ==========================================================================
 * name entry
 * ========================================================================== */
static void name_draw(void)
{
    title_draw();
    DrawRectangle(0, 0, VW, VH, (Color){ 24, 22, 26, 140 });
    Rectangle r = { 340, 250, 600, 230 };
    paper_panel(r, 5.0f, 13000);
    art_text_fit(ui_str("name.prompt"),
                 (Rectangle){ r.x + 30, r.y + 22, r.width - 60, 56 }, 20, col_ink());

    Rectangle field = { r.x + 30, r.y + 84, r.width - 60, 54 };
    ink_rect(field, 2.2f, 0.8f, 13001, col_ink_soft());
    char shown[32];
    snprintf(shown, sizeof shown, "%s%s", g_name,
             fmodf(g_time, 1.0f) < 0.5f ? "_" : " ");
    art_text(shown, field.x + 14, field.y + 12, 24, col_ink());

    pz_button((Rectangle){ r.x + 30, r.y + 156, 240, 52 }, ui_str("name.begin"),
              true, 13002);
    pz_button((Rectangle){ r.x + 320, r.y + 156, 240, 52 }, ui_str("name.back"),
              true, 13004);
}

static void name_update(void)
{
    int c;
    while ((c = GetCharPressed())) {
        int n = (int)strlen(g_name);
        if (c >= 32 && c < 127 && n < 15) { g_name[n] = (char)c; g_name[n + 1] = 0; }
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        int n = (int)strlen(g_name);
        if (n) g_name[n - 1] = 0;
    }
    Rectangle r = { 340, 250, 600, 230 };
    if (pz_button_clicked((Rectangle){ r.x + 30, r.y + 156, 240, 52 }, true) ||
        IsKeyPressed(KEY_ENTER))
        begin_new_game();
    if (pz_button_clicked((Rectangle){ r.x + 320, r.y + 156, 240, 52 }, true))
        g_state = A_TITLE;
}

/* ==========================================================================
 * recap — "Previously, in Prismbrook..." (§1.2.1)
 * ========================================================================== */
static void recap_draw(void)
{
    scene_draw_backdrop("square", g_time * 0.2f);
    DrawRectangle(0, 0, VW, VH, (Color){ 240, 234, 220, 170 });
    Rectangle r = { 190, 150, VW - 380, 380 };
    paper_panel(r, 6.0f, 14000);
    art_text(ui_str("recap.title"), r.x + 34, r.y + 30,
             art_text_size_for(ui_str("recap.title"), r.width - 68, 28), col_ink());
    ink_line(r.x + 32, r.y + 76, r.x + r.width - 32, r.y + 76, 2.0f, 1.4f, 14001,
             col_ink_soft());
    art_text_fit(g_recap, (Rectangle){ r.x + 34, r.y + 100, r.width - 68,
                                      r.height - 200 }, 19, col_ink());
    pz_button((Rectangle){ r.x + r.width * 0.5f - 130, r.y + r.height - 82, 260, 56 },
              ui_str("recap.go"), true, 14002);
    doodle(D_FEATHER, r.x + r.width - 70, r.y + r.height - 60, 18, -0.5f,
           col_accent_a());
    paper_grain(0.5f);
}

static void recap_update(void)
{
    Rectangle r = { 190, 150, VW - 380, 380 };
    if (pz_button_clicked((Rectangle){ r.x + r.width * 0.5f - 130,
                                       r.y + r.height - 82, 260, 56 }, true) ||
        IsKeyPressed(KEY_ENTER))
        enter_scene(save_last_scene());
}

/* ==========================================================================
 * settings (§5.4)
 * ========================================================================== */
static void settings_screen_draw(void)
{
    DrawRectangle(0, 0, VW, VH, (Color){ 26, 22, 26, 140 });
    Rectangle r = { 300, 70, VW - 600, VH - 150 };
    paper_panel(r, 5.0f, 15000);
    art_text(ui_str("set.title"), r.x + 30, r.y + 24,
             art_text_size_for(ui_str("set.title"), r.width - 60, 26), col_ink());

    Settings *s = settings();
    static const char *textnames[3] = { "normal", "large", "largest" };
    char buf[96];
    const char *rows[6];
    char b0[96], b1[96], b2[96], b3[96], b4[96], b5[96];
    snprintf(b0, sizeof b0, "Text size: %s", textnames[s->text_size % 3]);
    snprintf(b1, sizeof b1, "Highlight interactables: %s", s->highlight ? "on" : "off");
    snprintf(b2, sizeof b2, "Music: %d", s->music_vol);
    snprintf(b3, sizeof b3, "Sound: %d", s->sfx_vol);
    snprintf(b4, sizeof b4, "Fullscreen: %s", s->fullscreen ? "on" : "off");
    snprintf(b5, sizeof b5, "Reduce motion: %s", s->reduce_motion ? "on" : "off");
    rows[0] = b0; rows[1] = b1; rows[2] = b2; rows[3] = b3; rows[4] = b4; rows[5] = b5;

    for (int i = 0; i < 6; i++)
        pz_button((Rectangle){ r.x + 40, r.y + 74 + i * 62.0f, r.width - 80, 50 },
                  rows[i], true, 15010 + i * 2);
    snprintf(buf, sizeof buf, "%s", ui_str("set.back"));
    pz_button((Rectangle){ r.x + r.width * 0.5f - 120, r.y + 452, 240, 50 },
              buf, true, 15040);
    art_text_fit(ui_str("set.note"),
                 (Rectangle){ r.x + 40, r.y + 512, r.width - 80, 50 },
                 13, col_ink_soft());
}

static void settings_screen_update(void)
{
    Rectangle r = { 300, 70, VW - 600, VH - 150 };
    Settings *s = settings();
    for (int i = 0; i < 6; i++) {
        if (!pz_button_clicked((Rectangle){ r.x + 40, r.y + 74 + i * 62.0f,
                                            r.width - 80, 50 }, true)) continue;
        switch (i) {
        case 0: s->text_size = (s->text_size + 1) % 3; break;
        case 1: s->highlight = !s->highlight; break;
        case 2: s->music_vol = (s->music_vol + 1) % 11; break;
        case 3: s->sfx_vol = (s->sfx_vol + 1) % 11; audio_apply_volumes(); break;
        case 4: s->fullscreen = !s->fullscreen; ToggleFullscreen(); break;
        default: s->reduce_motion = !s->reduce_motion; break;
        }
        settings_store();
    }
    if (pz_button_clicked((Rectangle){ r.x + r.width * 0.5f - 120,
                                       r.y + 452, 240, 50 }, true) ||
        IsKeyPressed(KEY_ESCAPE))
        g_state = scene_current()[0] ? A_TOWN : A_TITLE;
}

/* ==========================================================================
 * the town HUD
 * ========================================================================== */
static void hud_draw(void)
{
    art_text(scene_title(), 26, 20,
             art_text_size_for(scene_title(), 950, 20), col_ink());

    char fb[32];
    snprintf(fb, sizeof fb, "%d", flag_get("feathers"));
    doodle(D_FEATHER, VW - 300, 34, 16, -0.4f, col_accent_a());
    art_text(fb, VW - 282, 22, 20, col_ink());

    pz_button((Rectangle){ VW - 244, 14, 110, 42 }, ui_str("hud.journal"), true, 16000);
    pz_button((Rectangle){ VW - 124, 14, 100, 42 }, ui_str("hud.menu"), true, 16002);

    /* the hue ribbon — you can see how far you have come from any screen */
    static const Color band[7] = {
        { 0, 0, 0, 0 }, { 96, 150, 210, 255 }, { 232, 200, 84, 255 },
        { 200, 88, 82, 255 }, { 104, 168, 116, 255 }, { 152, 112, 196, 255 },
        { 232, 186, 92, 255 }
    };
    int stage = palette_stage();
    for (int i = 1; i <= 6; i++) {
        Rectangle r = { 26 + (i - 1) * 34.0f, 52, 28, 12 };
        Vector2 q[4] = { { r.x, r.y }, { r.x + r.width, r.y },
                         { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height } };
        ink_fill(q, 4, HATCH_NONE, 16010 + i, i <= stage ? band[i] : col_paper_dark());
        ink_rect(r, 1.4f, 0.5f, 16020 + i, col_ink_soft());
    }
}

static void hud_update(void)
{
    if (pz_button_clicked((Rectangle){ VW - 244, 14, 110, 42 }, true) ||
        IsKeyPressed(KEY_J) || IsKeyPressed(KEY_TAB)) {
        journal_open();
        g_state = A_JOURNAL;
    } else if (pz_button_clicked((Rectangle){ VW - 124, 14, 100, 42 }, true) ||
               IsKeyPressed(KEY_ESCAPE)) {
        g_state = A_SETTINGS;
    }
}

/* ==========================================================================
 * request routing — the seam between scene, dialogue, puzzle, board and cut
 * ========================================================================== */
static void handle_scene_request(scene_request req)
{
    const char *id = scene_request_id();
    switch (req) {
    case SC_TALK:
        g_resume_dlg = false;
        if (dlg_start(id)) g_state = A_DLG;
        break;
    case SC_EXIT:
        enter_scene(id);
        break;
    case SC_BOARD:
        if (board_start(id)) g_state = A_BOARD;
        break;
    case SC_PUZZLE:
        g_after_puzzle = A_TOWN;
        g_resume_dlg = false;
        start_puzzle(id);
        break;
    case SC_CUT:
        start_cut(id, A_TOWN);
        break;
    default: break;
    }
}

static void finish_puzzle(bool solved)
{
    if (solved) {
        const char *cl = puzzle_clue();
        if (cl && cl[0]) clue_grant(cl);
        char key[FLAG_MAX_KEY];
        snprintf(key, sizeof key, "pzdone.%s", puzzle_current_id());
        flag_set(key, 1);
        if (puzzle_was_skipped()) flag_set("skipped_any", 1);
    }
    /* Walking out of a puzzle unsolved never pays out the conversation that
     * asked for it — the payoff lines are on the other side of solving it.
     * Re-opening the conversation offers the puzzle again, so nothing locks. */
    if (solved && g_resume_dlg) { dlg_resume(); g_state = A_DLG; return; }
    g_resume_dlg = false;
    music_mood(scene_district_mood());
    g_state = solved ? g_after_puzzle : A_TOWN;
}

static void finish_board(bool solved)
{
    if (!solved) { music_mood(scene_district_mood()); g_state = A_TOWN; return; }
    const char *kind = board_solved_kind();
    const char *id = board_solved_id();
    save_autosave(scene_current());
    if (eq(kind, "cut") && id[0]) { start_cut(id, A_TOWN); return; }
    if (eq(kind, "goto") && id[0]) {
        g_resume_dlg = false;
        if (dlg_start(id)) { g_state = A_DLG; return; }
    }
    music_mood(scene_district_mood());
    g_state = A_TOWN;
}

static void handle_dlg(dlg_status st)
{
    switch (st) {
    case DLG_WANT_PUZZLE:
        g_resume_dlg = true;
        g_after_puzzle = A_DLG;
        start_puzzle(dlg_request());
        break;
    case DLG_WANT_CUT:
        g_resume_dlg = true;
        start_cut(dlg_request(), A_DLG);
        break;
    case DLG_WANT_BOARD:
        g_resume_dlg = true;
        if (board_start(dlg_request())) g_state = A_BOARD;
        else { dlg_resume(); }
        break;
    case DLG_DONE:
        g_resume_dlg = false;
        music_mood(scene_district_mood());
        save_autosave(scene_current());
        g_state = A_TOWN;
        break;
    default: break;
    }
}

/* ==========================================================================
 * frame
 * ========================================================================== */
void app_init(void)
{
    settings_load();
    content_init();
    art_init();
    audio_init();
    flags_reset();
    palette_set_stage(0);
    g_state = A_TITLE;
    music_mood(MOOD_SAD);
    snprintf(g_name, sizeof g_name, "%s", settings()->detective);
}

/* The capture harness. It fabricates exactly enough world state to make the
 * target screen honest — a board with no clues would draw an empty pool and
 * photograph a lie. */
bool app_debug_goto(const char *spec)
{
    const char *colon = strchr(spec, ':');
    char kind[16];
    int n = colon ? (int)(colon - spec) : (int)strlen(spec);
    if (n > 15) n = 15;
    memcpy(kind, spec, (size_t)n);
    kind[n] = 0;
    const char *id = colon ? colon + 1 : "";

    flag_set("save_seed", 1234);
    flag_set("feathers", 6);
    snprintf(settings()->detective, sizeof settings()->detective, "%s", "Quill");

    if (eq(kind, "title")) { g_state = A_TITLE; return true; }
    if (eq(kind, "settings")) { g_state = A_SETTINGS; return true; }
    if (eq(kind, "name")) { g_state = A_NAME; return true; }
    if (eq(kind, "journal")) {
        for (int i = 0; i < npc_count(); i++) {
            char k[FLAG_MAX_KEY];
            snprintf(k, sizeof k, "met.%s", npc_key(i));
            flag_set(k, 1);
            trust_add(npc_key(i), 2);
        }
        for (int i = 0; i < content_block_count("@node"); i++) {
            char nid[48];
            Block b;
            if (!content_block_at("@node", i, nid, sizeof nid, &b)) continue;
            Cursor c;
            cur_open(&c, &b);
            char line[512];
            while (cur_line(&c, line, sizeof line)) {
                const char *p2 = line;
                char kw[32];
                word(&p2, kw, sizeof kw);
                if (!eq(kw, "grant")) continue;
                char t[64];
                while (word(&p2, t, sizeof t) && t[0]) clue_grant(t);
            }
        }
        palette_set_stage(3);
        scene_load("p_square");
        journal_open();
        g_state = A_JOURNAL;
        return true;
    }
    if (eq(kind, "scene")) {
        if (!scene_load(id)) return false;
        /* light the town up to whichever chapter this screen belongs to */
        int stage = (id[0] == 'c' && id[1] == 'h') ? id[2] - '0' - 1 : 0;
        palette_set_stage(stage < 0 ? 0 : stage);
        g_state = A_TOWN;
        return true;
    }
    if (eq(kind, "board")) {
        /* grant everything: the pools are only interesting when full */
        for (int i = 0; i < content_block_count("@node"); i++) {
            char nid[48];
            Block b;
            if (!content_block_at("@node", i, nid, sizeof nid, &b)) continue;
            Cursor c;
            cur_open(&c, &b);
            char line[512];
            while (cur_line(&c, line, sizeof line)) {
                const char *p = line;
                char kw[32];
                word(&p, kw, sizeof kw);
                if (!eq(kw, "grant")) continue;
                char t[64];
                while (word(&p, t, sizeof t) && t[0]) clue_grant(t);
            }
        }
        for (int i = 0; i < puzzle_count(); i++) {
            const pz_def *d = puzzle_at(i);
            if (d->clue_granted && d->clue_granted[0]) clue_grant(d->clue_granted);
        }
        palette_set_stage(3);
        if (!board_start(id)) return false;
        g_state = A_BOARD;
        return true;
    }
    if (eq(kind, "puzzle")) {
        /* "puzzle:<id>+hint" photographs the hint note, which is the widest
         * prose a puzzle ever shows and therefore the one worth checking */
        char pid[48];
        snprintf(pid, sizeof pid, "%s", id);
        char *want_hint = strchr(pid, '+');
        if (want_hint) *want_hint = 0;
        palette_set_stage(3);
        if (!puzzle_start(pid, 1, 1234)) return false;
        if (want_hint) { puzzle_spend_hint(); puzzle_spend_hint(); }
        g_state = A_PUZZLE;
        return true;
    }
    if (eq(kind, "cut")) {
        if (!cut_play(id)) return false;
        g_after_cut = A_TOWN;
        g_state = A_CUT;
        return true;
    }
    return false;
}

void app_shutdown(void)
{
    settings_store();
    audio_shutdown();
    art_shutdown();
}

void app_frame(float dt)
{
    g_time += dt;
    audio_update(dt);
    art_begin_frame(dt);

    switch (g_state) {
    case A_TITLE:
        title_draw();
        title_update();
        break;

    case A_NAME:
        name_draw();
        name_update();
        break;

    case A_RECAP:
        recap_draw();
        recap_update();
        break;

    case A_SETTINGS:
        if (scene_current()[0]) scene_draw();
        else                    title_draw();
        settings_screen_draw();
        settings_screen_update();
        break;

    case A_TOWN: {
        scene_draw();
        hud_draw();
        hud_update();
        if (g_state == A_TOWN) {
            scene_request req = scene_update(dt);
            if (req != SC_NONE) handle_scene_request(req);
        }
        break;
    }

    case A_DLG: {
        scene_draw();
        dlg_draw();
        dlg_status st = dlg_update(dt);
        if (st != DLG_RUNNING) handle_dlg(st);
        break;
    }

    case A_PUZZLE: {
        puzzle_draw();
        pz_status st = puzzle_update(dt);
        if (st == PZ_SOLVED) finish_puzzle(true);
        else if (st == PZ_EXITED) finish_puzzle(false);
        break;
    }

    case A_BOARD: {
        board_draw();
        board_status st = board_update(dt);
        if (st == BOARD_SOLVED) finish_board(true);
        else if (st == BOARD_EXITED) finish_board(false);
        break;
    }

    case A_CUT: {
        cut_draw();
        if (cut_update(dt)) {
            art_fade_set(0);
            if (g_after_cut == A_DLG && g_resume_dlg) { dlg_resume(); g_state = A_DLG; }
            else { music_mood(scene_district_mood()); g_state = A_TOWN; }
            save_autosave(scene_current());
        }
        break;
    }

    case A_JOURNAL:
        scene_draw();
        hud_draw();
        journal_draw();
        if (journal_update(dt)) {
            const char *rp = journal_replay_request();
            if (rp[0]) {
                g_after_puzzle = A_TOWN;
                g_resume_dlg = false;
                char id[48];
                snprintf(id, sizeof id, "%s", rp);
                journal_clear_replay();
                start_puzzle(id);
            } else {
                g_state = A_TOWN;
            }
        }
        break;

    default:
        g_quit = true;
        break;
    }

    art_end_frame();
}
