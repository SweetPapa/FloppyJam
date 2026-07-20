#include "audio.h"
#include <math.h>
#include <string.h>
static AudioEngine *A;
static float hz(int n){return 440.0f*powf(2.0f,(n-69)/12.0f);}
static float triangle(float p){return asinf(sinf(p*6.2831853f))*.63662f;}
static float noise_sample(void){A->noise=A->noise*1664525u+1013904223u;return((A->noise>>8)*(1.0f/8388608.0f))-1;}
static float voice_sample(SynthVoice*q){
 float step=q->freq/SFBS_RATE,x=0;q->phase+=step*(q->voice==3?.35f+.65f*q->env:1);q->phase2+=step*(q->pan<0?.997f:1.003f);if(q->phase>=1)q->phase-=1;if(q->phase2>=1)q->phase2-=1;
 switch(q->voice){
  case 0:x=triangle(q->phase)*.68f+triangle(q->phase2)*.32f;break;
  case 1:x=sinf(q->phase*6.2831853f)*.78f+triangle(q->phase)*.22f;break;
  case 2:x=triangle(q->phase)*.72f+sinf(q->phase2*12.56637f)*.18f*q->env;break;
  case 3:x=sinf(q->phase*6.2831853f)*(1+.35f*q->env);break;
  case 4:x=(triangle(q->phase)*.65f+noise_sample()*.35f);break;
  default:{float n=noise_sample(),hp=n-A->noise_last*.82f;A->noise_last=n;x=hp;}break;
 }
 float decay=q->voice==0?.99996f:q->voice==1?.9997f:q->voice==2?.99935f:q->voice==3?.9948f:q->voice==4?.991f:.986f;q->env*=decay;if(q->env<.0005f)q->on=false;return x*q->env;
}
static void callback(void *buffer,unsigned int frames){
 float*out=(float*)buffer;if(!A||!A->song){memset(out,0,frames*2*sizeof(float));return;}
 for(unsigned int i=0;i<frames;i++,A->clock++){
  while(A->next<A->song->count&&A->song->events[A->next].sample<=A->clock){const MusicEvent*e=&A->song->events[A->next++];for(int v=0;v<SFBS_VOICES;v++)if(!A->voices[v].on){float pan=e->stem==0?((e->note%3)-1)*.32f:e->stem==1?((e->voice&1)?-.18f:.18f):e->stem==2?0:((e->note%5)-2)*.16f;A->voices[v]=(SynthVoice){true,e->stem,e->note,e->voice,0,.25f,1,hz(e->note),pan};break;}}
  float left=0,right=0;for(int st=0;st<4;st++)A->stem[st]+=(A->stem_target[st]-A->stem[st])*.0008f;A->filter+=(A->filter_target-A->filter)*.00025f;
  for(int v=0;v<SFBS_VOICES;v++){SynthVoice*q=&A->voices[v];if(!q->on)continue;float x=voice_sample(q)*A->stem[q->stem]*.045f;left+=x*(1-q->pan*.55f);right+=x*(1+q->pan*.55f);}
  float dl=A->delay_l[A->delay_i_l],dr=A->delay_r[A->delay_i_r];A->delay_l[A->delay_i_l]=left+dr*.24f;A->delay_r[A->delay_i_r]=right+dl*.22f;A->delay_i_l=(A->delay_i_l+1)&4095;A->delay_i_r++;if(A->delay_i_r>=6144)A->delay_i_r=0;left+=dl*.18f;right+=dr*.18f;
  float in[2]={left,right};for(int ch=0;ch<2;ch++){A->lp[ch]+=(in[ch]-A->lp[ch])*A->filter;float y=tanhf(A->lp[ch]*1.28f),dc=y-A->dcx[ch]+.995f*A->dcy[ch];A->dcx[ch]=y;A->dcy[ch]=dc;out[i*2+ch]=dc;}
 }
}
void sfbs_audio_open(AudioEngine*a,const Song*s){memset(a,0,sizeof*a);A=a;a->song=s;a->noise=0x72a9f31u;a->filter=a->filter_target=.22f;a->stem[0]=a->stem_target[0]=1;InitAudioDevice();SetAudioStreamBufferSizeDefault(1024);a->stream=LoadAudioStream(SFBS_RATE,32,2);SetAudioStreamCallback(a->stream,callback);PlayAudioStream(a->stream);PauseAudioStream(a->stream);a->paused=true;a->ready=true;}
void sfbs_audio_close(AudioEngine*a){if(!a->ready)return;StopAudioStream(a->stream);UnloadAudioStream(a->stream);CloseAudioDevice();a->ready=false;A=0;}
void sfbs_audio_restart(AudioEngine*a,const Song*s){if(a->ready)StopAudioStream(a->stream);AudioStream stream=a->stream;bool ready=a->ready;memset(a,0,sizeof*a);a->stream=stream;a->ready=ready;a->song=s;a->noise=0x72a9f31u;a->filter=a->filter_target=.22f;a->stem[0]=a->stem_target[0]=1;A=a;if(a->ready){PlayAudioStream(a->stream);a->paused=false;}}
uint64_t sfbs_audio_clock(const AudioEngine*a){return a->clock;}
void sfbs_audio_stem(AudioEngine*a,int n,bool on){if(n>=0&&n<4)a->stem_target[n]=on?1:0;}
void sfbs_audio_damage(AudioEngine*a){a->filter_target=.045f;}
void sfbs_audio_pause(AudioEngine*a,bool pause){if(!a->ready||pause==a->paused)return;if(pause)PauseAudioStream(a->stream);else ResumeAudioStream(a->stream);a->paused=pause;}
void sfbs_audio_phase(AudioEngine*a,Phase p,bool failed){a->stem_target[0]=1;a->stem_target[1]=!failed&&p>=PH_PULSE;a->stem_target[2]=!failed&&p>=PH_FLOW;a->stem_target[3]=failed?0:p>=PH_BLOOM?1:p>=PH_SIGNAL?.2f:0;a->filter_target=failed?.035f:.22f;}
bool sfbs_audio_offline_check(const Song*s,uint64_t frames,float*peak){AudioEngine test={0},*previous=A;float block[512*2],maximum=0,difference=0;test.song=s;test.noise=0x72a9f31u;test.filter=test.filter_target=.22f;for(int i=0;i<4;i++)test.stem[i]=test.stem_target[i]=1;A=&test;while(test.clock<frames){unsigned int n=(unsigned int)(frames-test.clock);if(n>512)n=512;callback(block,n);for(unsigned int i=0;i<n;i++){float l=fabsf(block[i*2]),r=fabsf(block[i*2+1]);if(!isfinite(l)||!isfinite(r)){A=previous;return false;}if(l>maximum)maximum=l;if(r>maximum)maximum=r;difference+=fabsf(block[i*2]-block[i*2+1]);}}A=previous;if(peak)*peak=maximum;return test.clock==frames&&test.next>0&&maximum>0&&maximum<=1.01f&&difference>.01f;}
