/* Microgames 11-15: BLIND, SWAP, MEASURE, CALLBACK, FINALE */
#include "micro_common.h"

/* ------------------------------------------------------------------ */
/* 11. BLIND — drums out, keep time. Spectators tap the table.         */
/* ------------------------------------------------------------------ */
static void blind_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    for (int b = 0; b < PHRASE_BARS; b++) {
        BarPattern bp;
        mc_plain(g, ctx, b, &bp);
        for (int s = 0; s < STEPS_PER_BAR; s++) {
            if (!bp.kick[s]) continue;
            if (b == PHRASE_BARS - 1 && s >= 15) continue;
            Note *n = chart_add(c, b, s, LANE_SPACE, NOTE_TAP, 0);
            if (n) n->midi = (int8_t)bp.chord_notes[0];
        }
    }
    /* drum stems to zero for the phrase, restored on the bar after it */
    static const int drums[3] = { STEM_KICK, STEM_HAT, STEM_SNARE };
    for (int i = 0; i < 3; i++) {
        audio_stem_gain(drums[i], 0.0f, ctx->base_bar);
        audio_stem_gain(drums[i], heat_stem_active(ctx->tier, drums[i]) ? 1.0f : 0.0f,
                        ctx->base_bar + PHRASE_BARS);
    }
}

static void blind_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{
    m->fx[3] = 1.3f;                  /* be kind (§5.2) */
    mc_update_standard(m, in, clk, 0);
}

static void blind_draw(MicroState *m)
{
    const Palette *p = pal();
    mc_draw_standard(m, 1, LANES_SPACE);
    gtext_center("NO DRUMS", (int)pf_center_x(), (int)pf_center_y() - 120, 6, p->dim);
    gtext_center("KEEP TIME", (int)pf_center_x(), (int)pf_center_y() - 60, 6, p->main);
}
const Microgame MG_BLIND = {
    "BLIND", "DRUMS OUT - KEEP TIME", KEYMASK_SPACE, 10, 2,
    blind_build, blind_update, blind_draw, mc_judge_default
};

/* ------------------------------------------------------------------ */
/* 12. SWAP — mirror-brain comedy.                                     */
/* ------------------------------------------------------------------ */
static void swap_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    int k = 0;
    for (int b = 0; b < PHRASE_BARS; b++) {
        BarPattern bp;
        mc_plain(g, ctx, b, &bp);
        for (int s = 0; s < STEPS_PER_BAR; s++) {
            if (!bp.hat[s]) continue;
            if (!ctx->dense && (s & 1)) continue;
            if (b == PHRASE_BARS - 1 && s >= 15) continue;
            Note *n = chart_add(c, b, s, LANES_FGHJ[k & 3], NOTE_TAP, 0);
            if (n) n->midi = (int8_t)bp.chord_notes[k % bp.chord_count];
            k++;
        }
    }
}

static void swap_ensure_perm(MicroState *m)
{
    if (m->ix[3]) return;
    m->ix[3] = 1;
    int perm[4] = { 0, 1, 2, 3 };
    Rng r = rng_stream(m->ctx.seed_code, RNG_STREAM_SCHEDULER + 200u
                                       + (uint32_t)m->ctx.phrase_index);
    do { rng_shuffle(&r, perm, 4); } while (perm[0] == 0 && perm[1] == 1);
    for (int i = 0; i < 4; i++) m->key_map[LANES_FGHJ[i]] = (int8_t)LANES_FGHJ[perm[i]];
    m->key_map[LANE_SPACE] = LANE_SPACE;
}

static void swap_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{
    swap_ensure_perm(m);
    InputEvents tr = *in;
    for (int i = 0; i < tr.count; i++)
        tr.e[i].lane = (uint8_t)m->key_map[tr.e[i].lane % LANE_COUNT];
    mc_update_standard(m, &tr, clk, 1);
}

