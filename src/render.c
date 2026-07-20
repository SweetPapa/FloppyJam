#include "render.h"
#include <math.h>
#include <stdio.h>

static const Palette palettes[8]={
 {{6,9,18,255},{17,25,43,255},{91,220,194,255},{255,206,112,255},{244,249,221,255},{238,113,151,255},{11,10,20,255},{112,65,92,255}},
 {{12,8,17,255},{40,19,39,255},{242,153,135,255},{255,224,138,255},{255,245,214,255},{145,197,213,255},{17,10,21,255},{101,72,110,255}},
 {{5,15,17,255},{16,43,42,255},{128,219,178,255},{247,207,103,255},{231,255,241,255},{240,129,108,255},{5,13,17,255},{74,99,101,255}},
 {{13,11,27,255},{29,25,55,255},{157,140,255,255},{255,187,128,255},{244,241,255,255},{102,221,211,255},{12,9,18,255},{88,67,108,255}},
 {{10,14,24,255},{30,39,57,255},{121,190,255,255},{255,213,107,255},{242,249,255,255},{255,126,146,255},{8,10,18,255},{70,81,103,255}},
 {{15,11,9,255},{47,31,22,255},{225,174,105,255},{255,228,159,255},{255,249,225,255},{120,196,170,255},{16,10,9,255},{101,70,57,255}},
 {{6,15,25,255},{15,43,63,255},{82,211,208,255},{254,173,102,255},{234,255,252,255},{188,132,255,255},{5,11,18,255},{53,80,96,255}},
 {{16,8,12,255},{51,18,32,255},{244,117,151,255},{255,203,111,255},{107,205,180,255},{107,205,180,255},{15,7,11,255},{103,48,68,255}}};
static const float rings[3]={105,190,270};
Palette sfbs_palette(int n){return palettes[n&7];}
static Vector3 w3(V2 p,float y){return(Vector3){p.x,y,p.y};}
static uint32_t mix32(uint64_t x){x^=x>>33;x*=0xff51afd7ed558ccdULL;x^=x>>33;return(uint32_t)x;}
void sfbs_center_text(const char*s,int y,int size,Color c){DrawText(s,320-MeasureText(s,size)/2,y,size,c);}

static Camera3D camera_for(const Game*g,Transport t,bool first,float yaw,float pitch){Camera3D c={0};float beat=sinf(t.beat_phase*SFBS_PI),bob=beat*.45f;if(first){Vector3 eye=w3(g->player.pos,9.5f+bob),forward={sinf(yaw)*cosf(pitch),sinf(pitch),cosf(yaw)*cosf(pitch)};c.position=eye;c.target=(Vector3){eye.x+forward.x*45,eye.y+forward.y*45,eye.z+forward.z*45};c.fovy=72;}else{float orbit=.35f+t.bar_phase*.08f;c.position=(Vector3){sinf(orbit)*385,345+beat*4,cosf(orbit)*385};c.target=(Vector3){0,0,0};c.fovy=48;}c.up=(Vector3){0,1,0};c.projection=CAMERA_PERSPECTIVE;return c;}

static void disk_geometry(Palette p,Transport t){
 DrawCylinderEx((Vector3){0,-3,0},(Vector3){0,-1,0},300,300,96,p.bg2);DrawCylinderWiresEx((Vector3){0,-3,0},(Vector3){0,-1,0},300,300,96,ColorAlpha(p.edge,.5f));
 for(int r=0;r<3;r++){DrawCircle3D((Vector3){0,.05f,0},rings[r],(Vector3){1,0,0},90,ColorAlpha(p.stable,.3f));DrawCircle3D((Vector3){0,.08f,0},rings[r]+1.3f,(Vector3){1,0,0},90,ColorAlpha(p.bg,.8f));}
 for(int i=0;i<32;i++){float a=2*SFBS_PI*i/32,r0=i%4?286:279;DrawLine3D((Vector3){cosf(a)*r0,.15f,sinf(a)*r0},(Vector3){cosf(a)*298,.15f,sinf(a)*298},ColorAlpha(p.edge,i%4?.25f:.6f));}
 float a=t.bar_phase*2*SFBS_PI;DrawLine3D((Vector3){0,.25f,0},(Vector3){cosf(a)*296,.25f,sinf(a)*296},ColorAlpha(p.active,.7f));DrawSphere((Vector3){cosf(a)*296,.4f,sinf(a)*296},2.2f,p.active);
 DrawCylinderEx((Vector3){0,-1,0},(Vector3){0,3,0},20,14,32,p.bg);DrawCylinderWiresEx((Vector3){0,-1,0},(Vector3){0,3,0},20,14,32,p.accent);
}

