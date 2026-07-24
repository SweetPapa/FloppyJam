/* music_preview — render the procedural soundtrack to a WAV so it can be
 * auditioned (and sanity-checked) without launching the game.
 *
 *   cc -O2 -I src tools/music_preview.c src/music.c -lm -o music_preview
 *   ./music_preview 3 40 out.wav      # theme seed 3, 40 seconds
 */
#include "music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SR 44100

static void put32(FILE *f, unsigned int v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, unsigned short v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv) {
    int seed = argc > 1 ? atoi(argv[1]) : 1;
    double secs = argc > 2 ? atof(argv[2]) : 30.0;
    const char *path = argc > 3 ? argv[3] : "music_preview.wav";
    /* optional: cross-fade into another theme halfway through */
    int switch_seed = argc > 4 ? atoi(argv[4]) : 0;

    int frames = (int)(SR * secs);
    short *buf = malloc(sizeof(short) * 2 * frames);
    if (!buf) return 1;

    music_init(seed, SR);
    char desc[160];
    music_describe(desc, sizeof desc);
    printf("theme: %s\n", desc);
    /* render in chunks, like the audio callback does */
    int done = 0, switched = 0;
    while (done < frames) {
        int n = frames - done; if (n > 1024) n = 1024;
        /* sweep tension across the render so both moods are audible */
        music_set_intensity((float)done / (float)frames);
        if (switch_seed && !switched && done >= frames / 2) {
            music_set_theme(switch_seed);
            switched = 1;
            char d2[160];
            music_describe(d2, sizeof d2);
            printf("cross-fade at %.1fs -> %s\n", (double)done / SR, d2);
        }
        music_render(buf + done * 2, n);
        done += n;
    }

    /* stats: prove it is neither silent nor clipping */
    double sum = 0; int peak = 0, clipped = 0;
    for (int i = 0; i < frames * 2; i++) {
        int v = buf[i]; if (v < 0) v = -v;
        sum += (double)buf[i] * buf[i];
        if (v > peak) peak = v;
        if (v >= 32700) clipped++;
    }
    double rms = sqrt(sum / (frames * 2.0));
    printf("seed=%d  %.1fs  rms=%.0f (%.1f%% FS)  peak=%d (%.1f%% FS)  clipped=%d\n",
           seed, secs, rms, rms / 327.68, peak, peak / 327.68, clipped);

    /* biggest sample-to-sample step: a hard cut or a click shows up here */
    int max_jump = 0;
    for (int i = 2; i < frames * 2; i++) {
        int d = buf[i] - buf[i - 2]; /* same channel */
        if (d < 0) d = -d;
        if (d > max_jump) max_jump = d;
    }
    printf("max sample step = %d (%.1f%% FS)\n", max_jump, max_jump / 327.68);

    if (switch_seed) {
        /* windowed loudness across the transition: an equal-power cross-fade
         * should hold roughly steady, with no gap and no pile-up */
        printf("loudness through the transition (0.25s windows):\n  ");
        int w = SR / 4;
        int start = frames / 2 - 3 * w, end = frames / 2 + 8 * w;
        if (start < 0) start = 0;
        if (end > frames) end = frames;
        for (int s = start; s + w <= end; s += w) {
            double ws = 0;
            for (int i = s * 2; i < (s + w) * 2; i++) ws += (double)buf[i] * buf[i];
            double wr = sqrt(ws / (w * 2.0));
            printf("%4.1f ", wr / 327.68);
        }
        printf("(%% FS)\n");
    }

    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); return 1; }
    unsigned int data_bytes = (unsigned int)(frames * 2 * 2);
    fwrite("RIFF", 1, 4, f); put32(f, 36 + data_bytes); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); put32(f, 16); put16(f, 1); put16(f, 2);
    put32(f, SR); put32(f, SR * 2 * 2); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, data_bytes);
    fwrite(buf, 1, data_bytes, f);
    fclose(f);
    printf("wrote %s\n", path);
    free(buf);
    return 0;
}
