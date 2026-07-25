/* synth.c — parameterised recipes -> short PCM buffers, plus a small swing
 * jazz combo streamed a sample at a time. Nothing here reads a file.
 *
 * The combo is the reason this file grew: BREAK PAR is played at two in the
 * morning under neon, and a straight 16-step pentatonic loop was the one
 * thing on screen still saying "daytime". What runs now is a walking bass, a
 * comping electric piano, a swung ride cymbal, brushes and a sparse vibes
 * line over an eight-bar chord chart, in stereo, through a slap delay. All of
 * it is a few hundred bytes of chord table plus arithmetic. */
#include "synth.h"
#include "core.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>

/* 32 kHz, up from 22: brushes and ride cymbals live above 8 kHz and turned to
 * mush at the old rate. Costs nothing on disk — every buffer is generated. */
#define SR         32000
#define VOICES     4          /* polyphony per sfx slot */
#define MUS_FRAMES 512        /* stereo frames per streamed chunk */
#define DELAY_LEN  6400       /* 200 ms slap, the room the combo plays in */

/* ------------------------------------------------------------------ */
/* cosmetic-only PRNG (never touched by the sim)                       */

static unsigned int rs = 0x9e3779b9u;
static float nz(void)
{
    rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
    return ((float)(rs & 0xffffu) / 32768.0f) - 1.0f;
}

/* separate, seekable stream for musical choices, so the improviser plays the
 * same solo on the same bar instead of drifting with the sfx load */