static void swap_draw(MicroState *m)
{
    const Palette *p = pal();
    swap_ensure_perm(m);
    uint64_t now = m->g->clk.sample_clock;

    /* lanes drawn with their REMAPPED keycap, huge, so the joke is legible */
    for (int i = 0; i < 4; i++) {
        float x = pf_lane_x(i, 4);
        Color c = pal_mix(p->dim, 0.0f); c.a = 150;
        DrawRectangle((int)(x - 44), (int)pf_top(), 88, (int)(pf_bottom() - pf_top()), c);
        int phys = 0;
        for (int k = 0; k < 4; k++)
            if (m->key_map[LANES_FGHJ[k]] == LANES_FGHJ[i]) phys = LANES_FGHJ[k];
        DrawRectangle((int)(x - 52), (int)(pf_bottom() - 62), 104, 5, p->hot);
        gkeycap(LANE_LABEL[phys], (int)x - 20, (int)pf_bottom() - 46, 4, p->hot,
                (m->g->in.down_mask & (1u << phys)) != 0);
        float flash = m->lane_flash[LANES_FGHJ[i]];
        if (flash > 0.01f) {
            Color fc = p->accent; fc.a = (unsigned char)(flash * 150.0f);
            DrawRectangle((int)(x - 44), (int)pf_top(), 88, (int)(pf_bottom() - pf_top()), fc);
        }
    }
    for (int i = 0; i < m->chart.count; i++) {
        Note *n = &m->chart.n[i];
        if (n->kind == NOTE_MARK || !pf_note_visible(m, n, now)) continue;
        int li = 0;
        for (int k = 0; k < 4; k++) if (LANES_FGHJ[k] == n->lane) li = k;
        pf_draw_note(m, n, now, li, 4);
    }
    gtext_center("KEYS REMAPPED!", (int)pf_center_x(), (int)pf_top() + 20, 6, p->hot);
}
const Microgame MG_SWAP = {
    "SWAP", "KEYS REMAPPED!", KEYMASK_FGHJ, 11, 3,
    swap_build, swap_update, swap_draw, mc_judge_default
};

/* ------------------------------------------------------------------ */
/* 13. MEASURE — pure feel. Hold exactly four beats.                   */
/* ------------------------------------------------------------------ */
#define MEASURE_TOL_MS 120.0f

static void measure_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    BarPattern bp;
    for (int b = 0; b < PHRASE_BARS; b++) mc_plain(g, ctx, b, &bp);
    chart_add(c, 0, 0, LANE_SPACE, NOTE_MARK, 0);
}

static void measure_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{
    /* ix[0]: 0 = waiting, 1 = holding, 2 = done */
    for (int i = 0; i < in->count; i++) {
        const InputEvent *e = &in->e[i];
        if (e->lane != LANE_SPACE) continue;
        if (e->down && m->ix[0] == 0) {
            m->ix[0] = 1;
            m->ux[0] = e->t;
            audio_sfx(SFX_UI, 0);
        } else if (!e->down && m->ix[0] == 1) {
            m->ix[0] = 2;
            double target = (double)clk->samples_per_beat * 4.0;
            double held = (double)e->t - (double)m->ux[0];
            double off_ms = (held - target) * 1000.0 / SAMPLE_RATE;
            m->fx[0] = (float)off_ms;
            Judgment j;
            if (fabs(off_ms) <= MEASURE_TOL_MS * 0.4) j = J_PERFECT;
            else if (fabs(off_ms) <= MEASURE_TOL_MS) j = J_GOOD;
            else j = J_MISS;
            for (int k = 0; k < 4; k++) stats_add(&m->stats, j);
            micro_feedback(m, j, -1, pf_center_x(), pf_center_y());
            char buf[24];
            snprintf(buf, sizeof buf, "%+d MS", (int)off_ms);
            pop_text(buf, pf_center_x(), pf_center_y() + 90, 4, pal()->main, 1.4f);
        }
    }
    /* never even tried: that's a miss */
    if (m->ix[0] == 0 && m->chart.bar_known[3] && clk->sample_clock > m->chart.bar_start[3]) {
        m->ix[0] = 3;
        for (int k = 0; k < 4; k++) stats_add(&m->stats, J_MISS);
        micro_feedback(m, J_MISS, -1, pf_center_x(), pf_center_y());
    }
}

static void measure_draw(MicroState *m)
{
    const Palette *p = pal();
    float cx = pf_center_x(), cy = pf_center_y();
    double target = (double)m->g->clk.samples_per_beat * 4.0;
    float u = 0.0f;
    if (m->ix[0] == 1 && target > 0)
        u = (float)(((double)m->g->clk.sample_clock - (double)m->ux[0]) / target);

    gtext_center("HOLD EXACTLY 4 BEATS", (int)cx, (int)pf_top() + 24, 5, p->accent);
    float w = 800.0f, h = 60.0f;
    Color fill = (u > 1.15f) ? p->hot : cmix(p->main, p->accent, g_clampf(u, 0, 1));
    draw_bar_meter(cx - w * 0.5f, cy - h * 0.5f, w, h, g_clampf(u, 0.0f, 1.25f) / 1.25f,
                   fill, p->dim);
    /* the target line */
    float tx = cx - w * 0.5f + w * (1.0f / 1.25f);
    DrawRectangle((int)tx - 3, (int)(cy - h * 0.5f) - 16, 6, (int)h + 32, p->hot);
    for (int b = 1; b <= 4; b++) {
        float bx = cx - w * 0.5f + w * ((float)b / 4.0f) / 1.25f;
        DrawRectangle((int)bx - 1, (int)(cy - h * 0.5f), 2, (int)h, p->bg);
    }
    gkeycap("SPC", (int)cx - 20, (int)cy + 110, 4, p->main,
            (m->g->in.down_mask & KEYMASK_SPACE) != 0);
    if (m->ix[0] >= 2) {
        char buf[32];
        snprintf(buf, sizeof buf, "%+d MS", (int)m->fx[0]);
        gtext_center(buf, (int)cx, (int)cy - 130, 6,
                     fabsf(m->fx[0]) <= MEASURE_TOL_MS ? p->accent : p->hot);
    }
}
const Microgame MG_MEASURE = {
    "MEASURE", "HOLD EXACTLY 4 BEATS", KEYMASK_SPACE, 12, 3,
    measure_build, measure_update, measure_draw, mc_judge_default
};

