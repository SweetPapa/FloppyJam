/* main.c — window, main loop, input, and screen state machine.
 *
 * The simulation (sim.c) runs at a fixed 60 Hz so the frame-based swing
 * feel matches the original exactly; rendering interpolates the camera. */
#include "app.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

#define STEP (1.0f / 60.0f)

static App app;

/* edge-triggered color input, original priority up>down>left>right */
static int poll_color(void) {
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    return COL_RED;
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  return COL_BLUE;
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  return COL_YELLOW;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) return COL_GREEN;
    return -1;
}

/* greedy climbing bot for demo/attract mode */
static int bot_color(GameSim *g) {
    if (g->state != PS_ATTACHED) return -1;
    int best = -1; float besty = g->py - 5.0f;
    for (int c = 0; c < 4; c++) {
        int idx = sim_find_magnet(g, g->px, g->py, (MagColor)c, g->attached_idx);
        if (idx >= 0 && g->lv->mag[idx].y < besty) { besty = g->lv->mag[idx].y; best = c; }
    }
    if (best < 0)
        for (int c = 0; c < 4; c++) {
            int idx = sim_find_magnet(g, g->px, g->py, (MagColor)c, g->attached_idx);
            if (idx >= 0 && g->lv->mag[idx].y < g->py) return c;
        }
    return best;
}

static void start_level(int id) {
    app.level_id = id;
    sim_init(&app.sim, id);
    memset(app.parts, 0, sizeof(app.parts));
    app.part_head = 0;
    app.cam_shake = 0;
    app.lava_flash = 0;
    app.accum = 0;
    app.cam_y = (LEVEL_HEIGHT - app.sim.py) * WS;
    app.screen = SCR_PLAYING;
}

/* respond to one simulation step's event flags */
static void handle_events(void) {
    GameSim *g = &app.sim;
    if (g->ev_swing) audio_play(&app, SFX_SWING);
    if (g->ev_attach) {
        audio_play(&app, g->ev_attach_forward ? SFX_LAND : SFX_LAND_BACK);
        render_spawn_burst(&app, g->px, g->py, mag_color(g->color, 1), 10, 120.0f);
        app.cam_shake = fmaxf(app.cam_shake, 0.03f);
    }
    if (g->ev_checkpoint) {
        audio_play(&app, SFX_CHECKPOINT);
        render_spawn_burst(&app, g->px, g->py, (Color){120, 255, 170, 255}, 24, 200.0f);
        app.lava_flash = fmaxf(app.lava_flash, 0.5f);
    }
    if (g->ev_death) {
        audio_play(&app, SFX_DEATH);
        render_spawn_burst(&app, g->px, g->py, (Color){255, 120, 60, 255}, 40, 300.0f);
        app.cam_shake = 0.18f;
    }
    if (g->ev_complete) {
        audio_play(&app, SFX_COMPLETE);
        render_spawn_burst(&app, g->px, g->py, (Color){255, 230, 120, 255}, 60, 320.0f);
        int stars = sim_stars(g);
        save_record(&app.save, g->level_id, stars);
        app.screen = SCR_COMPLETE;
        app.complete_t = 0;
    }
}

static int g_idle = 0; /* debug: demo player never swings (lets lava rise) */

/* --- playthrough / screenshot-sequence controller (demo mode) --- */
static int g_play[64], g_playn = 0, g_plidx = 0;
static int g_shots = 0, g_shotevery = 45, g_shotn = 0;
static int g_shotper = 14, g_levelshotn = 0, g_lastshotlevel = -1; /* cap per level */
static int g_levelframes = 0, g_levelcap = 2600; /* ~43s safety cap per level */

/* advance the demo to the next playlist level; returns 0 when finished */
static int demo_advance(void) {
    g_plidx++;
    if (g_plidx >= g_playn) return 0;
    start_level(g_play[g_plidx]);
    g_levelframes = 0;
    return 1;
}

