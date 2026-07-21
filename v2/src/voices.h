/* Purpose-built voices (§8.3) + announcer (§8.4). All synthesis, no samples. */
#ifndef G_VOICES_H
#define G_VOICES_H

#include "common.h"

typedef enum {
    V_PAD = 0, V_BASS, V_PLUCK, V_BELL, V_KICK, V_HAT, V_SNARE,
    V_VOX, V_DING, V_NOISE, V_SWEEP
} VoiceType;

#define VOX_MAX_SYL 4
#define POLYPHONY 16

typedef struct {
    uint8_t  type;
    uint8_t  active;
    int8_t   bus;        /* >=0 stem index, -1 = sfx, -2 = announcer */
    uint32_t age;        /* for oldest-first stealing */
    uint32_t t;          /* samples since note-on */

    double   ph, ph2, ph3;
    float    f0, f1;
    float    amp, pan;

    /* amplitude env */
    float    env, atk, dec, sus, rel;
    int      stage;      /* 0 attack, 1 decay->sustain, 2 release */
    int      gate;

    /* filter */
    float    lp_cut, lp_z1, lp_z2, lp_env, lp_env_dec, lp_amt;

    /* pitch env (kick drop, scratch) */
    float    p_env, p_env_dec, p_amt;

    float    fm_amt;
    uint32_t noise;

    /* svf state (vox formants) */
    float    svf_l[2], svf_b[2];

    /* announcer contour */
    int      syl_count, syl_idx;
    float    syl_t;
    float    syl_pitch[VOX_MAX_SYL];
    float    syl_dur[VOX_MAX_SYL];
    float    syl_bright[VOX_MAX_SYL];
} Voice;

float note_freq(float midi);

void voice_clear(Voice *v);
void voice_note(Voice *v, VoiceType type, int bus, float midi, float amp, float pan);
void voice_vox(Voice *v, int word_id);
void voice_release(Voice *v);
/* renders `frames` samples, ADDING into busL/busR per bus index.
 * busL/busR are arrays of [STEM_COUNT+2][frames] laid out by caller. */
void voice_render(Voice *v, float *outL, float *outR, int frames, float pitch_mul);

#endif
