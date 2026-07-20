#include "audio.h"
#include "render.h"
#include "save.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef enum { ST_BOOT,ST_TITLE,ST_SEED,ST_MODE,ST_PLAY,ST_PAUSE,ST_RESULTS,ST_CREDITS } AppState;
static const char seed_alpha[]="23456789ABCDEFGHJKMNPQRSTUVWXYZ";
static RenderTexture2D target;
static Game game;
static AudioEngine audio;
static SaveData save;
static AppState state=ST_BOOT;
static float boot_time,damage_audio_time;
static bool debug,collision,gallery,exit_requested;
static char entry[7];
static int entry_len,entry_cursor;

static uint64_t entropy(void){return (uint64_t)time(0)^(uint64_t)(GetTime()*1000000.0);}
static void apply_settings(void){SetMasterVolume(save.volume/100.0f);if((bool)save.fullscreen!=IsWindowFullscreen())ToggleFullscreen();}
static void open_seed(void){memcpy(entry,save.seed,7);entry_len=6;entry_cursor=0;state=ST_SEED;}
static void start_run(void){sfbs_game_init(&game,save.seed,(GameMode)save.mode);sfbs_audio_restart(&audio,&game.song);damage_audio_time=0;state=ST_PLAY;}
static bool confirm(void){return IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)||(IsGamepadAvailable(0)&&IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN));}
static bool back(void){return IsKeyPressed(KEY_ESCAPE)||(IsGamepadAvailable(0)&&IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));}
static void screen_begin(void){BeginTextureMode(target);ClearBackground(BLACK);}
static void screen_end(void){EndTextureMode();BeginDrawing();ClearBackground(BLACK);float s=fminf(GetScreenWidth()/640.0f,GetScreenHeight()/360.0f);Rectangle src={0,0,640,-360},dst={(GetScreenWidth()-640*s)/2,(GetScreenHeight()-360*s)/2,640*s,360*s};DrawTexturePro(target.texture,src,dst,(Vector2){0,0},0,WHITE);EndDrawing();}
static void menu_text(const char*sub){Palette p=sfbs_palette(game.genome.palette);sfbs_center_text("SONGS FROM BAD SECTORS",64,28,p.player);sfbs_center_text(sub,112,16,p.active);}
static void cycle_seed_char(int delta){int count=(int)strlen(seed_alpha),old=(int)(strchr(seed_alpha,entry[entry_cursor])-seed_alpha);old=(old+delta+count)%count;entry[entry_cursor]=seed_alpha[old];}

