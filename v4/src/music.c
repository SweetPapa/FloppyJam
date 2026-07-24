/* music.c — procedural soundtrack generator (pure C, no raylib).
 *
 * A small tracker: a 16th-note sequencer drives five voices (kick, snare,
 * hat, bass, arp, pad) through a tempo-synced delay and a soft limiter.
 *
 * The "steady formula" that keeps it musical no matter the seed:
 *   root note + 7-note scale + a hand-picked diatonic chord progression.
 * Everything melodic is drawn from the current chord's tones, so it can
 * never land on a wrong note. The seed only chooses *which* key, scale,
 * progression, tempo and groove — never whether the harmony is valid.
 *
 * Per-bar variation is re-derived from hash(seed, bar), so the music keeps
 * evolving (it never loops audibly) while staying deterministic.
 */
#include "music.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define TAU 6.28318530718f
#define MAX_ARP 4          /* overlapping pluck voices */
#define DELAY_MAX 65536    /* ~1.4s at 44.1k */

/* ---- deterministic hashing -------------------------------------- */
static unsigned int hash_u(unsigned int x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
static unsigned int hash2(unsigned int a, unsigned int b) {
    return hash_u(a * 0x9e3779b9u ^ hash_u(b));
}
/* 0..n-1 */
static int hpick(unsigned int h, int n) { return (int)(h % (unsigned)n); }

/* ---- scales & progressions (the formula) ------------------------ */
/* All 7-note scales so triads stack cleanly by thirds. */
static const int SCALES[4][7] = {
    {0, 2, 3, 5, 7, 8, 10},  /* natural minor  - brooding      */
    {0, 2, 3, 5, 7, 9, 10},  /* dorian         - cool, hopeful */
    {0, 2, 4, 5, 7, 9, 11},  /* major (ionian) - bright        */
    {0, 2, 4, 6, 7, 9, 11},  /* lydian         - dreamy        */
};
/* Progressions as scale degrees (0 = tonic). All are common, strong loops. */
static const int PROGS[6][4] = {
    {0, 5, 3, 4},  /* i  - VI - iv - v   */
    {0, 3, 5, 4},  /* i  - iv - VI - v   */
    {5, 3, 0, 4},  /* VI - iv - i  - v   */
    {0, 4, 5, 3},  /* i  - v  - VI - iv  */
    {0, 6, 3, 4},  /* i  - VII- iv - v   */
    {0, 2, 5, 4},  /* i  - III- VI - v   */
};

typedef struct {
    float ph, env, freq, pan;
    int active;
} Pluck;

typedef struct {
    int sr;
    /* theme */
    int seed, pending_seed, have_pending;
    int root;              /* midi note of the tonic */
    const int *scale;
    const int *prog;
    float bpm;
    int groove;            /* which drum/bass pattern family */
    int arp_mode;
    /* transport */
    double samples_per_step;
    double step_acc;
    long step;             /* global 16th counter */
    /* voices */
    float kick_env, kick_ph, kick_pitch;
    float snare_env, snare_tone_ph;
    float hat_env;
    float bass_env, bass_ph, bass_freq;
    Pluck arp[MAX_ARP];
    float pad_ph[6], pad_freq[6], pad_amp, pad_target;
    /* fx */
    float lp_l, lp_r;
    float delay[DELAY_MAX * 2];
    int delay_pos, delay_len;
    /* control */
    float intensity;       /* 0..1, opens up the mix as danger rises */
    float master;
    unsigned int noise_rng;
    int started;
} Music;

static Music M;

static float noisef(void) {
    M.noise_rng = M.noise_rng * 1664525u + 1013904223u;
    return ((float)((M.noise_rng >> 9) & 0xFFFF) / 32768.0f) - 1.0f;
}

static float midi_hz(float midi) {
    return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f);
}

/* nth tone of the triad built on a scale degree (0=root,1=third,2=fifth) */
static int chord_tone(int degree, int idx) {
    int di = degree + idx * 2;
    int oct = di / 7;
    return M.scale[di % 7] + 12 * oct;
}

