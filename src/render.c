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
 {{16,8,12,255},{51,18,32,255},{244,117,151,255},{255,203,111,255},{255,241,226,255},{107,205,180,255},{15,7,11,255},{103,48,68,255}}};
static const float rings[3]={105,190,270};
Palette sfbs_palette(int n){return palettes[n&7];}
static Vector2 rv(V2 v){return(Vector2){320+v.x,180+v.y};}
static uint32_t mix32(uint64_t x){x^=x>>33;x*=0xff51afd7ed558ccdULL;x^=x>>33;return(uint32_t)x;}
void sfbs_center_text(const char*s,int y,int size,Color c){DrawText(s,320-MeasureText(s,size)/2,y,size,c);}

static void particles(const Game*g,Transport t,Palette p,Phase phase){
 int count=12+phase*9+sfbs_recovery(g)/5;float seconds=t.sample/(float)SFBS_RATE;
 for(int i=0;i<count;i++){uint32_t h=mix32(g->genome.visual_seed+(uint64_t)i*0x9e3779b9u);float radius=35+(h%260),speed=.025f+((h>>9)%30)*.001f,angle=((h>>16)%628)*.01f+seconds*speed*(i&1?1:-1);Vector2 x={320+cosf(angle)*radius,180+sinf(angle)*radius*.58f};float pulse=.5f+.5f*sinf(seconds*(.7f+(h&7)*.1f)+i);DrawCircleV(x,.5f+pulse*1.2f,ColorAlpha(i&1?p.stable:p.accent,.08f+.14f*pulse));}
}

static void decorative_geometry(const Game*g,Transport t,Palette p,Phase phase){
 if(phase<PH_FLOW)return;float seconds=t.sample/(float)SFBS_RATE,amount=.08f+.18f*sfbs_recovery(g)/100.0f;Vector2 last={0};
 for(int i=0;i<=96;i++){float u=i/96.0f*2*SFBS_PI,rad=68+14*sinf(5*u+seconds*.35f);Vector2 x={320+cosf(u+seconds*.025f)*rad,180+sinf(u+seconds*.025f)*rad};if(i)DrawLineEx(last,x,1,ColorAlpha(p.stable,amount));last=x;}
 if(phase<PH_FINAL)return;
 for(int i=0;i<=96;i++){float u=i/96.0f*2*SFBS_PI;Vector2 x={320+sinf(3*u+seconds*.1f)*145,180+sinf(4*u)*72};if(i)DrawLineEx(last,x,1,ColorAlpha(p.accent,.12f+.18f*sfbs_recovery(g)/100.0f));last=x;}
}

static void player_trail(const Game*g,Transport t,Palette p){
 uint64_t tick=(t.sample*(uint64_t)g->genome.bpm*SFBS_PPQN)/((uint64_t)SFBS_RATE*60),life=(uint64_t)((1.5f+3.5f*sfbs_recovery(g)/100.0f)*SFBS_PPQN);
 if(g->mode!=MODE_BLOOM){int max=g->mode==MODE_FLOW?3:2;if(g->integrity<max)life=life*(uint64_t)(g->integrity+1)/(uint64_t)(max+1);}
 V2 last={0};bool have=false;for(int k=0;k<g->path.count;k++){int i=(g->path.head-1-k+SFBS_PATH)%SFBS_PATH;uint64_t age=tick>=g->path.tick[i]?tick-g->path.tick[i]:0;if(age>life)break;float alpha=.55f*(1-age/(float)(life?life:1));if(have)DrawLineEx(rv(last),rv(g->path.p[i]),1.5f+2*alpha,ColorAlpha(p.player,alpha));last=g->path.p[i];have=true;}
}

static void corruption(const Game*g,Transport t,Palette p,bool collision){
 if(!g->echo_active)return;uint64_t tick=(t.sample*(uint64_t)g->genome.bpm*SFBS_PPQN)/((uint64_t)SFBS_RATE*60),delay=(uint64_t)(g->echo_delay*SFBS_PPQN),head=tick>delay?tick-delay:0,persist=4*SFBS_PPQN;V2 last={0};bool have=false;
 for(int k=0;k<g->path.count;k++){int i=(g->path.head-1-k+SFBS_PATH)%SFBS_PATH;if(g->path.tick[i]>head)continue;if(head-g->path.tick[i]>persist)break;V2 q=g->path.p[i];if(have){float throb=1+.12f*sinf(t.beat_phase*SFBS_PI);DrawLineEx(rv(last),rv(q),g->echo_width*2*throb,ColorAlpha(p.bad,.38f));DrawLineEx(rv(last),rv(q),collision?2:4,p.edge);}last=q;have=true;}
}

