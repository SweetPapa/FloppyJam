/* synth.c — parameterised recipes -> short PCM buffers, plus a 16-step
 * tracker for the ambient bed. Nothing here reads a file. */
#include "synth.h"
#include "core.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>

#define SR         22050
#define VOICES     4          /* polyphony per sfx slot */
#define MUS_CHUNK  1024

/* ------------------------------------------------------------------ */
/* cosmetic-only PRNG (never touched by the sim)                       */

static unsigned int rs = 0x9e3779b9u;
static float nz(void)
{
    rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
    return ((float)(rs & 0xffffu) / 32768.0f) - 1.0f;
}

/* ------------------------------------------------------------------ */
/* buffer building                                                     */

typedef struct { short *s; int n; } Buf;

static Buf buf_new(float seconds)
{
    Buf b;
    b.n = (int)(seconds * SR);
    if (b.n < 8) b.n = 8;
    b.s = (short *)calloc((size_t)b.n, sizeof(short));
    return b;
}

static void buf_add(Buf *b, int i, float v)
{
    int q;
    if (i < 0 || i >= b->n) return;
    q = b->s[i] + (int)(v * 12000.0f);
    if (q >  32000) q =  32000;
    if (q < -32000) q = -32000;
    b->s[i] = (short)q;
}

/* one-pole lowpass sweep over an existing buffer */
static void buf_lp(Buf *b, float c0, float c1)
{
    float y = 0.0f;
    int i;
    for (i = 0; i < b->n; ++i) {
        float t = (float)i / (float)b->n;
        float c = bp_lerpf(c0, c1, t);
        y += c * ((float)b->s[i] - y);
        b->s[i] = (short)y;
    }
}

/* noise burst: exponential decay */
static void mix_noise(Buf *b, float start, float len, float amp, float decay)
{
    int i0 = (int)(start * SR), n = (int)(len * SR), i;
    for (i = 0; i < n; ++i) {
        float e = expf(-decay * (float)i / (float)SR);
        buf_add(b, i0 + i, nz() * amp * e);
    }
}

/* tone: shape 0 sine, 1 triangle, 2 square; freq sweeps f0 -> f1 */
static void mix_tone(Buf *b, float start, float len, float f0, float f1,
                     float amp, float decay, int shape)
{
    int i0 = (int)(start * SR), n = (int)(len * SR), i;
    float ph = 0.0f;
    for (i = 0; i < n; ++i) {
        float t = (float)i / (float)n;
        float f = bp_lerpf(f0, f1, t);
        float e = expf(-decay * (float)i / (float)SR);
        float v;
        ph += f / (float)SR;
        if (ph > 1.0f) ph -= 1.0f;
        switch (shape) {
        case 1:  v = 4.0f * fabsf(ph - 0.5f) - 1.0f; break;
        case 2:  v = ph < 0.5f ? 1.0f : -1.0f;       break;
        default: v = sinf(ph * BP_TAU);              break;
        }
        buf_add(b, i0 + i, v * amp * e);
    }
}

/* ------------------------------------------------------------------ */

static Sound  sfx[SFX_COUNT][VOICES];
static int    sfx_rr[SFX_COUNT];
static Sound  roll[3];              /* felt, ice, rough loops */
static int    roll_cur = -1;
static int    ready = 0;

static float vol_master = 0.8f, vol_music = 0.6f, vol_sfx = 0.9f;
static float duck = 0.0f;

static AudioStream music;
static int   mus_mood = 0;
static int   mus_step = 0;
static int   mus_pos = 0;           /* samples into the current step */
static short mus_buf[MUS_CHUNK];

/* voice state for the tracker */
static float bass_ph, bass_env, blip_ph, blip_env, perc_env, perc_lp;
static float bass_f, blip_f;

static Sound make(Buf b, int loop)
{
    Wave w;
    Sound s;
    (void)loop;
    w.frameCount = (unsigned int)b.n;
    w.sampleRate = SR;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = b.s;
    s = LoadSoundFromWave(w);
    return s;
}

