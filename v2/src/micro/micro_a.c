/* Microgames 1-5: TAP, UPBEAT, ALTERNATE, HOLD, REST */
#include "micro_common.h"

/* ------------------------------------------------------------------ */
/* 1. TAP — the tutorial-by-osmosis game. Always the first phrase.      */
/* ------------------------------------------------------------------ */
static void tap_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
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
        if (ctx->dense) {
            for (int s = 1; s < STEPS_PER_BAR - 1; s += 2) {
                if (!bp.hat[s]) continue;
                Note *n = chart_add(c, b, s, LANE_SPACE, NOTE_TAP, 0);
                if (n) n->midi = (int8_t)bp.chord_notes[bp.chord_count > 1 ? 1 : 0];
            }
        }
    }
}
static void std_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{ mc_update_standard(m, in, clk, 0); }
static void space_draw(MicroState *m) { mc_draw_standard(m, 1, LANES_SPACE); }


const Microgame MG_TAP = {
    "TAP", "TAP THE KICK", KEYMASK_SPACE, 0, 0,
    tap_build, std_update, space_draw, mc_judge_default
};

/* ------------------------------------------------------------------ */
/* 2. UPBEAT — everyone rushes it.                                     */
/* ------------------------------------------------------------------ */
static void upbeat_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    for (int b = 0; b < PHRASE_BARS; b++) {
        BarPattern bp;
        mc_plain(g, ctx, b, &bp);
        for (int s = 2; s < STEPS_PER_BAR; s += 4) {
            Note *n = chart_add(c, b, s, LANE_SPACE, NOTE_TAP, 0);
            if (n) n->midi = (int8_t)bp.chord_notes[bp.chord_count > 1 ? 1 : 0];
        }
        if (ctx->dense) {
            for (int s = 1; s < STEPS_PER_BAR - 1; s += 4) {
                Note *n = chart_add(c, b, s, LANE_SPACE, NOTE_TAP, 0);
                if (n) n->midi = (int8_t)bp.chord_notes[0];
            }
        }
    }
}
const Microgame MG_UPBEAT = {
    "UPBEAT", "HIT THE AND", KEYMASK_SPACE, 1, 0,
    upbeat_build, std_update, space_draw, mc_judge_default
};

/* ------------------------------------------------------------------ */
/* 3. ALTERNATE — DDR-adjacent stamina, off the hi-hat.                */
/* ------------------------------------------------------------------ */
static void alternate_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    int alt = 0;
    for (int b = 0; b < PHRASE_BARS; b++) {
        BarPattern bp;
        mc_plain(g, ctx, b, &bp);
        for (int s = 0; s < STEPS_PER_BAR; s++) {
            if (!bp.hat[s]) continue;
            if (!ctx->dense && (s & 1)) continue;     /* 8ths unless DENSE */
            if (b == PHRASE_BARS - 1 && s >= 15) continue;
            int lane = (alt++ & 1) ? LANE_J : LANE_F;
            Note *n = chart_add(c, b, s, lane, NOTE_TAP, 0);
            if (n) n->midi = (int8_t)bp.chord_notes[(alt) % bp.chord_count];
        }
    }
}
static void fj_draw(MicroState *m) { mc_draw_standard(m, 2, LANES_FJ); }

const Microgame MG_ALTERNATE = {
    "ALTERNATE", "LEFT RIGHT LEFT", KEYMASK_FJ, 2, 0,
    alternate_build, std_update, fj_draw, mc_judge_default
};

/* ------------------------------------------------------------------ */
/* 4. HOLD — slow-burn tension; the crowd holds its breath.            */
/* ------------------------------------------------------------------ */
static void hold_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    for (int b = 0; b < PHRASE_BARS; b += 2) {
        BarPattern bp;
        mc_plain(g, ctx, b, &bp);
        mc_plain(g, ctx, b + 1, &bp);
        /* press on the "and of 3", release on the next downbeat */
        Note *n = chart_add(c, b, 8, LANE_SPACE, NOTE_HOLD, 8);
        if (n) n->midi = (int8_t)bp.chord_notes[0];
    }
}

static void hold_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{
    mc_expire(m, clk);
    float ws = run_window_scale();
    double good_w = (double)WINDOW_GOOD_MS * ws * SAMPLE_RATE / 1000.0;

    for (int i = 0; i < in->count; i++) {
        const InputEvent *e = &in->e[i];
        if (e->lane != LANE_SPACE) continue;

        if (e->down) {
            int idx = chart_find_hit(&m->chart, LANE_SPACE, e->t, ws);
            if (idx < 0) { m->extra++; continue; }
            Note *n = &m->chart.n[idx];
            n->holding = 1;
            n->tag = (uint8_t)chart_judge_offset((double)e->t - (double)n->t, ws);
            micro_flash_lane(m, LANE_SPACE);
            audio_sfx(SFX_GOOD, 0);
        } else {
            for (int k = 0; k < m->chart.count; k++) {
                Note *n = &m->chart.n[k];
                if (!n->holding || n->judged) continue;
                double off = (double)e->t - (double)n->t_end;
                Judgment j;
                if (off < -good_w) j = J_MISS;            /* let go early */
                else j = chart_judge_offset(off, ws);
                /* the release can't beat the press */
                if (n->tag == J_MISS) j = J_MISS;
                else if (n->tag == J_GOOD && j == J_PERFECT) j = J_GOOD;
                n->judged = (uint8_t)j;
                n->off_ms = samples_to_ms(off);
                n->holding = 0;
                stats_add(&m->stats, j);
                micro_feedback(m, j, k, pf_center_x(), pf_center_y());
                break;
            }
        }
    }
}

