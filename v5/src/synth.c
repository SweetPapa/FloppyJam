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
/* Stereo frames per streamed chunk, and it MUST equal the size raylib gives the
 * stream's sub-buffer, which is why SetAudioStreamBufferSizeDefault is called
 * with the same number before LoadAudioStream.
 *
 * This is the bug that made the music sound muffled for so long.
 * UpdateAudioStream does not append — it fills one whole sub-buffer and then
 * (raudio.c: "Any leftover frames should be filled with zeros") memsets the
 * remainder to silence. With the default sizing, subBufferSize is
 * deviceSampleRate/30, so 1600 frames on a 48 kHz device. Writing 512 into 1600
 * meant every chunk was 512 frames of combo followed by 1088 frames of nothing:
 * a 32% duty cycle, gated at 30 Hz. That reads as muffled, buzzy and quiet, and
 * it never touched the sound effects because those are Sound objects that go
 * nowhere near the streaming path.
 *
 * 4096 frames is 128 ms at our sample rate and comfortably larger than any
 * realistic device period (miniaudio is typically 480-1024 frames), so raylib
 * will not quietly raise it past what we generate. */
#define MUS_FRAMES 4096
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
    /* Before LoadAudioStream, not after: this is what sizes the stream's
     * sub-buffer, and it has to match the MUS_FRAMES we generate per chunk or
     * raylib pads the difference with silence. See the note on MUS_FRAMES. */
    SetAudioStreamBufferSizeDefault(MUS_FRAMES);
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
/* Grid: eighth notes, swung 2:1, eight to the bar, SIXTEEN bars to a */
/* chorus. Everything below decides what the players do when a step   */
/* turns over; the per-sample block renders whatever is still ringing. */
/*                                                                    */
/* The first version of this picked every melody note independently   */
/* at random from a scale, which is why it noodled: random notes in   */
/* the right key still say nothing. What is here now has a rhythmic   */
/* cell it commits to, a contour it follows, and an answering phrase  */
/* that repeats the cell with the contour inverted — the cheapest     */
/* thing you can do that makes a line sound composed rather than      */
/* generated. Everything else (voice-led comping, sections, a horn    */
/* that trades with the vibes) serves the same goal: sounding like    */
/* four people who have played together before.                       */

typedef struct { unsigned char root, type; } Chord;

/* chord types -> semitones above the chord root */
enum { CH_M7 = 0, CH_DOM7, CH_MAJ7, CH_M7B5 };
static const signed char TONES[4][4] = {
    { 0, 3, 7, 10 },   /* m7    */
    { 0, 4, 7, 10 },   /* 7     */
    { 0, 4, 7, 11 },   /* maj7  */
    { 0, 3, 6, 10 },   /* m7b5  */
};
/* The scale each soloist improvises out of, per chord type. Seven notes so
 * stepwise motion has somewhere to go; the old six-note set kept forcing
 * leaps, which is half of why it sounded restless. */
static const signed char SCALE[4][7] = {
    {  0,  2,  3,  5,  7,  9, 10 },   /* dorian over m7    */
    {  0,  2,  4,  5,  7,  9, 10 },   /* mixolydian over 7 */
    {  0,  2,  4,  5,  7,  9, 11 },   /* ionian over maj7  */
    {  0,  1,  3,  5,  6,  8, 10 },   /* locrian over m7b5 */
};
/* Degrees 0,2,4,6 of those scales are the chord tones — the melody is pulled
 * onto one whenever a bar turns over. */
#define IS_CHORD_TONE(d) (((d) % 7) % 2 == 0)

#define FORM_BARS 16

/* Three charts, sixteen bars each. Eight bars looped often enough that you
 * heard the seam inside a single hole; sixteen with a different second half
 * takes long enough that the ear stops counting. */
