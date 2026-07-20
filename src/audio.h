#ifndef SFBS_AUDIO_H
#define SFBS_AUDIO_H
#include "sim.h"
#include "raylib.h"
typedef struct { bool on; uint8_t stem,note,voice; float phase,env,freq; } SynthVoice;
typedef struct { AudioStream stream; const Song *song; uint64_t clock; int next; SynthVoice voices[SFBS_VOICES]; float stem[4],lp,dcx,dcy; bool ready,paused; } AudioEngine;
void sfbs_audio_open(AudioEngine*,const Song*); void sfbs_audio_close(AudioEngine*); void sfbs_audio_restart(AudioEngine*,const Song*); uint64_t sfbs_audio_clock(const AudioEngine*); void sfbs_audio_stem(AudioEngine*,int,bool); void sfbs_audio_damage(AudioEngine*);
bool sfbs_audio_offline_check(const Song*,uint64_t,float*);
#endif