static void make_all(int id, Buf b)
{
    int v;
    for (v = 0; v < VOICES; ++v) sfx[id][v] = make(b, 0);
    free(b.s);
}

/* the pentatonic set the super-bumpers sing in */
static const float BOING_HZ[5] = { 392.0f, 440.0f, 523.25f, 587.33f, 698.46f };

static void build_sfx(void)
{
    Buf b;
    int i;

    /* CRACK — the soul of the game: a short bright transient over a
     * pitched thock. Playback pitch scales with power at call time. */
    b = buf_new(0.22f);
    mix_noise(&b, 0.000f, 0.045f, 0.95f, 150.0f);
    mix_tone (&b, 0.000f, 0.130f, 720.0f, 190.0f, 0.75f, 42.0f, 1);
    mix_tone (&b, 0.001f, 0.060f, 2100.0f, 900.0f, 0.35f, 120.0f, 0);
    buf_lp(&b, 0.95f, 0.35f);
    make_all(SFX_CRACK, b);

    /* CLACK — bandpassed click, the classic pool contact */
    b = buf_new(0.14f);
    mix_noise(&b, 0.0f, 0.020f, 0.8f, 320.0f);
    mix_tone (&b, 0.0f, 0.090f, 1450.0f, 1150.0f, 0.6f, 70.0f, 0);
    mix_tone (&b, 0.0f, 0.050f, 2600.0f, 2300.0f, 0.3f, 130.0f, 0);
    buf_lp(&b, 0.9f, 0.6f);
    make_all(SFX_CLACK, b);

    /* THUMP — cushion */
    b = buf_new(0.20f);
    mix_noise(&b, 0.0f, 0.035f, 0.55f, 130.0f);
    mix_tone (&b, 0.0f, 0.150f, 260.0f, 120.0f, 0.8f, 34.0f, 0);
    buf_lp(&b, 0.5f, 0.15f);
    make_all(SFX_THUMP, b);

    /* RIM — the rattle */
    b = buf_new(0.16f);
    mix_tone(&b, 0.0f, 0.110f, 900.0f, 640.0f, 0.55f, 60.0f, 1);
    mix_noise(&b, 0.0f, 0.02f, 0.3f, 250.0f);
    make_all(SFX_RIM, b);

    /* SWALLOW — the cup takes it: descending gulp then a soft thud */
    b = buf_new(0.55f);
    mix_tone (&b, 0.00f, 0.26f, 620.0f, 130.0f, 0.7f, 9.0f, 0);
    mix_tone (&b, 0.20f, 0.30f, 150.0f, 70.0f, 0.6f, 12.0f, 1);
    mix_noise(&b, 0.24f, 0.10f, 0.25f, 45.0f);
    buf_lp(&b, 0.7f, 0.25f);
    make_all(SFX_SWALLOW, b);

    /* SPLASH */
    b = buf_new(0.60f);
    mix_noise(&b, 0.0f, 0.55f, 0.85f, 9.0f);
    mix_tone (&b, 0.0f, 0.30f, 420.0f, 90.0f, 0.3f, 14.0f, 0);
    buf_lp(&b, 0.9f, 0.10f);
    make_all(SFX_SPLASH, b);

    /* SCRATCH — record-scratch-ish rasp */
    b = buf_new(0.42f);
    for (i = 0; i < 7; ++i)
        mix_tone(&b, 0.05f * (float)i, 0.05f, 1500.0f - 130.0f * (float)i,
                 380.0f + 90.0f * (float)i, 0.55f, 22.0f, 2);
    mix_noise(&b, 0.0f, 0.35f, 0.35f, 12.0f);
    buf_lp(&b, 0.85f, 0.5f);
    make_all(SFX_SCRATCH, b);

    /* SAD — the rim-out slide. Tragedy, but funny. */
    b = buf_new(0.75f);
    mix_tone(&b, 0.0f, 0.70f, 760.0f, 155.0f, 0.55f, 4.0f, 0);
    mix_tone(&b, 0.0f, 0.70f, 764.0f, 152.0f, 0.30f, 4.0f, 1);
    make_all(SFX_SAD, b);

    /* BANNER slam */
    b = buf_new(0.35f);
    mix_noise(&b, 0.0f, 0.05f, 0.7f, 90.0f);
    mix_tone (&b, 0.0f, 0.28f, 180.0f, 165.0f, 0.7f, 16.0f, 2);
    mix_tone (&b, 0.0f, 0.28f, 269.0f, 247.0f, 0.4f, 16.0f, 1);
    buf_lp(&b, 0.6f, 0.3f);
    make_all(SFX_BANNER, b);

    /* KICK pad — a rising whoosh with a click */
    b = buf_new(0.30f);
    mix_tone (&b, 0.0f, 0.22f, 200.0f, 900.0f, 0.55f, 12.0f, 2);
    mix_noise(&b, 0.0f, 0.22f, 0.35f, 16.0f);
    buf_lp(&b, 0.3f, 0.95f);
    make_all(SFX_KICK, b);

    /* WARP — swept shimmer */
    b = buf_new(0.45f);
    mix_tone(&b, 0.0f, 0.40f, 300.0f, 1800.0f, 0.45f, 7.0f, 0);
    mix_tone(&b, 0.0f, 0.40f, 452.0f, 2710.0f, 0.25f, 7.0f, 0);
    mix_noise(&b, 0.0f, 0.18f, 0.2f, 30.0f);
    make_all(SFX_WARP, b);

    /* UNCAP — the rack hole opening: a fanfare stab */
    b = buf_new(0.70f);
    mix_tone(&b, 0.00f, 0.20f, 261.63f, 261.63f, 0.55f, 10.0f, 1);
    mix_tone(&b, 0.10f, 0.20f, 329.63f, 329.63f, 0.55f, 10.0f, 1);
    mix_tone(&b, 0.20f, 0.45f, 392.00f, 392.00f, 0.65f, 6.0f, 1);
    mix_tone(&b, 0.20f, 0.45f, 523.25f, 523.25f, 0.40f, 6.0f, 0);
    make_all(SFX_UNCAP, b);

    /* UI tick */
    b = buf_new(0.07f);
    mix_tone(&b, 0.0f, 0.055f, 1250.0f, 1050.0f, 0.35f, 90.0f, 1);
    make_all(SFX_UI, b);

    /* LAND — the ball touching down */
    b = buf_new(0.16f);
    mix_noise(&b, 0.0f, 0.05f, 0.5f, 90.0f);
    mix_tone (&b, 0.0f, 0.10f, 190.0f, 120.0f, 0.5f, 50.0f, 0);
    buf_lp(&b, 0.4f, 0.15f);
    make_all(SFX_LAND, b);

    /* BOING set — musical, pentatonic, one per bumper index */
    for (i = 0; i < 5; ++i) {
        b = buf_new(0.40f);
        mix_tone(&b, 0.0f, 0.34f, BOING_HZ[i] * 1.6f, BOING_HZ[i], 0.75f, 11.0f, 0);
        mix_tone(&b, 0.0f, 0.20f, BOING_HZ[i] * 3.2f, BOING_HZ[i] * 2.0f, 0.25f, 26.0f, 1);
        mix_noise(&b, 0.0f, 0.02f, 0.25f, 260.0f);
        make_all(SFX_BOING0 + i, b);
    }
}

