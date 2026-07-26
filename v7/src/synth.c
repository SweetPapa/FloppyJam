/* synth.c — §9. Everything you hear, made here.
 *
 * Two halves. The impacts are rendered into short PCM buffers once at boot and
 * fired with a pitch that tracks the ball; the crowd and the music share one
 * generative stream that is filled a block at a time while the game runs.
 *
 * The music is not a loop with a volume envelope on it — the layers are
 * genuinely generated, so heat 3 and heat 12 are different arrangements of the
 * same key rather than the same recording played louder.
 */
#include "synth.h"
#include "core.h"
#include "raylib.h"

#include <string.h>

#define SR        22050
#define BLOCK     1024
#define MAXSFXLEN (SR * 2)

static int   g_ready = 0;
static Sound g_sfx[VB_SFX_COUNT];
static AudioStream g_stream;
static int   g_stream_ok = 0;

static float g_master = 0.85f, g_music = 0.55f, g_sfxv = 0.9f, g_crowdv = 0.6f;

/* ---- the music engine's state ----------------------------------------- */
static struct {
    int   layers;
    int   ambient;
    int   lift;              /* half-step up at match point (§9)           */
    float heat01, gasp;
    double step;             /* 16ths elapsed, fractional                  */
    double ph[6];            /* oscillator phases                          */
    float lp, lp2;           /* one-pole filters for the pad and the crowd */
    unsigned rng;
    float crowd_env;
    float target_heat;
} M;

/* ---- helpers ----------------------------------------------------------- */

static float frand(unsigned *s) {
    *s = (*s * 1664525u) + 1013904223u;
    return (float)((*s >> 9) & 0x7FFFFF) / 8388608.0f * 2.0f - 1.0f;
}

static float saw(double p)  { double f = p - (double)(long)p; return (float)(f * 2.0 - 1.0); }
static float sq(double p, float duty) {
    double f = p - (double)(long)p;
    return f < duty ? 1.0f : -1.0f;
}
static float tri(double p) {
    double f = p - (double)(long)p;
    return (float)(f < 0.5 ? (f * 4.0 - 1.0) : (3.0 - f * 4.0));
}
static float sine(double p) { return sinf((float)(p * VB_TAU)); }

/* A minor pentatonic, which is the whole reason a generative line like this
 * can never land on a wrong note. Semitones from the root. */
static const int SCALE[5] = { 0, 3, 5, 7, 10 };
static float note_hz(int root, int degree, int octave) {
    int semi = root + SCALE[((degree % 5) + 5) % 5] + 12 * octave;
    return 55.0f * powf(2.0f, (float)semi / 12.0f);
}

/* ---- one-shot synthesis ------------------------------------------------ */

typedef struct { float *s; int n; } Buf;

static void env_mul(Buf b, float attack, float decay, float curve) {
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / (float)SR;
        float a = attack > 0.0001f ? vb_clampf(t / attack, 0, 1) : 1.0f;
        float d = expf(-t / vb_maxf(decay, 0.001f));
        b.s[i] *= a * powf(d, curve);
    }
}

static void add_noise(Buf b, float amp, unsigned seed) {
    unsigned s = seed;
    for (int i = 0; i < b.n; i++) b.s[i] += frand(&s) * amp;
}

/* a cheap resonant lowpass sweep — this is what turns noise into a material */
static void lowpass(Buf b, float f0, float f1, float q) {
    float lp = 0, bp = 0;
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / (float)b.n;
        float f = vb_lerpf(f0, f1, t);
        float c = vb_clampf(2.0f * sinf(VB_PI * f / (float)SR), 0.0f, 1.0f);
        float in = b.s[i];
        float hp = in - lp - q * bp;
        bp += c * hp;
        lp += c * bp;
        b.s[i] = lp;
    }
}

static void add_tone(Buf b, float f0, float f1, float amp, int shape) {
    double p = 0;
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / (float)b.n;
        float f = vb_lerpf(f0, f1, t);
        p += (double)f / (double)SR;
        float v = shape == 0 ? sine(p) : shape == 1 ? tri(p)
                : shape == 2 ? saw(p) : sq(p, 0.5f);
        b.s[i] += v * amp;
    }
}

static Sound bake(float *tmp, int n) {
    /* normalise politely: leave headroom so stacked hits do not clip */
    float peak = 0.0001f;
    for (int i = 0; i < n; i++) { float a = vb_absf(tmp[i]); if (a > peak) peak = a; }
    float k = 0.82f / peak;
    static short pcm[MAXSFXLEN];
    for (int i = 0; i < n; i++)
        pcm[i] = (short)vb_clampf(tmp[i] * k * 32000.0f, -32000.0f, 32000.0f);
    Wave w;
    w.frameCount = (unsigned)n;
    w.sampleRate = SR;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = pcm;
    return LoadSoundFromWave(w);
}

