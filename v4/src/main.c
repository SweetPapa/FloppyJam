/* main.c — window, main loop, input, and screen state machine.
 *
 * The simulation (sim.c) runs at a fixed 60 Hz so the frame-based swing
 * feel matches the original exactly; rendering interpolates the camera. */
#include "app.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

#define STEP (1.0f / 60.0f)

static App app;
static int demo_wait = 0;
static int demo_delay_ticks = 15;
static float frame_samples[32768];
static int compare_frame(const void *a, const void *b) {
    float x=*(const float *)a,y=*(const float *)b;
    return (x>y)-(x<y);
}

static void apply_language(void) {
    const char *code=app.save.language;
    if(!*code) {
#ifdef _WIN32
        static char tag[LOCALE_NAME_MAX_LENGTH]; wchar_t wide[LOCALE_NAME_MAX_LENGTH];
        if(GetUserDefaultLocaleName(wide,LOCALE_NAME_MAX_LENGTH))
            WideCharToMultiByte(CP_UTF8,0,wide,-1,tag,sizeof tag,NULL,NULL);
        code=tag;
#else
        code=getenv("LC_ALL");
        if(!code || !*code)code=getenv("LC_MESSAGES");
        if(!code || !*code)code=getenv("LANG");
#endif
    }
    ml_set_language(code);
}
static void change_setting(int row,int delta) {
    static const float rates[]={.5f,.75f,1,1.25f,1.5f,2,3};
    if(row==0) {
        int i=4;for(int n=0;n<7;n++)if(fabsf(app.save.lava_rate-rates[n])<.001f)i=n;
        i=(i+delta+7)%7;app.save.lava_rate=rates[i];sim_set_lava_rate(&app.sim,rates[i]);
    } else if(row==1) {
        int i=*app.save.language ? ml_language_index()+1 : 0;i=(i+delta+8)%8;
        snprintf(app.save.language,sizeof app.save.language,"%s",i ? ml_language_code(i-1) : "");apply_language();
    } else if(row==2) {
        app.muted=!app.muted;app.save.muted=app.muted;audio_music_volume(&app,app.muted?0:.60f);
    } else if(row==3) app.save.reduced_motion=!app.save.reduced_motion;
    else if(row==4)app.screen=app.prev_screen;
    if(!app.demo)save_store(&app.save);
}
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
    demo_wait = 0;
    if (id < 1) id = 1;
    if (id > LEVEL_COUNT) id = LEVEL_COUNT;
    app.level_id = id;
    sim_init(&app.sim, id);
    sim_set_lava_rate(&app.sim,app.save.lava_rate);
    render_reset_fx(&app);
    app.cam_shake = 0;
    app.lava_flash = 0;
    app.accum = 0;
    app.pending_color = -1;
    app.previous_x = app.render_x = app.sim.px;
    app.previous_y = app.render_y = app.sim.py;
    app.new_best = 0;
    app.cam_y = (LEVEL_HEIGHT - app.sim.py) * WS;
    app.cam_x = 0;
    app.cam_dist = 11.5f;
    app.score_shown = 0;
    app.intro_t = 0;      /* replays the level-name card */
    app.fade = 1.0f;      /* fade up from black */
    app.warn_timer = 0;
    app.prev_state = app.sim.state;
    audio_music_theme(&app, id); /* every level gets its own key & groove */
    audio_music_intensity(&app, 0.0f);
    app.screen = SCR_PLAYING;
}

