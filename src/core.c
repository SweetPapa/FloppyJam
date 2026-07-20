#include "core.h"
#include <string.h>
#include <math.h>
#include <ctype.h>
static const char alpha[]="23456789ABCDEFGHJKMNPQRSTUVWXYZ";
uint64_t sfbs_hash(const char *s){ uint64_t h=1469598103934665603ULL; while(*s){h^=(unsigned char)*s++;h*=1099511628211ULL;} h^=h>>30;h*=0xbf58476d1ce4e5b9ULL;h^=h>>27;h*=0x94d049bb133111ebULL;return h^(h>>31); }
uint64_t sfbs_rng(Rng *r){ uint64_t z=(r->s+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31); }
float sfbs_randf(Rng *r){return (float)(sfbs_rng(r)>>40)*(1.0f/16777216.0f);}
void sfbs_seed_normalize(const char *in,char out[7]){int n=0;memset(out,0,7);while(*in&&n<6){char c=(char)toupper((unsigned char)*in++);if(strchr(alpha,c))out[n++]=c;}uint64_t h=sfbs_hash(out);while(n<6){int i=n;out[n++]=alpha[(h>>(i*5))&31];}out[6]=0;}
void sfbs_random_seed(uint64_t e,char out[7]){Rng r={e^0xa51f07d390ULL};for(int i=0;i<6;i++)out[i]=alpha[sfbs_rng(&r)&31];out[6]=0;}
Genome sfbs_genome(const char seed[7]){Genome g={0};g.master=sfbs_hash(seed);Rng r={g.master};g.music_seed=sfbs_rng(&r);g.layout_seed=sfbs_rng(&r);g.gameplay_seed=sfbs_rng(&r);g.visual_seed=sfbs_rng(&r);static const int bpms[]={84,88,92,96,100,104,108,112};Rng m={g.music_seed};g.bpm=bpms[sfbs_rng(&m)&7];g.root=45+(int)(sfbs_rng(&m)%12);g.scale=(int)(sfbs_rng(&m)%5);g.progression=(int)(sfbs_rng(&m)%8);Rng v={g.visual_seed};g.palette=(int)(sfbs_rng(&v)%8);g.primary_steps=(int[]){8,12,16}[sfbs_rng(&m)%3];g.swing=sfbs_randf(&m)*.12f;g.warmth=.65f+sfbs_randf(&m)*.3f;g.symmetry=.65f+sfbs_randf(&v)*.35f;return g;}
Pattern sfbs_euclid(int k,int n,int rot){Pattern p={0};if(n<1)n=1;if(n>16)n=16;if(k<1)k=1;if(k>=n)k=n-1;rot=((rot%n)+n)%n;for(int i=0;i<n;i++)if(((i+1)*k)/n!=(i*k)/n)p.bits|=(uint16_t)(1u<<((i+rot)%n));p.steps=(uint8_t)n;p.pulses=(uint8_t)k;p.rotation=(uint8_t)rot;return p;}
int sfbs_popcount(uint16_t x){int n=0;while(x){x&=(uint16_t)(x-1);n++;}return n;}
uint64_t sfbs_tick_sample(uint64_t t,int bpm){return (t*(uint64_t)SFBS_RATE*60ULL)/(uint64_t)(bpm*SFBS_PPQN);}
Transport sfbs_transport(uint64_t s,int bpm){Transport t={0};uint64_t tick=(s*(uint64_t)bpm*SFBS_PPQN)/((uint64_t)SFBS_RATE*60ULL);uint64_t q=tick/SFBS_PPQN;t.sample=s;t.bar=(uint32_t)(q/4);t.beat=(uint8_t)(q%4);t.step=(uint8_t)((tick%(SFBS_PPQN*4))/(SFBS_PPQN/4));t.beat_phase=(float)(tick%SFBS_PPQN)/SFBS_PPQN;t.bar_phase=(float)(tick%(SFBS_PPQN*4))/(SFBS_PPQN*4);return t;}
void sfbs_phrases(const Genome*g,GameMode mode,Phrase o[SFBS_PHRASES]){Rng r={g->layout_seed^(uint64_t)mode};Pattern prev={0};for(int i=0;i<SFBS_PHRASES;i++){int phase=i<1?PH_BOOT:i<3?PH_SIGNAL:i<6?PH_PULSE:i<9?PH_FLOW:i<12?PH_BLOOM:PH_FINAL;int n=(int[]){8,12,16}[sfbs_rng(&r)%3];int lo=n==8?3:n==12?4:5, hi=n==8?6:n==12?8:10;int k=lo+(int)(sfbs_rng(&r)%(uint64_t)(hi-lo+1));if(mode==MODE_PULSE&&i>=9&&k<hi)k++;int rot=1+(int)(sfbs_rng(&r)%(uint64_t)(n-1));Pattern p=sfbs_euclid(k,n,rot);if(p.bits==prev.bits&&p.steps==prev.steps)p=sfbs_euclid(k,n,rot+1);o[i]=(Phrase){i*4,4,p,(uint8_t)(sfbs_rng(&r)%3),(uint8_t)(phase>=PH_FLOW?2:phase>=PH_PULSE?1:0),(uint8_t)phase,(float)i/13.0f};prev=p;}o[12].pattern=sfbs_euclid(7,12,o[4].pattern.rotation+o[8].pattern.rotation);o[13].pattern=sfbs_euclid(9,16,o[5].pattern.rotation+1);}
const char*sfbs_mode_name(GameMode m){return m==MODE_BLOOM?"BLOOM":m==MODE_PULSE?"PULSE":"FLOW";}
const char*sfbs_result_label(int p){return p==100?"NO BAD SECTORS":p>=90?"NEAR-PERFECT READ":p>=70?"TRACK RESTORED":p>=40?"PARTIAL RECOVERY":"SIGNAL FRAGMENT";}
float sfbs_resonance(float p,int bpm,float sigma){float d=fminf(p,1-p)*60.0f/bpm;return expf(-.5f*(d/sigma)*(d/sigma));}
float sfbs_clampf(float x,float a,float b){return x<a?a:x>b?b:x;}V2 sfbs_add(V2 a,V2 b){return(V2){a.x+b.x,a.y+b.y};}V2 sfbs_scale(V2 a,float s){return(V2){a.x*s,a.y*s};}float sfbs_len(V2 a){return sqrtf(a.x*a.x+a.y*a.y);}V2 sfbs_norm(V2 a){float l=sfbs_len(a);return l>.0001f?sfbs_scale(a,1/l):(V2){0,0};}