static void build_rolls(void)
{
    Buf b;
    int i;
    /* felt: dull filtered noise */
    b = buf_new(1.0f);
    for (i = 0; i < b.n; ++i) buf_add(&b, i, nz() * 0.5f);
    buf_lp(&b, 0.10f, 0.10f);
    roll[0] = make(b, 1);
    free(b.s);
    /* ice: noise plus a ringing partial */
    b = buf_new(1.0f);
    for (i = 0; i < b.n; ++i) buf_add(&b, i, nz() * 0.28f);
    buf_lp(&b, 0.45f, 0.45f);
    mix_tone(&b, 0.0f, 1.0f, 1760.0f, 1764.0f, 0.16f, 0.0f, 0);
    mix_tone(&b, 0.0f, 1.0f, 2637.0f, 2631.0f, 0.09f, 0.0f, 0);
    roll[1] = make(b, 1);
    free(b.s);
    /* rough / sand: very dull, grainier */
    b = buf_new(1.0f);
    for (i = 0; i < b.n; ++i) buf_add(&b, i, nz() * 0.7f);
    buf_lp(&b, 0.045f, 0.045f);
    roll[2] = make(b, 1);
    free(b.s);
}

void bp_synth_init(void)
{
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    build_sfx();
    build_rolls();
    music = LoadAudioStream(SR, 16, 1);
    SetAudioStreamVolume(music, vol_master * vol_music);
    PlayAudioStream(music);
    ready = 1;
}

