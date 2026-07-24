/* HUEDUNIT — a cozy detective puzzle-adventure.
 * One town, one bird, one tower, no villains.
 *
 * One binary, no asset files: every stroke is drawn and every sound is
 * synthesised at startup, and the whole of the content tree is baked into the
 * executable by tools/bakery at build time.
 *
 *   huedunit                          play
 *   huedunit --shot <spec> [out.png]  jump to one screen and photograph it
 *
 * The --shot form is the capture harness the integration cards ask for.
 * F12 photographs whatever is on screen at any time.
 */
#include "raylib.h"
#include "app.h"
#include "save/save.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *shot = NULL, *shot_out = "huedunit_shot.png";
    int shot_wait = 100;             /* frames to let the screen settle */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
            shot = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') shot_out = argv[++i];
            /* a cutscene needs long enough to reach the line worth looking at */
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
                shot_wait = atoi(argv[++i]);
        }
    }

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "HUEDUNIT");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);            /* ESC is a game verb, not a quit key */

    app_init();
    if (settings()->fullscreen && !shot) ToggleFullscreen();

    int shot_frames = -1;
    if (shot) {
        if (!app_debug_goto(shot)) {
            fprintf(stderr, "huedunit: --shot: nothing called '%s'\n", shot);
            app_shutdown();
            CloseWindow();
            return 2;
        }
        shot_frames = shot_wait;     /* let idle animation and blooms settle */
    }

    while (!WindowShouldClose() && !app_wants_quit()) {
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;    /* a dragged window never fast-forwards */
        app_frame(dt);

        if (IsKeyPressed(KEY_F12)) TakeScreenshot(shot_out);
        if (shot_frames > 0 && --shot_frames == 0) {
            TakeScreenshot(shot_out);
            break;
        }
    }

    app_shutdown();
    CloseWindow();
    return 0;
}
