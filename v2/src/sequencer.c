#include "sequencer.h"

/* ---- scales --------------------------------------------------------- */
static const int8_t SCALES[4][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },  /* major */
    { 0, 2, 3, 5, 7, 8, 10 },  /* minor */
    { 0, 2, 3, 5, 7, 9, 10 },  /* dorian */
    { 0, 2, 4, 5, 7, 9, 10 }   /* mixolydian */
};
static const char *SCALE_NAMES[4] = { "MAJOR", "MINOR", "DORIAN", "MIXO" };

/* ---- the 7 proven progressions (§8.2), as scale degrees -------------- */
/* alt: 0 none, 1 minorize 3rd, 2 sus4, 3 add9 */
typedef struct { int8_t deg[4]; int8_t alt[4]; } Progression;
static const Progression PROGS[7] = {
    { { 0, 4, 5, 3 }, { 0, 0, 0, 0 } },  /* I  - V  - vi - IV */
    { { 0, 2, 3, 3 }, { 0, 0, 0, 1 } },  /* I  - iii- IV - iv */
    { { 0, 5, 1, 4 }, { 0, 0, 0, 0 } },  /* I  - vi - ii - V  */
    { { 0, 3, 0, 4 }, { 0, 0, 0, 2 } },  /* I  - IV - I  - Vsus */
    { { 0, 5, 2, 6 }, { 0, 0, 0, 0 } },  /* i  - VI - III- VII */
    { { 0, 3, 5, 4 }, { 0, 3, 0, 0 } },  /* I  - IV - vi - V  */
    { { 0, 0, 5, 5 }, { 0, 3, 0, 3 } }   /* two-chord modal vamp */
};

/* motif contours (§8.2) — 4-note shapes in scale steps, relative */
static const int8_t MOTIF_SHAPES[4][4] = {
    { 0, 2, 1, 4 },   /* rising question */
    { 4, 2, 3, 0 },   /* falling answer  */
    { 0, 1, 2, 3 },   /* stairs          */
    { 2, 0, 4, 1 }    /* zig             */
};

void genome_init(Genome *g, uint32_t seed_code)
{
    Rng r = rng_stream(seed_code, RNG_STREAM_MUSIC);
    memset(g, 0, sizeof *g);
    g->seed_code    = seed_code;
    g->rng_state    = rng_u32(&r);
    g->root         = (uint8_t)rng_range(&r, 0, 11);
    g->scale        = (uint8_t)rng_range(&r, 0, 3);
    g->chord_family = (uint8_t)rng_range(&r, 0, 6);
    g->euclid_n     = (uint8_t)(rng_range(&r, 0, 1) ? 8 : 16);
    /* 3-6 kicks per bar: dense enough to groove, sparse enough that
     * "TAP THE KICK" stays a readable chart rather than a 16th-note wall. */
    g->euclid_k     = (uint8_t)(g->euclid_n == 8 ? rng_range(&r, 3, 5) : rng_range(&r, 4, 6));
    g->bass_pattern = (uint8_t)rng_range(&r, 0, 3);
    g->motif_shape  = (uint8_t)rng_range(&r, 0, 3);
    g->palette      = (uint8_t)rng_range(&r, 0, 3);
    g->bpm_base     = (uint16_t)rng_range(&r, 104, 124);
    for (int i = 0; i < 4; i++) {
        int8_t base = MOTIF_SHAPES[g->motif_shape][i];
        g->motif[i] = (int8_t)(base + (rng_range(&r, 0, 5) == 0 ? 1 : 0));
    }
}

const char *genome_scale_name(const Genome *g) { return SCALE_NAMES[g->scale & 3]; }

int genome_scale_note(const Genome *g, int degree, int octave)
{
    int oct = octave;
    while (degree < 0) { degree += 7; oct--; }
    while (degree > 6) { degree -= 7; oct++; }
    return 12 * oct + g->root + SCALES[g->scale & 3][degree];
}

int genome_chord(const Genome *g, int bar_index, int out_notes[4], int octave)
{
    const Progression *p = &PROGS[g->chord_family % 7];
    int slot = ((bar_index % 4) + 4) % 4;
    int d = p->deg[slot];
    int alt = p->alt[slot];
    int n = 0;
    out_notes[n++] = genome_scale_note(g, d, octave);
    if (alt == 2) out_notes[n++] = genome_scale_note(g, d + 3, octave);   /* sus4 */
    else {
        int third = genome_scale_note(g, d + 2, octave);
        if (alt == 1) third -= 1;                                         /* minorize */
        out_notes[n++] = third;
    }
    out_notes[n++] = genome_scale_note(g, d + 4, octave);
    if (alt == 3) out_notes[n++] = genome_scale_note(g, d + 8, octave);   /* add9 */
    return n;
}

/* ---- euclidean rhythm ------------------------------------------------ */
static void euclid(int k, int n, uint8_t *out)
{
    /* simple, stable bucket algorithm — even spread of k pulses over n slots */
    int bucket = 0;
    for (int i = 0; i < n; i++) {
        bucket += k;
        if (bucket >= n) { bucket -= n; out[i] = 1; }
        else out[i] = 0;
    }
}

