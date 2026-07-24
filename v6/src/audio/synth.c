#include "synth.h"
#include "save/save.h"
#include "art/artkit.h"

#include "raylib.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#define SR       22050
#define MUSIC_SR 22050
#define MBUF     1024

static bool  g_ready;
static Sound g_sfx[SFX_COUNT];
static AudioStream g_stream;
static short g_mix[MBUF];

/* ==========================================================================
 * SFX: every one is a short recipe, not a file
 * ========================================================================== */
static unsigned g_rng = 0x1a2b3c4du;
static float frand(void)
{
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return (float)(g_rng & 0xffff) / 32767.5f - 1.0f;
}

static Sound make(float seconds, void (*fill)(short *, int))
{
    int n = (int)(seconds * SR);
    short *d = calloc((size_t)n, sizeof(short));
    fill(d, n);
    Wave w = { .frameCount = (unsigned)n, .sampleRate = SR, .sampleSize = 16,
               .channels = 1, .data = d };
    Sound s = LoadSoundFromWave(w);
    free(d);
    return s;
}

static float env(int i, int n, float attack, float decay)
{
    float t = (float)i / (float)n;
    if (t < attack) return t / attack;
    float u = (t - attack) / (1.0f - attack);
    return expf(-u * decay);
}

/* a wooden UI tick: filtered noise burst */
static void f_tick(short *d, int n)
{
    float lp = 0;
    for (int i = 0; i < n; i++) {
        lp += (frand() - lp) * 0.35f;
        d[i] = (short)(lp * 7000.0f * env(i, n, 0.004f, 9.0f));
    }
}

static void f_click(short *d, int n)
{
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float f = 660.0f - 180.0f * (float)i / n;
        d[i] = (short)(sinf(2 * PI * f * t) * 7000.0f * env(i, n, 0.005f, 7.0f));
    }
}

/* a page turn: swept noise, the sound of paper being paper */
static void f_page(short *d, int n)
{
    float lp = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        lp += (frand() - lp) * (0.10f + 0.5f * t);
        d[i] = (short)(lp * 8200.0f * sinf(t * PI) * (0.5f + 0.5f * t));
    }
}

/* the ink scritch under dialogue: one letter's worth of nib on paper */
static void f_blip(short *d, int n)
{
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float f = 420.0f;
        float s = sinf(2 * PI * f * t) * 0.6f + frand() * 0.25f;
        d[i] = (short)(s * 5200.0f * env(i, n, 0.02f, 12.0f));
    }
}

static void two_tone(short *d, int n, float f1, float f2, float amp, float decay)
{
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float e = env(i, n, 0.01f, decay);
        float s = sinf(2 * PI * f1 * t) * 0.6f + sinf(2 * PI * f2 * t) * 0.4f;
        d[i] = (short)(s * amp * e);
    }
}

static void f_chime(short *d, int n) { two_tone(d, n, 880.0f, 1320.0f, 6200.0f, 5.0f); }
static void f_clunk(short *d, int n) { two_tone(d, n, 148.0f,  96.0f, 6800.0f, 9.0f); }
static void f_sparkle(short *d, int n) { two_tone(d, n, 1760.0f, 2640.0f, 4200.0f, 6.0f); }
static void f_door(short *d, int n) { two_tone(d, n, 210.0f, 140.0f, 5200.0f, 6.0f); }

static void f_step(short *d, int n)
{
    float lp = 0;
    for (int i = 0; i < n; i++) {
        lp += (frand() - lp) * 0.18f;
        d[i] = (short)(lp * 4200.0f * env(i, n, 0.006f, 14.0f));
    }
}

/* solved: a rising three-note phrase, the "well done" of the whole game */
static void f_solve(short *d, int n)
{
    static const float note[3] = { 523.25f, 659.25f, 783.99f };
    int seg = n / 3;
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < seg; i++) {
            float t = (float)i / SR;
            float e = env(i, seg, 0.02f, 4.0f);
            float s = sinf(2 * PI * note[k] * t) * 0.7f +
                      sinf(4 * PI * note[k] * t) * 0.2f;
            int at = k * seg + i;
            if (at < n) d[at] = (short)(s * 6800.0f * e);
        }
    }
}

