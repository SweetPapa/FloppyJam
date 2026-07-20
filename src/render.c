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
Palette sfbs_palette(int n){return palettes[n&7];}static const float rings[3]={105,190,270};static Vector2 rv(V2 v){return(Vector2){320+v.x,180+v.y};}
void sfbs_center_text(const char*s,int y,int size,Color c){DrawText(s,320-MeasureText(s,size)/2,y,size,c);}
void sfbs_render_game(const Game*g,Transport t,bool debug,bool collision){Palette p=sfbs_palette(g->genome.palette);ClearBackground(p.bg);float breathe=1+sinf(t.beat_phase*SFBS_PI)*.012f;Vector2 c={320,180};for(int i=0;i<3;i++){DrawCircleLines((int)c.x,(int)c.y,rings[i]*breathe,ColorAlpha(p.bg2,.9f));}float a=t.bar_phase*2*SFBS_PI;DrawLineEx(c,(Vector2){c.x+cosf(a)*296,c.y+sinf(a)*296},1,ColorAlpha(p.active,.22f));
 for(int i=0;i<g->node_count;i++){const Node*n=&g->nodes[i];if(n->state==NODE_EXPIRED)continue;Vector2 x=rv(n->pos);Color q=n->state==NODE_RECOVERED?p.stable:n->state==NODE_CORRUPTED?p.edge:p.active;float r=n->state==NODE_AVAILABLE?6:4;DrawCircleV(x,r+5,ColorAlpha(q,.08f));DrawPoly(x,6,r,t.bar_phase*60,q);if(n->state==NODE_CORRUPTED){DrawLine((int)x.x-5,(int)x.y-5,(int)x.x+5,(int)x.y+5,p.bad);DrawLine((int)x.x+5,(int)x.y-5,(int)x.x-5,(int)x.y+5,p.bad);}}
 if(g->echo_active){uint64_t tick=(t.sample*(uint64_t)g->genome.bpm*SFBS_PPQN)/((uint64_t)SFBS_RATE*60),delay=(uint64_t)(g->echo_delay*SFBS_PPQN);V2 last={0};bool have=false;for(int k=0;k<g->path.count;k++){int i=(g->path.head-1-k+SFBS_PATH)%SFBS_PATH;if(g->path.tick[i]+delay>tick)continue;V2 q=g->path.p[i];if(have){DrawLineEx(rv(last),rv(q),g->echo_width*2,ColorAlpha(p.bad,.35f));DrawLineEx(rv(last),rv(q),collision?2:4,p.edge);}last=q;have=true;if(k>160)break;}}
 Vector2 pp=rv(g->player.pos);DrawCircleV(pp,18+g->player.glow*20,ColorAlpha(p.player,.08f));DrawCircleV(pp,8,p.player);DrawCircleLines((int)pp.x,(int)pp.y,13+g->player.pulse_cd*8,ColorAlpha(p.accent,.8f));char hud[64];snprintf(hud,sizeof hud,"%3d%%   %s",sfbs_recovery(g),g->seed);DrawText(hud,14,12,15,p.player);if(g->mode!=MODE_BLOOM)for(int i=0;i<(g->mode==MODE_FLOW?3:2);i++)DrawRing(c,12+i*5,14+i*5,-110,-70,16,i<g->integrity?p.active:p.bad);
 if(debug){snprintf(hud,sizeof hud,"bar %u beat %u step %u  phrase %d  ev %d",t.bar,t.beat,t.step,g->phrase_index,g->song.count);DrawText(hud,10,336,10,p.accent);}}
