#include "audio.h"
#include "control.h"
#include "i18n.h"
#include "headless.h"
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
static float boot_time,damage_audio_time,camera_yaw,camera_pitch=-.32f;
static bool debug,collision,gallery,exit_requested;
static char entry[7];
static int entry_len,entry_cursor;

static uint64_t entropy(void){return (uint64_t)time(0)^(uint64_t)(GetTime()*1000000.0);}
static void apply_settings(void){SetMasterVolume(save.volume/100.0f);if((bool)save.fullscreen!=IsWindowFullscreen())ToggleFullscreen();}
static void open_seed(void){memcpy(entry,save.seed,7);entry_len=6;entry_cursor=0;state=ST_SEED;}
static void start_run(void){sfbs_game_init(&game,save.seed,(GameMode)save.mode);sfbs_audio_restart(&audio,&game.song);damage_audio_time=0;camera_yaw=0;camera_pitch=-.32f;state=ST_PLAY;if(save.first_person)DisableCursor();else EnableCursor();}
static bool confirm(void){return IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)||(IsGamepadAvailable(0)&&IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN));}
static bool back(void){return IsKeyPressed(KEY_ESCAPE)||(IsGamepadAvailable(0)&&IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));}
static bool pause_pressed(void){return IsKeyPressed(KEY_P)||(IsGamepadAvailable(0)&&IsGamepadButtonPressed(0,GAMEPAD_BUTTON_MIDDLE_RIGHT));}
static void screen_begin(void){BeginTextureMode(target);ClearBackground(BLACK);}
static void screen_end(void){EndTextureMode();BeginDrawing();ClearBackground(BLACK);float s=fminf(GetScreenWidth()/640.0f,GetScreenHeight()/360.0f);Rectangle src={0,0,640,-360},dst={(GetScreenWidth()-640*s)/2,(GetScreenHeight()-360*s)/2,640*s,360*s};DrawTexturePro(target.texture,src,dst,(Vector2){0,0},0,WHITE);EndDrawing();}
static Language language(void){return(Language)save.language;}
static void menu_text(const char*sub){Palette p=sfbs_palette(game.genome.palette);DrawRectangle(56,42,528,92,ColorAlpha(p.bg,.78f));DrawRectangleLinesEx((Rectangle){56,42,528,92},1,ColorAlpha(p.active,.45f));sfbs_center_text(sfbs_text(language(),TXT_TITLE),64,28,p.player);sfbs_center_text(sub,112,16,p.active);}
static void action_text(const char*s,int y,Palette p){const Color colors[4]={p.active,p.stable,p.accent,p.edge};const char*part=s;int widths[4]={0},count=0,total=0;while(*part&&count<4){const char*end=strstr(part,"   ");int n=end?(int)(end-part):(int)strlen(part);char item[48];if(n>(int)sizeof(item)-1)n=(int)sizeof(item)-1;memcpy(item,part,(size_t)n);item[n]=0;widths[count]=MeasureText(item,14);total+=widths[count]+(count?20:0);count++;if(!end)break;part=end+3;}int x=320-total/2;part=s;for(int i=0;i<count;i++){const char*end=strstr(part,"   ");int n=end?(int)(end-part):(int)strlen(part);char item[48];if(n>(int)sizeof(item)-1)n=(int)sizeof(item)-1;memcpy(item,part,(size_t)n);item[n]=0;DrawText(item,x,y,14,colors[i]);x+=widths[i]+20;part=end?end+3:part+n;}}
static void cycle_language(void){save.language=(save.language+1)%LANG_COUNT;sfbs_save_write("sfbs.sav",&save);}
static void cycle_seed_char(int delta){int count=(int)strlen(seed_alpha),old=(int)(strchr(seed_alpha,entry[entry_cursor])-seed_alpha);old=(old+delta+count)%count;entry[entry_cursor]=seed_alpha[old];}

