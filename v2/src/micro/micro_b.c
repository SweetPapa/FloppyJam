/* Microgames 6-10: SNIPE, DOUBLE, STAIRS, RAPID, ECHO */
#include "micro_common.h"

/* ------------------------------------------------------------------ */
/* 6. SNIPE — precision showpiece. Only beat 4. Nothing else.          */
/* ------------------------------------------------------------------ */
static void snipe_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    for (int b = 0; b < PHRASE_BARS; b++) {
        BarPattern bp;
        mc_plain(g, ctx, b, &bp);
        Note *n = chart_add(c, b, 12, LANE_SPACE, NOTE_TAP, 0);
        if (n) n->midi = (int8_t)bp.chord_notes[bp.chord_count - 1];
    }
}
static void snipe_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{ mc_update_standard(m, in, clk, 1); }

static void snipe_draw(MicroState *m)
{
    const Palette *p = pal();
    mc_draw_standard(m, 1, LANES_SPACE);
    /* crosshair: makes the "only beat 4" read from across the room */
    float cx = pf_center_x(), cy = pf_center_y() - 40.0f;
    float beat = m->g->clk.step % STEPS_PER_BAR;
    int on_four = beat >= 11 && beat <= 13;
    Color c = on_four ? p->hot : p->dim;
    ring(cx, cy, 90.0f, 4.0f, c);
    DrawRectangle((int)cx - 130, (int)cy - 2, 60, 4, c);
    DrawRectangle((int)cx + 70, (int)cy - 2, 60, 4, c);
    DrawRectangle((int)cx - 2, (int)cy - 130, 4, 60, c);
    DrawRectangle((int)cx - 2, (int)cy + 70, 4, 60, c);
    gtext_center("4", (int)cx, (int)cy - 24, 6, c);
}
static bool snipe_judge(const MicroState *m)
{
    return m->stats.miss == 0 && m->extra == 0 && m->stats.judged >= 4;
}
const Microgame MG_SNIPE = {
    "SNIPE", "ONLY BEAT 4", KEYMASK_SPACE, 5, 1,
    snipe_build, snipe_update, snipe_draw, snipe_judge
};

/* ------------------------------------------------------------------ */
/* 7. DOUBLE — both hands, on the chord change.                        */
/* ------------------------------------------------------------------ */
#define DOUBLE_TOL_MS 30.0f

static void double_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    for (int b = 0; b < PHRASE_BARS; b++) {
        BarPattern bp;
        mc_plain(g, ctx, b, &bp);
        for (int s = 0; s < STEPS_PER_BAR; s++) {
            if (!bp.chord_change[s]) continue;
            Note *n = chart_add(c, b, s, LANE_F, NOTE_TAP, 0);
            if (n) n->midi = (int8_t)bp.chord_notes[0];
        }
        if (ctx->dense) {
            Note *n = chart_add(c, b, 8, LANE_F, NOTE_TAP, 0);
            if (n) n->midi = (int8_t)bp.chord_notes[bp.chord_count - 1];
        }
    }
}

static void double_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{
    mc_expire(m, clk);
    float ws = run_window_scale();
    double tol = ms_to_samples(DOUBLE_TOL_MS);

    for (int i = 0; i < in->count; i++) {
        const InputEvent *e = &in->e[i];
        if (!e->down) continue;
        if (e->lane == LANE_F) { m->ux[0] = e->t; m->ix[0] = 1; }
        else if (e->lane == LANE_J) { m->ux[1] = e->t; m->ix[1] = 1; }
        else continue;

        if (!(m->ix[0] && m->ix[1])) continue;
        double d = (double)m->ux[0] - (double)m->ux[1];
        if (fabs(d) > tol) continue;           /* not a chord yet — keep waiting */
        uint64_t t = (m->ux[0] > m->ux[1]) ? m->ux[0] : m->ux[1];
        m->ix[0] = m->ix[1] = 0;

        int idx = chart_find_hit(&m->chart, LANE_F, t, ws);
        if (idx < 0) {
            m->extra++;
            stats_add(&m->stats, J_MISS);
            micro_feedback(m, J_MISS, -1, pf_center_x(), pf_center_y());
            continue;
        }
        Note *n = &m->chart.n[idx];
        double off = (double)t - (double)n->t;
        Judgment j = chart_judge_offset(off, ws);
        n->judged = (uint8_t)j;
        n->off_ms = samples_to_ms(off);
        stats_add(&m->stats, j);
        micro_feedback(m, j, idx, pf_center_x(), pf_center_y());
        micro_flash_lane(m, LANE_F);
        micro_flash_lane(m, LANE_J);
    }
}

