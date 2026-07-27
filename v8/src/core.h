#ifndef PW_CORE_H
#define PW_CORE_H
#include <stdint.h>
#include <math.h>
#define PW_HZ 240
#define PW_DT (1.0f/240.0f)
#define PW_MAX_GATES 96
#define PW_MAX_SHOTS 32
#define PW_MAX_GHOST 180000
#define PW_GRAVITY 23.0f
#define PW_PULSE 7.8f
#define PW_FLOOR (-15.0f)
#define PW_CEIL 15.0f
static inline float pw_clamp(float x,float a,float b){return x<a?a:x>b?b:x;}
static inline float pw_lerp(float a,float b,float t){return a+(b-a)*t;}
typedef struct { uint32_t s; } PwRng;
static inline uint32_t pw_rand(PwRng *r){uint32_t x=r->s? r->s:1u;x^=x<<13;x^=x>>17;x^=x<<5;return r->s=x;}
static inline float pw_unit(PwRng *r){return (pw_rand(r)&0xffff)/65535.0f;}
#endif