static void world_particles(const Game*g,Transport t,Palette p,Phase phase){float seconds=t.sample/(float)SFBS_RATE;int count=20+phase*12+sfbs_recovery(g)/3;BeginBlendMode(BLEND_ADDITIVE);for(int i=0;i<count;i++){uint32_t h=mix32(g->genome.visual_seed+(uint64_t)i*0x9e3779b9u);float radius=35+h%260,a=(h%628)*.01f+seconds*(.025f+(h>>9)%20*.001f)*(i&1?1:-1),y=3+(h>>17)%38+8*sinf(seconds*.18f+i);Vector3 x={cosf(a)*radius,y,sinf(a)*radius};DrawSphere(x,.5f+(h&3)*.22f,ColorAlpha(i&1?p.stable:p.accent,.22f));}EndBlendMode();}

static void nodes_3d(const Game*g,Transport t,Palette p){
 for(int i=0;i<g->node_count;i++){const Node*n=&g->nodes[i];if(n->state==NODE_EXPIRED)continue;Color q=n->state==NODE_RECOVERED?p.stable:n->state==NODE_CORRUPTED?p.edge:p.active;float pulse=.5f+.5f*sinf(t.beat_phase*SFBS_PI),h=n->state==NODE_TELEGRAPHING?5+8*pulse:n->state==NODE_AVAILABLE?10:n->state==NODE_RECOVERED?4:2,r=n->state==NODE_AVAILABLE?4.2f:3.1f;Vector3 base=w3(n->pos,.1f),top=w3(n->pos,h);DrawCylinderEx(base,top,r,r*.68f,10,ColorAlpha(q,n->state==NODE_CORRUPTED?.4f:.8f));DrawCylinderWiresEx(base,top,r+.35f,r*.68f+.35f,10,q);BeginBlendMode(BLEND_ADDITIVE);DrawSphere(top,r*(1.15f+pulse*.25f),ColorAlpha(q,.22f));if(n->state==NODE_TELEGRAPHING)DrawSphereWires(top,8+8*t.beat_phase,8,8,ColorAlpha(q,1-t.beat_phase));EndBlendMode();if(n->state==NODE_CORRUPTED){DrawLine3D((Vector3){base.x-5,1,base.z-5},(Vector3){base.x+5,7,base.z+5},p.bad);DrawLine3D((Vector3){base.x+5,1,base.z-5},(Vector3){base.x-5,7,base.z+5},p.bad);}}
}

static void trails_3d(const Game*g,Transport t,Palette p,bool collision){
 uint64_t tick=(t.sample*(uint64_t)g->genome.bpm*SFBS_PPQN)/((uint64_t)SFBS_RATE*60),life=(uint64_t)((1.5f+3.5f*sfbs_recovery(g)/100.0f)*SFBS_PPQN);V2 last={0};bool have=false;BeginBlendMode(BLEND_ADDITIVE);for(int k=0;k<g->path.count;k++){int i=(g->path.head-1-k+SFBS_PATH)%SFBS_PATH;uint64_t age=tick>=g->path.tick[i]?tick-g->path.tick[i]:0;if(age>life)break;if(have)DrawLine3D(w3(last,.8f),w3(g->path.p[i],.8f),ColorAlpha(p.player,.65f*(1-age/(float)life)));last=g->path.p[i];have=true;}EndBlendMode();
 if(!g->echo_active)return;uint64_t delay=(uint64_t)(g->echo_delay*SFBS_PPQN),head=tick>delay?tick-delay:0,persist=4*SFBS_PPQN;have=false;for(int k=0;k<g->path.count;k++){int i=(g->path.head-1-k+SFBS_PATH)%SFBS_PATH;if(g->path.tick[i]>head)continue;if(head-g->path.tick[i]>persist)break;V2 q=g->path.p[i];if(have){V2 d={q.x-last.x,q.y-last.y};if(sfbs_len(d)>.1f){float width=g->echo_width*(collision?1:.55f),pulse=1+.13f*sinf(t.beat_phase*SFBS_PI);DrawCylinderEx(w3(last,.35f),w3(q,.35f),width*pulse,width*pulse,7,ColorAlpha(p.bad,.72f));DrawLine3D(w3(last,.7f),w3(q,.7f),p.edge);}}last=q;have=true;}
}

