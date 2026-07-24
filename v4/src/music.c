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
    int seed;
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

/* Two independent generators so a theme change can cross-fade: the old one
 * keeps playing and fades out while the new one fades in underneath. */
static Music g_a, g_b;
static Music *g_cur = &g_a, *g_nxt = &g_b;
static int   g_started = 0;
static int   g_fading = 0;
static float g_fade_pos = 0.0f;   /* 0..1 through the cross-fade */
static float g_fade_rate = 0.0f;  /* per sample */
#define XFADE_SECONDS 1.25f

static float noisef(Music *m) {
    m->noise_rng = m->noise_rng * 1664525u + 1013904223u;
    return ((float)((m->noise_rng >> 9) & 0xFFFF) / 32768.0f) - 1.0f;
}

static float midi_hz(float midi) {
    return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f);
}

/* nth tone of the triad built on a scale degree (0=root,1=third,2=fifth) */
static int chord_tone(Music *m, int degree, int idx) {
    int di = degree + idx * 2;
    int oct = di / 7;
    return m->scale[di % 7] + 12 * oct;
}

static void apply_theme(Music *m, int seed) {
    unsigned int h = hash_u((unsigned int)seed * 2654435761u + 17u);
    m->seed = seed;
    /* keep the tonic in a comfortable register */
    static const int ROOTS[6] = {45, 47, 48, 50, 52, 53}; /* A2..F3 */
    m->root = ROOTS[hpick(h, 6)];
    m->scale = SCALES[hpick(hash_u(h + 1), 4)];
    m->prog = PROGS[hpick(hash_u(h + 2), 6)];
    m->bpm = 88.0f + (float)hpick(hash_u(h + 3), 6) * 7.0f; /* 88..123 */
    m->groove = hpick(hash_u(h + 4), 3);
    m->arp_mode = hpick(hash_u(h + 5), 4);
    m->samples_per_step = (double)m->sr * 60.0 / ((double)m->bpm * 4.0);
    /* delay = a dotted 8th, the classic "big space" tempo sync */
    m->delay_len = (int)(m->samples_per_step * 6.0);
    if (m->delay_len > DELAY_MAX - 4) m->delay_len = DELAY_MAX - 4;
    if (m->delay_len < 8) m->delay_len = 8;
}

static void do_step(Music *m);

/* Bring one generator up on a theme, ready to play from its own downbeat. */
static void spin_up(Music *m, int seed, int sr, float intensity) {
    memset(m, 0, sizeof *m);
    m->sr = sr > 0 ? sr : 44100;
    m->noise_rng = 0x1234567u;
    m->master = 1.15f;
    m->intensity = intensity;
    apply_theme(m, seed);
    m->started = 1;
    /* Fire step 0 immediately. The transport only calls do_step *after* a
     * full step has elapsed, so without this the downbeat — and with it the
     * pad's chord — would not land until the second bar, leaving a new theme
     * thin and quiet for seconds. Very audible when cross-fading. */
    do_step(m);
}

void music_init(int seed, int sample_rate) {
    g_cur = &g_a; g_nxt = &g_b;
    spin_up(g_cur, seed, sample_rate, 0.0f);
    memset(g_nxt, 0, sizeof *g_nxt);
    g_fading = 0;
    g_fade_pos = 0.0f;
    g_fade_rate = 1.0f / (XFADE_SECONDS * (float)g_cur->sr);
    g_started = 1;
}

/* Cross-fade into a new theme: the incoming generator starts on its own
 * downbeat while the outgoing one plays out underneath it. */
void music_set_theme(int seed) {
    if (!g_started) { music_init(seed, 44100); return; }
    /* already heading there (or already there) — nothing to do */
    if (g_fading && g_nxt->seed == seed) return;
    if (!g_fading && g_cur->seed == seed) return;

    /* A change arriving mid-fade: land the one in flight first, so we never
     * reconfigure a generator the audio thread is currently reading. */
    if (g_fading) {
        Music *t = g_cur; g_cur = g_nxt; g_nxt = t;
        g_fading = 0;
    }
    if (g_cur->seed == seed) return;

    spin_up(g_nxt, seed, g_cur->sr, g_cur->intensity);
    g_fade_pos = 0.0f;
    g_fade_rate = 1.0f / (XFADE_SECONDS * (float)g_cur->sr);
    g_fading = 1;   /* set last: the audio thread only reads g_nxt once this is up */
}

