/* audio.c — procedurally synthesized sound effects (no asset files).
 * Each SFX is rendered into an in-memory 16-bit PCM buffer at init. */
#include "app.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SR 22050
#define TAU 6.28318530718f

/* rng for noise */
static unsigned int nrng = 0x9e3779b9u;
static float noise(void) {
    nrng = nrng * 1664525u + 1013904223u;
    return ((float)((nrng >> 9) & 0x7FFFF) / 262143.0f) * 2.0f - 1.0f;
}

/* Build a Sound from a synthesized envelope.
 * shape: 0 sine sweep, 1 pluck, 2 arpeggio, 3 noise-sweep (death) */
static Sound synth(int shape, float dur, float f0, float f1, float amp) {
    int n = (int)(SR * dur);
    if (n < 1) n = 1;
    short *buf = (short *)malloc(sizeof(short) * n);
    float phase = 0.0f;
    const float arp[4] = {523.25f, 659.25f, 783.99f, 1046.5f};
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;      /* 0..1 */
        float env;
        float s;
        switch (shape) {
        case 0: { /* sine sweep, soft attack/decay */
            float f = f0 + (f1 - f0) * t;
            phase += TAU * f / SR;
            env = sinf(t * 3.14159f);        /* smooth bell */
            s = sinf(phase) * env;
            break;
        }
        case 1: { /* pluck: fixed freq, sharp attack, exp decay + slight overtone */
            phase += TAU * f0 / SR;
            env = expf(-5.0f * t);
            s = (sinf(phase) + 0.3f * sinf(phase * 2.0f)) * env;
            break;
        }
        case 2: { /* arpeggio: step through 4 notes */
            int step = (int)(t * 4.0f); if (step > 3) step = 3;
            float f = arp[step] * (f0 / 523.25f); /* f0 lets us transpose */
            phase += TAU * f / SR;
            float lt = (t * 4.0f) - step;         /* local 0..1 per note */
            env = expf(-3.0f * lt);
            s = sinf(phase) * env;
            break;
        }
        default: { /* noise-sweep death */
            float f = f0 + (f1 - f0) * t;
            phase += TAU * f / SR;
            env = expf(-3.0f * t);
            s = (0.6f * sinf(phase) + 0.4f * noise()) * env;
            break;
        }
        }
        float v = s * amp;
        if (v > 1.0f) v = 1.0f; else if (v < -1.0f) v = -1.0f;
        buf[i] = (short)(v * 32000.0f);
    }
    Wave w;
    w.frameCount = (unsigned int)n;
    w.sampleRate = SR;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = buf;
    Sound snd = LoadSoundFromWave(w);
    free(buf);
    return snd;
}

void audio_init(App *a) {
    a->audio_ok = 0;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    a->audio_ok = 1;
    a->sfx[SFX_SWING]      = synth(0, 0.16f, 320.0f, 720.0f, 0.35f);
    a->sfx[SFX_LAND]       = synth(1, 0.18f, 523.25f, 0.0f, 0.5f);
    a->sfx[SFX_LAND_BACK]  = synth(1, 0.18f, 261.63f, 0.0f, 0.45f);
    a->sfx[SFX_CHECKPOINT] = synth(2, 0.42f, 523.25f, 0.0f, 0.45f);
    a->sfx[SFX_DEATH]      = synth(3, 0.55f, 420.0f, 70.0f, 0.55f);
    a->sfx[SFX_COMPLETE]   = synth(2, 0.7f, 523.25f, 0.0f, 0.5f);
    a->sfx[SFX_UI]         = synth(1, 0.08f, 660.0f, 0.0f, 0.3f);
    a->sfx[SFX_WARN]       = synth(0, 0.30f, 150.0f, 90.0f, 0.4f); /* lava rumble */
}

void audio_shutdown(App *a) {
    if (a->audio_ok) {
        for (int i = 0; i < SFX_COUNT; i++) UnloadSound(a->sfx[i]);
        CloseAudioDevice();
    }
}

void audio_play(App *a, SfxId id) {
    if (!a->audio_ok || a->muted || id < 0 || id >= SFX_COUNT) return;
    SetSoundPitch(a->sfx[id], 1.0f);
    PlaySound(a->sfx[id]);
}

/* Same, but transposed — used to raise the landing tone as a combo builds. */
void audio_play_pitched(App *a, SfxId id, float pitch) {
    if (!a->audio_ok || a->muted || id < 0 || id >= SFX_COUNT) return;
    if (pitch < 0.5f) pitch = 0.5f;
    if (pitch > 2.5f) pitch = 2.5f;
    SetSoundPitch(a->sfx[id], pitch);
    PlaySound(a->sfx[id]);
}
