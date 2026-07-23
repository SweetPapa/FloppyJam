#include "audio.h"

#include "raylib.h"
#include <math.h>
#include <stdint.h>

#define PB_AUDIO_RATE 48000
#define PB_AUDIO_VOICES 32
#define TAU 6.28318530718f

typedef struct PbVoice {
    float phase, frequency, gain, decay, envelope;
    uint32_t noise;
    uint8_t wave, bus;
    bool active;
} PbVoice;

typedef struct PbAudioState {
    AudioStream stream;
    PbVoice voices[PB_AUDIO_VOICES];
    uint64_t sample_clock;
    uint64_t next_step;
    volatile uint32_t pending_sfx;
    volatile int level;
    volatile int chase;
    volatile int paused;
    volatile int master;
    volatile int music;
    volatile int sfx;
    volatile int glint_chain;
    int step;
} PbAudioState;

static PbAudioState audio;
static const float garden_lead[16]={523.25f,0,659.25f,0,783.99f,659.25f,587.33f,0,523.25f,587.33f,659.25f,783.99f,880,783.99f,659.25f,587.33f};
static const float cascade_lead[16]={311.13f,369.99f,415.30f,466.16f,369.99f,415.30f,466.16f,554.37f,415.30f,466.16f,554.37f,622.25f,466.16f,554.37f,622.25f,739.99f};
static const float garden_bass[8]={130.81f,0,146.83f,0,164.81f,0,146.83f,0};
static const float cascade_bass[8]={77.78f,77.78f,92.50f,77.78f,103.83f,92.50f,116.54f,103.83f};
#if POLYBLOOM_INCLUDE_LEVEL3
static const float foundry_lead[16]={220,0,329.63f,440,246.94f,0,369.99f,493.88f,277.18f,329.63f,415.30f,554.37f,329.63f,415.30f,493.88f,659.25f};
static const float foundry_bass[8]={55,55,61.74f,55,69.30f,61.74f,73.42f,82.41f};
#endif
#if POLYBLOOM_INCLUDE_LEVEL4
static const float crown_lead[16]={293.66f,349.23f,440,523.25f,349.23f,440,523.25f,587.33f,440,523.25f,698.46f,783.99f,523.25f,698.46f,880,1046.5f};
static const float crown_bass[8]={73.42f,73.42f,87.31f,73.42f,98,87.31f,110,98};
#endif

static void add_voice(float frequency, float gain, float decay, int wave, int bus)
{
    int i;
    for(i=0;i<PB_AUDIO_VOICES;++i) if(!audio.voices[i].active) {
        audio.voices[i]=(PbVoice){0,frequency,gain,decay,0,0x9e3779b9u+(uint32_t)i,(uint8_t)wave,(uint8_t)bus,true};
        return;
    }
}

static void voice(float frequency,float gain,float decay,int wave) { add_voice(frequency,gain,decay,wave,1); }
static void music_voice(float frequency,float gain,float decay,int wave) { if(frequency>0) add_voice(frequency,gain,decay,wave,0); }

static void trigger_sfx(int id)
{
    switch(id) {
        case PB_SFX_JUMP: voice(330,.18f,.99955f,2); break;
        case PB_SFX_GLIDE_OPEN: voice(520,.12f,.99975f,1); break;
        case PB_SFX_GLIDE_RING: voice(740,.16f,.9997f,0); break;
        case PB_SFX_BURST: voice(180,.25f,.99935f,3); voice(520,.1f,.9995f,1); break;
        case PB_SFX_BOUNCE: voice(220,.2f,.9997f,2); break;
        case PB_SFX_LANDING: voice(90,.16f,.9987f,3); break;
        case PB_SFX_GLINT: {
            static const float pitches[8]={659.25f,739.99f,783.99f,880,987.77f,1046.5f,1174.66f,1318.51f};
            voice(pitches[audio.glint_chain<8?audio.glint_chain:7],.13f,.9997f,0);
        } break;
        case PB_SFX_SEED: voice(523.25f,.14f,.99985f,0); voice(659.25f,.12f,.99985f,0); break;
        case PB_SFX_CHECKPOINT: voice(659.25f,.13f,.99985f,2); break;
        case PB_SFX_DAMAGE: voice(115,.25f,.9995f,1); break;
        case PB_SFX_FALL: voice(80,.2f,.9998f,2); break;
        case PB_SFX_ENEMY_DEFEAT: voice(392,.18f,.9996f,2); break;
        case PB_SFX_PRISM_BREAK: voice(260,.18f,.9989f,3); break;
        case PB_SFX_TILE_WARNING: voice(196,.12f,.9998f,1); break;
        case PB_SFX_GATE: voice(392,.15f,.9999f,0); break;
        case PB_SFX_MENU_MOVE: voice(440,.08f,.998f,2); break;
        case PB_SFX_MENU_CONFIRM: voice(660,.1f,.9994f,2); break;
        case PB_SFX_LEVEL_COMPLETE:
            voice(523.25f,.14f,.99992f,0); voice(659.25f,.12f,.99992f,0); voice(783.99f,.12f,.99992f,0); break;
        default: break;
    }
}