/* respond to one simulation step's event flags */
static void handle_events(void) {
    GameSim *g = &app.sim;
    char buf[16];
    if (g->ev_swing) audio_play(&app, SFX_SWING);
    if (g->ev_attach) {
        /* the landing tone climbs with the combo — the core feedback loop */
        int mult = g->combo < COMBO_MAX ? g->combo : COMBO_MAX;
        float pitch = 1.0f + (mult - 1) * 0.06f;
        audio_play_pitched(&app, g->ev_attach_forward ? SFX_LAND : SFX_LAND_BACK,
                           g->ev_attach_forward ? pitch : 0.85f);
        render_spawn_burst(&app, g->px, g->py, mag_color(g->color, 1),
                           5 + mult / 2, 85.0f + mult * 5.0f);
        app.cam_shake = fmaxf(app.cam_shake, 0.012f + mult * 0.001f);
        app.land_pop = 1.0f;
        app.land_idx = g->attached_idx;
        snprintf(buf, sizeof buf, "+%d", SCORE_LANDING * mult);
        render_push_popup(&app, g->px, g->py - 30.0f, buf,
                          mult > 1 ? (Color){255, 150, 220, 255} : (Color){255, 235, 160, 255});
    }
    if (g->ev_bounce) {
        audio_play_pitched(&app, SFX_LAND_BACK, 1.5f);
        render_spawn_burst(&app, g->px, g->py, (Color){200, 220, 255, 255}, 8, 140.0f);
        app.cam_shake = fmaxf(app.cam_shake, 0.05f);
    }
    if (g->ev_checkpoint) {
        audio_play(&app, SFX_CHECKPOINT);
        render_spawn_burst(&app, g->px, g->py, (Color){120, 255, 170, 255}, 28, 220.0f);

        render_push_popup(&app, g->px, g->py - 60.0f, "CHECKPOINT", (Color){120, 255, 170, 255});
    }
    if (g->ev_death) {
        audio_play(&app, SFX_DEATH);
        render_spawn_burst(&app, g->px, g->py, (Color){255, 120, 60, 255}, 48, 320.0f);
        app.cam_shake = 0.10f;
        app.death_flash = 1.0f;
        app.time_scale = app.save.reduced_motion ? 1.0f : 0.65f; /* slow-motion death, as in the original */
    }
    if (g->ev_complete) {
        audio_play(&app, SFX_COMPLETE);
        render_spawn_burst(&app, g->px, g->py, (Color){255, 230, 120, 255}, 80, 340.0f);
        app.cam_shake = fmaxf(app.cam_shake, 0.10f);
        int i = g->level_id - 1;
        app.new_best = app.save.best_time[i] == 0 || g->elapsed < app.save.best_time[i];
        if (!app.demo) save_result(&app.save, g);
        app.screen = SCR_COMPLETE;
        app.complete_t = 0;
    }
    /* respawn: flash back in */
    if (app.prev_state == PS_DEAD && g->state != PS_DEAD) {
        render_reset_fx(&app);
        app.previous_x = app.render_x = g->px;
        app.previous_y = app.render_y = g->py;
        app.cam_y = (LEVEL_HEIGHT - g->py) * WS;
        app.fade = 0.25f;
        render_spawn_burst(&app, g->px, g->py, (Color){160, 220, 255, 255}, 24, 180.0f);
    }
    app.prev_state = g->state;
}

static int g_idle = 0; /* debug: demo player never swings (lets lava rise) */

/* --- playthrough / screenshot-sequence controller (demo mode) --- */
static int g_play[64], g_playn = 0, g_plidx = 0;
static int g_shots = 0, g_shotevery = 45, g_shotn = 0;
static int g_shotper = 14, g_levelshotn = 0, g_lastshotlevel = -1; /* cap per level */
static int g_levelframes = 0, g_levelcap = 2600; /* ~43s safety cap per level */

/* advance the demo to the next playlist level; returns 0 when finished */
static int demo_advance(void) {
    fprintf(stdout,"PLAYTEST level=%d key=%s won=%d deaths=%d time=%.2f frames=%d\n",
            app.level_id,app.sim.lv->key,app.sim.won,app.sim.deaths,app.sim.elapsed,g_levelframes);
    g_plidx++;
    if (g_plidx >= g_playn) return 0;
    start_level(g_play[g_plidx]);
    g_levelframes = 0;
    return 1;
}

