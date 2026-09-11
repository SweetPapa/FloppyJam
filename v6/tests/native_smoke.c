/* A real raylib window, renderer and app state machine with deterministic input.
 * These definitions replace only input polling when linking this test executable;
 * the shipped game still polls raylib normally. No operating-system input access. */
#include "raylib.h"
#include "core/app.h"
#include "art/artkit.h"
#include "board/board.h"
#include "content/content.h"
#include "dlg/dlg.h"
#include "flags/flags.h"
#include "investigation/investigation.h"
#include "puzzle/puzzle.h"
#include "save/save.h"
#include "scene/scene.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static Vector2 mouse={-100,-100};static bool click;static int key;static float wheel;
bool IsMouseButtonPressed(int button){return button==MOUSE_BUTTON_LEFT&&click;}
bool IsMouseButtonDown(int button){return button==MOUSE_BUTTON_LEFT&&click;}
bool IsMouseButtonReleased(int button){return false;}
bool IsKeyPressed(int k){return k==key&&key!=0;}
bool IsKeyDown(int k){return k==key&&key!=0;}
float GetMouseWheelMove(void){return wheel;}
Vector2 GetMousePosition(void){float s=fminf(GetScreenWidth()/1280.f,GetScreenHeight()/720.f);return (Vector2){(GetScreenWidth()-1280*s)/2+mouse.x*s,(GetScreenHeight()-720*s)/2+mouse.y*s};}
static int failures,checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL native %d: %s\n",__LINE__,#x);failures++;}}while(0)
static void frames(int n){for(int i=0;i<n;i++){app_frame(1.f/60);click=false;key=0;wheel=0;}}
static void tap(float x,float y){mouse=(Vector2){x,y};click=true;frames(2);}
static void press(int k){key=k;frames(2);}
static void shot(const char *name){frames(2);TakeScreenshot(name);}
static void fill_board(const char *id){
    Block b;CHECK(content_block("board",id,&b));Cursor c;cur_open(&c,&b);char line[512];
    while(cur_line(&c,line,sizeof line)){const char *p=line;char kw[24],name[24],answer[48];word(&p,kw,sizeof kw);if(!eq(kw,"blank"))continue;word(&p,name,sizeof name);word(&p,kw,sizeof kw);word(&p,answer,sizeof answer);CHECK(board_place(name,answer));}
}
int main(void){
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);InitWindow(1280,720,"HUEDUNIT native play checks");SetExitKey(KEY_NULL);SetTargetFPS(0);
    save_allow_writes(false);app_init();settings()->reduce_motion=1;
    CHECK(app_debug_goto("scene:p_gate"));frames(3);
    /* Walk to both physical observations, navigate through the casebook, reason,
     * and collect a reward once. The click positions are what a player sees. */
    tap(520,330);frames(100);CHECK(small_case_seen(0,0));CHECK(scene_inspecting());
    press(KEY_SPACE);CHECK(!scene_inspecting());
    flag_set("seen.p_square",1);
    press(KEY_M);tap(680,66);tap(740,380);frames(3);CHECK(eq(scene_current(),"p_square"));
    tap(710,350);frames(120);CHECK(small_case_seen(0,1));press(KEY_SPACE);
    press(KEY_M);tap(680,66);
    int feathers=flag_get("feathers");tap(740,488);CHECK(!small_case_solved(0));CHECK(flag_get("feathers")==feathers);
    tap(740,540);CHECK(small_case_solved(0));CHECK(flag_get("feathers")==feathers+2);
    tap(740,540);CHECK(flag_get("feathers")==feathers+2);shot("native-mystery-solved.png");
    tap(1110,66);shot("native-square-keepsake.png");
    /* Free first hint, paid next tier, modal dismissal and optional cooperation. */
    CHECK(app_debug_goto("puzzle:mirror_light"));frames(2);feathers=flag_get("feathers");
    tap(260,626);CHECK(flag_get("feathers")==feathers);CHECK(flag_get("phint.mirror_light")==1);shot("native-hint.png");
    tap(900,500);tap(260,626);CHECK(flag_get("feathers")==feathers-1);press(KEY_ESCAPE);
    tap(800,674);CHECK(flag_get("feathers")==feathers-1);press(KEY_ESCAPE);
    tap(510,626);press(KEY_ESCAPE);CHECK(!clue_has("clue.nona_key"));
    tap(510,626);tap(900,500);frames(70);CHECK(clue_has("clue.nona_key"));
    /* Every board, empty and filled, at all text sizes. */
    for(int size=0;size<3;size++)for(int i=0;i<content_block_count("board");i++){
        char id[48],spec[70];Block b;content_block_at("board",i,id,sizeof id,&b);snprintf(spec,sizeof spec,"board:%s",id);
        settings()->text_size=size;CHECK(app_debug_goto(spec));frames(2);
        CHECK(board_layout_fits());fill_board(id);frames(2);CHECK(board_layout_fits());
        if(size==2 && eq(id,"ch5_where"))shot("native-final-board-large.png");
        tap(160,637);frames(95);CHECK(board_is_solved(id));
    }
    /* The final replay in the journal is reachable and cannot alter the case. */
    settings()->text_size=0;CHECK(app_debug_goto("journal"));
    for(int i=0;i<puzzle_count();i++){char k[48];snprintf(k,sizeof k,"pzdone.%s",puzzle_at(i)->id);flag_set(k,1);}
    frames(2);tap(620,74);wheel=-20;frames(2);shot("native-journal-last-page.png");
    tap(820,567);CHECK(eq(puzzle_current_id(),puzzle_at(puzzle_count()-1)->id));
    int clues=clue_count();tap(510,626);tap(900,500);frames(70);CHECK(clue_count()==clues);CHECK(!flag_get("skipped_any"));
    /* Large dialogue choices and a smaller window still use the same mouse coordinates. */
    settings()->text_size=2;CHECK(app_debug_goto("dialogue:p_mayor_gate"));frames(2);
    for(int i=0;i<40&&!dlg_choice_count();i++)press(KEY_SPACE);
    CHECK(dlg_choice_count()==3);shot("native-dialogue-large.png");press(KEY_TWO);CHECK(dlg_choice_count()==0);
    settings()->text_size=0;CHECK(app_debug_goto("map"));SetWindowSize(960,600);frames(6);shot("native-map-small.png");
    tap(203,332);frames(4);CHECK(eq(scene_current(),"ch1_dock"));
    app_shutdown();CloseWindow();printf("native_smoke: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