static void music_step(void)
{
    const float *lead=audio.level?cascade_lead:garden_lead;
    const float *bass=audio.level?cascade_bass:garden_bass;
#if POLYBLOOM_INCLUDE_LEVEL3
    if(audio.level==2) { lead=foundry_lead; bass=foundry_bass; }
#endif
#if POLYBLOOM_INCLUDE_LEVEL4
    if(audio.level==3) { lead=crown_lead; bass=crown_bass; }
#endif
    float note=lead[audio.step&15];
    if(note>0) music_voice(note,.045f,.99988f,audio.level?1:0);
    if((audio.step&1)==0) music_voice(bass[(audio.step>>1)&7],.055f,.9999f,2);
    if((audio.step&3)==0) music_voice(58,.06f,.9965f,3);
    if(audio.chase&&(audio.step&1)==0) music_voice(48,.09f,.9992f,1);
    audio.step++;
}

static float sample_voice(PbVoice *v)
{
    float s;
    if(v->wave==0) s=sinf(v->phase*TAU);
    else if(v->wave==1) s=v->phase<.35f?1:-1;
    else if(v->wave==2) s=1-4*fabsf(v->phase-.5f);
    else { v->noise=v->noise*1664525u+1013904223u; s=((v->noise>>9)/8388608.0f)*2-1; }
    v->phase+=v->frequency/PB_AUDIO_RATE;
    if(v->phase>=1) v->phase-=1;
    v->gain*=v->decay;
    v->envelope=fminf(1,v->envelope+.005f);
    if(v->gain<.0001f) v->active=false;
    return s*v->gain*v->envelope;
}

static void audio_callback(void *buffer_data, unsigned int frames)
{
    float *out=buffer_data;
    unsigned int frame;
    uint32_t pending=audio.pending_sfx;
    int effect;
    audio.pending_sfx=0;
    for(effect=0;effect<PB_SFX_COUNT;++effect) if(pending&(1u<<effect)) trigger_sfx(effect);
    for(frame=0;frame<frames;++frame) {
        float mix=0;
        int i;
        if(!audio.paused&&audio.sample_clock>=audio.next_step) {
            int bpm=audio.level==0?114:audio.level==1?128:audio.level==2?122:136;
            music_step();
            audio.next_step=audio.sample_clock+(uint64_t)(PB_AUDIO_RATE*60/(bpm*4));
        }
        for(i=0;i<PB_AUDIO_VOICES;++i) if(audio.voices[i].active)
            mix+=sample_voice(&audio.voices[i])*(audio.voices[i].bus?audio.sfx:audio.music)/100.0f;
        mix*=audio.master/100.0f;
        mix=mix/(1+fabsf(mix));
        out[frame*2]=out[frame*2+1]=mix;
        audio.sample_clock++;
    }
}

void pb_audio_open(void)
{
    audio=(PbAudioState){0}; audio.master=80; audio.music=80; audio.sfx=80;
    InitAudioDevice();
    audio.stream=LoadAudioStream(PB_AUDIO_RATE,32,2);
    SetAudioStreamCallback(audio.stream,audio_callback);
    PlayAudioStream(audio.stream);
}

void pb_audio_close(void) { StopAudioStream(audio.stream); UnloadAudioStream(audio.stream); }
void pb_audio_set_level(int level) { audio.level=level; audio.step=0; audio.next_step=audio.sample_clock; }
void pb_audio_set_chase(bool active) { audio.chase=active; }
void pb_audio_set_paused(bool paused) { if(audio.paused&&!paused) audio.next_step=audio.sample_clock; audio.paused=paused; }
void pb_audio_set_volume(int master, int music, int sfx) { audio.master=master; audio.music=music; audio.sfx=sfx; }
void pb_audio_sfx(PbSfx effect) { if(effect<PB_SFX_COUNT) audio.pending_sfx|=1u<<effect; }
void pb_audio_glint(int chain)
{
    audio.glint_chain=chain>0?chain-1:0;
    audio.pending_sfx|=1u<<PB_SFX_GLINT;
}