static void final_bloom(const Game*g,Transport t,Palette p,Phase phase){if(phase!=PH_FINAL&&!g->ended)return;float strength=sfbs_recovery(g)/100.0f;if(g->ended&&!g->won)strength*=.2f;BeginBlendMode(BLEND_ADDITIVE);for(int i=0;i<20;i++){float a=2*SFBS_PI*i/20+t.bar_phase*.2f,r=45+(80+80*sinf(t.beat_phase*SFBS_PI))*strength;Vector3 lo={cosf(a)*35,2,sinf(a)*35},hi={cosf(a)*r,18+25*strength,sinf(a)*r};DrawLine3D(lo,hi,ColorAlpha(i&1?p.stable:p.accent,.55f*strength));DrawSphere(hi,2+4*strength,ColorAlpha(p.active,.35f));}EndBlendMode();}

static void scene(const Game*g,Transport t,bool first,float yaw,float pitch,bool collision){Palette p=sfbs_palette(g->genome.palette);Phase phase=(Phase)g->phrases[g->phrase_index<SFBS_PHRASES?g->phrase_index:SFBS_PHRASES-1].phase;ClearBackground(p.bg);DrawRectangleGradientV(0,0,640,360,p.bg2,p.bg);BeginMode3D(camera_for(g,t,first,yaw,pitch));disk_geometry(p,t);world_particles(g,t,p,phase);nodes_3d(g,t,p);trails_3d(g,t,p,collision);final_bloom(g,t,p,phase);if(!first){Vector3 player=w3(g->player.pos,5);BeginBlendMode(BLEND_ADDITIVE);DrawSphere(player,10+g->player.glow*10,ColorAlpha(p.player,.2f));EndBlendMode();DrawSphere(player,5,p.player);}EndMode3D();}

void sfbs_render_backdrop(const Game*g,float yaw){Transport t=sfbs_transport(0,g->genome.bpm);scene(g,t,false,yaw,-.18f,false);DrawRectangle(0,0,640,360,ColorAlpha(sfbs_palette(g->genome.palette).bg,.4f));}

static void cockpit(const Game*g,Transport t,Palette p,bool first,const char*view){
 DrawRectangle(0,0,640,28,ColorAlpha(p.bg,.78f));DrawRectangle(0,328,640,32,ColorAlpha(p.bg,.82f));DrawLine(0,28,640,28,ColorAlpha(p.active,.45f));DrawLine(0,328,640,328,ColorAlpha(p.active,.45f));
 char txt[96];snprintf(txt,sizeof txt,"RECOVERY %03d%%",sfbs_recovery(g));DrawText(txt,16,8,14,p.player);snprintf(txt,sizeof txt,"%s  /  %s",g->seed,sfbs_mode_name(g->mode));DrawText(txt,640-MeasureText(txt,12)-16,9,12,p.stable);DrawText(view,16,338,10,p.accent);
 float progress=(t.bar+t.bar_phase)/SFBS_BARS;DrawRectangle(190,340,260,5,ColorAlpha(p.bg2,.9f));DrawRectangle(190,340,(int)(260*sfbs_clampf(progress,0,1)),5,p.active);snprintf(txt,sizeof txt,"BAR %02u / %02d",t.bar+1,SFBS_BARS);sfbs_center_text(txt,349,9,p.stable);
 if(g->mode!=MODE_BLOOM){int max=g->mode==MODE_FLOW?3:2;for(int i=0;i<max;i++){Color q=i<g->integrity?p.active:p.bad;DrawCircle(580+i*15,343,5,q);}}
 if(first){DrawLine(315,180,325,180,ColorAlpha(p.player,.7f));DrawLine(320,175,320,185,ColorAlpha(p.player,.7f));DrawCircleLines(320,180,7+g->player.pulse_cd*8,ColorAlpha(p.accent,.75f));}
}

void sfbs_render_game(const Game*g,Transport t,bool debug,bool collision,bool first,float yaw,float pitch,const char*view){Palette p=sfbs_palette(g->genome.palette);scene(g,t,first,yaw,pitch,collision);cockpit(g,t,p,first,view);if(g->mode!=MODE_BLOOM){int max=g->mode==MODE_FLOW?3:2,lost=max-g->integrity;if(lost>0)DrawRectangle(0,0,640,360,ColorAlpha(p.bad,.035f*lost));}if(g->ended&&!g->won)DrawRectangle(0,0,640,360,ColorAlpha(p.bad,.2f));if(debug){char txt[112];snprintf(txt,sizeof txt,"bar %u beat %u step %u phrase %d events %d yaw %.2f pitch %.2f",t.bar,t.beat,t.step,g->phrase_index,g->song.count,yaw,pitch);DrawText(txt,10,310,9,p.accent);}}