static void apply_theme(int seed) {
    unsigned int h = hash_u((unsigned int)seed * 2654435761u + 17u);
    M.seed = seed;
    /* keep the tonic in a comfortable register */
    static const int ROOTS[6] = {45, 47, 48, 50, 52, 53}; /* A2..F3 */
    M.root = ROOTS[hpick(h, 6)];
    M.scale = SCALES[hpick(hash_u(h + 1), 4)];
    M.prog = PROGS[hpick(hash_u(h + 2), 6)];
    M.bpm = 88.0f + (float)hpick(hash_u(h + 3), 6) * 7.0f; /* 88..123 */
    M.groove = hpick(hash_u(h + 4), 3);
    M.arp_mode = hpick(hash_u(h + 5), 4);
    M.samples_per_step = (double)M.sr * 60.0 / ((double)M.bpm * 4.0);
    /* delay = a dotted 8th, the classic "big space" tempo sync */
    M.delay_len = (int)(M.samples_per_step * 6.0);
    if (M.delay_len > DELAY_MAX - 4) M.delay_len = DELAY_MAX - 4;
    if (M.delay_len < 8) M.delay_len = 8;
}

void music_init(int seed, int sample_rate) {
    memset(&M, 0, sizeof M);
    M.sr = sample_rate > 0 ? sample_rate : 44100;
    M.noise_rng = 0x1234567u;
    M.master = 1.15f;
    M.intensity = 0.0f;
    apply_theme(seed);
    M.started = 1;
}

void music_set_theme(int seed) {
    if (!M.started) { music_init(seed, 44100); return; }
    if (seed == M.seed) return;
    M.pending_seed = seed;      /* swap on the next bar so it never lurches */
    M.have_pending = 1;
}

void music_set_intensity(float v) {
    if (v < 0) v = 0; else if (v > 1) v = 1;
    M.intensity = v;
}

/* Human-readable summary of the current theme (for tooling / debugging). */
void music_describe(char *buf, int n) {
    static const char *NOTES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    static const char *SCALE_NAMES[4] = {"minor", "dorian", "major", "lydian"};
    int si = 0;
    for (int i = 0; i < 4; i++) if (M.scale == SCALES[i]) si = i;
    int pi = 0;
    for (int i = 0; i < 6; i++) if (M.prog == PROGS[i]) pi = i;
    const char *rn = NOTES[((M.root % 12) + 12) % 12];
    snprintf(buf, (size_t)n,
             "key=%s %s  prog=[%d %d %d %d]  bpm=%.0f  groove=%d  arp=%d",
             rn, SCALE_NAMES[si], M.prog[0], M.prog[1], M.prog[2], M.prog[3],
             (double)M.bpm, M.groove, M.arp_mode);
    (void)pi;
}