int main(int argc,char**argv){
 if(argc>1&&!strcmp(argv[1],"--control-test")){ControlStatus control=sfbs_control_verify();sfbs_control_json(&control);return control.failures?1:0;}
 if(argc>1&&(!strcmp(argv[1],"--validate")||!strcmp(argv[1],"--headless-test")||!strcmp(argv[1],"--status-json"))){HeadlessStatus status=sfbs_headless_verify();sfbs_status_json(&status);return status.failures?1:0;}
 SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT);InitWindow(1280,720,"Songs From Bad Sectors");SetTargetFPS(60);
 target=LoadRenderTexture(640,360);SetTextureFilter(target.texture,TEXTURE_FILTER_POINT);
 sfbs_save_load("sfbs.sav",&save);if(save.tutorial)state=ST_TITLE;apply_settings();sfbs_game_init(&game,save.seed,(GameMode)save.mode);sfbs_audio_open(&audio,&game.song);
 while(!WindowShouldClose()&&!exit_requested){
  float dt=GetFrameTime();
  if(IsKeyPressed(KEY_F11)){save.fullscreen=!save.fullscreen;ToggleFullscreen();sfbs_save_write("sfbs.sav",&save);}
  if(IsKeyPressed(KEY_MINUS)&&save.volume>=5){save.volume-=5;SetMasterVolume(save.volume/100.0f);}
  if(IsKeyPressed(KEY_EQUAL)&&save.volume<=95){save.volume+=5;SetMasterVolume(save.volume/100.0f);}
  if(state!=ST_PLAY&&state!=ST_SEED&&(IsKeyPressed(KEY_L)||(IsGamepadAvailable(0)&&IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_UP))))cycle_language();
  if(state!=ST_SEED&&(IsKeyPressed(KEY_V)||(IsGamepadAvailable(0)&&IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_THUMB)))){save.first_person=!save.first_person;sfbs_save_write("sfbs.sav",&save);if(state==ST_PLAY&&save.first_person)DisableCursor();else EnableCursor();}
#ifdef SFBS_DEBUG
  if(IsKeyPressed(KEY_F1))debug=!debug;if(IsKeyPressed(KEY_F2))collision=!collision;if(IsKeyPressed(KEY_F3))game.freeze_echo=!game.freeze_echo;if(IsKeyPressed(KEY_F4))gallery=!gallery;
  if(IsKeyPressed(KEY_F5)){sfbs_force_damage(&game);sfbs_audio_damage(&audio);damage_audio_time=1.5f;}
  if(IsKeyPressed(KEY_F6))for(int i=0;i<game.node_count;i++)if(game.nodes[i].state!=NODE_EXPIRED&&game.nodes[i].state!=NODE_RECOVERED){game.nodes[i].state=NODE_RECOVERED;game.recovered+=game.nodes[i].weight;}
  if(IsKeyPressed(KEY_F7)){int next=game.phrase_index+1;while(next<SFBS_PHRASES&&game.phrases[next].phase==game.phrases[game.phrase_index].phase)next++;if(next<SFBS_PHRASES){sfbs_audio_pause(&audio,true);audio.clock=sfbs_tick_sample((uint64_t)game.phrases[next].start_bar*4*SFBS_PPQN,game.genome.bpm);sfbs_audio_pause(&audio,false);}}
  if(IsKeyPressed(KEY_F8)){sfbs_random_seed(entropy(),save.seed);start_run();}
  if(IsKeyPressed(KEY_F9))printf("{\"seed\":\"%s\",\"bpm\":%d,\"root\":%d,\"scale\":%d,\"progression\":%d,\"palette\":%d}\n",game.seed,game.genome.bpm,game.genome.root,game.genome.scale,game.genome.progression,game.genome.palette);
  if(IsKeyPressed(KEY_LEFT_BRACKET))game.echo_delay=fmaxf(2,game.echo_delay-1);if(IsKeyPressed(KEY_RIGHT_BRACKET))game.echo_delay=fminf(12,game.echo_delay+1);
  if(IsKeyPressed(KEY_SEMICOLON))game.echo_width=fmaxf(4,game.echo_width-1);if(IsKeyPressed(KEY_APOSTROPHE))game.echo_width=fminf(30,game.echo_width+1);
  for(int i=0;i<4;i++)if(IsKeyPressed(KEY_ONE+i))sfbs_audio_stem(&audio,i,audio.stem_target[i]==0);