/* ---- the pure pattern generator -------------------------------------- */
void seq_bar_pattern(const Genome *g, int bar_index, BarFlavor flavor, BarPattern *out)
{
    memset(out, 0, sizeof *out);
    for (int i = 0; i < STEPS_PER_BAR; i++) {
        out->bell_note[i] = -1;
        out->bass_note[i] = -1;
        out->pluck_note[i] = -1;
    }

    if (bar_index < 0) bar_index = 0;

    out->chord_count = genome_chord(g, bar_index, out->chord_notes, 4);
    out->chord_change[0] = 1;

    if (flavor == FLAVOR_REST) { out->is_rest = 1; return; }

    /* deterministic per-bar variation stream */
    Rng r = rng_stream(g->seed_code, RNG_STREAM_MUSIC + 64u + (uint32_t)bar_index);

    int is_fill = (bar_index % 4) == 3;

    /* --- kick: euclidean skeleton ---------------------------------- */
    uint8_t e[16];
    euclid(g->euclid_k, g->euclid_n, e);
    int mul = STEPS_PER_BAR / g->euclid_n;
    for (int i = 0; i < g->euclid_n; i++)
        if (e[i]) out->kick[i * mul] = 1;
    out->kick[0] = 1; /* always land the downbeat: spectators need the anchor */

    /* --- hats: 8ths, subdividing to 16ths on fills ------------------ */
    for (int i = 0; i < STEPS_PER_BAR; i += 2) out->hat[i] = (i % 4 == 0) ? 2 : 1;
    for (int i = 1; i < STEPS_PER_BAR; i += 2)
        if (is_fill || rng_range(&r, 0, 3) == 0) out->hat[i] = 1;

    /* --- snare backbeat + fills ------------------------------------- */
    out->snare[4] = 1;
    out->snare[12] = 1;
    if (flavor == FLAVOR_SNARE_ROLL) {
        for (int i = 0; i < STEPS_PER_BAR; i++) out->snare[i] = 1;
    } else if (is_fill) {
        for (int i = 12; i < STEPS_PER_BAR; i++) out->snare[i] = 1;
    }

    /* --- bass ------------------------------------------------------- */
    int root = out->chord_notes[0] - 24;
    int fifth = out->chord_notes[out->chord_count - 1] - 24;
    switch (g->bass_pattern & 3) {
    case 0: /* root on the kick */
        for (int i = 0; i < STEPS_PER_BAR; i++)
            if (out->kick[i]) { out->bass[i] = 1; out->bass_note[i] = (int8_t)root; }
        break;
    case 1: /* driving 8ths */
        for (int i = 0; i < STEPS_PER_BAR; i += 2) {
            out->bass[i] = 1;
            out->bass_note[i] = (int8_t)((i == 8) ? fifth : root);
        }
        break;
    case 2: /* dotted pulse */
        out->bass[0] = out->bass[6] = out->bass[10] = 1;
        out->bass_note[0] = (int8_t)root;
        out->bass_note[6] = (int8_t)root;
        out->bass_note[10] = (int8_t)fifth;
        break;
    default: /* root - octave walk */
        out->bass[0] = out->bass[4] = out->bass[8] = out->bass[12] = 1;
        out->bass_note[0] = (int8_t)root;
        out->bass_note[4] = (int8_t)root;
        out->bass_note[8] = (int8_t)fifth;
        out->bass_note[12] = (int8_t)(root + 12);
        break;
    }

    /* --- pluck: arpeggio ------------------------------------------- */
    if (flavor == FLAVOR_ARP_UP) {
        /* ascending 4-note arpeggio on the beats — STAIRS reads this */
        for (int b = 0; b < 4; b++) {
            int step = b * 4;
            int note = (b < out->chord_count)
                     ? out->chord_notes[b]
                     : out->chord_notes[0] + 12;
            out->pluck[step] = 1;
            out->pluck_note[step] = (int8_t)note;
        }
    } else if (flavor != FLAVOR_BREAKDOWN) {
        for (int i = 0; i < STEPS_PER_BAR; i += 2) {
            if (rng_range(&r, 0, 2) == 0) {
                out->pluck[i] = 1;
                out->pluck_note[i] =
                    (int8_t)(out->chord_notes[rng_range(&r, 0, out->chord_count - 1)] + 12);
            }
        }
    }

    /* --- bell motif -------------------------------------------------- */
    int bell_bar = (flavor == FLAVOR_BELL_MOTIF) || ((bar_index % 4) == 2);
    if (bell_bar && flavor != FLAVOR_BREAKDOWN) {
        int slot = ((bar_index % 4) + 4) % 4;
        int base = PROGS[g->chord_family % 7].deg[slot];
        for (int i = 0; i < 4; i++) {
            int step = i * 4;
            out->bell_note[step] = (int8_t)genome_scale_note(g, base + g->motif[i], 6);
        }
    }

    if (flavor == FLAVOR_BREAKDOWN) {
        /* drums only — the groove never stops during a card pick */
        for (int i = 0; i < STEPS_PER_BAR; i++) {
            out->bass[i] = 0; out->pluck[i] = 0; out->bell_note[i] = -1;
        }
    }
}
