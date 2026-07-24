/* music.h — procedural soundtrack generator (pure C, no raylib).
 * See music.c for the generative formula. */
#ifndef MUSIC_H
#define MUSIC_H

/* Build a theme from a seed (use the level id) at the given sample rate. */
void music_init(int seed, int sample_rate);

/* Switch theme; the change lands on the next bar so it never lurches. */
void music_set_theme(int seed);

/* 0..1 tension control — opens the filter and thickens the percussion. */
void music_set_intensity(float v);

/* Render interleaved stereo 16-bit PCM. Safe to call from an audio thread. */
void music_render(short *out, int frames);

/* Human-readable summary of the active theme (key, progression, tempo). */
void music_describe(char *buf, int n);

#endif /* MUSIC_H */