static void update_playing(int demo) {
    int pressed = demo ? -1 : poll_color();
    if (pressed >= 0) app.pending_color = pressed;
    if (!demo && !IsWindowFocused()) { app.pending_color = -1; app.screen = SCR_PAUSED; return; }
    if (!demo && IsKeyPressed(KEY_R)) { start_level(app.level_id); return; }
    if (!demo && IsKeyPressed(KEY_ESCAPE)) { app.screen = SCR_PAUSED; return; }

    float raw = GetFrameTime();
    if (raw > 0.1f) raw = 0.1f; /* avoid spiral after a stall */
    float dt = raw * app.time_scale; /* slow-motion affects the sim clock */
    app.accum += dt;
    int backtrack_prev = app.sim.backtrack_active;
    while (app.accum >= STEP) {
        app.previous_x = app.sim.px; app.previous_y = app.sim.py;
        int pending = app.pending_color;
        if (demo) {
            pending = -1;
            if (app.sim.state != PS_ATTACHED) demo_wait = 0;
            else if (!g_idle && demo_wait++ >= demo_delay_ticks) pending = bot_color(&app.sim);
        }
        sim_update(&app.sim, STEP, pending);
        app.pending_color = -1;
        handle_events();
        app.accum -= STEP;
        if (app.screen != SCR_PLAYING) break; /* completed mid-substep */
    }
    if (app.sim.backtrack_active && !backtrack_prev) {
        app.lava_flash = fmaxf(app.lava_flash, 0.7f);
        app.cam_shake = fmaxf(app.cam_shake, 0.06f);
    }

    /* lava proximity rumble, paced so it never machine-guns */
    float gap = app.sim.lava_y - app.sim.py;
    if (gap < LAVA_WARNING_DIST && app.sim.state != PS_DEAD && !app.sim.game_over) {
        app.warn_timer -= raw;
        if (app.warn_timer <= 0) {
            audio_play(&app, SFX_WARN);
            app.warn_timer = 0.35f + 0.65f * (gap / LAVA_WARNING_DIST);
        }
    } else {
        app.warn_timer = 0;
    }

    /* music tension tracks danger and momentum */
    {
        float lava_t = 1.0f - fminf(1.0f, fmaxf(0.0f, gap) / 700.0f);
        float want = fmaxf(lava_t, app.speed_norm * 0.6f);
        audio_music_intensity(&app, want);
    }

    /* HUD score counts up smoothly toward the real value */
    app.score_shown += ((float)app.sim.score - app.score_shown) * fminf(1.0f, raw * 6.0f);

    float alpha = fminf(1.0f, app.accum / STEP);
    app.render_x = app.previous_x + (app.sim.px - app.previous_x) * alpha;
    app.render_y = app.previous_y + (app.sim.py - app.previous_y) * alpha;
    render_update_fx(&app, raw); /* visuals run on unscaled time */
    app.cam_shake *= expf(-10.0f * raw);
    app.lava_flash *= expf(-10.0f * raw);
}

