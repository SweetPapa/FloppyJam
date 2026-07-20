#include "audio.h"
#include <math.h>
#include <string.h>
static AudioEngine *A;
static float hz(int n){return 440.0f*powf(2.0f,(n-69)/12.0f);}
static void callback(void *buffer,unsigned int frames){float*out=(float*)buffer;if(!A||!A->song){memset(out,0,frames*2*sizeof(float));return;}for(unsigned int i=0;i<frames;i++,A->clock++){while(A->next<A->song->count&&A->song->events[A->next].sample<=A->clock){const MusicEvent*e=&A->song->events[A->next++];for(int v=0;v<SFBS_VOICES;v++)if(!A->voices[v].on){A->voices[v]=(SynthVoice){true,e->stem,e->note,e->voice,0,1,hz(e->note)};break;}}float mix=0;for(int v=0;v<SFBS_VOICES;v++){SynthVoice*q=&A->voices[v];if(!q->on)continue;q->phase+=q->freq/SFBS_RATE;if(q->phase>=1)q->phase-=1;float x=q->voice==3?sinf(q->phase*6.28318f):q->voice>=4?((q->phase<.5f?1:-1)*(1-q->env)):asinf(sinf(q->phase*6.28318f))*.6366f;float decay=q->voice==0?.99996f:q->voice==1?.9997f:q->voice==2?.99935f:.995f;q->env*=decay;mix+=x*q->env*A->stem[q->stem]*.055f;if(q->env<.0005f)q->on=false;}A->lp+=(mix-A->lp)*.22f;float y=tanhf(A->lp*1.2f);float dc=y-A->dcx+.995f*A->dcy;A->dcx=y;A->dcy=dc;out[i*2]=dc;out[i*2+1]=dc;}}
void sfbs_audio_open(AudioEngine*a,const Song*s){memset(a,0,sizeof*a);A=a;a->song=s;for(int i=0;i<4;i++)a->stem[i]=1;InitAudioDevice();SetAudioStreamBufferSizeDefault(1024);a->stream=LoadAudioStream(SFBS_RATE,32,2);SetAudioStreamCallback(a->stream,callback);PlayAudioStream(a->stream);a->ready=true;}
void sfbs_audio_close(AudioEngine*a){if(!a->ready)return;StopAudioStream(a->stream);UnloadAudioStream(a->stream);CloseAudioDevice();a->ready=false;A=0;}
void sfbs_audio_restart(AudioEngine*a,const Song*s){a->song=s;a->clock=0;a->next=0;memset(a->voices,0,sizeof a->voices);}
uint64_t sfbs_audio_clock(const AudioEngine*a){return a->clock;}
void sfbs_audio_stem(AudioEngine*a,int n,bool on){if(n>=0&&n<4)a->stem[n]=on?1:0;}
void sfbs_audio_damage(AudioEngine*a){a->lp*=.55f;}
bool sfbs_audio_offline_check(const Song*s,uint64_t frames,float*peak){AudioEngine test={0},*previous=A;float block[512*2];test.song=s;for(int i=0;i<4;i++)test.stem[i]=1;A=&test;float maximum=0;while(test.clock<frames){unsigned int n=(unsigned int)(frames-test.clock);if(n>512)n=512;callback(block,n);for(unsigned int i=0;i<n*2;i++){float x=fabsf(block[i]);if(!isfinite(x)){A=previous;return false;}if(x>maximum)maximum=x;}}A=previous;if(peak)*peak=maximum;return test.clock==frames&&test.next>0&&maximum>0&&maximum<=1.01f;}