static unsigned int mhash(unsigned int x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
static float mrnd(unsigned int x) { return (float)(mhash(x) & 0xffffu) / 65535.0f; }

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
static short mus_buf[MUS_FRAMES * 2];

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

    /* SAD — the rim-out slide. Tragedy, but funny. Now with a muted-trumpet
     * "wah wah" tail, because this is a jazz club. */
    b = buf_new(0.95f);
    mix_tone(&b, 0.0f, 0.70f, 760.0f, 155.0f, 0.55f, 4.0f, 0);
    mix_tone(&b, 0.0f, 0.70f, 764.0f, 152.0f, 0.30f, 4.0f, 1);
    mix_tone(&b, 0.52f, 0.20f, 233.08f, 220.00f, 0.45f, 9.0f, 2);
    mix_tone(&b, 0.70f, 0.25f, 207.65f, 174.61f, 0.45f, 7.0f, 2);
    buf_lp(&b, 0.6f, 0.30f);
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

    /* TICK — the aim detent. Tiny, dry, and cheap enough to fire often. */
    b = buf_new(0.035f);
    mix_tone (&b, 0.0f, 0.014f, 2400.0f, 1500.0f, 0.30f, 320.0f, 1);
    mix_noise(&b, 0.0f, 0.006f, 0.16f, 700.0f);
    make_all(SFX_TICK, b);

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

    /* ---- the night-out set ------------------------------------------ */

    /* STAB — a three-horn shell voicing hit hard and cut off. This is what
     * a birdie should sound like: a section, not a chime. */
    b = buf_new(0.85f);
    {
        static const float V[4] = { 220.00f, 277.18f, 329.63f, 415.30f };
        for (i = 0; i < 4; ++i) {
            mix_tone(&b, 0.000f, 0.55f, V[i] * 0.985f, V[i], 0.34f, 5.5f, 2);
            mix_tone(&b, 0.004f, 0.50f, V[i] * 2.0f,   V[i] * 2.0f, 0.11f, 8.0f, 1);
        }
        mix_noise(&b, 0.0f, 0.03f, 0.35f, 130.0f);
    }
    buf_lp(&b, 0.85f, 0.28f);
    make_all(SFX_STAB, b);

    /* CROWD — a small room applauding. Filtered noise swelling under a
     * scatter of individual claps so it does not read as static. */
    b = buf_new(1.90f);
    mix_noise(&b, 0.0f, 1.85f, 0.34f, 1.5f);
    for (i = 0; i < 90; ++i) {
        float when = 0.02f + 1.60f * ((float)(mhash((unsigned int)i * 7919u) & 0xffffu)
                                      / 65535.0f);
        float amp = 0.16f + 0.22f * ((float)(mhash((unsigned int)i * 104729u) & 0xffu)
                                     / 255.0f);
        mix_noise(&b, when, 0.020f, amp, 320.0f);
    }
    buf_lp(&b, 0.60f, 0.40f);
    make_all(SFX_CROWD, b);

    /* CHIME — struck vibraphone bar with its octave and a touch of the
     * inharmonic fourth partial. Used for targets and gold. */
    b = buf_new(1.30f);
    mix_tone(&b, 0.0f, 1.20f, 1046.50f, 1046.50f, 0.50f, 2.4f, 0);
    mix_tone(&b, 0.0f, 0.90f, 2093.00f, 2093.00f, 0.22f, 4.0f, 0);
    mix_tone(&b, 0.0f, 0.45f, 4186.00f, 4186.00f, 0.09f, 9.0f, 0);
    mix_noise(&b, 0.0f, 0.008f, 0.20f, 600.0f);
    make_all(SFX_CHIME, b);

    /* COIN — the fairground payout, two quick bright pips */
    b = buf_new(0.36f);
    mix_tone(&b, 0.00f, 0.09f, 987.77f, 987.77f, 0.55f, 26.0f, 1);
    mix_tone(&b, 0.07f, 0.26f, 1318.51f, 1318.51f, 0.55f, 11.0f, 1);
    mix_tone(&b, 0.07f, 0.26f, 2637.02f, 2637.02f, 0.16f, 14.0f, 0);
    make_all(SFX_COIN, b);

    /* WHOOSH — brushed air for camera moves and menu travel */
    b = buf_new(0.42f);
    mix_noise(&b, 0.0f, 0.40f, 0.55f, 7.0f);
    buf_lp(&b, 0.06f, 0.85f);
    make_all(SFX_WHOOSH, b);
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
    music = LoadAudioStream(SR, 16, 2);
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

/* ================================================================== */
/* the combo                                                          */
/*                                                                    */
/* Grid: eighth notes, swung 2:1, eight to the bar, eight bars to the */
/* chorus. Everything below decides what to play when a step turns    */
/* over, then the per-sample block renders whatever is ringing.       */

typedef struct { unsigned char root, type; } Chord;

/* chord types -> semitones above the chord root */
enum { CH_M7 = 0, CH_DOM7, CH_MAJ7, CH_M7B5 };
static const signed char TONES[4][4] = {
    { 0, 3, 7, 10 },   /* m7    */
    { 0, 4, 7, 10 },   /* 7     */
    { 0, 4, 7, 11 },   /* maj7  */
    { 0, 3, 6, 10 },   /* m7b5  */
};
/* the scale the vibes player improvises out of, per chord type */
static const signed char SCALE[4][6] = {
    {  0,  3,  5,  7, 10, 14 },   /* minor pentatonic over m7        */
    {  0,  4,  7,  9, 10, 14 },   /* mixolydian bones over 7         */
    {  0,  2,  4,  7,  9, 11 },   /* major over maj7                 */
    {  0,  3,  6, 10, 13, 15 },   /* half-diminished, with tensions  */
};

/* Three charts. 0 is the menu — a lazy major turnaround. 1 is the front
 * nine, a bright minor ii-V-i that keeps moving. 2 is the back nine: a slow
 * minor blues, because by hole ten it should feel late. */
static const Chord PROG[3][8] = {
    { {0,CH_MAJ7},{5,CH_MAJ7},{9,CH_M7},  {2,CH_DOM7},
      {7,CH_M7},  {0,CH_MAJ7},{9,CH_M7},  {2,CH_DOM7} },
    { {0,CH_M7},  {5,CH_M7},  {2,CH_M7B5},{7,CH_DOM7},
      {0,CH_M7},  {8,CH_MAJ7},{2,CH_M7B5},{7,CH_DOM7} },
    { {0,CH_M7},  {0,CH_M7},  {5,CH_M7},  {5,CH_M7},
      {0,CH_M7},  {8,CH_DOM7},{7,CH_DOM7},{7,CH_DOM7} },
};
static const int   BPM[3]     = { 116, 138, 92 };
static const float TONIC[3]   = { 174.61f, 130.81f, 110.00f };   /* F3, C3, A2 */

static int   mus_mood = 0;        /* 0 silence, 1 front, 2 back, 3 menu */
static int   mus_bar, mus_step, mus_pos, mus_len, mus_chorus;

/* voice state */
static float bass_ph, bass_env, bass_f, bass_lp;
static float pf[3], pph[3], piano_env, trem_ph;
static float ride_env, ride_hp, ride_prev, rph1, rph2;
static float brush_env, brush_lp;
static float kick_ph, kick_env, kick_f;
static float vib_ph, vib_env, vib_f, vib_lfo;

static float delay_l[DELAY_LEN], delay_r[DELAY_LEN];
static int   delay_pos;
static float delay_lp;

static float note_hz(float tonic, int semi)
{
    return tonic * powf(2.0f, (float)semi / 12.0f);
}

static void music_reset(void)
{
    mus_bar = mus_step = mus_pos = 0;
    mus_len = 1;
    mus_chorus = 0;
    bass_env = piano_env = ride_env = brush_env = kick_env = vib_env = 0.0f;
    memset(delay_l, 0, sizeof(delay_l));
    memset(delay_r, 0, sizeof(delay_r));
    delay_pos = 0;
}

/* how many samples this eighth lasts — swing lives entirely in here */
static int step_len(int m, int step)
{
    int q = SR * 60 / BPM[m];          /* one quarter note */
    int lng = q * 2 / 3;               /* 2:1 triplet feel */
    return (step & 1) ? (q - lng) : lng;
}

/* Called once at each step boundary: decide what the five players do. */
static void music_step_begin(int m)
{
    const Chord *ch = &PROG[m][mus_bar];
    const Chord *nx = &PROG[m][(mus_bar + 1) & 7];
    unsigned int sd = (unsigned int)((mus_chorus * 8 + mus_bar) * 8 + mus_step) * 2654435761u;
    int s = mus_step, beat = s >> 1, onbeat = !(s & 1);
    float tonic = TONIC[m];

    /* --- walking bass: four to the bar, last beat leans into the next --- */
    if (onbeat) {
        int semi;
        switch (beat) {
        case 0:  semi = ch->root; break;
        case 1:  semi = ch->root + TONES[ch->type][(mrnd(sd) < 0.5f) ? 2 : 1]; break;
        case 2:  semi = ch->root + TONES[ch->type][(mrnd(sd + 1u) < 0.6f) ? 3 : 2]; break;
        default: semi = (int)nx->root + ((mrnd(sd + 2u) < 0.5f) ? -1 : 1); break;
        }
        bass_f = note_hz(tonic * 0.25f, semi);
        bass_env = 1.0f;
    }

    /* --- piano comping: the Charleston, plus the odd extra push --- */
    if (s == 0 || s == 3 || (s == 6 && mrnd(sd + 3u) < 0.45f)) {
        const signed char *tn = TONES[ch->type];
        /* rootless shell: 3rd, 7th, 9th. It is what a piano actually plays
         * behind a bass that is already covering the root. */
        pf[0] = note_hz(tonic, ch->root + tn[1]);
        pf[1] = note_hz(tonic, ch->root + tn[3]);
        pf[2] = note_hz(tonic, ch->root + 14);
        piano_env = (s == 0) ? 0.85f : 0.60f;
    }

    /* --- ride: ding, ding-a-ding. Beats plus the swung a of 2 and 4. --- */
    if (s == 0 || s == 2 || s == 4 || s == 6) ride_env = (s == 2 || s == 6) ? 0.62f : 0.46f;
    else if (s == 3 || s == 7)                ride_env = 0.34f;

    /* --- brushes on 2 and 4, kick feathered on 1 --- */
    if (s == 2 || s == 6) brush_env = 0.55f;
    if (s == 0 && (mus_bar & 1) == 0) { kick_env = 0.60f; kick_f = 74.0f; }

    /* --- vibes: sparse, off-beat, phrased in twos and threes --- */
    {
        float want = onbeat ? 0.16f : 0.42f;
        if (mus_bar == 3 || mus_bar == 7) want += 0.22f;    /* fill the turnaround */
        if (mrnd(sd + 4u) < want) {
            const signed char *sc = SCALE[ch->type];
            int deg = (int)(mrnd(sd + 5u) * 5.999f);
            int oct = (mrnd(sd + 6u) < 0.3f) ? 12 : 0;
            vib_f = note_hz(tonic * 2.0f, ch->root + sc[deg] + oct);
            vib_env = 0.34f + 0.20f * mrnd(sd + 7u);
        }
    }
}

void bp_music_mood(int mood)
{
    if (mood == mus_mood) return;
    mus_mood = mood;
    music_reset();
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
    /* moods are 1 front, 2 back, 3 menu; charts are indexed 1, 2, 0 */
    m = (mus_mood == 3) ? 0 : mus_mood - 1;

    while (IsAudioStreamProcessed(music)) {
        int i;
        for (i = 0; i < MUS_FRAMES; ++i) {
            float l = 0.0f, r = 0.0f, dry, wet;
            if (m < 0) { mus_buf[i * 2] = mus_buf[i * 2 + 1] = 0; continue; }

            if (mus_pos == 0) {
                music_step_begin(m);
                mus_len = step_len(m, mus_step);
            }

            /* bass: triangle through a fixed lowpass, plucked */
            if (bass_env > 0.0004f) {
                float v;
                bass_ph += bass_f / (float)SR;
                if (bass_ph > 1.0f) bass_ph -= 1.0f;
                v = (4.0f * fabsf(bass_ph - 0.5f) - 1.0f) * bass_env;
                bass_lp += 0.14f * (v - bass_lp);
                l += bass_lp * 0.50f; r += bass_lp * 0.50f;
                bass_env *= 0.99992f;
            }
            /* electric piano: three sines, tremolo, sits left of centre */
            if (piano_env > 0.0004f) {
                float v = 0.0f;
                int j;
                trem_ph += 5.2f / (float)SR;
                if (trem_ph > 1.0f) trem_ph -= 1.0f;
                for (j = 0; j < 3; ++j) {
                    pph[j] += pf[j] / (float)SR;
                    if (pph[j] > 1.0f) pph[j] -= 1.0f;
                    v += sinf(pph[j] * BP_TAU);
                }
                v *= piano_env * (0.86f + 0.14f * sinf(trem_ph * BP_TAU));
                l += v * 0.115f; r += v * 0.075f;
                piano_env *= 0.99984f;
            }
            /* ride cymbal: highpassed noise plus two inharmonic partials,
             * parked right of centre so it does not fight the piano */
            if (ride_env > 0.0004f) {
                float n = nz(), v;
                ride_hp = 0.96f * (ride_hp + n - ride_prev);
                ride_prev = n;
                rph1 += 5240.0f / (float)SR; if (rph1 > 1.0f) rph1 -= 1.0f;
                rph2 += 7930.0f / (float)SR; if (rph2 > 1.0f) rph2 -= 1.0f;
                v = (ride_hp * 0.55f + sinf(rph1 * BP_TAU) * 0.14f +
                     sinf(rph2 * BP_TAU) * 0.09f) * ride_env;
                l += v * 0.070f; r += v * 0.135f;
                ride_env *= 0.99955f;
            }
            /* brushes: a short swish, not a snare crack */
            if (brush_env > 0.0004f) {
                brush_lp += 0.16f * (nz() - brush_lp);
                l += brush_lp * brush_env * 0.34f;
                r += brush_lp * brush_env * 0.30f;
                brush_env *= 0.99930f;
            }
            /* kick: felt beater, mostly felt rather than heard */
            if (kick_env > 0.0004f) {
                kick_ph += kick_f / (float)SR;
                if (kick_ph > 1.0f) kick_ph -= 1.0f;
                kick_f *= 0.99994f;
                l += sinf(kick_ph * BP_TAU) * kick_env * 0.34f;
                r += sinf(kick_ph * BP_TAU) * kick_env * 0.34f;
                kick_env *= 0.99975f;
            }
            /* vibraphone: sine with the motor running */
            if (vib_env > 0.0004f) {
                float v;
                vib_ph += vib_f / (float)SR;
                if (vib_ph > 1.0f) vib_ph -= 1.0f;
                vib_lfo += 6.4f / (float)SR;
                if (vib_lfo > 1.0f) vib_lfo -= 1.0f;
                v = sinf(vib_ph * BP_TAU) * vib_env *
                    (0.70f + 0.30f * sinf(vib_lfo * BP_TAU));
                l += v * 0.150f; r += v * 0.170f;
                vib_env *= 0.99978f;
            }

            /* one slap back, cross-fed, lowpassed: a small club, not a hall */
            dry = (l + r) * 0.5f;
            wet = delay_l[delay_pos];
            delay_lp += 0.30f * (dry + wet * 0.30f - delay_lp);
            delay_l[delay_pos] = delay_lp;
            delay_r[delay_pos] = delay_lp * 0.85f;
            delay_pos = (delay_pos + 1) % DELAY_LEN;
            l += delay_r[delay_pos] * 0.26f;
            r += wet * 0.20f;

            mus_buf[i * 2]     = (short)(bp_clampf(l, -1.0f, 1.0f) * 8800.0f);
            mus_buf[i * 2 + 1] = (short)(bp_clampf(r, -1.0f, 1.0f) * 8800.0f);

            if (++mus_pos >= mus_len) {
                mus_pos = 0;
                if (++mus_step >= 8) {
                    mus_step = 0;
                    if (++mus_bar >= 8) { mus_bar = 0; ++mus_chorus; }
                }
            }
        }
        UpdateAudioStream(music, mus_buf, MUS_FRAMES);
    }
}