int main(void) {
    int demo = getenv("MAGLAVA_DEMO") != NULL;
    g_idle = getenv("MAGLAVA_IDLE") != NULL;
    if (getenv("MAGLAVA_DEMO_DELAY_MS")) {
        long delay = strtol(getenv("MAGLAVA_DEMO_DELAY_MS"), NULL, 10);
        if (delay < 0) delay = 0;
        if (delay > 5000) delay = 5000;
        demo_delay_ticks = (int)ceilf(delay * 0.06f);
    }
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
    InitWindow(1280, 800, "Maglava | Rise or Burn");
    SetWindowMinSize(960, 640);
    int refresh = GetMonitorRefreshRate(GetCurrentMonitor());
    SetTargetFPS(refresh > 60 ? refresh : 60);
    SetExitKey(0); /* we handle ESC ourselves */

    save_load(&app.save);
    apply_language();
    app.demo = demo || shot || g_shots;
    app.muted = app.save.muted;
    audio_init(&app);
    audio_music_volume(&app, app.muted ? 0 : 0.60f);
    /* render_init needs a valid sim.lv for cam_y; seed with level 1 */
    sim_init(&app.sim, demo_level);
    app.level_id = demo_level;
    render_init(&app);
    ui_init();

    app.screen = demo ? SCR_PLAYING : SCR_TITLE;
    app.select_cursor = app.save.unlocked - 1;
    if (demo) start_level(demo_level);

    /* Read-only UI captures for layout review at different window sizes. */
    const char *capture = getenv("MAGLAVA_SCREEN");
    if (capture && shot) {
        app.demo = 1;
        if(getenv("MAGLAVA_LANGUAGE"))ml_set_language(getenv("MAGLAVA_LANGUAGE"));
        if(!strcmp(capture,"settings"))app.screen=SCR_SETTINGS;
        if(!strcmp(capture,"pause"))app.screen=SCR_PAUSED;
        if (!strcmp(capture,"select")) {
            app.screen = SCR_SELECT;
            app.select_cursor = getenv("MAGLAVA_CURSOR") ? atoi(getenv("MAGLAVA_CURSOR")) : 0;
            if (app.select_cursor < 0) app.select_cursor = 0;
            if (app.select_cursor >= LEVEL_COUNT) app.select_cursor = LEVEL_COUNT - 1;
        }
    }
    if (getenv("MAGLAVA_COMPACT")) SetWindowSize(960,640);
    int frame = 0, sample_count = 0;
    int profile = getenv("MAGLAVA_PROFILE") != NULL;
    while (!WindowShouldClose()) {
        app.frame_dt = fminf(GetFrameTime(), 0.1f);
        if (app.screen != SCR_PAUSED) app.t += app.frame_dt;
        frame++;
        if (profile && frame > 30 && sample_count < 32768)
            frame_samples[sample_count++] = GetFrameTime()*1000;
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        if (IsKeyPressed(KEY_M)) {
            app.muted = !app.muted; app.save.muted = app.muted;
            audio_music_volume(&app, app.muted ? 0 : 0.60f);
            if (!app.demo) save_store(&app.save);
        }
        if (IsKeyPressed(KEY_V)) {
            app.save.reduced_motion = !app.save.reduced_motion;
            if (!app.demo) save_store(&app.save);
        }

        switch (app.screen) {
        case SCR_TITLE: {
            int clicked=-1;
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))for(int i=0;i<3;i++)
                if(CheckCollisionPointRec(GetMousePosition(),ui_menu_button(i)))clicked=i;
            if(clicked==0 || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                audio_play(&app,SFX_UI);app.screen=SCR_SELECT;
            }
            if(clicked==1 || IsKeyPressed(KEY_F2)) { app.prev_screen=SCR_TITLE;app.screen=SCR_SETTINGS;app.settings_cursor=0; }
            if(clicked==2)goto done;
            break;
        }
        case SCR_SETTINGS: {
            if(IsKeyPressed(KEY_ESCAPE))app.screen=app.prev_screen;
            if(IsKeyPressed(KEY_UP))app.settings_cursor=(app.settings_cursor+4)%5;
            if(IsKeyPressed(KEY_DOWN))app.settings_cursor=(app.settings_cursor+1)%5;
            if(IsKeyPressed(KEY_LEFT))change_setting(app.settings_cursor,-1);
            if(IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_ENTER))change_setting(app.settings_cursor,1);
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))for(int i=0;i<5;i++) {
                Rectangle rect=ui_settings_row(i);
                if(CheckCollisionPointRec(GetMousePosition(),rect)) {
                    app.settings_cursor=i;change_setting(i,GetMouseX()<GetScreenWidth()/2 ? -1 : 1);break;
                }
            }
            break;
        }
        case SCR_SELECT: {
            if (capture && shot) break;
            int c = app.select_cursor;
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) c++;
            if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) c--;
            if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) c += 5;
            if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) c -= 5;
            if (IsKeyPressed(KEY_PAGE_DOWN)) c += 20;
            if (IsKeyPressed(KEY_PAGE_UP)) c -= 20;
            if (c < 0) c = 0;
            if (c >= LEVEL_COUNT) c = LEVEL_COUNT - 1;
            int clicked = -1;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int first = (app.select_cursor / 20) * 20;
                for (int i = first; i < first + 20 && i < LEVEL_COUNT; i++)
                    if (CheckCollisionPointRec(GetMousePosition(), ui_level_cell(i))) { c = i; clicked = i; }
            }
            if (c != app.select_cursor) { app.select_cursor = c; audio_play(&app, SFX_UI); }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || clicked >= 0) {
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
            if(IsKeyPressed(KEY_F2) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(),ui_settings_row(4)))) { app.prev_screen=SCR_PAUSED;app.screen=SCR_SETTINGS;app.settings_cursor=0; }
            if (IsKeyPressed(KEY_ESCAPE)) { app.pending_color = -1; app.screen = SCR_PLAYING; }
            if (IsKeyPressed(KEY_R)) { audio_play(&app, SFX_UI); start_level(app.level_id); }
            if (IsKeyPressed(KEY_Q)) { app.select_cursor = app.level_id - 1; app.screen = SCR_SELECT; }
            break;
        case SCR_COMPLETE:
            app.complete_t += GetFrameTime();
            app.render_x = app.sim.px; app.render_y = app.sim.py;
            render_update_fx(&app, app.frame_dt);
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
        } else if (app.screen == SCR_SETTINGS) {
            ui_settings(&app);
        } else {
            DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                                   (Color){10, 10, 22, 255}, (Color){20, 16, 34, 255});
            ui_select(&app);
        }
        if (app.muted) ui_text("MUTED", GetScreenWidth() - 78, GetScreenHeight() - 24, 16, GRAY);
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
    if (sample_count) {
        double sum=0;for(int i=0;i<sample_count;i++)sum+=frame_samples[i];
        qsort(frame_samples,sample_count,sizeof(float),compare_frame);
        fprintf(stdout,"FRAME_TIMING samples=%d mean_ms=%.3f p95_ms=%.3f max_ms=%.3f\n",
            sample_count,sum/sample_count,frame_samples[(sample_count-1)*95/100],frame_samples[sample_count-1]);
    }
    ui_shutdown();
    render_shutdown(&app);
    audio_shutdown(&app);
    CloseWindow();
    return 0;
}