/* ---- sequencer: fire the events for one 16th step ---------------- */
static void do_step(void) {
    long s = M.step;
    int step_in_bar = (int)(s & 15);
    int bar = (int)(s >> 4);

    if (step_in_bar == 0) {
        if (M.have_pending) { apply_theme(M.pending_seed); M.have_pending = 0; }
        /* advance the chord once per bar */
        int degree = M.prog[bar & 3];
        for (int i = 0; i < 3; i++) {
            int semi = chord_tone(degree, i);
            /* two detuned oscillators per chord tone = width */
            M.pad_freq[i * 2 + 0] = midi_hz((float)(M.root + 12 + semi));
            M.pad_freq[i * 2 + 1] = midi_hz((float)(M.root + 12 + semi) + 0.09f);
        }
        M.pad_target = 0.16f + 0.05f * M.intensity;
    }

    unsigned int bh = hash2((unsigned int)M.seed, (unsigned int)bar);
    int degree = M.prog[bar & 3];

    /* --- drums: steady backbone, seeded fills --- */
    int kick = 0, snare = 0;
    if (M.groove == 0) kick = (step_in_bar == 0 || step_in_bar == 6 || step_in_bar == 8);
    else if (M.groove == 1) kick = (step_in_bar == 0 || step_in_bar == 8 || step_in_bar == 11);
    else kick = (step_in_bar == 0 || step_in_bar == 4 || step_in_bar == 8 || step_in_bar == 12);
    snare = (step_in_bar == 4 || step_in_bar == 12);
    /* occasional extra kick, chosen per bar so it stays coherent */
    if (step_in_bar == 14 && (hash2(bh, 7u) & 3) == 0) kick = 1;

    if (kick) { M.kick_env = 1.0f; M.kick_ph = 0; M.kick_pitch = 1.0f; }
    if (snare) M.snare_env = 1.0f;
    /* hats: 8ths normally, 16ths when the heat is on */
    int hat = (step_in_bar % 2 == 0) || (M.intensity > 0.45f);
    if (hat) M.hat_env = (step_in_bar % 4 == 0) ? 0.55f : 0.32f;

    /* --- bass: roots and fifths locked to the chord --- */
    int bass_hit = (step_in_bar == 0 || step_in_bar == 3 || step_in_bar == 6 ||
                    step_in_bar == 8 || step_in_bar == 11);
    if (M.groove == 2) bass_hit = (step_in_bar % 4 == 0) || step_in_bar == 14;
    if (bass_hit) {
        int semi = chord_tone(degree, 0);
        if (step_in_bar == 6 || step_in_bar == 11) semi = chord_tone(degree, 2); /* fifth */
        M.bass_freq = midi_hz((float)(M.root - 12 + semi));
        M.bass_env = 1.0f;
    }

    /* --- arp / lead: chord tones only, so it is always consonant --- */
    int play_arp = 0;
    switch (M.arp_mode) {
    case 0: play_arp = (step_in_bar % 2) == 0; break;          /* 8ths     */
    case 1: play_arp = 1; break;                                /* 16ths    */
    case 2: play_arp = (step_in_bar % 4) != 3; break;           /* gallop   */
    default: play_arp = (step_in_bar % 2) == 1; break;          /* offbeat  */
    }
    if (play_arp) {
        unsigned int nh = hash2(bh, (unsigned int)(step_in_bar + 31));
        int idx = (int)(nh % 3u);                 /* which chord tone */
        int oct = ((nh >> 5) & 3u) == 0 ? 12 : 0; /* occasional octave lift */
        int semi = chord_tone(degree, idx);
        float f = midi_hz((float)(M.root + 24 + semi + oct));
        for (int i = 0; i < MAX_ARP; i++) {
            if (!M.arp[i].active || M.arp[i].env < 0.08f) {
                M.arp[i].active = 1;
                M.arp[i].ph = 0;
                M.arp[i].env = 0.5f + 0.3f * ((nh >> 9) & 1u);
                M.arp[i].freq = f;
                /* alternate the stereo field for a wide, moving lead */
                M.arp[i].pan = ((step_in_bar & 1) ? 0.72f : 0.28f);
                break;
            }
        }
    }
}