void bp_synth_shutdown(void)
{
    int i, v;
    if (!ready) { if (IsAudioDeviceReady()) CloseAudioDevice(); return; }
    for (i = 0; i < SFX_COUNT; ++i)
        for (v = 0; v < VOICES; ++v) UnloadSound(sfx[i][v]);
    for (i = 0; i < 3; ++i) UnloadSound(roll[i]);
    UnloadAudioStream(music);
    CloseAudioDevice();
    ready = 0;
}

void bp_synth_volumes(float master, float music_v, float sfx_v)
{
    vol_master = master; vol_music = music_v; vol_sfx = sfx_v;
    if (ready) SetAudioStreamVolume(music, vol_master * vol_music * (1.0f - 0.5f * duck));
}

void bp_sfx(int id, float pitch, float gain)
{
    Sound s;
    if (!ready || id < 0 || id >= SFX_COUNT) return;
    s = sfx[id][sfx_rr[id]];
    sfx_rr[id] = (sfx_rr[id] + 1) % VOICES;
    SetSoundPitch(s, bp_clampf(pitch, 0.35f, 3.0f));
    SetSoundVolume(s, bp_clampf(gain, 0.0f, 1.0f) * vol_sfx * vol_master);
    PlaySound(s);
}

void bp_roll(float speed, int surf)
{
    int want = 0;
    float g, p;
    if (!ready) return;
    if (surf == 2) want = 1;                 /* SURF_ICE   */
    else if (surf == 1 || surf == 3) want = 2; /* ROUGH/SAND */

    if (want != roll_cur) {
        if (roll_cur >= 0) StopSound(roll[roll_cur]);
        roll_cur = want;
        if (speed > 0.05f) PlaySound(roll[roll_cur]);
    }
    if (speed <= 0.05f) {
        if (roll_cur >= 0 && IsSoundPlaying(roll[roll_cur])) StopSound(roll[roll_cur]);
        return;
    }
    if (!IsSoundPlaying(roll[roll_cur])) PlaySound(roll[roll_cur]);
    g = bp_clampf(speed * 0.13f, 0.0f, 0.55f);
    p = bp_clampf(0.45f + speed * 0.16f, 0.4f, 2.2f);
    SetSoundVolume(roll[roll_cur], g * vol_sfx * vol_master);
    SetSoundPitch(roll[roll_cur], p);
}

/* ------------------------------------------------------------------ */
/* music: 16-step tracker, three voices, patterns as bytes             */

/* Scale degrees; 255 = rest. Two moods: the front nine is warm, the back
 * nine sits a fourth lower and slower — the course "ages" as you play. */
