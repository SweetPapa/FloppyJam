#include "app.h"
#include "routes.h"
#include "rlgl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static Color mix(Color a,Color b,float t){return (Color){(unsigned char)pw_lerp(a.r,b.r,t),(unsigned char)pw_lerp(a.g,b.g,t),(unsigned char)pw_lerp(a.b,b.b,t),255};}
static Sound tone(float hz,float sec,float decay){
    int n=(int)(44100*sec);short *d=MemAlloc((size_t)n*sizeof *d);
    for(int i=0;i<n;i++){float t=i/44100.0f,e=powf(1.0f-t/sec,decay);float q=sinf(6.283185f*hz*t)+.28f*sinf(6.283185f*hz*2.01f*t);d[i]=(short)(q*e*9000);}
    Wave w={n,44100,16,1,d};Sound s=LoadSoundFromWave(w);MemFree(d);return s;
}
static void audio_start(PwApp*a){if(a->audio_ready)return;InitAudioDevice();if(!IsAudioDeviceReady())return;a->pulse=tone(150,.16f,2);a->gate=tone(420,.20f,2);a->perfect=tone(780,.30f,2);a->crash=tone(64,.42f,.8f);a->laser=tone(920,.07f,2);a->deflect=tone(1180,.32f,2);a->warning=tone(210,.20f,1.5f);a->audio_ready=1;}
static int tapped(void){return IsKeyPressed(KEY_SPACE)||IsMouseButtonPressed(MOUSE_BUTTON_LEFT)||(IsGamepadAvailable(0)&&IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN));}
static void particle(PwApp*a,Vector3 p,Vector3 v,Color c,float life,float size){PwParticle*q=&a->particles[a->particle_at++%512];*q=(PwParticle){p,v,c,life,size};}
static void burst(PwApp*a,Color c,int n,float y){for(int i=0;i<n;i++){float q=(float)i/n*6.283f+(float)GetRandomValue(0,99)/99;particle(a,(Vector3){0,y,0},(Vector3){cosf(q)*GetRandomValue(2,12),sinf(q)*GetRandomValue(2,12),(float)GetRandomValue(-8,5)},c,.35f+GetRandomValue(0,40)/100.0f,.08f+GetRandomValue(0,20)/100.0f);}}
static void save_progress(PwApp*a){
#if !defined(PLATFORM_WEB)
    FILE*f=fopen("pulsewing.save","wb");if(f){fwrite("PW8!",1,4,f);fwrite(&a->unlocked,sizeof(int),1,f);fwrite(a->medals,sizeof a->medals,1,f);fwrite(&a->reduce_motion,sizeof(int),1,f);fclose(f);}
#endif
}
static void load_progress(PwApp*a){
#if !defined(PLATFORM_WEB)
    FILE*f=fopen("pulsewing.save","rb");char m[4];if(f&&fread(m,1,4,f)==4&&!memcmp(m,"PW8!",4)){fread(&a->unlocked,sizeof(int),1,f);fread(a->medals,sizeof a->medals,1,f);fread(&a->reduce_motion,sizeof(int),1,f);}if(f)fclose(f);
#endif
    if(a->unlocked<1||a->unlocked>9)a->unlocked=1;
}
static void start(PwApp*a){unsigned seed=a->mode==1?20260726u:(unsigned)(1777+a->stage*97+a->tier*17);pw_sim_init(&a->sim,seed,a->mode,a->stage,a->tier);memset(&a->run,0,sizeof a->run);a->run.seed=seed;if(a->best.count&&a->best.seed==seed)pw_sim_init(&a->ghost_sim,seed,a->mode,a->stage,a->tier);a->acc=0;a->callout_t=2.2f;snprintf(a->callout,sizeof a->callout,"%s",a->mode? (a->mode==2?"THE MONASTERY":"TODAY'S SKY"):PW_STAGES[a->stage].name);a->state=PW_PLAY;}
void app_init(PwApp*a,int graybox){memset(a,0,sizeof *a);a->running=1;a->graybox=graybox;a->unlocked=1;load_progress(a);a->camera.up=(Vector3){0,1,0};a->camera.fovy=72;a->camera.projection=CAMERA_PERSPECTIVE;}
void app_shutdown(PwApp*a){save_progress(a);if(a->audio_ready){UnloadSound(a->pulse);UnloadSound(a->gate);UnloadSound(a->perfect);UnloadSound(a->crash);UnloadSound(a->laser);UnloadSound(a->deflect);UnloadSound(a->warning);CloseAudioDevice();}}
static void input(PwApp*a,PwInput*in){
    memset(in,0,sizeof *in);in->pulse=tapped();in->fire=(IsKeyDown(KEY_F)||IsMouseButtonDown(MOUSE_BUTTON_RIGHT)||a->autofire);in->roll=IsKeyPressed(KEY_R);
    in->drift=(IsKeyDown(KEY_RIGHT)||IsKeyDown(KEY_D))-(IsKeyDown(KEY_LEFT)||IsKeyDown(KEY_A));
    if(IsGamepadAvailable(0)){float x=GetGamepadAxisMovement(0,GAMEPAD_AXIS_LEFT_X);if(fabsf(x)>.2f)in->drift=x>0?1:-1;in->fire|=IsGamepadButtonDown(0,GAMEPAD_BUTTON_RIGHT_FACE_LEFT);in->roll|=IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);}
    if(a->mode==2){in->fire=0;in->roll=0;in->drift=0;}
}
static void events(PwApp*a){
    for(int i=0;i<a->sim.event_count;i++){PwEvent*e=&a->sim.ev[i];
        if(e->type==PW_EV_PULSE){if(a->audio_ready)PlaySound(a->pulse);for(int k=0;k<14;k++)particle(a,(Vector3){a->sim.x,a->sim.y,-1},(Vector3){GetRandomValue(-3,3),GetRandomValue(-2,2),-GetRandomValue(8,22)},(Color){108,235,255,255},.45f,.08f);}
        if(e->type==PW_EV_GATE&&a->audio_ready){SetSoundPitch(a->gate,1.0f+.08f*(a->sim.combo-1));PlaySound(a->gate);}
        if(e->type==PW_EV_PERFECT){if(a->audio_ready){SetSoundPitch(a->perfect,1+.06f*e->value);PlaySound(a->perfect);}burst(a,GOLD,36,a->sim.y);a->flash=.16f;a->callout_t=.8f;snprintf(a->callout,sizeof a->callout,"PERFECT  x%d",e->value);}
        if(e->type==PW_EV_SHOT){a->laser_t=.07f;if(a->audio_ready)PlaySound(a->laser);}
        if(e->type==PW_EV_KILL){burst(a,(Color){255,75,120,255},28,e->y);a->callout_t=.7f;snprintf(a->callout,sizeof a->callout,e->value?"DRONE CUT":"BARRICADE SHATTERED");}
        if(e->type==PW_EV_WARN){if(a->audio_ready)PlaySound(a->warning);a->callout_t=.7f;snprintf(a->callout,sizeof a->callout,"INCOMING // ROLL");}
        if(e->type==PW_EV_DEFLECT){if(a->audio_ready)PlaySound(a->deflect);a->flash=.12f;a->shake=a->reduce_motion?0:.25f;burst(a,GOLD,50,a->sim.y);a->callout_t=1;snprintf(a->callout,sizeof a->callout,"DEFLECT +300");}
        if(e->type==PW_EV_HIT){a->shake=a->reduce_motion?0:.45f;a->callout_t=1;snprintf(a->callout,sizeof a->callout,"HULL BREACH // %d LEFT",e->value);}
        if(e->type==PW_EV_SECRET){burst(a,VIOLET,55,a->sim.y);a->callout_t=1.2f;snprintf(a->callout,sizeof a->callout,"HIGH ROAD FOUND");}
        if(e->type==PW_EV_CRASH){if(a->audio_ready)PlaySound(a->crash);burst(a,(Color){255,91,70,255},90,a->sim.y);a->shake=a->reduce_motion?0:.7f;a->deaths++;}
        if(e->type==PW_EV_STAGE){int m=pw_medal(a->sim.score,a->sim.gates,a->sim.perfects,1);if(m>a->medals[a->stage])a->medals[a->stage]=m;if(a->stage+1>a->unlocked&&a->unlocked<9)a->unlocked++;a->tier=pw_next_tier(a->tier,m,a->sim.secrets);save_progress(a);a->state=PW_MAP;}
    }
}
static void sky(int w,int h,float t,int tier){
    Color top=tier==2?(Color){43,25,83,255}:(Color){31,55,91,255};Color bot=tier==0?(Color){250,143,67,255}:(Color){200,96,151,255};
    for(int y=0;y<h;y+=4)DrawRectangle(0,y,w,5,mix(top,bot,(float)y/h));
    DrawCircle(w*.78f,h*.36f,55+5*sinf(t*.3f),(Color){255,227,151,220});
    for(int i=0;i<45;i++){int x=(i*193+(int)(t*8))%(w+200)-100;int y=h*2/3+(i*71)%180;DrawEllipse(x,y,80+(i%5)*30,18+(i%3)*8,(Color){255,211,176,35});}
}
static void ring3(Vector3 p,float r,Color c){
    for(int i=0;i<32;i++){float a=i*6.283185f/32,b=(i+1)*6.283185f/32;
        DrawLine3D((Vector3){p.x+cosf(a)*r,p.y+sinf(a)*r,p.z},
                   (Vector3){p.x+cosf(b)*r,p.y+sinf(b)*r,p.z},c);
        DrawLine3D((Vector3){p.x+cosf(a)*(r+.18f),p.y+sinf(a)*(r+.18f),p.z-.08f},
                   (Vector3){p.x+cosf(b)*(r+.18f),p.y+sinf(b)*(r+.18f),p.z-.08f},c);
    }
}
static void draw_gate(const PwApp*a,const PwGate*g,float dz){
    float gy=pw_gate_gap_y(g,a->sim.tick*PW_DT);Color stone=a->graybox?GRAY:(Color){55,47,70,255};Color edge=a->graybox?LIGHTGRAY:(Color){238,175,90,255};
    float half=g->gap_h*.5f;if(g->type==PW_CANYON)half*=.78f;
    if(g->type==PW_RING||g->type==PW_ROLLING||g->type==PW_SECRET){Color c=g->type==PW_SECRET?VIOLET:GOLD;ring3((Vector3){g->lateral,gy,dz},3.7f,c);return;}
    DrawCube((Vector3){-8,gy+half+8,dz},15,16,2,stone);DrawCube((Vector3){8,gy+half+8,dz},15,16,2,stone);
    DrawCube((Vector3){-8,gy-half-8,dz},15,16,2,stone);DrawCube((Vector3){8,gy-half-8,dz},15,16,2,stone);
    DrawCubeWires((Vector3){0,gy+half+8,dz},30,16,2,edge);DrawCubeWires((Vector3){0,gy-half-8,dz},30,16,2,edge);
    if(g->type==PW_BARRICADE&&!g->destroyed){DrawCube((Vector3){0,gy,dz-.4f},6,g->gap_h,1.2f,(Color){119,64,74,255});DrawCubeWires((Vector3){0,gy,dz-.4f},6,g->gap_h,1.2f,GOLD);}
    if(g->enemy){DrawSphere((Vector3){g->lateral,gy+half+1.5f,dz-1},.5f,(Color){255,67,95,255});}
}
static void ship(const PwApp*a,float x,float y,float roll,Color c){
    rlPushMatrix();rlTranslatef(x,y,0);rlRotatef(roll*720,0,0,1);
    DrawTriangle3D((Vector3){0,.45f,1},(Vector3){-2,-.3f,-.6f},(Vector3){0,-.15f,-.2f},c);
    DrawTriangle3D((Vector3){0,.45f,1},(Vector3){2,-.3f,-.6f},(Vector3){0,-.15f,-.2f},c);
    DrawTriangle3D((Vector3){0,.45f,1},(Vector3){0,-.15f,-.2f},(Vector3){0,0,-1.2f},mix(c,GOLD,.35f));rlPopMatrix();
}
static void boss_fortress(PwApp*a){
    if(a->mode||!(a->stage==2||a->stage==5||a->stage==8))return;
    float dz=a->sim.stage_length-a->sim.distance-25;if(dz<20||dz>330)return;
    Color core=a->stage==5?(Color){115,210,255,255}:(Color){255,102,126,255};
    float breathe=1+sinf(a->time*2)*.12f;
    DrawCube((Vector3){0,2,dz},25,9,13,(Color){34,31,52,255});
    DrawCube((Vector3){-15,0,dz+2},7,24,8,(Color){48,43,65,255});DrawCube((Vector3){15,0,dz+2},7,24,8,(Color){48,43,65,255});
    DrawSphere((Vector3){0,2,dz-7},2.2f*breathe,core);ring3((Vector3){0,2,dz-7.2f},4.2f,GOLD);
}
static void world(PwApp*a){
    int w=GetScreenWidth(),h=GetScreenHeight();sky(w,h,a->time,a->tier);
    float jitter=a->shake>0?GetRandomValue(-8,8)*a->shake:0;a->camera.position=(Vector3){jitter*.02f,3.0f+jitter*.01f,-11};a->camera.target=(Vector3){a->sim.x*.22f,a->sim.y*.38f,30};
    a->camera.fovy=72+(a->reduce_motion?0:(a->sim.speed-22)*.45f);BeginMode3D(a->camera);
    for(int i=0;i<22;i++){float z=fmodf(i*55-a->sim.distance*.35f,1210);float side=(i&1)?-1:1;DrawCube((Vector3){side*(17+(i%4)*4),-14+(i%3),z},9+(i%5)*3,5+(i%4)*4,14,(Color){42,41,63,255});}
    for(int i=0;i<46;i++){float z=fmodf(i*37-a->sim.distance*2.5f,700);float x=-18+(i*47)%36,y=-13+(i*29)%27;DrawLine3D((Vector3){x,y,z},(Vector3){x,y,z-3-a->sim.speed*.15f},(Color){235,225,255,(unsigned char)(35+a->sim.combo*8)});}
    for(int i=a->sim.next_gate;i<a->sim.gate_count;i++){float dz=a->sim.gate[i].z-a->sim.distance;if(dz<-4)continue;if(dz>340)break;draw_gate(a,&a->sim.gate[i],dz);}
    boss_fortress(a);
    for(int i=0;i<a->sim.shot_count;i++){PwShot*q=&a->sim.shot[i];if(q->life>0){DrawSphere((Vector3){q->x,q->y,q->z},.38f,(Color){255,54,118,255});DrawLine3D((Vector3){q->x,q->y,q->z},(Vector3){q->x,q->y,q->z+5},(Color){255,110,155,180});}}
    Color trail=a->sim.combo>=8?(Color){255,161,62,240}:(Color){88,221,255,210};
    DrawLine3D((Vector3){a->sim.x-1.1f,a->sim.y-.2f,-.5f},(Vector3){a->sim.px-1.1f,a->sim.py-.2f,-12},trail);
    DrawLine3D((Vector3){a->sim.x+1.1f,a->sim.y-.2f,-.5f},(Vector3){a->sim.px+1.1f,a->sim.py-.2f,-12},trail);
    if(a->laser_t>0){DrawLine3D((Vector3){a->sim.x-.65f,a->sim.y,0},(Vector3){a->sim.x-.65f,a->sim.y,100},(Color){112,255,225,255});DrawLine3D((Vector3){a->sim.x+.65f,a->sim.y,0},(Vector3){a->sim.x+.65f,a->sim.y,100},(Color){112,255,225,255});}
    if(a->best.count&&a->ghost_sim.alive)ship(a,a->ghost_sim.x,a->ghost_sim.y,a->ghost_sim.roll_t,(Color){117,224,255,90});
    ship(a,a->sim.x,a->sim.y,a->sim.roll_t,(Color){244,240,221,255});
    for(int i=0;i<512;i++){PwParticle*p=&a->particles[i];if(p->life>0)DrawSphere(p->p,p->size,p->c);}
    EndMode3D();
}
static void text_center(const char*s,int y,int size,Color c){int x=(GetScreenWidth()-MeasureText(s,size))/2;DrawText(s,x,y,size,c);}
static void hud(PwApp*a){
    char b[128];snprintf(b,sizeof b,"%06d",a->sim.score);DrawText(b,32,26,32,RAYWHITE);
    snprintf(b,sizeof b,"x%d  //  %dm",a->sim.combo,(int)a->sim.distance);DrawText(b,34,62,18,(a->sim.combo>=6)?GOLD:(Color){180,220,235,255});
    DrawRectangle(GetScreenWidth()/2-130,26,260,5,(Color){255,255,255,50});DrawRectangle(GetScreenWidth()/2-130,26,(int)(260*pw_clamp(a->sim.distance/a->sim.stage_length,0,1)),5,GOLD);
    for(int i=0;i<3;i++)DrawTriangle((Vector2){GetScreenWidth()-42-i*23,30},(Vector2){GetScreenWidth()-50-i*23,45},(Vector2){GetScreenWidth()-34-i*23,45},i<a->sim.hull?GOLD:(Color){80,70,85,170});
    DrawText("ROLL",GetScreenWidth()-116,58,12,(Color){220,230,240,140});DrawRectangle(GetScreenWidth()-76,61,42,4,(Color){255,255,255,45});DrawRectangle(GetScreenWidth()-76,61,(int)(42*(1-pw_clamp(a->sim.roll_cd/1.5f,0,1))),4,(Color){105,235,255,220});
    if(a->sim.warning>0){text_center("BOUNDARY // PULSE NOW",110,22,(Color){255,100,85,(unsigned char)(150+100*a->sim.warning)});}
    DrawText("SPACE / CLICK  PULSE",32,GetScreenHeight()-42,16,(Color){255,255,255,150});DrawText("F FIRE   R ROLL   A/D DRIFT",GetScreenWidth()-310,GetScreenHeight()-42,16,(Color){255,255,255,130});
    if(a->callout_t>0)text_center(a->callout,GetScreenHeight()/3,22,a->callout_t<.2f?(Color){255,255,255,(unsigned char)(a->callout_t*1200)}:GOLD);
    if(!a->sim.alive&&!a->sim.won){text_center("IMPACT",GetScreenHeight()/2-48,44,RAYWHITE);text_center(a->sim.death_t>.25f?"TAP TO FLY AGAIN":"",GetScreenHeight()/2+12,22,GOLD);}
}
static void menu(PwApp*a){
    int w=GetScreenWidth(),h=GetScreenHeight();sky(w,h,a->time,1);
    for(int i=0;i<40;i++){float z=fmodf(i*31-a->time*24,800);float s=1+180/(z+20);DrawCircle((i*149)%w,(i*83)%h,s,(Color){255,235,190,100});}
    text_center("P U L S E W I N G",h/2-120,52,RAYWHITE);text_center("ONE BUTTON. ONE SKY. AGAIN.",h/2-54,18,GOLD);
    if(a->state==PW_TITLE){text_center("TAP TO ENTER",h/2+62,22,(Color){220,240,255,(unsigned char)(130+100*sinf(a->time*3))});text_center("Best flight lives one heartbeat ahead.",h-55,16,(Color){210,220,240,130});}
    else {const char*items[]={"CAMPAIGN  //  THE NINE ROADS","ENDLESS  //  TODAY'S SKY","PURE  //  PULSE ONLY","REDUCE MOTION"};for(int i=0;i<4;i++){Color c=i==a->selection?GOLD:(Color){220,230,240,150};char line[96];if(i==3)snprintf(line,sizeof line,"%s  //  %s",items[i],a->reduce_motion?"ON":"OFF");else snprintf(line,sizeof line,"%s",items[i]);text_center(line,h/2-30+i*45,21,c);}}
}
static void map(PwApp*a){sky(GetScreenWidth(),GetScreenHeight(),a->time,a->tier);text_center("THE SUNFALL ROUTES",45,30,RAYWHITE);for(int i=0;i<9;i++){int x=100+i*(GetScreenWidth()-200)/8,y=GetScreenHeight()/2+(i%3-1)*85;Color c=i<=a->unlocked?GOLD:(Color){80,80,105,255};DrawLineEx((Vector2){x-70,y+(i?((i-1)%3-1)*-85:0)},(Vector2){x,y},3,(Color){180,120,90,100});DrawCircle(x,y,18,c);char b[8];snprintf(b,sizeof b,"%d",i+1);DrawText(b,x-5,y-8,16,(Color){30,30,45,255});for(int m=0;m<a->medals[i];m++)DrawCircle(x-10+m*10,y+30,3,GOLD);}
    char b[128];snprintf(b,sizeof b,"%02d  %s",a->stage+1,PW_STAGES[a->stage].name);text_center(b,GetScreenHeight()-120,24,RAYWHITE);text_center(PW_STAGES[a->stage].subtitle,GetScreenHeight()-84,17,(Color){220,220,240,160});text_center("LEFT / RIGHT CHOOSE    TAP LAUNCH",GetScreenHeight()-38,15,GOLD);}