static void double_draw(MicroState *m)
{
    const Palette *p = pal();
    uint64_t now = m->g->clk.sample_clock;
    mc_draw_standard(m, 2, LANES_FJ);
    /* mirror every note into the right lane so the "both" reads instantly */
    for (int i = 0; i < m->chart.count; i++) {
        Note *n = &m->chart.n[i];
        if (n->kind == NOTE_MARK) continue;
        if (!pf_note_visible(m, n, now)) continue;
        pf_draw_note(m, n, now, 1, 2);
    }
    gtext_center("BOTH", (int)pf_center_x(), (int)pf_top() + 30, 5, p->accent);
}
const Microgame MG_DOUBLE = {
    "DOUBLE", "BOTH ON THE ACCENT", KEYMASK_FJ, 6, 1,
    double_build, double_update, double_draw, mc_judge_default
};

/* ------------------------------------------------------------------ */
/* 8. STAIRS — you play what you hear.                                 */
/* ------------------------------------------------------------------ */
static void stairs_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    for (int b = 0; b < PHRASE_BARS; b++) {
        BarPattern bp;
        mc_bar(g, ctx, b, FLAVOR_ARP_UP, &bp);
        int k = 0;
        for (int s = 0; s < STEPS_PER_BAR; s++) {
            if (!bp.pluck[s]) continue;
            int lane = LANES_FGHJ[k & 3];
            Note *n = chart_add(c, b, s, lane, NOTE_TAP, 0);
            if (n) n->midi = bp.pluck_note[s];
            k++;
        }
    }
}
static void fghj_draw(MicroState *m) { mc_draw_standard(m, 4, LANES_FGHJ); }
static void strict_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{ mc_update_standard(m, in, clk, 1); }

const Microgame MG_STAIRS = {
    "STAIRS", "RUN UP THE KEYS", KEYMASK_FGHJ, 7, 1,
    stairs_build, strict_update, fghj_draw, mc_judge_default
};

/* ------------------------------------------------------------------ */
/* 9. RAPID — flailing looks hilarious.                                */
/* ------------------------------------------------------------------ */
#define RAPID_SLOTS 32
#define RAPID_TARGET 0.80f

static void rapid_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    BarPattern bp;
    mc_plain(g, ctx, 0, &bp);
    mc_bar(g, ctx, 1, FLAVOR_SNARE_ROLL, &bp);
    mc_plain(g, ctx, 2, &bp);
    mc_bar(g, ctx, 3, FLAVOR_SNARE_ROLL, &bp);
    chart_add(c, 1, 0, LANE_F, NOTE_MARK, 0);
    chart_add(c, 3, 0, LANE_F, NOTE_MARK, 0);
}

static int rapid_slot_for(MicroState *m, uint64_t t)
{
    for (int b = 1; b <= 3; b += 2) {
        if (!m->chart.bar_known[b] || !m->chart.bar_known[b + 1]) continue;
        uint64_t a = m->chart.bar_start[b], z = m->chart.bar_start[b + 1];
        if (t < a || t >= z) continue;
        int step = (int)(((double)(t - a) / (double)(z - a)) * STEPS_PER_BAR);
        step = g_clampi(step, 0, STEPS_PER_BAR - 1);
        return ((b == 1) ? 0 : STEPS_PER_BAR) + step;
    }
    return -1;
}

static void rapid_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{
    (void)clk;
    for (int i = 0; i < in->count; i++) {
        const InputEvent *e = &in->e[i];
        if (!e->down) continue;
        if (e->lane != LANE_F && e->lane != LANE_J) continue;
        int slot = rapid_slot_for(m, e->t);
        if (slot < 0) { m->extra++; continue; }
        int lane_bit = (e->lane == LANE_F) ? 1 : 2;
        if (m->ix[1] == lane_bit) {           /* same hand twice — no credit */
            micro_flash_lane(m, e->lane);
            continue;
        }
        m->ix[1] = lane_bit;
        if (m->slot[slot]) continue;
        m->slot[slot] = 1;
        m->ix[0]++;
        stats_add(&m->stats, J_GOOD);
        micro_flash_lane(m, e->lane);
        audio_sfx(SFX_GOOD, 0);
        particles_burst(pf_lane_x(e->lane == LANE_F ? 0 : 1, 2), pf_bottom() - 90.0f,
                        3, pal()->accent, 200.0f, 0.35f);
    }
}

