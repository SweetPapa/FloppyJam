#include "raylib.h"
#include "app.h"
#include <string.h>
#include <stdlib.h>
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif
static PwApp app;
static int shot_after,frames;static const char*shot_path="pulsewing-shot.png";
static void frame(void){app_frame(&app);if(shot_after&&++frames>=shot_after){TakeScreenshot(shot_path);app.running=0;}}
int main(int argc,char**argv){int gray=0;for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--graybox"))gray=1;else if(!strcmp(argv[i],"--shot")&&i+1<argc)shot_after=atoi(argv[++i]);else if(!strcmp(argv[i],"--out")&&i+1<argc)shot_path=argv[++i];}SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT|FLAG_VSYNC_HINT);InitWindow(1280,720,"PULSEWING");SetWindowMinSize(800,450);SetExitKey(KEY_NULL);app_init(&app,gray);
#if defined(PLATFORM_WEB)
emscripten_set_main_loop(frame,0,1);
#else
while(app.running&&!WindowShouldClose())frame();app_shutdown(&app);CloseWindow();
#endif
return 0;}
