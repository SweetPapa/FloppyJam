/* Audio callback, mixer, master FX and the transport (§8.1).
 * The sequencer runs INSIDE the callback. Nothing else owns time. */
#ifndef G_AUDIO_H
#define G_AUDIO_H

#include "common.h"
#include "sequencer.h"

typedef struct {
    uint64_t sample_clock;
    float    bpm;
    float    samples_per_beat;
    int      bar;
    int      step;        /* 0..15 within the bar */
    float    beat_phase;  /* 0..1 within the current beat */
    float    bar_phase;   /* 0..1 within the current bar */
    int      running;
} ClockSnap;

bool audio_init(void);
void audio_shutdown(void);

void audio_set_genome(const Genome *g);   /* safe while stopped */
void audio_push(SeqCmd c);
bool audio_pop_bar(BarInfo *out);         /* game thread drains published bars */
void audio_snapshot(ClockSnap *out);

/* convenience wrappers */
void audio_start(float bpm);
void audio_stop(void);
void audio_stem_gain(int stem, float gain, int apply_bar);
void audio_bar_flavor(int bar, BarFlavor f);
void audio_set_bpm(float bpm, int apply_bar);
void audio_sfx(int sfx_id, float param);
void audio_announce(int word_id);
void audio_master_lp(float cutoff, float seconds);
void audio_master_pitch(float mul, float seconds);
void audio_master_gain(float g, float seconds);
void audio_metronome(int on);
void audio_drum_mute(int on);

/* peak level of the bass bus, for SPIN (§6.2) */
float audio_bass_level(void);
float audio_master_level(void);

#endif