/* ---- render interleaved stereo 16-bit ---------------------------- */
void music_render(short *out, int frames) {
    if (!M.started) { memset(out, 0, sizeof(short) * 2 * frames); return; }

    const float sr = (float)M.sr;
    /* decay coefficients (per sample) */
    float kick_d = expf(-6.0f / sr * 60.0f);
    float snare_d = expf(-9.0f / sr * 60.0f);
    float hat_d = expf(-30.0f / sr * 60.0f);
    float bass_d = expf(-3.2f / sr * 60.0f);
    float arp_d = expf(-4.5f / sr * 60.0f);
    /* filter cutoff opens as intensity rises */
    float cut = 0.16f + 0.30f * M.intensity;

    for (int n = 0; n < frames; n++) {
        /* advance the sequencer */
        M.step_acc += 1.0;
        if (M.step_acc >= M.samples_per_step) {
            M.step_acc -= M.samples_per_step;
            M.step++;
            do_step();
        }

        float l = 0, r = 0;

        /* kick: sine with a fast pitch drop */
        if (M.kick_env > 0.001f) {
            M.kick_pitch *= 0.9995f;
            float f = 48.0f + 90.0f * M.kick_pitch;
            M.kick_ph += TAU * f / sr;
            float v = sinf(M.kick_ph) * M.kick_env * 0.85f;
            l += v; r += v;
            M.kick_env *= kick_d;
        }
        /* snare: noise + body tone */
        if (M.snare_env > 0.001f) {
            M.snare_tone_ph += TAU * 190.0f / sr;
            float v = (noisef() * 0.7f + sinf(M.snare_tone_ph) * 0.3f) * M.snare_env * 0.32f;
            l += v; r += v;
            M.snare_env *= snare_d;
        }
        /* hat: bright noise, panned slightly right for groove */
        if (M.hat_env > 0.001f) {
            float v = noisef() * M.hat_env * 0.12f;
            l += v * 0.8f; r += v;
            M.hat_env *= hat_d;
        }
        /* bass: sine + a touch of its octave for definition */
        if (M.bass_env > 0.001f) {
            M.bass_ph += TAU * M.bass_freq / sr;
            float v = (sinf(M.bass_ph) + 0.25f * sinf(M.bass_ph * 2.0f)) * M.bass_env * 0.42f;
            l += v; r += v;
            M.bass_env *= bass_d;
        }
        /* pad: six detuned sines, slow swell */
        M.pad_amp += (M.pad_target - M.pad_amp) * 0.00004f;
        if (M.pad_amp > 0.0005f) {
            float pv_l = 0, pv_r = 0;
            for (int i = 0; i < 6; i++) {
                if (M.pad_freq[i] <= 0) continue;
                M.pad_ph[i] += TAU * M.pad_freq[i] / sr;
                if (M.pad_ph[i] > TAU) M.pad_ph[i] -= TAU;
                float s = sinf(M.pad_ph[i]);
                if (i & 1) pv_r += s; else pv_l += s;
            }
            l += pv_l * M.pad_amp * 0.33f;
            r += pv_r * M.pad_amp * 0.33f;
        }
        /* arp plucks */
        for (int i = 0; i < MAX_ARP; i++) {
            if (!M.arp[i].active) continue;
            if (M.arp[i].env < 0.0008f) { M.arp[i].active = 0; continue; }
            M.arp[i].ph += TAU * M.arp[i].freq / sr;
            float s = sinf(M.arp[i].ph) + 0.35f * sinf(M.arp[i].ph * 2.0f);
            float v = s * M.arp[i].env * 0.20f;
            l += v * (1.0f - M.arp[i].pan);
            r += v * M.arp[i].pan;
            M.arp[i].env *= arp_d;
        }

        /* one-pole lowpass keeps it warm rather than harsh */
        M.lp_l += (l - M.lp_l) * cut;
        M.lp_r += (r - M.lp_r) * cut;
        l = M.lp_l; r = M.lp_r;

        /* tempo-synced ping-pong delay for depth */
        int rd = M.delay_pos - M.delay_len;
        if (rd < 0) rd += DELAY_MAX;
        float dl = M.delay[rd * 2 + 0];
        float dr = M.delay[rd * 2 + 1];
        l += dr * 0.30f;   /* cross-feed = ping-pong */
        r += dl * 0.30f;
        M.delay[M.delay_pos * 2 + 0] = l * 0.55f;
        M.delay[M.delay_pos * 2 + 1] = r * 0.55f;
        M.delay_pos++;
        if (M.delay_pos >= DELAY_MAX) M.delay_pos = 0;

        /* tanh saturator: near-linear at normal levels, gracefully rounds off
         * peaks instead of squashing the whole mix like x/(1+|x|) does */
        l = tanhf(l * M.master);
        r = tanhf(r * M.master);

        out[n * 2 + 0] = (short)(l * 30000.0f);
        out[n * 2 + 1] = (short)(r * 30000.0f);
    }
}
