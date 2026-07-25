/* test_music.c — render the combo offline and check it is actually music.
 *
 * The music engine streams into a raylib AudioStream, which makes it awkward
 * to hear on a build machine and impossible to assert on. This harness stubs
 * out the six raylib audio calls synth.c uses, drives bp_music_update by hand,
 * and writes a WAV so a human can listen — then asserts the things that were
 * actually wrong before: silence, DC offset, clipping, a dead channel, and a
 * chorus that repeats verbatim.
 *
 *   make testmusic          # renders build/music-{menu,front,back}.wav
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- raylib audio stubs --------------------------------------------
 *
 * These use raylib's OWN types, and that is not a style choice. The first
 * version of this harness declared its own one-int Sound / AudioStream / Wave.
 * synth.c includes the real raylib.h, so it was passing and returning 32-byte
 * structs by value to functions compiled here as taking 4-byte ones — an ABI
 * mismatch that scribbles on the stack. arm64 macOS happened to survive it;
 * x86-64 Linux caught it as "*** buffer overflow detected ***" and aborted the
 * whole test binary. Including the real header means the compiler checks every
 * signature against the declarations synth.c is compiled against.
 *
 * Nothing links libraylib, so these definitions are what synth.c calls.
 */
#include "raylib.h"

static short  cap_buf[1 << 22];      /* captured stereo frames */
static int    cap_frames = 0;
static int    cap_limit  = 0;

/* raylib sizes a stream's sub-buffer as max(defaultSize, devicePeriod) and then
 * UpdateAudioStream fills ONE WHOLE sub-buffer, zeroing whatever the caller did
 * not supply. Modelling that here rather than accepting any frameCount is the
 * whole point: writing fewer frames than the sub-buffer holds is what made the
 * music play at a 32% duty cycle and sound muffled, and a stub that silently
 * accepts short writes cannot see it. */
#define STUB_DEVICE_PERIOD 1024      /* a plausible miniaudio period, in frames */
static int stub_default_size = 0;
static int stub_subbuffer = STUB_DEVICE_PERIOD;
static int stub_short_writes = 0;

void InitAudioDevice(void) {}
void CloseAudioDevice(void) {}
bool IsAudioDeviceReady(void) { return true; }

Sound LoadSoundFromWave(Wave wave)
{
    Sound s;
    (void)wave;
    memset(&s, 0, sizeof s);
    return s;
}
void UnloadSound(Sound sound) { (void)sound; }
void PlaySound(Sound sound) { (void)sound; }
void StopSound(Sound sound) { (void)sound; }
bool IsSoundPlaying(Sound sound) { (void)sound; return false; }
void SetSoundVolume(Sound sound, float volume) { (void)sound; (void)volume; }
void SetSoundPitch(Sound sound, float pitch) { (void)sound; (void)pitch; }

void SetAudioStreamBufferSizeDefault(int size) { stub_default_size = size; }

AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize,
                            unsigned int channels)
{
    AudioStream a;
    memset(&a, 0, sizeof a);
    a.sampleRate = sampleRate;
    a.sampleSize = sampleSize;
    a.channels = channels;
    stub_subbuffer = (stub_default_size > STUB_DEVICE_PERIOD)
                   ? stub_default_size : STUB_DEVICE_PERIOD;
    return a;
}
void UnloadAudioStream(AudioStream stream) { (void)stream; }
void PlayAudioStream(AudioStream stream) { (void)stream; }
void SetAudioStreamVolume(AudioStream stream, float volume) { (void)stream; (void)volume; }

bool IsAudioStreamProcessed(AudioStream stream)
{
    (void)stream;
    return cap_frames + stub_subbuffer <= cap_limit;
}

void UpdateAudioStream(AudioStream stream, const void *data, int frameCount)
{
    int filled = frameCount, pad;
    (void)stream;
    if (frameCount > stub_subbuffer) return;      /* raylib logs and drops these */
    pad = stub_subbuffer - filled;
    if (pad > 0) ++stub_short_writes;             /* raylib would zero this much */
    if (cap_frames + stub_subbuffer > cap_limit) return;
    memcpy(cap_buf + (size_t)cap_frames * 2, data,
           (size_t)filled * 2 * sizeof(short));
    if (pad > 0)
        memset(cap_buf + (size_t)(cap_frames + filled) * 2, 0,
               (size_t)pad * 2 * sizeof(short));
    cap_frames += stub_subbuffer;
}

#include "../src/synth.h"

/* synth.c's own declarations come from its includes; we only need the API */
void bp_synth_init(void);
void bp_music_mood(int mood);
void bp_music_update(void);

#define SR 32000

static int failures = 0, checks = 0;
static void ok(int cond, const char *what, const char *detail)
{
    ++checks;
    printf("  %s %s%s%s\n", cond ? "ok  " : "FAIL", what,
           detail ? " — " : "", detail ? detail : "");
    if (!cond) ++failures;
}