/* Deduce!: a four-note fanfare with a little swagger */
static void f_deduce(short *d, int n)
{
    static const float note[4] = { 392.0f, 523.25f, 659.25f, 1046.5f };
    int seg = n / 4;
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < seg; i++) {
            float t = (float)i / SR;
            float e = env(i, seg, 0.015f, 3.2f);
            float s = sinf(2 * PI * note[k] * t) +
                      0.35f * sinf(2 * PI * note[k] * 2.0f * t);
            int at = k * seg + i;
            if (at < n) d[at] = (short)(s * 5200.0f * e);
        }
}

static void f_feather(short *d, int n)
{
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float f = 1200.0f + 900.0f * (float)i / n;
        d[i] = (short)(sinf(2 * PI * f * t) * 4400.0f * env(i, n, 0.01f, 7.0f));
    }
}

/* the Prism mending: everything the town has, at once */
static void f_mend(short *d, int n)
{
    static const float chord[5] = { 261.63f, 329.63f, 392.0f, 523.25f, 659.25f };
    for (int i = 0; i < n; i++) {
        float t = (float)i / SR;
        float u = (float)i / (float)n;
        float e = (u < 0.35f) ? (u / 0.35f) : expf(-(u - 0.35f) * 3.4f);
        float s = 0;
        for (int k = 0; k < 5; k++) {
            float wob = 1.0f + 0.004f * sinf(2 * PI * (1.2f + k * 0.4f) * t);
            s += sinf(2 * PI * chord[k] * wob * t) * (1.0f - k * 0.12f);
        }
        s += frand() * 0.06f * (1.0f - u);
        d[i] = (short)(s * 2400.0f * e);
    }
}

/* ==========================================================================
 * music: three voices, one mood table per district (§8.4)
 * ========================================================================== */
typedef struct {
    int   root;          /* semitone offset from C3 */
    int   bpm;
    int   scale[7];      /* degrees in semitones */
    int   nscale;
    float warmth;        /* 0 airy .. 1 close */
    int   pattern[8];    /* lead degrees, -1 = rest */
} Mood;

static const Mood g_mood[MOOD_COUNT] = {
    /* SILENT   */ { 0, 60, {0}, 0, 0.0f, {-1,-1,-1,-1,-1,-1,-1,-1} },
    /* TOWN     */ { 0,  76, {0,2,4,5,7,9,11}, 7, 0.55f, { 0, 4, 2, 4, 5, 4, 2, 0 } },
    /* HARBOR   */ { -2, 68, {0,2,3,5,7,8,10}, 7, 0.35f, { 0, 2, 4, 2, 6, 4, 2,-1 } },
    /* MARKET   */ { 5,  92, {0,2,4,5,7,9,11}, 7, 0.70f, { 0, 2, 4, 5, 4, 2, 0, 4 } },
    /* QUARTER  */ { -4, 64, {0,2,3,5,7,8,10}, 7, 0.45f, { 0, 3, 2, 0,-1, 3, 5, 3 } },
    /* GARDEN   */ { 2,  72, {0,2,4,7,9,11,12}, 7, 0.62f, { 0, 2, 4, 6, 4, 2,-1, 0 } },
    /* TOWER    */ { -5, 60, {0,2,3,5,7,8,10}, 7, 0.30f, { 0,-1, 4, 3, 2,-1, 5, 4 } },
    /* PUZZLE   */ { 3,  84, {0,2,4,5,7,9,11}, 7, 0.50f, { 0, 4, 2, 5, 4, 2, 6, 4 } },
    /* BOARD    */ { -1, 70, {0,2,3,5,7,9,10}, 7, 0.40f, { 0, 2,-1, 4, 2,-1, 5, 2 } },
    /* SAD      */ { -7, 54, {0,2,3,5,7,8,10}, 7, 0.25f, { 0,-1,-1, 3,-1,-1, 2,-1 } },
    /* FESTIVAL */ { 0, 100, {0,2,4,5,7,9,11}, 7, 0.95f, { 0, 4, 7, 4, 5, 7, 4, 2 } },
};