static void update_playing(int demo) {
    int pending = demo ? (g_idle ? -1 : bot_color(&app.sim)) : poll_color();
    if (!demo && IsKeyPressed(KEY_ESCAPE)) { app.screen = SCR_PAUSED; return; }

    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.1f; /* avoid spiral after a stall */
    app.accum += dt;
    int backtrack_prev = app.sim.backtrack_active;
    while (app.accum >= STEP) {
        sim_update(&app.sim, STEP, pending);
        pending = (demo && !g_idle) ? bot_color(&app.sim) : -1;
        handle_events();
        app.accum -= STEP;
        if (app.screen != SCR_PLAYING) break; /* completed mid-substep */
    }
    if (app.sim.backtrack_active && !backtrack_prev)
        app.lava_flash = fmaxf(app.lava_flash, 0.6f);

    render_update_particles(&app, dt);
    app.cam_shake *= 0.88f;
    app.lava_flash *= 0.90f;
}

int main(void) {
    int demo = getenv("MAGLAVA_DEMO") != NULL;
    g_idle = getenv("MAGLAVA_IDLE") != NULL;
    int shot = getenv("MAGLAVA_SHOT") ? atoi(getenv("MAGLAVA_SHOT")) : 0;
    int demo_level = getenv("MAGLAVA_LEVEL") ? atoi(getenv("MAGLAVA_LEVEL")) : 1;

    /* Playthrough sequence: MAGLAVA_LEVELS="1,2,3,7" plays each in turn.
     * MAGLAVA_SHOTS enables periodic screenshots every MAGLAVA_SHOTEVERY
     * frames (written to the current working directory). */
    g_shots = getenv("MAGLAVA_SHOTS") != NULL;
    if (getenv("MAGLAVA_SHOTEVERY")) g_shotevery = atoi(getenv("MAGLAVA_SHOTEVERY"));
    if (g_shotevery < 1) g_shotevery = 45;
    if (getenv("MAGLAVA_SHOTPERLEVEL")) g_shotper = atoi(getenv("MAGLAVA_SHOTPERLEVEL"));
    if (getenv("MAGLAVA_LEVELCAP")) g_levelcap = atoi(getenv("MAGLAVA_LEVELCAP"));
    if (getenv("MAGLAVA_LEVELS")) {
        char list[256];
        strncpy(list, getenv("MAGLAVA_LEVELS"), sizeof list - 1);
        list[sizeof list - 1] = 0;
        for (char *tok = strtok(list, ","); tok && g_playn < 64; tok = strtok(NULL, ",")) {
            int id = atoi(tok);
            if (id >= 1 && id <= LEVEL_COUNT) g_play[g_playn++] = id;
        }
        demo = 1; /* a playlist implies demo/attract mode */
    }
    if (demo && g_playn == 0) { g_play[g_playn++] = demo_level; }
    if (g_playn > 0) demo_level = g_play[0];

    /* Send screenshots (MAGLAVA_SHOT / MAGLAVA_SHOTS) to MAGLAVA_SHOTDIR.
     * Must happen before InitWindow: raylib caches the screenshot base path
     * from the working directory at init time. */
    if (getenv("MAGLAVA_SHOTDIR")) {
        if (chdir(getenv("MAGLAVA_SHOTDIR")) != 0)
            fprintf(stderr, "warning: could not chdir to MAGLAVA_SHOTDIR\n");
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(960, 720, "MagLava");
    SetTargetFPS(60);
    SetExitKey(0); /* we handle ESC ourselves */

    save_load(&app.save);
    audio_init(&app);
    /* render_init needs a valid sim.lv for cam_y; seed with level 1 */
    sim_init(&app.sim, demo_level);
    app.level_id = demo_level;
    render_init(&app);

    app.screen = demo ? SCR_PLAYING : SCR_TITLE;
    app.select_cursor = 0;
    if (demo) start_level(demo_level);

    int frame = 0;
    while (!WindowShouldClose()) {
        app.t += GetFrameTime();
        frame++;

        switch (app.screen) {
        case SCR_TITLE:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                audio_play(&app, SFX_UI); app.screen = SCR_SELECT;
            }
            break;
        case SCR_SELECT: {
            int c = app.select_cursor;
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) c++;
            if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) c--;
            if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) c += 5;
            if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) c -= 5;
            if (c < 0) c = 0; if (c >= LEVEL_COUNT) c = LEVEL_COUNT - 1;
            if (c != app.select_cursor) { app.select_cursor = c; audio_play(&app, SFX_UI); }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (app.select_cursor + 1 <= app.save.unlocked) {
                    audio_play(&app, SFX_UI); start_level(app.select_cursor + 1);
                }
            }
            if (IsKeyPressed(KEY_ESCAPE)) app.screen = SCR_TITLE;
            break;
        }
        case SCR_PLAYING:
            update_playing(demo);
            if (demo) {
                g_levelframes++;
                if (g_levelframes > g_levelcap) { /* stuck: move on */
                    if (!demo_advance()) goto done;
                }
            }
            break;
        case SCR_PAUSED:
            if (IsKeyPressed(KEY_ESCAPE)) app.screen = SCR_PLAYING;
            if (IsKeyPressed(KEY_R)) { audio_play(&app, SFX_UI); start_level(app.level_id); }
            if (IsKeyPressed(KEY_M)) app.muted = !app.muted;
            if (IsKeyPressed(KEY_Q)) { app.screen = SCR_SELECT; }
            break;
        case SCR_COMPLETE:
            app.complete_t += GetFrameTime();
            render_update_particles(&app, GetFrameTime());
            if (demo) {
                if (app.complete_t > 1.6f) { if (!demo_advance()) goto done; }
                break;
            }
            if (app.complete_t > 1.2f) {
                if (IsKeyPressed(KEY_ENTER) && app.level_id < LEVEL_COUNT &&
                    app.level_id + 1 <= app.save.unlocked) {
                    audio_play(&app, SFX_UI); start_level(app.level_id + 1);
                }
                if (IsKeyPressed(KEY_R)) { audio_play(&app, SFX_UI); start_level(app.level_id); }
                if (IsKeyPressed(KEY_ESCAPE)) { app.select_cursor = app.level_id - 1; app.screen = SCR_SELECT; }
            }
            break;
        }

        BeginDrawing();
        ClearBackground((Color){6, 6, 14, 255});
        if (app.screen == SCR_PLAYING || app.screen == SCR_PAUSED || app.screen == SCR_COMPLETE) {
            render_world(&app);
            ui_hud(&app);
            if (app.screen == SCR_PAUSED) ui_pause(&app);
            if (app.screen == SCR_COMPLETE) ui_complete(&app);
        } else if (app.screen == SCR_TITLE) {
            DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                                   (Color){14, 8, 20, 255}, (Color){40, 14, 10, 255});
            ui_title(&app);
        } else {
            DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                                   (Color){10, 10, 22, 255}, (Color){20, 16, 34, 255});
            ui_select(&app);
        }
        if (app.muted) DrawText("MUTED", GetScreenWidth() - 78, GetScreenHeight() - 24, 16, GRAY);
        EndDrawing();

        if (shot && frame == shot) {
            TakeScreenshot("maglava_shot.png");
        }
        if (shot && frame >= shot + 2) break;

        /* periodic playthrough captures (balanced: capped per level) */
        if (g_shots && (frame % g_shotevery == 0) &&
            (app.screen == SCR_PLAYING || app.screen == SCR_COMPLETE)) {
            if (app.level_id != g_lastshotlevel) { g_lastshotlevel = app.level_id; g_levelshotn = 0; }
            if (g_shotper <= 0 || g_levelshotn < g_shotper) {
                char name[64];
                snprintf(name, sizeof name, "shot_%03d_L%s.png",
                         ++g_shotn, LEVEL_LABELS[app.level_id - 1]);
                TakeScreenshot(name);
                g_levelshotn++;
            }
        }
    }
done:
    render_shutdown(&app);
    audio_shutdown(&app);
    CloseWindow();
    return 0;
}