static void hold_draw(MicroState *m)
{
    const Palette *p = pal();
    uint64_t now = m->g->clk.sample_clock;
    mc_draw_standard(m, 1, LANES_SPACE);

    for (int i = 0; i < m->chart.count; i++) {
        Note *n = &m->chart.n[i];
        if (!n->resolved || n->judged) continue;
        if (!n->holding) continue;
        double span = (double)n->t_end - (double)n->t;
        float u = span > 0 ? (float)(((double)now - (double)n->t) / span) : 0.0f;
        u = g_clampf(u, 0.0f, 1.0f);
        float r = 40.0f + 160.0f * u;
        Color c = cmix(p->main, p->hot, u);
        ring(pf_center_x(), pf_center_y(), r, 8.0f + 10.0f * u, c);
        if (u > 0.9f)
            gtext_center("RELEASE", (int)pf_center_x(), (int)pf_center_y() - 16, 5, p->hot);
    }
}

const Microgame MG_HOLD = {
    "HOLD", "HOLD... RELEASE!", KEYMASK_SPACE, 3, 0,
    hold_build, hold_update, hold_draw, mc_judge_default
};

/* ------------------------------------------------------------------ */
/* 5. REST — someone always touches it. Peak comedy.                   */
/* ------------------------------------------------------------------ */
static void rest_build(Chart *c, const Genome *g, const PhraseCtx *ctx)
{
    BarPattern bp;
    mc_plain(g, ctx, 0, &bp);
    mc_bar(g, ctx, 1, FLAVOR_REST, &bp);
    mc_bar(g, ctx, 2, FLAVOR_REST, &bp);
    mc_plain(g, ctx, 3, &bp);
    /* markers so the danger region has a shape on screen */
    chart_add(c, 1, 0, LANE_SPACE, NOTE_MARK, 0);
    chart_add(c, 3, 0, LANE_SPACE, NOTE_MARK, 0);
}

static void rest_update(MicroState *m, const InputEvents *in, const ClockSnap *clk)
{
    if (!m->chart.bar_known[1] || !m->chart.bar_known[3]) return;
    uint64_t from = m->chart.bar_start[1];
    uint64_t to   = m->chart.bar_start[3];
    uint64_t now  = clk->sample_clock;

    for (int i = 0; i < in->count; i++) {
        const InputEvent *e = &in->e[i];
        if (!e->down) continue;
        if (e->t < from || e->t > to) continue;
        if (m->hard_fail) continue;
        m->hard_fail = 1;
        audio_sfx(SFX_HONK, 0);
        shake_add(0.9f);
        pop_text("YOU TOUCHED IT", pf_center_x(), pf_center_y() - 40, 6, pal()->hot, 1.4f);
        particles_burst(pf_center_x(), pf_center_y(), 40, pal()->hot, 320.0f, 0.8f);
    }

    if (!m->ix[1] && now > to) {
        m->ix[1] = 1;
        if (!m->hard_fail) {
            for (int k = 0; k < 4; k++) stats_add(&m->stats, J_PERFECT);
            audio_announce(VOX_NICE);
            pop_text("CLEAN", pf_center_x(), pf_center_y() - 40, 7, pal()->accent, 1.2f);
            particles_burst(pf_center_x(), pf_center_y(), 36, pal()->accent, 260.0f, 0.9f);
        }
    }
}

static void rest_draw(MicroState *m)
{
    const Palette *p = pal();
    uint64_t now = m->g->clk.sample_clock;
    float cx = pf_center_x(), cy = pf_center_y();

    int in_region = 0;
    float u = 0.0f;
    if (m->chart.bar_known[1] && m->chart.bar_known[3]) {
        uint64_t from = m->chart.bar_start[1], to = m->chart.bar_start[3];
        if (now >= from && now <= to) {
            in_region = 1;
            u = (float)((double)(now - from) / (double)(to - from));
        } else if (now < from) {
            uint64_t s0 = m->chart.bar_known[0] ? m->chart.bar_start[0] : from;
            float v = (float)((double)(now - s0) / (double)(from - s0 + 1));
            gtext_center("HANDS OFF IN...", (int)cx, (int)cy - 150, 4, p->main);
            ring(cx, cy, 60.0f + 120.0f * (1.0f - g_clampf(v, 0, 1)), 6.0f, p->accent);
        }
    }

    if (m->hard_fail) {
        gtext_center("HONK", (int)cx, (int)cy - 40, 10, p->hot);
    } else if (in_region) {
        Color c = cmix(p->accent, p->hot, u);
        float r = 250.0f * (1.0f - u) + 30.0f;
        ring(cx, cy, r, 10.0f, c);
        gtext_center("DON'T. TOUCH.", (int)cx, (int)cy - 60, 6, c);
        gtext_center("ANYTHING.", (int)cx, (int)cy + 10, 6, c);
        draw_bar_meter(cx - 300, cy + 120, 600, 18, 1.0f - u, c, p->dim);
    } else {
        gtext_center("SAFE", (int)cx, (int)cy - 20, 6, p->dim);
    }
}

static bool rest_judge(const MicroState *m) { return !m->hard_fail; }

const Microgame MG_REST = {
    "REST", "DON'T TOUCH ANYTHING", KEYMASK_ALL, 4, 1,
    rest_build, rest_update, rest_draw, rest_judge
};