int main(void){
 SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT);InitWindow(1280,720,"Songs From Bad Sectors");SetTargetFPS(60);
 target=LoadRenderTexture(640,360);SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);
 sfbs_save_load("sfbs.sav",&save);apply_settings();sfbs_game_init(&game,save.seed,(GameMode)save.mode);sfbs_audio_open(&audio,&game.song);
 while(!WindowShouldClose()&&!exit_requested){
  float dt=GetFrameTime();
  if(IsKeyPressed(KEY_F11)){save.fullscreen=!save.fullscreen;ToggleFullscreen();sfbs_save_write("sfbs.sav",&save);}
  if(IsKeyPressed(KEY_MINUS)&&save.volume>=5){save.volume-=5;SetMasterVolume(save.volume/100.0f);}
  if(IsKeyPressed(KEY_EQUAL)&&save.volume<=95){save.volume+=5;SetMasterVolume(save.volume/100.0f);}
#ifdef SFBS_DEBUG
  if(IsKeyPressed(KEY_F1))debug=!debug;if(IsKeyPressed(KEY_F2))collision=!collision;if(IsKeyPressed(KEY_F3))game.freeze_echo=!game.freeze_echo;if(IsKeyPressed(KEY_F4))gallery=!gallery;
  if(IsKeyPressed(KEY_F5)){sfbs_force_damage(&game);sfbs_audio_damage(&audio);damage_audio_time=1.5f;}
  if(IsKeyPressed(KEY_F6))for(int i=0;i<game.node_count;i++)if(game.nodes[i].state!=NODE_EXPIRED&&game.nodes[i].state!=NODE_RECOVERED){game.nodes[i].state=NODE_RECOVERED;game.recovered+=game.nodes[i].weight;}
  if(IsKeyPressed(KEY_F7)){sfbs_audio_pause(&audio,true);audio.clock+=sfbs_tick_sample(16*SFBS_PPQN,game.genome.bpm);sfbs_audio_pause(&audio,false);}
  if(IsKeyPressed(KEY_F8)){sfbs_random_seed(entropy(),save.seed);start_run();}
  for(int i=0;i<4;i++)if(IsKeyPressed(KEY_ONE+i))sfbs_audio_stem(&audio,i,audio.stem_target[i]==0);
#endif
  switch(state){
   case ST_BOOT:boot_time+=dt;if(boot_time>2.6f||confirm())state=ST_TITLE;break;
   case ST_TITLE:
    if(confirm())state=ST_MODE;else if(IsKeyPressed(KEY_S))open_seed();else if(IsKeyPressed(KEY_C))state=ST_CREDITS;else if(IsKeyPressed(KEY_Q)||back())exit_requested=true;
    break;
   case ST_MODE:
    if(IsKeyPressed(KEY_LEFT)||(IsGamepadAvailable(0)&&IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_LEFT)))save.mode=(save.mode+2)%3;
    if(IsKeyPressed(KEY_RIGHT)||(IsGamepadAvailable(0)&&IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_RIGHT)))save.mode=(save.mode+1)%3;
    if(confirm()){sfbs_save_write("sfbs.sav",&save);start_run();}else if(back())state=ST_TITLE;
    break;
   case ST_SEED:{
    int ch=GetCharPressed();while(ch){char c=(char)toupper((unsigned char)ch);if(strchr(seed_alpha,c)&&entry_len<6){entry[entry_len++]=c;entry[entry_len]=0;entry_cursor=entry_len%6;}ch=GetCharPressed();}
    if(IsKeyPressed(KEY_BACKSPACE)&&entry_len){entry[--entry_len]=0;entry_cursor=entry_len?entry_len-1:0;}
    if(IsKeyPressed(KEY_R)){sfbs_random_seed(entropy(),entry);entry_len=6;entry_cursor=0;}
    if(IsGamepadAvailable(0)&&entry_len==6){if(IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_LEFT))entry_cursor=(entry_cursor+5)%6;if(IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_RIGHT))entry_cursor=(entry_cursor+1)%6;if(IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_UP))cycle_seed_char(1);if(IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_DOWN))cycle_seed_char(-1);}
    if(confirm()&&entry_len){sfbs_seed_normalize(entry,save.seed);state=ST_MODE;}else if(back())state=ST_TITLE;
   }break;
   case ST_PLAY:{
    if(back()||IsKeyPressed(KEY_P)){sfbs_audio_pause(&audio,true);state=ST_PAUSE;break;}
    V2 in={(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT))-(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)),(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN))-(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))};
    if(IsGamepadAvailable(0)){in.x+=GetGamepadAxisMovement(0,GAMEPAD_AXIS_LEFT_X);in.y+=GetGamepadAxisMovement(0,GAMEPAD_AXIS_LEFT_Y);}
    bool pulse=IsKeyPressed(KEY_SPACE)||IsKeyPressed(KEY_Z)||IsKeyPressed(KEY_ENTER)||(IsGamepadAvailable(0)&&(IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN)||GetGamepadAxisMovement(0,GAMEPAD_AXIS_RIGHT_TRIGGER)>.5f));
    int integrity=game.integrity,recovered=game.recovered;sfbs_game_step(&game,dt,in,pulse,sfbs_audio_clock(&audio));
    Phase phase=(Phase)game.phrases[game.phrase_index].phase;sfbs_audio_phase(&audio,phase,false);
    if(game.integrity<integrity||game.recovered<recovered){damage_audio_time=1.5f;sfbs_audio_damage(&audio);}if(damage_audio_time>0){damage_audio_time-=dt;sfbs_audio_damage(&audio);}
    if(game.ended){sfbs_audio_phase(&audio,PH_FINAL,!game.won);save.tutorial=1;sfbs_save_write("sfbs.sav",&save);state=ST_RESULTS;}
   }break;
   case ST_PAUSE:
    if(confirm()||back()||IsKeyPressed(KEY_P)){sfbs_audio_pause(&audio,false);state=ST_PLAY;}else if(IsKeyPressed(KEY_R))start_run();
    break;
   case ST_RESULTS:
    if(IsKeyPressed(KEY_R)||confirm())start_run();else if(IsKeyPressed(KEY_N)){sfbs_random_seed(entropy(),save.seed);start_run();}else if(IsKeyPressed(KEY_M)){sfbs_audio_pause(&audio,true);state=ST_MODE;}else if(IsKeyPressed(KEY_S)){sfbs_audio_pause(&audio,true);open_seed();}else if(back()){sfbs_audio_pause(&audio,true);state=ST_TITLE;}
    break;
   case ST_CREDITS:if(confirm()||back())state=ST_TITLE;break;
  }
  screen_begin();Palette p=sfbs_palette(game.genome.palette);
  if(state==ST_PLAY||state==ST_PAUSE||state==ST_RESULTS){
   sfbs_render_game(&game,sfbs_transport(sfbs_audio_clock(&audio),game.genome.bpm),debug,collision);
   if(state==ST_PLAY&&!save.tutorial){if(game.phrase_index==0)sfbs_center_text("MOVE WITH WASD / ARROWS",315,11,p.player);else if(game.phrase_index==1)sfbs_center_text("SPACE / Z TO PULSE",315,11,p.active);else if(game.phrase_index==2)sfbs_center_text("YOUR OLD PATH BECOMES THE ECHO",315,11,p.edge);}
   if(state==ST_PAUSE){DrawRectangle(0,0,640,360,ColorAlpha(BLACK,.65f));sfbs_center_text("READ PAUSED",145,28,p.player);sfbs_center_text("ENTER RESUME   R RESTART",190,13,p.active);}
   else if(state==ST_RESULTS){DrawRectangle(0,0,640,360,ColorAlpha(p.bg,.78f));sfbs_center_text(game.won?"TRACK RECOVERED":"SIGNAL LOST",80,27,p.player);char x[80];snprintf(x,sizeof x,"%s  /  %s  /  %d%%",game.seed,sfbs_mode_name(game.mode),sfbs_recovery(&game));sfbs_center_text(x,130,18,p.active);sfbs_center_text(sfbs_result_label(sfbs_recovery(&game)),160,14,p.stable);sfbs_center_text("R REPLAY   N NEW   M MODE   S SEED",220,13,p.player);}
  }else if(state==ST_BOOT){sfbs_center_text("A:\\> READ TRACK01.DAT",120,15,p.stable);sfbs_center_text(boot_time<.8f?"SCANNING...":boot_time<1.7f?"BAD SECTORS DETECTED":"SIGNAL FOUND",155,16,p.active);}
  else if(state==ST_TITLE){menu_text(save.seed);sfbs_center_text("ENTER PLAY   S SEED   C CREDITS   Q EXIT",180,15,p.player);sfbs_center_text("WASD / ARROWS MOVE     SPACE PULSE",250,12,p.stable);char settings[64];snprintf(settings,sizeof settings,"MODE %s   VOLUME %d   F11 FULLSCREEN",sfbs_mode_name((GameMode)save.mode),save.volume);sfbs_center_text(settings,282,10,p.accent);}
  else if(state==ST_MODE){menu_text("SELECT PRESSURE");sfbs_center_text(sfbs_mode_name((GameMode)save.mode),165,30,p.active);sfbs_center_text("LEFT / RIGHT     ENTER BEGIN",225,13,p.player);}
  else if(state==ST_SEED){menu_text("ENTER SIX-CHARACTER SEED");sfbs_center_text(entry_len?entry:"______",160,34,p.active);if(entry_len==6)DrawRectangle(320-MeasureText(entry,34)/2+entry_cursor*MeasureText("A",34),198,MeasureText("A",34),2,p.accent);sfbs_center_text("TYPE OR D-PAD CYCLE   R RANDOM   ENTER ACCEPT",225,11,p.player);}
  else{menu_text("CREDITS");sfbs_center_text("DESIGNED FROM REQ_DESIGN.MD",150,13,p.active);sfbs_center_text("BUILT WITH RAYLIB 6.0 (ZLIB LICENSE)",178,12,p.stable);sfbs_center_text("SOURCE LICENSE: GPL-3.0",200,12,p.stable);}
  if(gallery)for(int i=0;i<8;i++){Palette q=sfbs_palette(i);DrawRectangle(10+i*78,310,70,20,q.stable);}screen_end();
 }
 sfbs_save_write("sfbs.sav",&save);sfbs_audio_close(&audio);UnloadRenderTexture(target);CloseWindow();return 0;
}