static const Chord PROG[3][FORM_BARS] = {
    /* 0 — menus: a lazy major turnaround in F, nothing urgent */
    { {0,CH_MAJ7},{0,CH_MAJ7},{5,CH_MAJ7},{5,CH_MAJ7},
      {4,CH_M7},  {9,CH_DOM7},{2,CH_M7},  {7,CH_DOM7},
      {0,CH_MAJ7},{9,CH_M7},  {2,CH_M7},  {7,CH_DOM7},
      {4,CH_M7},  {9,CH_DOM7},{2,CH_M7},  {7,CH_DOM7} },
    /* 1 — front nine: a bright minor ii-V-i in C that keeps moving */
    { {0,CH_M7},  {0,CH_M7},  {5,CH_M7},  {5,CH_M7},
      {2,CH_M7B5},{7,CH_DOM7},{0,CH_M7},  {0,CH_M7},
      {8,CH_MAJ7},{8,CH_MAJ7},{2,CH_M7B5},{7,CH_DOM7},
      {0,CH_M7},  {5,CH_M7},  {2,CH_M7B5},{7,CH_DOM7} },
    /* 2 — back nine: a slow minor blues in A with a four-bar tag, because by
     * hole ten it should feel late */
    { {0,CH_M7},  {5,CH_M7},  {0,CH_M7},  {0,CH_M7},
      {5,CH_M7},  {5,CH_M7},  {0,CH_M7},  {0,CH_M7},
      {8,CH_DOM7},{7,CH_DOM7},{0,CH_M7},  {7,CH_DOM7},
      {0,CH_M7},  {5,CH_M7},  {0,CH_M7},  {7,CH_DOM7} },
};
static const int   BPM[3]   = { 112, 136, 88 };
static const float TONIC[3] = { 174.61f, 130.81f, 110.00f };   /* F3, C3, A2 */

/* Rhythmic cells on the eighth grid, bit 0 = the downbeat. Every one of these
 * is syncopated somewhere — a cell that plays on all four beats reads as an
 * exercise, not a phrase. */
static const unsigned char CELL[8] = {
    0x92, 0x4A, 0x25, 0xA4, 0x52, 0x29, 0x8A, 0x45
};

static int   mus_mood = 0;        /* 0 silence, 1 front, 2 back, 3 menu */
static int   mus_bar, mus_step, mus_pos, mus_len, mus_chorus;

/* ---- voice state ---- */
static float bass_ph, bass_env, bass_f, bass_lp, bass_fenv, bass_click;
static int   bass_semi;
static float pph[4], pf[4], piano_env, piano_amp, trem_ph;
static int   pv[3];                          /* current voicing, semitones    */
static float ride_env, ride_hp, ride_prev, rph1, rph2, rph3, ride_bell;
static float brush_env, brush_lp, swirl_lp, swirl_ph;
static float kick_ph, kick_env, kick_f;
static float vib_ph, vib_env, vib_f, vib_lfo;
static float sax_ph, sax_env, sax_f, sax_tgt, sax_lp, sax_vib, sax_breath;

/* ---- melody state ---- */
static unsigned char mel_cell;               /* rhythm mask for this bar      */
static signed char   mel_move[8];            /* contour: degree deltas        */
static int           mel_deg;                /* current degree (7 per octave) */
static int           mel_lead;               /* 0 = vibes, 1 = horn           */

static float delay_l[DELAY_LEN], delay_r[DELAY_LEN];
static int   delay_pos;
static float delay_lp;
static float hp_l, hp_r;             /* bus rumble filter state       */

static float note_hz(float tonic, int semi)
{
    return tonic * powf(2.0f, (float)semi / 12.0f);
}

/* Move `want` into the octave nearest `prev`. This is the entire trick behind
 * comping that does not jump around the keyboard between chords. */
static int voice_lead(int want, int prev)
{
    while (want - prev >  6) want -= 12;
    while (prev - want >  6) want += 12;
    return want;
}

static void music_reset(void)
{
    int j;
    mus_bar = mus_step = mus_pos = 0;
    mus_len = 1;
    mus_chorus = 0;
    bass_env = piano_env = ride_env = brush_env = kick_env = 0.0f;
    vib_env = sax_env = ride_bell = 0.0f;
    bass_semi = 0;
    mel_deg = 7;
    mel_cell = CELL[0];
    for (j = 0; j < 8; ++j) mel_move[j] = 0;
    pv[0] = 4; pv[1] = 10; pv[2] = 14;
    memset(delay_l, 0, sizeof(delay_l));
    memset(delay_r, 0, sizeof(delay_r));
    delay_pos = 0;
    hp_l = hp_r = 0.0f;
}