static int   g_cur_mood = MOOD_SILENT;
static int   g_next_mood = MOOD_SILENT;
static bool  g_duck;
static float g_duck_gain = 1.0f;
static int   g_step;          /* 16th-note step   */
static long  g_step_len;
static long  g_step_pos;
static int   g_stage_cache;   /* palette stage: how bright the town sounds */

static float semi(int s) { return 130.81f * powf(2.0f, (float)s / 12.0f); }

static float voice_saw(float ph) { return 2.0f * (ph - floorf(ph + 0.5f)); }
static float voice_tri(float ph) { return 2.0f * fabsf(voice_saw(ph)) - 1.0f; }

/* three sustained oscillators, each with its own envelope */
typedef struct { float ph, inc, amp, gain, decay; int kind; } Voice;
static Voice g_v[4];

static void note_on(int slot, float freq, float gain, float decay, int kind)
{
    g_v[slot].inc = freq / (float)MUSIC_SR;
    g_v[slot].amp = 1.0f;
    g_v[slot].gain = gain;
    g_v[slot].decay = decay;
    g_v[slot].kind = kind;
}

static void sequencer_step(void)
{
    const Mood *m = &g_mood[g_cur_mood];
    if (g_cur_mood == MOOD_SILENT || m->nscale == 0) return;

    /* the town gains a voice and a brightness step per restored hue */
    int stage = g_stage_cache;
    int chord_root[4] = { 0, 5, 3, 4 };
    int bar = (g_step / 16) & 3;
    int deg = chord_root[bar];

    if (g_step % 16 == 0 || g_step % 16 == 8) {
        float f = semi(m->root + m->scale[deg % m->nscale] - 12);
        note_on(0, f, 0.30f + 0.06f * m->warmth, 1.6f, 0);          /* bass */
    }
    if (g_step % 8 == 0 && stage >= 1) {
        float f = semi(m->root + m->scale[(deg + 2) % m->nscale]);
        note_on(1, f, 0.15f + 0.05f * stage / 6.0f, 0.9f, 1);       /* pad  */
    }
    if (stage >= 2 && (g_step % 2) == 0) {
        int idx = (g_step / 2) % 8;
        int d = m->pattern[idx];
        if (d >= 0) {
            float f = semi(m->root + m->scale[d % m->nscale] + 12);
            note_on(2, f, 0.11f + 0.03f * stage / 6.0f, 3.2f, 2);   /* lead */
        }
    }
    if (stage >= 4 && (g_step % 16) == 12) {
        float f = semi(m->root + m->scale[(deg + 4) % m->nscale] + 24);
        note_on(3, f, 0.07f, 4.0f, 3);                              /* bells */
    }

    g_step++;
    if (g_step % 16 == 0 && g_next_mood != g_cur_mood) {
        g_cur_mood = g_next_mood;                 /* swap on the bar, never mid-phrase */
        const Mood *nm = &g_mood[g_cur_mood];
        g_step_len = (nm->bpm > 0) ? (long)((60.0 / nm->bpm / 4.0) * MUSIC_SR) : 1;
        g_step = 0;
    }
}

static void fill_music(short *out, int n)
{
    float master = settings()->music_vol / 10.0f;
    for (int i = 0; i < n; i++) {
        if (g_step_pos <= 0) { sequencer_step(); g_step_pos = g_step_len; }
        g_step_pos--;

        float s = 0;
        for (int v = 0; v < 4; v++) {
            if (g_v[v].amp <= 0.0005f) continue;
            g_v[v].ph += g_v[v].inc;
            if (g_v[v].ph > 1.0f) g_v[v].ph -= 1.0f;
            float w;
            switch (g_v[v].kind) {
            case 0: w = voice_tri(g_v[v].ph); break;
            case 1: w = sinf(2 * PI * g_v[v].ph) * 0.7f +
                        sinf(4 * PI * g_v[v].ph) * 0.2f; break;
            case 2: w = sinf(2 * PI * g_v[v].ph); break;
            default: w = sinf(2 * PI * g_v[v].ph) * 0.5f +
                         sinf(6 * PI * g_v[v].ph) * 0.2f; break;
            }
            s += w * g_v[v].amp * g_v[v].gain;
            g_v[v].amp -= g_v[v].amp * g_v[v].decay / (float)MUSIC_SR;
        }

        /* ducking is a slew, not a switch — music never lurches under a line */
        float target = g_duck ? 0.38f : 1.0f;
        g_duck_gain += (target - g_duck_gain) * 0.00025f;

        float o = s * master * g_duck_gain * 0.85f;
        if (o > 1.0f) o = 1.0f;
        if (o < -1.0f) o = -1.0f;
        out[i] = (short)(o * 26000.0f);
    }
}

