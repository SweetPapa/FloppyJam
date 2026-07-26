/* main.c — the entry point, and the only place the two platforms differ.
 *
 * §0: one app_frame() from day one. On the web emscripten owns the loop and
 * calls us; on desktop we own it and call ourselves. Nothing below that line
 * knows which of those happened, which is the whole reason the web build has
 * never been a port.
 */
#include "raylib.h"
#include "app.h"

#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static VbApp G;

/* --shot: render N frames, save one, exit. CI uses it to prove the renderer
 * still produces a frame on a headless runner, and it is how a beauty-pass
 * change gets looked at without anyone having to be at the machine. */
static int g_shot_after = 0, g_shot_taken = 0;
static const char *g_shot_path = "shot.png";
static int g_frames = 0;

static void frame(void) {
    app_frame(&G);
    if (g_shot_after && ++g_frames >= g_shot_after && !g_shot_taken) {
        TakeScreenshot(g_shot_path);
        g_shot_taken = 1;
        G.running = 0;
    }
}

int main(int argc, char **argv) {
    int graybox = 0, demo = 0;
    for (int i = 1; i < argc; i++) {
        /* DoD 6: the gray-box build is retained forever, behind this flag —
         * proof that the fun predates the glow. */
        if (!strcmp(argv[i], "--graybox")) graybox = 1;
        else if (!strcmp(argv[i], "--demo")) demo = 1;
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc)
            g_shot_after = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i + 1 < argc)
            g_shot_path = argv[++i];
        else if (!strcmp(argv[i], "--help")) {
            TraceLog(LOG_INFO, "volleybar [--graybox] [--demo] [--shot N] [--out FILE]");
            return 0;
        }
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "VOLLEYBAR");
    SetWindowMinSize(800, 480);
    SetExitKey(KEY_NULL);          /* ESC is a menu key here, not a quit key */

    app_init(&G, graybox);
    if (demo) app_start_demo(&G);

#if defined(PLATFORM_WEB)
    /* 0 = use the browser's own rAF cadence, which is what we want */
    emscripten_set_main_loop(frame, 0, 1);
#else
    SetTargetFPS(0);               /* vsync paces us; the sim is fixed-step */
    while (G.running && !WindowShouldClose()) frame();
    app_shutdown(&G);
    CloseWindow();
#endif
    return 0;
}