/* how many samples this eighth lasts — swing lives entirely in here */
static int step_len(int m, int step)
{
    int q = SR * 60 / BPM[m];          /* one quarter note */
    int lng = q * 2 / 3;               /* 2:1 triplet feel */
    return (step & 1) ? (q - lng) : lng;
}

/* Degree index -> semitones above the chord root, across octaves. */
static int deg_semi(const signed char *sc, int deg)
{
    int oct = deg / 7, d = deg % 7;
    if (d < 0) { d += 7; --oct; }
    return sc[d] + 12 * oct;
}

/* Called once at each step boundary: decide what the players do. */
static void music_step_begin(int m)
{
    const Chord *ch = &PROG[m][mus_bar];
    const Chord *nx = &PROG[m][(mus_bar + 1) % FORM_BARS];
    unsigned int sd = (unsigned int)((mus_chorus * FORM_BARS + mus_bar) * 8 + mus_step)
                      * 2654435761u;
    unsigned int bs = (unsigned int)(mus_chorus * FORM_BARS + mus_bar) * 40503u;
    int s = mus_step, beat = s >> 1, onbeat = !(s & 1);
    int last_bar = (mus_bar == FORM_BARS - 1);
    float tonic = TONIC[m];
    /* Sections rotate every chorus: the head, then the horn takes it, then the
     * vibes take it. Roughly forty seconds each, so a nine takes you through
     * the whole cycle two or three times without repeating verbatim. */
    int section = mus_chorus % 3;
    float density = (section == 0) ? 0.72f : 1.0f;

    mel_lead = (section == 1);

    /* --- walking bass ------------------------------------------------
     * Four to the bar. Beat 1 is the root, 2 and 3 are chord tones, and 4 is a
     * chromatic approach into the next bar's root. Octave choice is voice-led
     * so the line walks instead of leaping. */
    if (onbeat) {
        int semi;
        switch (beat) {
        case 0:  semi = ch->root; break;
        case 1:  semi = ch->root + TONES[ch->type][(mrnd(sd) < 0.55f) ? 2 : 1]; break;
        case 2:  semi = ch->root + TONES[ch->type][(mrnd(sd + 1u) < 0.6f) ? 3 : 2]; break;
        default: semi = (int)nx->root + ((mrnd(sd + 2u) < 0.5f) ? -1 : 1); break;
        }
        bass_semi = voice_lead(semi, bass_semi);
        while (bass_semi >  14) bass_semi -= 12;    /* stay in the bottom */
        while (bass_semi < -10) bass_semi += 12;
        /* tonic/2, not tonic/4. At a quarter the root landed at 27-33 Hz —
         * below the low E of a real double bass, inaudible on anything without
         * a subwoofer, and still eating most of the mix energy. An octave up
         * puts the line where an upright actually lives. */
        bass_f = note_hz(tonic * 0.5f, bass_semi);
        bass_env = 1.0f;
        bass_fenv = 1.0f;          /* the filter opens on the attack and shuts */
        bass_click = 1.0f;         /* the finger on the string                 */
    }

    /* --- piano comping ------------------------------------------------
     * Rootless shell (3rd, 7th, 9th) voice-led off the previous chord, hit on
     * the Charleston with an extra push in the busier sections. */
    if (s == 0 || s == 3 || (s == 6 && mrnd(sd + 3u) < 0.35f * density) ||
        (s == 5 && section == 2 && mrnd(sd + 8u) < 0.30f)) {
        const signed char *tn = TONES[ch->type];
        int want[3];
        int j;
        want[0] = ch->root + tn[1];
        want[1] = ch->root + tn[3];
        want[2] = ch->root + 14;                    /* the 9th on top */
        for (j = 0; j < 3; ++j) {
            pv[j] = voice_lead(want[j], pv[j]);
            while (pv[j] > 26) pv[j] -= 12;
            while (pv[j] <  2) pv[j] += 12;
            pf[j] = note_hz(tonic, pv[j]);
        }
        /* a fourth partial an octave under the 3rd gives the Rhodes some body */
        pf[3] = note_hz(tonic, pv[0] - 12);
        piano_env = 1.0f;
        piano_amp = ((s == 0) ? 0.95f : 0.62f) * (0.75f + 0.25f * density);
    }

    /* --- ride: ding, ding-a-ding, with the bell on the turnaround --- */
    if (s == 0 || s == 2 || s == 4 || s == 6) ride_env = (s == 2 || s == 6) ? 0.64f : 0.48f;
    else if (s == 3 || s == 7)                ride_env = 0.36f;
    if (last_bar && s == 4) ride_bell = 1.0f;

    /* --- brushes on 2 and 4, plus a fill across the last bar --- */
    if (s == 2 || s == 6) brush_env = 0.58f;
    if (last_bar && s >= 4 && (s & 1) == 0) brush_env = 0.70f;
    if (s == 0 && (mus_bar & 1) == 0) { kick_env = 0.58f; kick_f = 76.0f; }
    if (last_bar && s == 6) { kick_env = 0.75f; kick_f = 80.0f; }

    /* --- melody -------------------------------------------------------
     * A new rhythmic cell and contour every two bars. On the ANSWERING two
     * bars the same cell returns with the contour inverted, which is what
     * turns two phrases into a sentence. */
    if (s == 0 && (mus_bar & 1) == 0) {
        int answer = (mus_bar >> 1) & 1;    /* odd phrases answer even ones */
        int j;
        if (!answer) {
            mel_cell = CELL[mhash(bs + 3u) % 8u];
            for (j = 0; j < 8; ++j) {
                float u = mrnd(bs + 10u + (unsigned int)j);
                /* mostly steps, sometimes a third, rarely a leap */
                mel_move[j] = (signed char)(u < 0.42f ?  1 : u < 0.72f ? -1 :
                                            u < 0.86f ?  2 : u < 0.95f ? -2 :
                                            u < 0.98f ?  4 : -4);
            }
        } else {
            for (j = 0; j < 8; ++j) mel_move[j] = (signed char)(-mel_move[j]);
        }
    }

    if (mel_cell & (1u << s)) {
        const signed char *sc = SCALE[ch->type];
        float want = (section == 0) ? 0.62f : 0.88f;
        if (mrnd(sd + 4u) < want) {
            int semi;
            mel_deg += mel_move[s];
            if (mel_deg > 13) mel_deg -= 7;         /* stay in an octave and a half */
            if (mel_deg <  3) mel_deg += 7;
            /* land on a chord tone when the bar turns over, so the line keeps
             * agreeing with the harmony instead of drifting off it */
            if (onbeat && !IS_CHORD_TONE(mel_deg))
                mel_deg += (mrnd(sd + 5u) < 0.5f) ? 1 : -1;
            semi = ch->root + deg_semi(sc, mel_deg);
            if (mel_lead) {
                sax_tgt = note_hz(tonic, semi);
                if (sax_env < 0.02f) sax_f = sax_tgt;   /* no glide out of silence */
                sax_env = 0.55f + 0.25f * mrnd(sd + 6u);
                sax_breath = 1.0f;
            } else {
                vib_f = note_hz(tonic * 2.0f, semi);
                vib_env = 0.40f + 0.22f * mrnd(sd + 6u);
            }
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
    /* Moods are 0 silence, 1 front, 2 back, 3 menu. Charts are 0 menu, 1 front,
     * 2 back — so the menu is the only one that moves. The old expression here
     * was `mus_mood - 1`, which quietly sent the front nine to the MENU chart
     * and the back nine to the front-nine chart, and left the slow blues in
     * PROG[2] unreachable for the entire game. make testmusic caught it: two
     * different moods rendered byte-identical band energy. */
    m = (mus_mood <= 0) ? -1 : (mus_mood == 3) ? 0 : mus_mood;

    while (IsAudioStreamProcessed(music)) {
        int i;
        for (i = 0; i < MUS_FRAMES; ++i) {
            float l = 0.0f, r = 0.0f, dry, wet;
            if (m < 0) { mus_buf[i * 2] = mus_buf[i * 2 + 1] = 0; continue; }

            if (mus_pos == 0) {
                music_step_begin(m);
                mus_len = step_len(m, mus_step);
            }

            /* ---- upright bass ----
             * Triangle through a lowpass whose cutoff falls across the note,
             * plus a short noise transient. The filter envelope is what makes
             * it read as plucked gut rather than a held synth tone. */
            if (bass_env > 0.0004f) {
                float v, k;
                bass_ph += bass_f / (float)SR;
                if (bass_ph > 1.0f) bass_ph -= 1.0f;
                v = (4.0f * fabsf(bass_ph - 0.5f) - 1.0f) * bass_env;
                if (bass_click > 0.001f) {
                    v += nz() * bass_click * 0.30f;
                    bass_click *= 0.9986f;
                }
                k = 0.105f + 0.30f * bass_fenv;
                bass_lp += k * (v - bass_lp);
                bass_fenv *= 0.99975f;
                l += bass_lp * 0.30f; r += bass_lp * 0.30f;
                bass_env *= 0.99991f;
            }

            /* ---- Rhodes ----
             * Four partials — the voicing plus an octave-down body note — and a
             * bell partial on the attack that decays far faster than the
             * fundamentals. That fast top is the tine; without it a stack of
             * sines is an organ, not an electric piano. */
            if (piano_env > 0.0004f) {
                float v = 0.0f, tine;
                int j;
                trem_ph += 4.6f / (float)SR;
                if (trem_ph > 1.0f) trem_ph -= 1.0f;
                for (j = 0; j < 4; ++j) {
                    pph[j] += pf[j] / (float)SR;
                    if (pph[j] > 1.0f) pph[j] -= 1.0f;
                    v += sinf(pph[j] * BP_TAU) * (j == 3 ? 0.55f : 1.0f);
                }
                tine = piano_env * piano_env;
                tine *= tine;
                v += sinf(pph[0] * BP_TAU * 4.0f) * tine * 1.6f;
                v *= piano_env * piano_amp * (0.88f + 0.12f * sinf(trem_ph * BP_TAU));
                l += v * 0.165f; r += v * 0.115f;
                piano_env *= 0.99986f;
            }

            /* ---- ride cymbal ----
             * Wash plus inharmonic partials, and a bell that only rings on the
             * turnaround. Parked right of centre, away from the piano. */
            if (ride_env > 0.0004f || ride_bell > 0.004f) {
                float n = nz(), v;
                ride_hp = 0.96f * (ride_hp + n - ride_prev);
                ride_prev = n;
                rph1 += 5240.0f / (float)SR; if (rph1 > 1.0f) rph1 -= 1.0f;
                rph2 += 7930.0f / (float)SR; if (rph2 > 1.0f) rph2 -= 1.0f;
                rph3 += 3110.0f / (float)SR; if (rph3 > 1.0f) rph3 -= 1.0f;
                v = (ride_hp * 0.52f + sinf(rph1 * BP_TAU) * 0.13f +
                     sinf(rph2 * BP_TAU) * 0.08f) * ride_env;
                v += sinf(rph3 * BP_TAU) * ride_bell * 0.22f;
                l += v * 0.110f; r += v * 0.205f;
                ride_env  *= 0.99955f;
                ride_bell *= 0.99970f;
            }

            /* ---- brushes ----
             * A continuous swirl under short accented swishes. The swirl is the
             * brush never leaving the head, and it is most of what makes a jazz
             * kit sound like a room rather than a drum machine. */
            swirl_ph += 2.3f / (float)SR;
            if (swirl_ph > 1.0f) swirl_ph -= 1.0f;
            swirl_lp += 0.10f * (nz() - swirl_lp);
            {
                float sw = swirl_lp * (0.30f + 0.22f * sinf(swirl_ph * BP_TAU)) * 0.11f;
                l += sw; r += sw * 0.92f;
            }
            if (brush_env > 0.0004f) {
                brush_lp += 0.17f * (nz() - brush_lp);
                l += brush_lp * brush_env * 0.26f;
                r += brush_lp * brush_env * 0.23f;
                brush_env *= 0.99930f;
            }

            /* ---- kick: felt beater, more felt than heard ---- */
            if (kick_env > 0.0004f) {
                kick_ph += kick_f / (float)SR;
                if (kick_ph > 1.0f) kick_ph -= 1.0f;
                kick_f *= 0.99994f;
                l += sinf(kick_ph * BP_TAU) * kick_env * 0.19f;
                r += sinf(kick_ph * BP_TAU) * kick_env * 0.19f;
                kick_env *= 0.99975f;
            }

            /* ---- vibraphone: sine with the motor running ---- */
            if (vib_env > 0.0004f) {
                float v;
                vib_ph += vib_f / (float)SR;
                if (vib_ph > 1.0f) vib_ph -= 1.0f;
                vib_lfo += 6.4f / (float)SR;
                if (vib_lfo > 1.0f) vib_lfo -= 1.0f;
                v = sinf(vib_ph * BP_TAU) * vib_env *
                    (0.70f + 0.30f * sinf(vib_lfo * BP_TAU));
                v += sinf(vib_ph * BP_TAU * 4.02f) * vib_env * vib_env * 0.16f;
                l += v * 0.215f; r += v * 0.240f;
                vib_env *= 0.99978f;
            }

            /* ---- tenor ----
             * Filtered saw with vibrato that fades IN across the note, a short
             * glide between pitches, and a breath transient. Delayed vibrato is
             * the single most identifiable thing a horn player does. */
            if (sax_env > 0.0004f) {
                float v, cut, vibd;
                sax_f += (sax_tgt - sax_f) * 0.0016f;      /* portamento */
                sax_vib += 5.1f / (float)SR;
                if (sax_vib > 1.0f) sax_vib -= 1.0f;
                vibd = (1.0f - sax_env) * 0.9f;            /* comes in late */
                sax_ph += (sax_f * (1.0f + 0.010f * vibd * sinf(sax_vib * BP_TAU)))
                          / (float)SR;
                if (sax_ph > 1.0f) sax_ph -= 1.0f;
                v = 2.0f * sax_ph - 1.0f;
                if (sax_breath > 0.001f) {
                    v += nz() * sax_breath * 0.45f;
                    sax_breath *= 0.9982f;
                }
                cut = 0.10f + 0.16f * sax_env;
                sax_lp += cut * (v - sax_lp);
                l += sax_lp * sax_env * 0.225f;
                r += sax_lp * sax_env * 0.190f;
                sax_env *= 0.99982f;
            }

            /* ---- bus ----
             * One slap back, cross-fed and lowpassed: a small club, not a hall.
             * Then a soft knee, which glues six independent voices into
             * something that sounds mixed rather than summed. */
            dry = (l + r) * 0.5f;
            wet = delay_l[delay_pos];
            delay_lp += 0.30f * (dry + wet * 0.30f - delay_lp);
            delay_l[delay_pos] = delay_lp;
            delay_r[delay_pos] = delay_lp * 0.85f;
            delay_pos = (delay_pos + 1) % DELAY_LEN;
            l += delay_r[delay_pos] * 0.24f;
            r += wet * 0.19f;

            /* Rumble filter. Nothing musical lives under ~35 Hz here, but the
             * kick and the bass both put energy there, and on a laptop speaker
             * it is headroom spent on something nobody can hear. */
            hp_l += 0.0068f * (l - hp_l);  l -= hp_l;
            hp_r += 0.0068f * (r - hp_r);  r -= hp_r;

            /* tanh-ish soft clip: cheap, and it never produces the hard edge a
             * hard clamp does when the whole band lands on beat one together */
            l = bp_clampf(l, -1.4f, 1.4f);
            r = bp_clampf(r, -1.4f, 1.4f);
            l = l - 0.28f * l * l * l;
            r = r - 0.28f * r * r * r;

            mus_buf[i * 2]     = (short)(bp_clampf(l, -1.0f, 1.0f) * 9200.0f);
            mus_buf[i * 2 + 1] = (short)(bp_clampf(r, -1.0f, 1.0f) * 9200.0f);

            if (++mus_pos >= mus_len) {
                mus_pos = 0;
                if (++mus_step >= 8) {
                    mus_step = 0;
                    if (++mus_bar >= FORM_BARS) { mus_bar = 0; ++mus_chorus; }
                }
            }
        }
        UpdateAudioStream(music, mus_buf, MUS_FRAMES);
    }
}