/* ========================================================================== */
void audio_init(void)
{
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;

    g_sfx[SFX_TICK]    = make(0.05f, f_tick);
    g_sfx[SFX_CLICK]   = make(0.10f, f_click);
    g_sfx[SFX_PAGE]    = make(0.32f, f_page);
    g_sfx[SFX_BLIP]    = make(0.045f, f_blip);
    g_sfx[SFX_CHIME]   = make(0.30f, f_chime);
    g_sfx[SFX_CLUNK]   = make(0.16f, f_clunk);
    g_sfx[SFX_SOLVE]   = make(0.60f, f_solve);
    g_sfx[SFX_DEDUCE]  = make(0.85f, f_deduce);
    g_sfx[SFX_FEATHER] = make(0.28f, f_feather);
    g_sfx[SFX_SPARKLE] = make(0.30f, f_sparkle);
    g_sfx[SFX_DOOR]    = make(0.34f, f_door);
    g_sfx[SFX_STEP]    = make(0.09f, f_step);
    g_sfx[SFX_MEND]    = make(3.20f, f_mend);

    SetAudioStreamBufferSizeDefault(MBUF);
    g_stream = LoadAudioStream(MUSIC_SR, 16, 1);
    PlayAudioStream(g_stream);
    g_step_len = (long)((60.0 / 76.0 / 4.0) * MUSIC_SR);
    g_ready = true;
    audio_apply_volumes();
}

void audio_shutdown(void)
{
    if (!g_ready) { CloseAudioDevice(); return; }
    for (int i = 0; i < SFX_COUNT; i++) UnloadSound(g_sfx[i]);
    UnloadAudioStream(g_stream);
    CloseAudioDevice();
    g_ready = false;
}

void audio_apply_volumes(void)
{
    if (!g_ready) return;
    float sv = settings()->sfx_vol / 10.0f;
    for (int i = 0; i < SFX_COUNT; i++) SetSoundVolume(g_sfx[i], sv);
    SetAudioStreamVolume(g_stream, 1.0f);
}

void audio_update(float dt)
{
    (void)dt;
    if (!g_ready) return;
    g_stage_cache = palette_stage();
    while (IsAudioStreamProcessed(g_stream)) {
        fill_music(g_mix, MBUF);
        UpdateAudioStream(g_stream, g_mix, MBUF);
    }
}

void sfx_play(int id)
{
    if (!g_ready || id < 0 || id >= SFX_COUNT) return;
    if (settings()->sfx_vol == 0) return;
    SetSoundPitch(g_sfx[id], 1.0f);
    PlaySound(g_sfx[id]);
}

void sfx_blip(int npc_id)
{
    if (!g_ready || settings()->sfx_vol == 0) return;
    /* every speaker gets their own nib: low for Otto, high for Tansy */
    static const float pitch[18] = {
        1.00f, 0.86f, 0.72f, 1.42f, 0.80f, 1.14f, 1.06f, 1.20f, 0.90f,
        1.02f, 0.96f, 0.92f, 1.10f, 0.76f, 1.26f, 0.84f, 1.16f, 1.60f
    };
    float p = (npc_id >= 0 && npc_id < 18) ? pitch[npc_id] : 1.0f;
    SetSoundPitch(g_sfx[SFX_BLIP], p);
    PlaySound(g_sfx[SFX_BLIP]);
}

void music_mood(int mood)
{
    if (mood < 0 || mood >= MOOD_COUNT) return;
    g_next_mood = mood;
    if (g_cur_mood == MOOD_SILENT) {              /* start immediately from nothing */
        g_cur_mood = mood;
        g_step = 0;
        g_step_len = (long)((60.0 / g_mood[mood].bpm / 4.0) * MUSIC_SR);
    }
}

int  music_current(void) { return g_cur_mood; }
void music_duck(bool ducked) { g_duck = ducked; }
