/* Genome, pattern generation and the bar/event bus (§8).
 *
 * The pattern generator is a PURE function of (genome, bar_index, flavor).
 * The sequencer plays it inside the audio callback; the game thread calls the
 * exact same function to build charts. That is what makes "the chart is the
 * music" literally true, with no cross-thread guessing.
 */
#ifndef G_SEQUENCER_H
#define G_SEQUENCER_H

#include "common.h"
#include "rng.h"

/* ---- genome (§8.2) -------------------------------------------------- */
typedef struct {
    uint32_t seed_code;
    uint32_t rng_state;
    uint8_t  root, scale;       /* scale: 0 major 1 minor 2 dorian 3 mixolydian */
    uint8_t  chord_family;      /* 0..6 */
    uint8_t  euclid_k, euclid_n;
    uint8_t  bass_pattern, motif_shape, palette;
    uint16_t bpm_base;          /* 104..124 */
    int8_t   motif[4];          /* scale degrees of the bell motif */
} Genome;

void genome_init(Genome *g, uint32_t seed_code);
/* midi note for scale degree (degree may be <0 or >6, octaves wrap) */
int  genome_scale_note(const Genome *g, int degree, int octave);
/* chord tones (3 or 4 notes) for a bar; returns count */
int  genome_chord(const Genome *g, int bar_index, int out_notes[4], int octave);
const char *genome_scale_name(const Genome *g);

/* ---- per-bar flavor requested by the game thread --------------------- */
typedef enum {
    FLAVOR_NONE = 0,
    FLAVOR_BELL_MOTIF,   /* bell plays the genome motif this bar */
    FLAVOR_ARP_UP,       /* pluck plays an ascending 4-note arpeggio */
    FLAVOR_SNARE_ROLL,   /* one bar of 16th snare */
    FLAVOR_REST,         /* everything but the pad drops out */
    FLAVOR_BREAKDOWN,    /* drums only (card pick bar) */
    FLAVOR_FILL          /* fill bar (telegraph) */
} BarFlavor;

/* ---- authored pattern for one bar ----------------------------------- */
typedef struct {
    uint8_t kick[STEPS_PER_BAR];
    uint8_t hat[STEPS_PER_BAR];      /* 1 = normal, 2 = accent */
    uint8_t snare[STEPS_PER_BAR];
    uint8_t bass[STEPS_PER_BAR];
    int8_t  bass_note[STEPS_PER_BAR];
    uint8_t pluck[STEPS_PER_BAR];
    int8_t  pluck_note[STEPS_PER_BAR];
    int8_t  bell_note[STEPS_PER_BAR];   /* -1 = none */
    uint8_t chord_change[STEPS_PER_BAR];
    uint8_t is_rest;
    int     chord_notes[4];
    int     chord_count;
} BarPattern;

/* PURE: identical on audio and game thread. */
void seq_bar_pattern(const Genome *g, int bar_index, BarFlavor flavor, BarPattern *out);

/* ---- bar info published by the sequencer ---------------------------- */
typedef struct {
    int      bar_index;
    float    bpm;
    uint64_t step_sample[STEPS_PER_BAR + 1]; /* absolute sample time of each 16th + bar end */
    BarFlavor flavor;
} BarInfo;

/* ---- commands, game thread -> audio thread (SPSC ring) --------------- */
typedef enum {
    CMD_NONE = 0,
    CMD_SET_BPM,          /* fa = bpm, applied at bar ia (ramped over 1 beat) */
    CMD_STEM_GAIN,        /* ia = stem, fa = gain, ib = apply bar (-1 = asap) */
    CMD_BAR_FLAVOR,       /* ia = bar index, ib = flavor */
    CMD_MASTER_LP,        /* fa = cutoff hz, fb = seconds to glide */
    CMD_MASTER_PITCH,     /* fa = target pitch mul, fb = seconds */
    CMD_MASTER_GAIN,      /* fa = target, fb = seconds */
    CMD_SFX,              /* ia = sfx id, fa = param */
    CMD_ANNOUNCE,         /* ia = word id */
    CMD_METRONOME,        /* ia = on/off */
    CMD_START,            /* start transport at bar 0 */
    CMD_STOP,
    CMD_SET_GENOME,       /* payload copied via seq_set_genome() before start */
    CMD_DRUM_MUTE         /* ia = on/off (DEAF card) */
} SeqCmdType;

typedef struct {
    uint8_t type;
    int     ia, ib;
    float   fa, fb;
} SeqCmd;

/* ---- SFX + announcer ids -------------------------------------------- */
enum {
    SFX_PERFECT = 0,  /* pitched ding, param = midi note */
    SFX_GOOD,
    SFX_MISS,
    SFX_HONK,         /* REST violation */
    SFX_SCRATCH,
    SFX_STAMP,
    SFX_CARD,
    SFX_UI,
    SFX_CLICK,
    SFX_HEAL,
    SFX_COUNT_
};

enum {
    VOX_READY = 0, VOX_GO, VOX_NICE, VOX_WIPED, VOX_OOF, VOX_HYPE, VOX_LAUGH,
    VOX_GAME_BASE   /* + microgame index */
};

#endif
