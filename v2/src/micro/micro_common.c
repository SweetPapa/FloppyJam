#include "micro_common.h"

const int LANES_SPACE[1] = { LANE_SPACE };
const int LANES_FJ[2]    = { LANE_F, LANE_J };
const int LANES_FGHJ[4]  = { LANE_F, LANE_G, LANE_H, LANE_J };
const int LANES_ALL[5]   = { LANE_F, LANE_G, LANE_H, LANE_J, LANE_SPACE };
const char *LANE_LABEL[LANE_COUNT] = { "F", "G", "H", "J", "SPC" };

void mc_bar(const Genome *g, const PhraseCtx *c, int bar, BarFlavor f, BarPattern *out)
{
    int abs_bar = c->base_bar + bar;
    audio_bar_flavor(abs_bar, f);
    seq_bar_pattern(g, abs_bar, f, out);
}

void mc_plain(const Genome *g, const PhraseCtx *c, int bar, BarPattern *out)
{
    mc_bar(g, c, bar, FLAVOR_NONE, out);
}

int mc_note_lane(int midi, int lanes)
{
    if (lanes <= 1) return 0;
    int v = midi % lanes;
    if (v < 0) v += lanes;
    return v;
}

bool mc_judge_default(const MicroState *m) { return chart_default_success(&m->stats); }

void mc_expire(MicroState *m, const ClockSnap *clk)
{
    int missed[16];
    int n = chart_expire(&m->chart, clk->sample_clock, run_window_scale(), missed, 16);
    for (int i = 0; i < n; i++) {
        Note *note = &m->chart.n[missed[i]];
        stats_add(&m->stats, J_MISS);
        int lane_idx = note->lane;
        micro_feedback(m, J_MISS, missed[i],
                       pf_lane_x(lane_idx, 5), pf_bottom() - 60.0f);
    }
}

void mc_update_standard(MicroState *m, const InputEvents *in, const ClockSnap *clk,
                        int strict_extra)
{
    mc_expire(m, clk);

    for (int i = 0; i < in->count; i++) {
        const InputEvent *e = &in->e[i];
        if (!e->down) continue;
        if (!(m->def->keys & (1u << e->lane))) continue;   /* key not used here */

        int idx = chart_find_hit(&m->chart, e->lane, e->t, run_window_scale());
        if (idx < 0) {
            m->extra++;
            if (strict_extra) stats_add(&m->stats, J_MISS);
            micro_feedback(m, J_MISS, -1, pf_lane_x(e->lane, 5), pf_bottom() - 60.0f);
            continue;
        }
        Note *n = &m->chart.n[idx];
        double off = (double)e->t - (double)n->t;
        Judgment j = chart_judge_offset(off, run_window_scale());
        n->judged = (uint8_t)j;
        n->off_ms = samples_to_ms(off);
        stats_add(&m->stats, j);
        micro_feedback(m, j, idx, pf_lane_x(e->lane, 5), pf_bottom() - 60.0f);
        micro_flash_lane(m, e->lane);
    }
}

void mc_draw_standard(MicroState *m, int lanes, const int *lane_ids)
{
    const Palette *p = pal();
    uint64_t now = m->g->clk.sample_clock;

    /* lane guides */
    for (int i = 0; i < lanes; i++) {
        float x = pf_lane_x(i, lanes);
        Color c = pal_mix(p->dim, 0.0f);
        c.a = 150;
        DrawRectangle((int)(x - 44), (int)pf_top(), 88, (int)(pf_bottom() - pf_top()), c);
        float flash = m->lane_flash[lane_ids[i]];
        if (flash > 0.01f) {
            Color fc = p->accent;
            fc.a = (unsigned char)(flash * 150.0f);
            DrawRectangle((int)(x - 44), (int)pf_top(), 88, (int)(pf_bottom() - pf_top()), fc);
        }
        /* the hit line + keycap */
        DrawRectangle((int)(x - 52), (int)(pf_bottom() - 62), 104, 5, p->main);
        gkeycap(LANE_LABEL[lane_ids[i]], (int)x - 20, (int)pf_bottom() - 46, 4, p->main,
                (m->g->in.down_mask & (1u << lane_ids[i])) != 0);
    }

    /* notes */
    for (int i = 0; i < m->chart.count; i++) {
        Note *n = &m->chart.n[i];
        if (n->kind == NOTE_MARK) continue;
        if (!pf_note_visible(m, n, now)) continue;
        int li = 0;
        for (int k = 0; k < lanes; k++) if (lane_ids[k] == n->lane) li = k;
        pf_draw_note(m, n, now, li, lanes);
    }
}
