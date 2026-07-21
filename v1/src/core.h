#ifndef SFBS_CORE_H
#define SFBS_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define SFBS_RATE 48000
#define SFBS_PPQN 96
#define SFBS_BARS 56
#define SFBS_PHRASES 14
#define SFBS_MAX_NODES 128
#define SFBS_PATH 1024
#define SFBS_EVENTS 8192
#define SFBS_VOICES 64
#define SFBS_PI 3.14159265358979323846f
typedef struct { float x,y; } V2;
typedef struct { uint64_t s; } Rng;
typedef struct { uint16_t bits; uint8_t steps,pulses,rotation; } Pattern;
typedef enum { MODE_BLOOM, MODE_FLOW, MODE_PULSE } GameMode;
typedef enum { PH_BOOT,PH_SIGNAL,PH_PULSE,PH_FLOW,PH_BLOOM,PH_FINAL } Phase;
typedef enum { NODE_DORMANT,NODE_TELEGRAPHING,NODE_AVAILABLE,NODE_RECOVERED,NODE_CORRUPTED,NODE_EXPIRED } NodeState;
typedef struct { uint64_t master,music_seed,layout_seed,gameplay_seed,visual_seed; int bpm,root,scale,progression,palette,primary_steps; float swing,warmth,symmetry; } Genome;
typedef struct { int start_bar,bars; Pattern pattern; uint8_t ring,stem,phase; float pressure; } Phrase;
typedef struct { V2 pos; uint8_t step,state,weight,phrase; float pulse; } Node;
typedef struct { uint64_t sample; uint32_t bar; uint8_t beat,step; float beat_phase,bar_phase; } Transport;
typedef struct { uint64_t sample,tick; uint8_t stem,voice,note,velocity; uint32_t length; } MusicEvent;
typedef struct { V2 p[SFBS_PATH]; uint64_t tick[SFBS_PATH]; int head,count; } PathBuffer;
uint64_t sfbs_hash(const char *s); uint64_t sfbs_rng(Rng *r); float sfbs_randf(Rng *r);
void sfbs_seed_normalize(const char *in,char out[7]); void sfbs_random_seed(uint64_t entropy,char out[7]);
Genome sfbs_genome(const char seed[7]); Pattern sfbs_euclid(int k,int n,int rot); int sfbs_popcount(uint16_t x);
uint64_t sfbs_tick_sample(uint64_t tick,int bpm); Transport sfbs_transport(uint64_t sample,int bpm);
void sfbs_phrases(const Genome *g,GameMode mode,Phrase out[SFBS_PHRASES]);
const char *sfbs_mode_name(GameMode m); const char *sfbs_result_label(int pct);
float sfbs_resonance(float beat_phase,int bpm,float sigma); float sfbs_clampf(float x,float a,float b);
V2 sfbs_add(V2 a,V2 b); V2 sfbs_scale(V2 a,float s); float sfbs_len(V2 a); V2 sfbs_norm(V2 a);
#endif