static const unsigned char PAT_BASS[2][16] = {
    { 0,255,255, 0, 255,255, 5,255,  3,255,255, 3, 255, 7,255,255 },
    { 0,255,255,255, 3,255,255,255,  5,255,255,255, 7,255, 5,255 },
};
static const unsigned char PAT_BLIP[2][16] = {
    { 255,12,255,15, 255,255,17,255, 255,19,255,255, 15,255,12,255 },
    { 255,255,15,255, 255,19,255,255, 22,255,255,17, 255,255,15,255 },
};
static const unsigned char PAT_PERC[2][16] = {
    { 2,0,1,0, 2,0,1,1, 2,0,1,0, 2,1,1,0 },
    { 2,0,0,0, 1,0,0,1, 2,0,0,0, 1,0,1,0 },
};
static const float ROOT[2] = { 55.00f, 43.65f };      /* A1, F1 */
static const int   STEP_SAMPLES[2] = { SR * 60 / (108 * 4), SR * 60 / (84 * 4) };

/* semitone offsets of a minor pentatonic, extended over three octaves */
static float degree_hz(float root, int deg)
{
    static const int SEMI[5] = { 0, 3, 5, 7, 10 };
    int oct = deg / 5, d = deg % 5;
    return root * powf(2.0f, (float)(SEMI[d] + 12 * oct) / 12.0f);
}

void bp_music_mood(int mood)
{
    if (mood == mus_mood) return;
    mus_mood = mood;
    mus_step = 0;
    mus_pos = 0;
    bass_env = blip_env = perc_env = 0.0f;
}

void bp_music_duck(float amount)
{
    duck = bp_clampf(amount, 0.0f, 1.0f);
    if (ready) SetAudioStreamVolume(music, vol_master * vol_music * (1.0f - 0.5f * duck));
}

void bp_music_update(void)
{
    int m;
    if (!ready) return;
    m = mus_mood - 1;

    while (IsAudioStreamProcessed(music)) {
        int i;
        for (i = 0; i < MUS_CHUNK; ++i) {
            float v = 0.0f;
            if (m < 0) { mus_buf[i] = 0; continue; }

            if (mus_pos == 0) {
                unsigned char b = PAT_BASS[m][mus_step];
                unsigned char p = PAT_BLIP[m][mus_step];
                unsigned char k = PAT_PERC[m][mus_step];
                if (b != 255) { bass_f = degree_hz(ROOT[m], b); bass_env = 1.0f; }
                if (p != 255) { blip_f = degree_hz(ROOT[m], p); blip_env = 0.55f; }
                if (k == 2)   perc_env = 0.55f;
                else if (k == 1) perc_env = 0.22f;
            }

            /* bass: soft triangle */
            if (bass_env > 0.0005f) {
                bass_ph += bass_f / (float)SR;
                if (bass_ph > 1.0f) bass_ph -= 1.0f;
                v += (4.0f * fabsf(bass_ph - 0.5f) - 1.0f) * bass_env * 0.30f;
                bass_env *= 0.99988f;
            }
            /* pad blip: sine with a quick decay */
            if (blip_env > 0.0005f) {
                blip_ph += blip_f / (float)SR;
                if (blip_ph > 1.0f) blip_ph -= 1.0f;
                v += sinf(blip_ph * BP_TAU) * blip_env * 0.16f;
                blip_env *= 0.99975f;
            }
            /* brush percussion: lowpassed noise */
            if (perc_env > 0.0005f) {
                perc_lp += 0.28f * (nz() - perc_lp);
                v += perc_lp * perc_env * 0.5f;
                perc_env *= 0.9993f;
            }

            mus_buf[i] = (short)(bp_clampf(v, -1.0f, 1.0f) * 9000.0f);

            if (++mus_pos >= STEP_SAMPLES[m]) {
                mus_pos = 0;
                mus_step = (mus_step + 1) & 15;
            }
        }
        UpdateAudioStream(music, mus_buf, MUS_CHUNK);
    }
}