static void make_sfx(void) {
    static float t[MAXSFXLEN];
    Buf b;

    /* BUNT — felt. A dead thud with the top taken off. */
    b.n = SR / 8; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 150, 78, 0.9f, 0);
    add_noise(b, 0.25f, 11);
    lowpass(b, 900, 240, 1.1f);
    env_mul(b, 0.001f, 0.035f, 1.0f);
    g_sfx[VB_SFX_BUNT] = bake(t, b.n);

    /* FLICK — struck glass. Bright, short, and it has a pitch. */
    b.n = SR / 5; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 880, 760, 0.55f, 0);
    add_tone(b, 1760, 1500, 0.30f, 0);
    add_tone(b, 2640, 2300, 0.16f, 0);
    add_noise(b, 0.18f, 23);
    lowpass(b, 6000, 1800, 0.6f);
    env_mul(b, 0.0006f, 0.055f, 1.0f);
    g_sfx[VB_SFX_FLICK] = bake(t, b.n);

    /* CHARGED — the same glass, leaned on: lower, longer, with a sweep. */
    b.n = SR / 4; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 620, 430, 0.7f, 0);
    add_tone(b, 1240, 900, 0.32f, 3);
    add_noise(b, 0.22f, 31);
    lowpass(b, 5200, 1100, 0.9f);
    env_mul(b, 0.002f, 0.085f, 1.0f);
    g_sfx[VB_SFX_CHARGED] = bake(t, b.n);

    /* SNAP — whipcrack. It should sound like it is not allowed. */
    b.n = SR / 5; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_noise(b, 1.0f, 47);
    lowpass(b, 9000, 900, 1.6f);
    add_tone(b, 2400, 320, 0.5f, 0);
    env_mul(b, 0.0002f, 0.030f, 1.2f);
    g_sfx[VB_SFX_SNAP] = bake(t, b.n);

    /* CHIP — hollow pop, the sound of something leaving the table. */
    b.n = SR / 6; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 420, 900, 0.8f, 1);
    add_noise(b, 0.1f, 53);
    lowpass(b, 2600, 3400, 1.3f);
    env_mul(b, 0.001f, 0.045f, 1.0f);
    g_sfx[VB_SFX_CHIP] = bake(t, b.n);

    /* LAND */
    b.n = SR / 10; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 300, 180, 0.6f, 0);
    add_noise(b, 0.3f, 59);
    lowpass(b, 2000, 500, 0.8f);
    env_mul(b, 0.0005f, 0.022f, 1.0f);
    g_sfx[VB_SFX_LAND] = bake(t, b.n);

    /* WALL — the bank. Short, woody, slightly rung. */
    b.n = SR / 9; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 520, 470, 0.55f, 0);
    add_tone(b, 1300, 1200, 0.2f, 0);
    add_noise(b, 0.15f, 67);
    lowpass(b, 4200, 1400, 1.0f);
    env_mul(b, 0.0004f, 0.028f, 1.0f);
    g_sfx[VB_SFX_WALL] = bake(t, b.n);

    /* PIN — everything stops. A soft closed sound with a little air. */
    b.n = SR / 7; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 200, 160, 0.7f, 0);
    add_noise(b, 0.18f, 71);
    lowpass(b, 1400, 380, 1.4f);
    env_mul(b, 0.003f, 0.05f, 1.0f);
    g_sfx[VB_SFX_PIN] = bake(t, b.n);

    /* WHIFF — air, and the sound of a lane opening. */
    b.n = SR / 5; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_noise(b, 0.8f, 83);
    lowpass(b, 1200, 5200, 1.7f);
    env_mul(b, 0.010f, 0.055f, 1.0f);
    g_sfx[VB_SFX_WHIFF] = bake(t, b.n);

    /* GOAL — subsonic detonation under a choir-like shimmer. */
    b.n = SR; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 90, 34, 1.0f, 0);
    add_tone(b, 140, 52, 0.55f, 0);
    add_noise(b, 0.5f, 97);
    lowpass(b, 3000, 200, 1.2f);
    {   /* the shimmer: a stack of detuned fifths, slow in, slow out */
        Buf s2; s2.s = t; s2.n = b.n;
        add_tone(s2, 523.25f, 523.25f, 0.10f, 0);
        add_tone(s2, 784.00f, 786.00f, 0.09f, 0);
        add_tone(s2, 1046.5f, 1044.0f, 0.06f, 0);
    }
    env_mul(b, 0.004f, 0.42f, 1.0f);
    g_sfx[VB_SFX_GOAL] = bake(t, b.n);

    /* SCORCHER — the same, bigger, and the glass goes with it. */
    b.n = (SR * 3) / 2; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 76, 26, 1.0f, 0);
    add_tone(b, 120, 40, 0.6f, 0);
    add_noise(b, 0.75f, 101);
    lowpass(b, 5200, 160, 1.4f);
    {
        Buf s2; s2.s = t; s2.n = b.n;
        add_tone(s2, 392.0f, 392.0f, 0.11f, 0);
        add_tone(s2, 587.3f, 590.0f, 0.10f, 0);
        add_tone(s2, 1174.7f, 1170.0f, 0.07f, 0);
    }
    env_mul(b, 0.003f, 0.55f, 1.0f);
    g_sfx[VB_SFX_SCORCHER] = bake(t, b.n);

    /* SAVE — a short rising figure, so a save reads as a save. */
    b.n = SR / 4; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 440, 1320, 0.6f, 1);
    add_noise(b, 0.12f, 107);
    lowpass(b, 2400, 6000, 0.9f);
    env_mul(b, 0.004f, 0.07f, 1.0f);
    g_sfx[VB_SFX_SAVE] = bake(t, b.n);

    /* MERCY — the breath. Nearly nothing, but you feel it. */
    b.n = SR / 6; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 1800, 2600, 0.35f, 0);
    add_noise(b, 0.08f, 113);
    lowpass(b, 3000, 8000, 1.1f);
    env_mul(b, 0.012f, 0.04f, 1.0f);
    g_sfx[VB_SFX_MERCY] = bake(t, b.n);

    /* REF — the anti-stall pulse. Two tones, unmistakably an instruction. */
    b.n = SR / 3; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 1200, 1200, 0.5f, 3);
    add_tone(b, 900, 900, 0.4f, 3);
    lowpass(b, 4000, 2400, 0.7f);
    env_mul(b, 0.004f, 0.10f, 1.0f);
    g_sfx[VB_SFX_REF] = bake(t, b.n);

    /* UI */
    b.n = SR / 14; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 1400, 1400, 0.5f, 1);
    env_mul(b, 0.0008f, 0.018f, 1.0f);
    g_sfx[VB_SFX_UI] = bake(t, b.n);

    b.n = SR / 6; b.s = t; memset(t, 0, sizeof(float) * (size_t)b.n);
    add_tone(b, 300, 620, 0.6f, 1);
    add_tone(b, 600, 1240, 0.3f, 0);
    env_mul(b, 0.002f, 0.06f, 1.0f);
    g_sfx[VB_SFX_UI_BIG] = bake(t, b.n);
}