void music_set_intensity(float v) {
    if (v < 0) v = 0; else if (v > 1) v = 1;
    g_a.intensity = v;
    g_b.intensity = v;
}

/* Human-readable summary of the theme (for tooling / debugging). During a
 * cross-fade this reports the incoming theme, i.e. the one you are moving to. */
void music_describe(char *buf, int n) {
    static const char *NOTES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    static const char *SCALE_NAMES[4] = {"minor", "dorian", "major", "lydian"};
    Music *m = g_fading ? g_nxt : g_cur;
    if (!m->scale) { snprintf(buf, (size_t)n, "(no theme)"); return; }
    int si = 0;
    for (int i = 0; i < 4; i++) if (m->scale == SCALES[i]) si = i;
    const char *rn = NOTES[((m->root % 12) + 12) % 12];
    snprintf(buf, (size_t)n,
             "key=%s %s  prog=[%d %d %d %d]  bpm=%.0f  groove=%d  arp=%d",
             rn, SCALE_NAMES[si], m->prog[0], m->prog[1], m->prog[2], m->prog[3],
             (double)m->bpm, m->groove, m->arp_mode);
}

/* ---- sequencer: fire the events for one 16th step ---------------- */
static void do_step(Music *m) {
    long s = m->step;
    int step_in_bar = (int)(s & 15);
    int bar = (int)(s >> 4);

    if (step_in_bar == 0) {
        /* advance the chord once per bar */
        int degree = m->prog[bar & 3];
        for (int i = 0; i < 3; i++) {
            int semi = chord_tone(m, degree, i);
            /* two detuned oscillators per chord tone = width */
            m->pad_freq[i * 2 + 0] = midi_hz((float)(m->root + 12 + semi));
            m->pad_freq[i * 2 + 1] = midi_hz((float)(m->root + 12 + semi) + 0.09f);
        }
        m->pad_target = 0.16f + 0.05f * m->intensity;
    }

    unsigned int bh = hash2((unsigned int)m->seed, (unsigned int)bar);
    int degree = m->prog[bar & 3];

    /* --- drums: steady backbone, seeded fills --- */
    int kick = 0, snare = 0;
    if (m->groove == 0) kick = (step_in_bar == 0 || step_in_bar == 6 || step_in_bar == 8);
    else if (m->groove == 1) kick = (step_in_bar == 0 || step_in_bar == 8 || step_in_bar == 11);
    else kick = (step_in_bar == 0 || step_in_bar == 4 || step_in_bar == 8 || step_in_bar == 12);
    snare = (step_in_bar == 4 || step_in_bar == 12);
    /* occasional extra kick, chosen per bar so it stays coherent */
    if (step_in_bar == 14 && (hash2(bh, 7u) & 3) == 0) kick = 1;

    if (kick) { m->kick_env = 1.0f; m->kick_ph = 0; m->kick_pitch = 1.0f; }
    if (snare) m->snare_env = 1.0f;
    /* hats: 8ths normally, 16ths when the heat is on */
    int hat = (step_in_bar % 2 == 0) || (m->intensity > 0.45f);
    if (hat) m->hat_env = (step_in_bar % 4 == 0) ? 0.55f : 0.32f;

    /* --- bass: roots and fifths locked to the chord --- */
    int bass_hit = (step_in_bar == 0 || step_in_bar == 3 || step_in_bar == 6 ||
                    step_in_bar == 8 || step_in_bar == 11);
    if (m->groove == 2) bass_hit = (step_in_bar % 4 == 0) || step_in_bar == 14;
    if (bass_hit) {
        int semi = chord_tone(m, degree, 0);
        if (step_in_bar == 6 || step_in_bar == 11) semi = chord_tone(m, degree, 2); /* fifth */
        m->bass_freq = midi_hz((float)(m->root - 12 + semi));
        m->bass_env = 1.0f;
    }

    /* --- arp / lead: chord tones only, so it is always consonant --- */
    int play_arp = 0;
    switch (m->arp_mode) {
    case 0: play_arp = (step_in_bar % 2) == 0; break;          /* 8ths     */
    case 1: play_arp = 1; break;                                /* 16ths    */
    case 2: play_arp = (step_in_bar % 4) != 3; break;           /* gallop   */
    default: play_arp = (step_in_bar % 2) == 1; break;          /* offbeat  */
    }
    if (play_arp) {
        unsigned int nh = hash2(bh, (unsigned int)(step_in_bar + 31));
        int idx = (int)(nh % 3u);                 /* which chord tone */
        int oct = ((nh >> 5) & 3u) == 0 ? 12 : 0; /* occasional octave lift */
        int semi = chord_tone(m, degree, idx);
        float f = midi_hz((float)(m->root + 24 + semi + oct));
        for (int i = 0; i < MAX_ARP; i++) {
            if (!m->arp[i].active || m->arp[i].env < 0.08f) {
                m->arp[i].active = 1;
                m->arp[i].ph = 0;
                m->arp[i].env = 0.5f + 0.3f * ((nh >> 9) & 1u);
                m->arp[i].freq = f;
                /* alternate the stereo field for a wide, moving lead */
                m->arp[i].pan = ((step_in_bar & 1) ? 0.72f : 0.28f);
                break;
            }
        }
    }
}