static void write_wav(const char *path, const short *pcm, int frames)
{
    FILE *f = fopen(path, "wb");
    int datasz = frames * 2 * 2, i;
    unsigned int u; unsigned short v;
    if (!f) { printf("  (could not write %s)\n", path); return; }
    fwrite("RIFF", 1, 4, f); u = (unsigned)(36 + datasz); fwrite(&u, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); u = 16; fwrite(&u, 4, 1, f);
    v = 1; fwrite(&v, 2, 1, f); v = 2; fwrite(&v, 2, 1, f);
    u = SR; fwrite(&u, 4, 1, f); u = SR * 4; fwrite(&u, 4, 1, f);
    v = 4; fwrite(&v, 2, 1, f); v = 16; fwrite(&v, 2, 1, f);
    fwrite("data", 1, 4, f); u = (unsigned)datasz; fwrite(&u, 4, 1, f);
    for (i = 0; i < frames * 2; ++i) fwrite(&pcm[i], 2, 1, f);
    fclose(f);
}

static void render(int mood, int seconds)
{
    cap_frames = 0;
    cap_limit = SR * seconds;
    if (cap_limit * 2 > (int)(sizeof(cap_buf) / sizeof(cap_buf[0])))
        cap_limit = (int)(sizeof(cap_buf) / sizeof(cap_buf[0])) / 2;
    bp_music_mood(mood);
    /* Stop when a pass makes no progress, not when the buffer is exactly full:
     * the stub only accepts a write if a whole sub-buffer still fits, so the
     * last partial chunk is never consumed and `cap_frames < cap_limit` would
     * spin forever. */
    for (;;) {
        int before = cap_frames;
        bp_music_update();
        if (cap_frames == before) break;
    }
}

int main(void)
{
    static const char *NAME[3] = { "front", "back", "menu" };
    static const int   MOOD[3] = { 1, 2, 3 };
    int k;
    double peak_all = 0.0;

    printf("BREAK PAR — music suite\n\n");
    bp_synth_init();

    for (k = 0; k < 3; ++k) {
        char path[128], detail[160];
        double sum = 0.0, sumsq = 0.0, peakl = 0.0, peakr = 0.0;
        double el = 0.0, er = 0.0;
        int i, clipped = 0, quiet_runs = 0, run = 0;

        stub_short_writes = 0;
        render(MOOD[k], 24);

        for (i = 0; i < cap_frames; ++i) {
            double a = cap_buf[i * 2] / 32768.0, b = cap_buf[i * 2 + 1] / 32768.0;
            sum += a + b; sumsq += a * a + b * b;
            el += a * a; er += b * b;
            if (fabs(a) > peakl) peakl = fabs(a);
            if (fabs(b) > peakr) peakr = fabs(b);
            if (fabs(a) > 0.985 || fabs(b) > 0.985) ++clipped;
            if (fabs(a) + fabs(b) < 0.0008) { if (++run > SR / 2) { ++quiet_runs; run = 0; } }
            else run = 0;
        }
        {
            double rms = sqrt(sumsq / (cap_frames * 2));
            double dc  = sum / (cap_frames * 2);
            double bal = (er > 1e-9) ? el / er : 999.0;
            if (peakl > peak_all) peak_all = peakl;
            if (peakr > peak_all) peak_all = peakr;

            snprintf(path, sizeof path, "build/music-%s.wav", NAME[k]);
            write_wav(path, cap_buf, cap_frames);

            printf("%s (%d s)\n", NAME[k], cap_frames / SR);
            snprintf(detail, sizeof detail, "rms %.4f, peak %.3f/%.3f", rms, peakl, peakr);
            ok(rms > 0.02 && rms < 0.45, "level is in a sane range", detail);

            snprintf(detail, sizeof detail, "dc offset %.5f", dc);
            ok(fabs(dc) < 0.02, "no DC offset", detail);

            snprintf(detail, sizeof detail, "%d samples at full scale", clipped);
            ok(clipped < cap_frames / 500, "not clipping", detail);

            snprintf(detail, sizeof detail, "L/R energy ratio %.2f", bal);
            ok(bal > 0.4 && bal < 2.5, "both channels carry the mix", detail);

            snprintf(detail, sizeof detail, "%d half-second gaps", quiet_runs);
            ok(quiet_runs == 0, "never drops to silence", detail);

            /* The muffling bug: raylib pads a short write with silence, so the
             * stream came out gated instead of continuous. Assert the chunk size
             * matches the sub-buffer exactly. */
            snprintf(detail, sizeof detail,
                     "%d short writes into a %d-frame sub-buffer",
                     stub_short_writes, stub_subbuffer);
            ok(stub_short_writes == 0,
               "fills the whole audio sub-buffer (no silence padding)", detail);

            printf("  ->  %s\n\n", path);
        }
    }

    /* The old engine looped an 8-bar chart forever. Compare the first half of a
     * long render against the second: they must not be the same audio. */
    {
        char detail[128];
        int half, i, same = 0;
        render(1, 90);
        half = cap_frames / 2;
        for (i = 0; i < half; i += 64)
            if (cap_buf[i * 2] == cap_buf[(half + i) * 2]) ++same;
        snprintf(detail, sizeof detail, "%d of %d probes identical", same, half / 64);
        printf("form\n");
        ok(same < (half / 64) / 8, "a later chorus is not a copy of an earlier one", detail);
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