void app_frame(PwApp*a){
    float dt=pw_clamp(GetFrameTime(),0,.05f);a->time+=dt;if(a->shake>0)a->shake-=dt*2;if(a->flash>0)a->flash-=dt;if(a->callout_t>0)a->callout_t-=dt;if(a->laser_t>0)a->laser_t-=dt;
    if(tapped())audio_start(a);
    if(a->state==PW_TITLE&&tapped())a->state=PW_MODE;
    else if(a->state==PW_MODE){if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_S))a->selection=(a->selection+1)%4;if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W))a->selection=(a->selection+3)%4;if(tapped()){if(a->selection==3){a->reduce_motion=!a->reduce_motion;}else{a->mode=a->selection;if(a->mode==0)a->state=PW_MAP;else start(a);}}}
    else if(a->state==PW_MAP){if(IsKeyPressed(KEY_RIGHT))a->stage=(a->stage+1)%a->unlocked;if(IsKeyPressed(KEY_LEFT))a->stage=(a->stage+a->unlocked-1)%a->unlocked;if(tapped())start(a);}
    else if(a->state==PW_PLAY){PwInput in;input(a,&in);if(!a->sim.alive&&a->sim.death_t>.25f&&in.pulse){if(a->sim.distance>a->best.best){a->run.best=a->sim.distance;a->best=a->run;}start(a);in.pulse=1;}
        a->acc+=dt;int guard=0;while(a->acc>=PW_DT&&guard++<20){pw_ghost_record(&a->run,in);pw_sim_step(&a->sim,in);if(a->best.count)pw_sim_step(&a->ghost_sim,pw_ghost_input(&a->best,(int)a->ghost_sim.tick));in.pulse=0;in.roll=0;a->acc-=PW_DT;events(a);}
    }
    if(IsKeyPressed(KEY_ESCAPE)){if(a->state==PW_PLAY||a->state==PW_MAP)a->state=PW_MODE;else if(a->state==PW_MODE)a->state=PW_TITLE;}
    for(int i=0;i<512;i++){PwParticle*p=&a->particles[i];if(p->life>0){p->life-=dt;p->p.x+=p->v.x*dt;p->p.y+=p->v.y*dt;p->p.z+=p->v.z*dt;p->v.y-=3*dt;}}
    BeginDrawing();ClearBackground(BLACK);if(a->state==PW_PLAY){world(a);hud(a);}else if(a->state==PW_MAP)map(a);else menu(a);if(a->flash>0&&!a->reduce_motion)DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),(Color){255,225,160,(unsigned char)(a->flash*400)});EndDrawing();
}