/* ---- render interleaved stereo 16-bit ---------------------------- */
/* Render one generator to interleaved stereo float. */
static void render_block(Music *m, float *out, int frames) {

    const float sr = (float)m->sr;
    /* decay coefficients (per sample) */
    float kick_d = expf(-6.0f / sr * 60.0f);
    float snare_d = expf(-9.0f / sr * 60.0f);
    float hat_d = expf(-30.0f / sr * 60.0f);
    float bass_d = expf(-3.2f / sr * 60.0f);
    float arp_d = expf(-4.5f / sr * 60.0f);
    /* filter cutoff opens as intensity rises */
    float cut = 0.16f + 0.30f * m->intensity;

    for (int n = 0; n < frames; n++) {
        /* advance the sequencer */
        m->step_acc += 1.0;
        if (m->step_acc >= m->samples_per_step) {
            m->step_acc -= m->samples_per_step;
            m->step++;
            do_step(m);
        }

        float l = 0, r = 0;

        /* kick: sine with a fast pitch drop */
        if (m->kick_env > 0.001f) {
            m->kick_pitch *= 0.9995f;
            float f = 48.0f + 90.0f * m->kick_pitch;
            m->kick_ph += TAU * f / sr;
            float v = sinf(m->kick_ph) * m->kick_env * 0.85f;
            l += v; r += v;
            m->kick_env *= kick_d;
        }
        /* snare: noise + body tone */
        if (m->snare_env > 0.001f) {
            m->snare_tone_ph += TAU * 190.0f / sr;
            float v = (noisef(m) * 0.7f + sinf(m->snare_tone_ph) * 0.3f) * m->snare_env * 0.32f;
            l += v; r += v;
            m->snare_env *= snare_d;
        }
        /* hat: bright noise, panned slightly right for groove */
        if (m->hat_env > 0.001f) {
            float v = noisef(m) * m->hat_env * 0.12f;
            l += v * 0.8f; r += v;
            m->hat_env *= hat_d;
        }
        /* bass: sine + a touch of its octave for definition */
        if (m->bass_env > 0.001f) {
            m->bass_ph += TAU * m->bass_freq / sr;
            float v = (sinf(m->bass_ph) + 0.25f * sinf(m->bass_ph * 2.0f)) * m->bass_env * 0.42f;
            l += v; r += v;
            m->bass_env *= bass_d;
        }
        /* pad: six detuned sines, slow swell */
        m->pad_amp += (m->pad_target - m->pad_amp) * 0.00004f;
        if (m->pad_amp > 0.0005f) {
            float pv_l = 0, pv_r = 0;
            for (int i = 0; i < 6; i++) {
                if (m->pad_freq[i] <= 0) continue;
                m->pad_ph[i] += TAU * m->pad_freq[i] / sr;
                if (m->pad_ph[i] > TAU) m->pad_ph[i] -= TAU;
                float s = sinf(m->pad_ph[i]);
                if (i & 1) pv_r += s; else pv_l += s;
            }
            l += pv_l * m->pad_amp * 0.33f;
            r += pv_r * m->pad_amp * 0.33f;
        }
        /* arp plucks */
        for (int i = 0; i < MAX_ARP; i++) {
            if (!m->arp[i].active) continue;
            if (m->arp[i].env < 0.0008f) { m->arp[i].active = 0; continue; }
            m->arp[i].ph += TAU * m->arp[i].freq / sr;
            float s = sinf(m->arp[i].ph) + 0.35f * sinf(m->arp[i].ph * 2.0f);
            float v = s * m->arp[i].env * 0.20f;
            l += v * (1.0f - m->arp[i].pan);
            r += v * m->arp[i].pan;
            m->arp[i].env *= arp_d;
        }

        /* one-pole lowpass keeps it warm rather than harsh */
        m->lp_l += (l - m->lp_l) * cut;
        m->lp_r += (r - m->lp_r) * cut;
        l = m->lp_l; r = m->lp_r;

        /* tempo-synced ping-pong delay for depth */
        int rd = m->delay_pos - m->delay_len;
        if (rd < 0) rd += DELAY_MAX;
        float dl = m->delay[rd * 2 + 0];
        float dr = m->delay[rd * 2 + 1];
        l += dr * 0.30f;   /* cross-feed = ping-pong */
        r += dl * 0.30f;
        m->delay[m->delay_pos * 2 + 0] = l * 0.55f;
        m->delay[m->delay_pos * 2 + 1] = r * 0.55f;
        m->delay_pos++;
        if (m->delay_pos >= DELAY_MAX) m->delay_pos = 0;

        /* tanh saturator: near-linear at normal levels, gracefully rounds off
         * peaks instead of squashing the whole mix like x/(1+|x|) does */
        l = tanhf(l * m->master);
        r = tanhf(r * m->master);

        out[n * 2 + 0] = l;
        out[n * 2 + 1] = r;
    }
}