static void rapid_draw(MicroState *m)
{
    const Palette *p = pal();
    uint64_t now = m->g->clk.sample_clock;
    mc_draw_standard(m, 2, LANES_FJ);

    int rolling = 0;
    for (int b = 1; b <= 3; b += 2)
        if (m->chart.bar_known[b] && m->chart.bar_known[b + 1]
            && now >= m->chart.bar_start[b] && now < m->chart.bar_start[b + 1]) rolling = 1;

    float cx = pf_center_x(), cy = pf_center_y();
    float fill = (float)m->ix[0] / (float)RAPID_SLOTS;
    gtext_center(rolling ? "MASH!" : "GET READY", (int)cx, (int)cy - 130,
                 rolling ? 9 : 5, rolling ? p->hot : p->dim);
    draw_bar_meter(cx - 320, cy - 20, 640, 34, fill, cmix(p->main, p->hot, fill), p->dim);
    /* the 80% line the crowd can see */
    DrawRectangle((int)(cx - 320 + 640 * RAPID_TARGET), (int)cy - 30, 4, 54, p->accent);
    char buf[32];
    snprintf(buf, sizeof buf, "%d/%d", m->ix[0], RAPID_SLOTS);
    gtext_center(buf, (int)cx, (int)cy + 40, 4, p->main);
}

static bool rapid_judge(const MicroState *m)
{
    return (float)m->ix[0] >= RAPID_TARGET * (float)RAPID_SLOTS;
}
const Microgame MG_RAPID = {
    "RAPID", "MASH THE ROLL", KEYMASK_FJ, 8, 2,
    rapid_build, rapid_update, rapid_draw, rapid_judge
};

/* ------------------------------------------------------------------ */
/* 10. ECHO — Simon-says on the beat, always in key.                   */
/* ------------------------------------------------------------------ */
static void echo_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    BarPattern call;
    mc_bar(g, ctx, 0, FLAVOR_BELL_MOTIF, &call);
    BarPattern tmp;
    mc_bar(g, ctx, 1, FLAVOR_BELL_MOTIF, &tmp);
    mc_plain(g, ctx, 2, &tmp);
    mc_plain(g, ctx, 3, &tmp);

    int notes[4], n = 0;
    for (int s = 0; s < STEPS_PER_BAR && n < 4; s++)
        if (call.bell_note[s] >= 0) notes[n++] = call.bell_note[s];
    while (n < 4) { notes[n] = notes[n > 0 ? n - 1 : 0]; n++; }

    /* the call, as markers, so the player can watch the lanes light up */
    for (int i = 0; i < 4; i++) {
        for (int b = 0; b < 2; b++) {
            Note *mk = chart_add(c, b, i * 4, LANES_FGHJ[mc_note_lane(notes[i], 4)],
                                 NOTE_MARK, 0);
            if (mk) { mk->midi = (int8_t)notes[i]; mk->tag = (uint8_t)i; }
        }
    }
    /* the answer */
    for (int b = 2; b < 4; b++)
        for (int i = 0; i < 4; i++) {
            Note *nn = chart_add(c, b, i * 4, LANES_FGHJ[mc_note_lane(notes[i], 4)],
                                 NOTE_TAP, 0);
            if (nn) { nn->midi = (int8_t)notes[i]; nn->tag = (uint8_t)i; }
        }
}

static void echo_draw(MicroState *m)
{
    const Palette *p = pal();
    uint64_t now = m->g->clk.sample_clock;
    mc_draw_standard(m, 4, LANES_FGHJ);

    int answering = m->chart.bar_known[2] && now >= m->chart.bar_start[2];
    gtext_center(answering ? "YOUR TURN" : "LISTEN",
                 (int)pf_center_x(), (int)pf_top() + 24, 5,
                 answering ? p->hot : p->accent);

    /* light the lane while the bell is speaking */
    if (!answering) {
        for (int i = 0; i < m->chart.count; i++) {
            Note *n = &m->chart.n[i];
            if (n->kind != NOTE_MARK || !n->resolved) continue;
            double d = (double)now - (double)n->t;
            if (d < 0 || d > SAMPLE_RATE * 0.35) continue;
            float u = 1.0f - (float)(d / (SAMPLE_RATE * 0.35));
            int li = 0;
            for (int k = 0; k < 4; k++) if (LANES_FGHJ[k] == n->lane) li = k;
            Color c = p->hot; c.a = (unsigned char)(u * 220.0f);
            DrawRectangle((int)(pf_lane_x(li, 4) - 44), (int)pf_top(), 88,
                          (int)(pf_bottom() - pf_top()), c);
        }
    }
}
const Microgame MG_ECHO = {
    "ECHO", "COPY THAT", KEYMASK_FGHJ, 9, 2,
    echo_build, strict_update, echo_draw, mc_judge_default
};