/* ---- the generative bed ------------------------------------------------ */

static void fill_block(short *out, int n) {
    /* 128 BPM: one 16th is SR*60/(128*4) frames */
    const double per16 = (double)SR * 60.0 / (128.0 * 4.0);
    int root = 9 + M.lift;                  /* A minor, up a half-step at MP */

    for (int i = 0; i < n; i++) {
        M.step += 1.0 / per16;
        int s16 = (int)M.step;
        float v = 0.0f;

        if (!M.ambient && M.layers >= 1) {
            /* pulse bass — the spine, on every 16th */
            int deg = (s16 / 8) % 5;
            float f = note_hz(root, deg, 1);
            M.ph[0] += (double)f / SR;
            float gate = ((s16 % 2) == 0) ? 1.0f : 0.35f;
            v += sq(M.ph[0], 0.35f) * 0.16f * gate;
            /* and a kick on the beat, because this is a stadium */
            double bt = M.step - (double)(s16 & ~3);
            if ((s16 % 4) == 0 && bt < 1.0) {
                float e = expf(-(float)bt * 9.0f);
                M.ph[5] += (double)vb_lerpf(120.0f, 45.0f, (float)bt) / SR;
                v += sine(M.ph[5]) * 0.28f * e;
            }
        }
        if (!M.ambient && M.layers >= 2) {
            /* arps — the motion */
            int deg = (s16 * 3 + (s16 / 16)) % 5;
            float f = note_hz(root, deg, 3);
            M.ph[1] += (double)f / SR;
            float e = 1.0f - (float)(M.step - (double)s16);
            v += saw(M.ph[1]) * 0.085f * e * e;
        }
        if (M.layers >= 3 || M.ambient) {
            /* pads — two detuned saws through a slow filter */
            int deg = (s16 / 32) % 5;
            float f = note_hz(root, deg, 2);
            M.ph[2] += (double)f / SR;
            M.ph[3] += (double)(f * 1.005f) / SR;
            float raw = (saw(M.ph[2]) + saw(M.ph[3])) * 0.5f;
            M.lp += (raw - M.lp) * 0.0016f;
            v += M.lp * (M.ambient ? 0.22f : 0.15f);
        }
        if (!M.ambient && M.layers >= 4) {
            /* the lead only ever arrives at twelve */
            int deg = (s16 / 4 + 2) % 5;
            float f = note_hz(root, deg, 4);
            M.ph[4] += (double)f / SR;
            float e = 1.0f - (float)(M.step - (double)(s16 & ~3)) * 0.25f;
            v += tri(M.ph[4]) * 0.075f * vb_clampf(e, 0, 1);
        }

        v *= g_music;

        /* the crowd: shaped noise that swells with the heat and gasps on a
         * save. Ten thousand people, none of them sampled. */
        float want = M.heat01 * (1.0f - M.gasp * 0.8f);
        M.crowd_env += (want - M.crowd_env) * 0.0006f;
        float nz = frand(&M.rng);
        M.lp2 += (nz - M.lp2) * (0.02f + 0.05f * M.crowd_env);
        v += M.lp2 * M.crowd_env * 0.55f * g_crowdv;

        out[i] = (short)vb_clampf(v * g_master * 20000.0f, -30000.0f, 30000.0f);
    }
}