/* ---- public renderer: mixes the two generators ------------------- *
 * Equal-power (cos/sin) gains keep perceived loudness constant across the
 * transition, instead of the dip you get from a plain linear fade. */
#define XF_CHUNK 512
static float g_bufA[XF_CHUNK * 2];
static float g_bufB[XF_CHUNK * 2];

void music_render(short *out, int frames) {
    if (!g_started) { memset(out, 0, sizeof(short) * 2 * (size_t)frames); return; }

    while (frames > 0) {
        int n = frames > XF_CHUNK ? XF_CHUNK : frames;
        render_block(g_cur, g_bufA, n);

        if (g_fading) {
            render_block(g_nxt, g_bufB, n);
            for (int i = 0; i < n; i++) {
                float t = g_fade_pos;
                if (t > 1.0f) t = 1.0f;
                float ga = cosf(t * 1.5707963f);   /* outgoing */
                float gb = sinf(t * 1.5707963f);   /* incoming */
                float l = g_bufA[i * 2 + 0] * ga + g_bufB[i * 2 + 0] * gb;
                float r = g_bufA[i * 2 + 1] * ga + g_bufB[i * 2 + 1] * gb;
                if (l >  1.0f) l =  1.0f; if (l < -1.0f) l = -1.0f;
                if (r >  1.0f) r =  1.0f; if (r < -1.0f) r = -1.0f;
                out[i * 2 + 0] = (short)(l * 30000.0f);
                out[i * 2 + 1] = (short)(r * 30000.0f);
                g_fade_pos += g_fade_rate;
            }
            if (g_fade_pos >= 1.0f) {
                Music *t = g_cur; g_cur = g_nxt; g_nxt = t;
                g_fading = 0;
                g_fade_pos = 0.0f;
            }
        } else {
            for (int i = 0; i < n; i++) {
                out[i * 2 + 0] = (short)(g_bufA[i * 2 + 0] * 30000.0f);
                out[i * 2 + 1] = (short)(g_bufA[i * 2 + 1] * 30000.0f);
            }
        }
        out += n * 2;
        frames -= n;
    }
}