#endif
  if(state==ST_PLAY&&!IsWindowFocused()){sfbs_audio_pause(&audio,true);EnableCursor();state=ST_PAUSE;}
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
    if(back()||pause_pressed()){sfbs_audio_pause(&audio,true);EnableCursor();state=ST_PAUSE;break;}
    V2 in={(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT))-(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)),(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN))-(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))};
    if(IsGamepadAvailable(0)){float sx=GetGamepadAxisMovement(0,GAMEPAD_AXIS_LEFT_X),sy=GetGamepadAxisMovement(0,GAMEPAD_AXIS_LEFT_Y);if(fabsf(sx)<.16f)sx=0;if(fabsf(sy)<.16f)sy=0;in.x+=sx;in.y+=sy;}
    if(save.first_person){Vector2 mouse=GetMouseDelta();float turn=in.x*2.35f*dt+mouse.x*.0022f,tilt=-mouse.y*.0018f;if(IsGamepadAvailable(0)){float rx=GetGamepadAxisMovement(0,GAMEPAD_AXIS_RIGHT_X),ry=GetGamepadAxisMovement(0,GAMEPAD_AXIS_RIGHT_Y);if(fabsf(rx)<.14f)rx=0;if(fabsf(ry)<.14f)ry=0;turn+=rx*2.5f*dt;tilt-=ry*2.0f*dt;}camera_yaw+=turn;if(camera_yaw>SFBS_PI)camera_yaw-=2*SFBS_PI;if(camera_yaw<-SFBS_PI)camera_yaw+=2*SFBS_PI;camera_pitch=sfbs_clampf(camera_pitch+tilt,-.78f,.05f);float forward=-in.y;in.x=forward*sinf(camera_yaw);in.y=forward*cosf(camera_yaw);}
    bool pulse=IsKeyPressed(KEY_SPACE)||IsKeyPressed(KEY_Z)||IsKeyPressed(KEY_ENTER)||(IsGamepadAvailable(0)&&(IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN)||GetGamepadAxisMovement(0,GAMEPAD_AXIS_RIGHT_TRIGGER)>.5f));
    int integrity=game.integrity,recovered=game.recovered;sfbs_game_step(&game,dt,in,pulse,sfbs_audio_clock(&audio));
    Phase phase=(Phase)game.phrases[game.phrase_index].phase;sfbs_audio_phase(&audio,phase,false,sfbs_recovery(&game)/100.0f);
    if(game.integrity<integrity||game.recovered<recovered){damage_audio_time=1.5f;sfbs_audio_damage(&audio);}if(damage_audio_time>0){damage_audio_time-=dt;sfbs_audio_damage(&audio);}
    if(game.ended){sfbs_audio_phase(&audio,PH_FINAL,!game.won,sfbs_recovery(&game)/100.0f);save.tutorial=1;sfbs_save_write("sfbs.sav",&save);EnableCursor();state=ST_RESULTS;}
   }break;
   case ST_PAUSE:
    if(confirm()||back()||pause_pressed()){sfbs_audio_pause(&audio,false);state=ST_PLAY;if(save.first_person)DisableCursor();}else if(IsKeyPressed(KEY_R))start_run();
    break;
   case ST_RESULTS:
    if(IsKeyPressed(KEY_R)||confirm())start_run();else if(IsKeyPressed(KEY_N)){sfbs_random_seed(entropy(),save.seed);start_run();}else if(IsKeyPressed(KEY_M)){sfbs_audio_pause(&audio,true);state=ST_MODE;}else if(IsKeyPressed(KEY_S)){sfbs_audio_pause(&audio,true);open_seed();}else if(back()){sfbs_audio_pause(&audio,true);state=ST_TITLE;}
    break;
   case ST_CREDITS:if(confirm()||back())state=ST_TITLE;break;
  }
  screen_begin();Palette p=sfbs_palette(game.genome.palette);if(state!=ST_PLAY&&state!=ST_PAUSE&&state!=ST_RESULTS&&state!=ST_BOOT)sfbs_render_backdrop(&game,camera_yaw);
  if(state==ST_PLAY||state==ST_PAUSE||state==ST_RESULTS){
   sfbs_render_game(&game,sfbs_transport(sfbs_audio_clock(&audio),game.genome.bpm),debug,collision,save.first_person,camera_yaw,camera_pitch,sfbs_text(language(),save.first_person?TXT_VIEW_FIRST:TXT_VIEW_OVERHEAD));
   if(state==ST_PLAY&&!save.tutorial){if(game.phrase_index==0)sfbs_center_text(sfbs_text(language(),TXT_TUTORIAL_MOVE),315,11,p.player);else if(game.phrase_index==1)sfbs_center_text(sfbs_text(language(),TXT_TUTORIAL_PULSE),315,11,p.active);else if(game.phrase_index==2){sfbs_center_text(sfbs_text(language(),TXT_TUTORIAL_ECHO_1),307,11,p.edge);sfbs_center_text(sfbs_text(language(),TXT_TUTORIAL_ECHO_2),321,10,p.edge);}}
   if(state==ST_PAUSE){DrawRectangle(0,0,640,360,ColorAlpha(BLACK,.65f));sfbs_center_text(sfbs_text(language(),TXT_PAUSED),145,28,p.player);sfbs_center_text(sfbs_text(language(),TXT_PAUSE_HELP),190,13,p.active);}
   else if(state==ST_RESULTS){DrawRectangle(0,0,640,360,ColorAlpha(p.bg,.78f));sfbs_center_text(sfbs_text(language(),game.won?TXT_TRACK_RECOVERED:TXT_SIGNAL_LOST),80,27,p.player);char x[80];snprintf(x,sizeof x,"%s  /  %s  /  %d%%",game.seed,sfbs_mode_name(game.mode),sfbs_recovery(&game));sfbs_center_text(x,130,18,p.active);sfbs_center_text(sfbs_result_text(language(),sfbs_recovery(&game)),160,14,p.stable);sfbs_center_text(sfbs_text(language(),TXT_RESULTS_HELP),220,13,p.player);}
  }else if(state==ST_BOOT){sfbs_center_text("A:\\> READ TRACK01.DAT",120,15,p.stable);sfbs_center_text(boot_time<.8f?"SCANNING...":boot_time<1.7f?"BAD SECTORS DETECTED":"SIGNAL FOUND",155,16,p.active);}
  else if(state==ST_TITLE){menu_text(save.seed);DrawRectangle(70,158,500,151,ColorAlpha(p.bg,.74f));action_text(sfbs_text(language(),TXT_TITLE_ACTIONS),180,p);char selected[48];snprintf(selected,sizeof selected,"MODE: %s",sfbs_mode_name((GameMode)save.mode));sfbs_center_text(selected,207,11,p.active);sfbs_center_text(sfbs_text(language(),TXT_CONTROLS),230,12,p.stable);sfbs_center_text(sfbs_text(language(),TXT_VIEW_HELP),251,10,p.stable);char settings[128];snprintf(settings,sizeof settings,"%s   %s   /   %s   /   VOL %d   /   F11",sfbs_text(language(),TXT_LANGUAGE),sfbs_language_name(language()),sfbs_text(language(),save.first_person?TXT_VIEW_FIRST:TXT_VIEW_OVERHEAD),save.volume);sfbs_center_text(settings,282,10,p.accent);}
  else if(state==ST_MODE){menu_text(sfbs_text(language(),TXT_SELECT_MODE));sfbs_center_text(sfbs_mode_name((GameMode)save.mode),143,27,p.active);TextId d1=save.mode==MODE_BLOOM?TXT_MODE_BLOOM_1:save.mode==MODE_FLOW?TXT_MODE_FLOW_1:TXT_MODE_PULSE_1,d2=(TextId)(d1+1);sfbs_center_text(sfbs_text(language(),d1),180,11,p.stable);sfbs_center_text(sfbs_text(language(),d2),195,10,p.stable);sfbs_center_text(sfbs_text(language(),TXT_ECHO_1),220,11,p.edge);sfbs_center_text(sfbs_text(language(),TXT_ECHO_2),235,10,p.edge);char view[96];snprintf(view,sizeof view,"V: %s",sfbs_text(language(),save.first_person?TXT_VIEW_FIRST:TXT_VIEW_OVERHEAD));sfbs_center_text(view,258,11,p.accent);sfbs_center_text(sfbs_text(language(),TXT_MODE_HELP),282,12,p.player);sfbs_center_text(sfbs_language_name(language()),309,9,p.accent);}
  else if(state==ST_SEED){menu_text(sfbs_text(language(),TXT_SEED_TITLE));sfbs_center_text(entry_len?entry:"______",160,34,p.active);if(entry_len==6)DrawRectangle(320-MeasureText(entry,34)/2+entry_cursor*MeasureText("A",34),198,MeasureText("A",34),2,p.accent);sfbs_center_text(sfbs_text(language(),TXT_SEED_HELP),225,11,p.player);}
  else{menu_text(sfbs_text(language(),TXT_CREDITS));sfbs_center_text(sfbs_text(language(),TXT_DESIGNED),150,13,p.active);sfbs_center_text(sfbs_text(language(),TXT_BUILT),178,12,p.stable);sfbs_center_text(sfbs_text(language(),TXT_SOURCE_LICENSE),200,12,p.stable);}
  if(gallery)for(int i=0;i<8;i++){Palette q=sfbs_palette(i);DrawRectangle(10+i*78,310,70,20,q.stable);}screen_end();
 }
 sfbs_save_write("sfbs.sav",&save);sfbs_audio_close(&audio);UnloadRenderTexture(target);CloseWindow();return 0;
}