/* ---- API --------------------------------------------------------------- */

void vb_synth_init(void) {
    if (g_ready) return;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "VOLLEYBAR: no audio device; running silent");
        return;
    }
    memset(&M, 0, sizeof M);
    M.rng = 2463534242u;
    make_sfx();
    SetAudioStreamBufferSizeDefault(BLOCK);
    g_stream = LoadAudioStream(SR, 16, 1);
    g_stream_ok = IsAudioStreamValid(g_stream);
    if (g_stream_ok) PlayAudioStream(g_stream);
    g_ready = 1;
}

void vb_synth_shutdown(void) {
    if (!g_ready) return;
    for (int i = 0; i < VB_SFX_COUNT; i++) UnloadSound(g_sfx[i]);
    if (g_stream_ok) UnloadAudioStream(g_stream);
    CloseAudioDevice();
    g_ready = 0;
    g_stream_ok = 0;
}

int vb_synth_ready(void) { return g_ready; }

void vb_synth_gesture(void) {
    /* On web the device cannot open until the player has touched something,
     * so the first input gesture retries it (§0). A no-op everywhere else. */
    if (!g_ready) vb_synth_init();
}

void vb_synth_volumes(float master, float music, float sfx, float crowd) {
    g_master = vb_clampf(master, 0, 1);
    g_music  = vb_clampf(music, 0, 1);
    g_sfxv   = vb_clampf(sfx, 0, 1);
    g_crowdv = vb_clampf(crowd, 0, 1);
}

void vb_sfx_at(int id, float pitch, float gain, float pan) {
    if (!g_ready || id < 0 || id >= VB_SFX_COUNT) return;
    Sound s = g_sfx[id];
    SetSoundPitch(s, vb_clampf(pitch, 0.35f, 3.0f));
    SetSoundVolume(s, vb_clampf(gain, 0, 1) * g_sfxv * g_master);
    /* raylib's pan is 1.0 hard left and 0.0 hard right. Kept well short of
     * either end: a sound that leaves one ear entirely reads as broken rather
     * than as placed, and the ball spends most of its life near the middle. */
    SetSoundPan(s, 0.5f - vb_clampf(pan, -1.0f, 1.0f) * 0.32f);
    PlaySound(s);
}

void vb_sfx(int id, float pitch, float gain) {
    vb_sfx_at(id, pitch, gain, 0.0f);
}

void vb_synth_crowd(float heat01, float gasp) {
    M.heat01 = vb_clampf(heat01, 0, 1);
    M.gasp = vb_clampf(gasp, 0, 1);
}

void vb_synth_music(int layers, int match_point, int ambient) {
    M.layers = vb_clampi(layers, 0, 4);
    M.lift = match_point ? 1 : 0;
    M.ambient = ambient;
}

void vb_synth_update(void) {
    if (!g_ready || !g_stream_ok) return;
    static short buf[BLOCK];
    while (IsAudioStreamProcessed(g_stream)) {
        fill_block(buf, BLOCK);
        UpdateAudioStream(g_stream, buf, BLOCK);
    }
}