static void final_form(const Game*g,Transport t,Palette p,Phase phase){
 if(phase!=PH_FINAL&&!g->ended)return;float strength=sfbs_recovery(g)/100.0f;if(g->ended&&!g->won)strength*=.25f;float bloom=(.35f+.65f*sinf(t.beat_phase*SFBS_PI))*strength;
 for(int i=0;i<16;i++){float a=2*SFBS_PI*i/16+t.bar_phase*.15f,r0=42,r1=48+145*bloom*(i%2?.75f:1);Vector2 a0={320+cosf(a)*r0,180+sinf(a)*r0},a1={320+cosf(a)*r1,180+sinf(a)*r1};DrawLineEx(a0,a1,1+i%3,ColorAlpha(i&1?p.stable:p.accent,.16f+.35f*bloom));DrawCircleV(a1,1+3*bloom,ColorAlpha(p.active,.3f+.45f*bloom));}
}

void sfbs_render_game(const Game*g,Transport t,bool debug,bool collision){
 Palette p=sfbs_palette(g->genome.palette);Phase phase=(Phase)g->phrases[g->phrase_index<SFBS_PHRASES?g->phrase_index:SFBS_PHRASES-1].phase;ClearBackground(p.bg);particles(g,t,p,phase);decorative_geometry(g,t,p,phase);
 float breathe=1+sinf(t.beat_phase*SFBS_PI)*(.008f+.008f*phase/PH_FINAL);Vector2 c={320,180};for(int i=0;i<3;i++)DrawCircleLines((int)c.x,(int)c.y,rings[i]*breathe,ColorAlpha(p.bg2,.9f));float sweep=t.bar_phase*2*SFBS_PI;DrawLineEx(c,(Vector2){c.x+cosf(sweep)*296,c.y+sinf(sweep)*296},1,ColorAlpha(p.active,.22f));
 for(int i=0;i<g->node_count;i++){const Node*n=&g->nodes[i];if(n->state==NODE_EXPIRED)continue;Vector2 x=rv(n->pos);Color q=n->state==NODE_RECOVERED?p.stable:n->state==NODE_CORRUPTED?p.edge:p.active;float r=n->state==NODE_AVAILABLE?6:n->state==NODE_TELEGRAPHING?4+2*sinf(t.beat_phase*SFBS_PI):4;DrawCircleV(x,r+5,ColorAlpha(q,n->state==NODE_TELEGRAPHING?.16f:.08f));DrawPoly(x,6,r,t.bar_phase*60,q);if(n->state==NODE_RECOVERED){V2 dir=sfbs_norm(n->pos);for(int j=1;j<=3;j++)DrawCircleV(rv(sfbs_add(n->pos,sfbs_scale(dir,-j*(3+t.beat_phase*2)))),2-j*.4f,ColorAlpha(q,.18f/j));}if(n->state==NODE_TELEGRAPHING)DrawCircleLines((int)x.x,(int)x.y,10+t.beat_phase*6,ColorAlpha(q,1-t.beat_phase));if(n->state==NODE_CORRUPTED){DrawLine((int)x.x-5,(int)x.y-5,(int)x.x+5,(int)x.y+5,p.bad);DrawLine((int)x.x+5,(int)x.y-5,(int)x.x-5,(int)x.y+5,p.bad);}}
 player_trail(g,t,p);corruption(g,t,p,collision);final_form(g,t,p,phase);
 Vector2 pp=rv(g->player.pos);DrawCircleV(pp,18+g->player.glow*20,ColorAlpha(p.player,.08f));DrawCircleV(pp,12,ColorAlpha(p.player,.12f));DrawCircleV(pp,8,p.player);DrawCircleLines((int)pp.x,(int)pp.y,13+g->player.pulse_cd*8,ColorAlpha(p.accent,.8f));
 char hud[64];snprintf(hud,sizeof hud,"%3d%%   %s",sfbs_recovery(g),g->seed);DrawText(hud,14,12,15,p.player);if(g->mode!=MODE_BLOOM)for(int i=0;i<(g->mode==MODE_FLOW?3:2);i++)DrawRing(c,12+i*5,14+i*5,-110,-70,16,i<g->integrity?p.active:p.bad);
 if(g->mode!=MODE_BLOOM){int max=g->mode==MODE_FLOW?3:2,lost=max-g->integrity;if(lost>0)DrawRectangle(0,0,640,360,ColorAlpha(p.bad,.035f*lost));}
 if(g->ended&&!g->won)DrawRectangle(0,0,640,360,ColorAlpha(p.bad,.24f));
 if(debug){snprintf(hud,sizeof hud,"bar %u beat %u step %u  phrase %d  ev %d",t.bar,t.beat,t.step,g->phrase_index,g->song.count);DrawText(hud,10,336,10,p.accent);}
}
