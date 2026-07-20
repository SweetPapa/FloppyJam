#ifndef SFBS_AUDIO_H
#define SFBS_AUDIO_H
#include "sim.h"
#include "raylib.h"
typedef struct { bool on; uint8_t stem,note,voice; float phase,phase2,env,freq,pan; } SynthVoice;
typedef struct { AudioStream stream; const Song *song; volatile uint64_t clock; int next; SynthVoice voices[SFBS_VOICES]; float stem[4]; volatile float stem_target[4],filter_target; float lp[2],dcx[2],dcy[2],filter,delay_l[4096],delay_r[6144],noise_last; uint32_t delay_i_l,delay_i_r,noise; bool ready,paused; } AudioEngine;
void sfbs_audio_open(AudioEngine*,const Song*); void sfbs_audio_close(AudioEngine*); void sfbs_audio_restart(AudioEngine*,const Song*); uint64_t sfbs_audio_clock(const AudioEngine*); void sfbs_audio_stem(AudioEngine*,int,bool); void sfbs_audio_damage(AudioEngine*); void sfbs_audio_pause(AudioEngine*,bool); void sfbs_audio_phase(AudioEngine*,Phase,bool);
bool sfbs_audio_offline_check(const Song*,uint64_t,float*);
#endif