/* ------------------------------------------------------------------ */
/* 14. CALLBACK — conversational rhythm. Charming.                     */
/* ------------------------------------------------------------------ */
static void callback_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    BarPattern ask0, ask1, tmp;
    mc_bar(g, ctx, 0, FLAVOR_BELL_MOTIF, &ask0);
    mc_plain(g, ctx, 1, &tmp);
    mc_bar(g, ctx, 2, FLAVOR_BELL_MOTIF, &ask1);
    mc_plain(g, ctx, 3, &tmp);

    const BarPattern *asks[2] = { &ask0, &ask1 };
    for (int q = 0; q < 2; q++) {
        int notes[4], n = 0;
        for (int s = 0; s < STEPS_PER_BAR && n < 4; s++)
            if (asks[q]->bell_note[s] >= 0) notes[n++] = asks[q]->bell_note[s];
        while (n < 4) { notes[n] = notes[n > 0 ? n - 1 : 0]; n++; }
        for (int i = 0; i < 4; i++) {
            Note *mk = chart_add(c, q * 2, i * 4, LANES_FGHJ[mc_note_lane(notes[i], 4)],
                                 NOTE_MARK, 0);
            if (mk) mk->midi = (int8_t)notes[i];
            Note *an = chart_add(c, q * 2 + 1, i * 4,
                                 LANES_FGHJ[mc_note_lane(notes[i], 4)], NOTE_TAP, 0);
            if (an) an->midi = (int8_t)notes[i];
        }
    }
}

static void callback_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{ mc_update_standard(m, in, clk, 1); }

static void callback_draw(MicroState *m)
{
    const Palette *p = pal();
    mc_draw_standard(m, 4, LANES_FGHJ);
    int bar_in_phrase = 0;
    uint64_t now = m->g->clk.sample_clock;
    for (int b = 0; b < PHRASE_BARS; b++)
        if (m->chart.bar_known[b] && now >= m->chart.bar_start[b]) bar_in_phrase = b;
    int answering = (bar_in_phrase % 2) == 1;
    gtext_center(answering ? "ANSWER!" : "THE BELL ASKS...",
                 (int)pf_center_x(), (int)pf_top() + 24, 5,
                 answering ? p->hot : p->accent);
}
const Microgame MG_CALLBACK = {
    "CALLBACK", "ANSWER THE BELL", KEYMASK_FGHJ, 13, 3,
    callback_build, callback_update, callback_draw, mc_judge_default
};

/* ------------------------------------------------------------------ */
/* 15. FINALE — the wall. Reaching it is the flex.                     */
/* ------------------------------------------------------------------ */
static void finale_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    int k = 0;
    for (int b = 0; b < PHRASE_BARS; b++) {
        BarPattern bp;
        mc_plain(g, ctx, b, &bp);
        for (int s = 0; s < STEPS_PER_BAR; s++) {
            if (b == PHRASE_BARS - 1 && s >= 15) continue;
            if (bp.kick[s]) {
                Note *n = chart_add(c, b, s, LANE_SPACE, NOTE_TAP, 0);
                if (n) n->midi = (int8_t)bp.chord_notes[0];
            } else if (bp.chord_change[s]) {
                Note *n = chart_add(c, b, s, LANE_F, NOTE_TAP, 0);
                if (n) n->midi = (int8_t)bp.chord_notes[0];
            } else if (bp.hat[s] && (ctx->dense || (s % 4) == 2)) {
                Note *n = chart_add(c, b, s, LANES_FGHJ[1 + (k++ % 3)], NOTE_TAP, 0);
                if (n) n->midi = (int8_t)bp.chord_notes[k % bp.chord_count];
            }
        }
    }
}

static void finale_draw(MicroState *m)
{
    const Palette *p = pal();
    mc_draw_standard(m, 5, LANES_ALL);
    gtext_center("EVERYTHING.", (int)pf_center_x(), (int)pf_top() + 16, 7, p->hot);
}

static bool finale_judge(const MicroState *m)
{
    if (m->stats.judged <= 0) return true;
    float r = (float)(m->stats.perfect + m->stats.good) / (float)m->stats.judged;
    return r >= 0.60f;    /* merciful */
}
const Microgame MG_FINALE = {
    "FINALE", "EVERYTHING.", KEYMASK_ALL, 14, 5,
    finale_build, callback_update, finale_draw, finale_judge
};
