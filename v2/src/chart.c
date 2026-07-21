#include "chart.h"

void chart_reset(Chart *c, int base_bar, int bars)
{
    memset(c, 0, sizeof *c);
    c->base_bar = base_bar;
    c->bars = bars > 0 ? bars : PHRASE_BARS;
}

Note *chart_add(Chart *c, int bar, int step, int lane, NoteKind kind, int dur_steps)
{
    if (c->count >= CHART_MAX) return NULL;
    Note *n = &c->n[c->count++];
    memset(n, 0, sizeof *n);
    n->bar = (int8_t)bar;
    n->step = (uint8_t)step;
    n->lane = (uint8_t)lane;
    n->kind = (uint8_t)kind;
    n->dur_steps = (uint8_t)dur_steps;
    n->midi = -1;
    return n;
}

void chart_resolve_bar(Chart *c, const BarInfo *bi)
{
    int rel = bi->bar_index - c->base_bar;
    if (rel < 0 || rel > c->bars) return;
    if (rel <= PHRASE_BARS) {
        c->bar_start[rel] = bi->step_sample[0];
        c->bar_known[rel] = 1;
        c->bar_spb[rel] = (float)(bi->step_sample[STEPS_PER_BAR] - bi->step_sample[0])
                        / (float)BEATS_PER_BAR;
    }
    for (int i = 0; i < c->count; i++) {
        Note *n = &c->n[i];
        if (n->resolved) continue;
        if (n->bar != rel) continue;
        int st = g_clampi(n->step, 0, STEPS_PER_BAR);
        n->t = bi->step_sample[st];
        int end = st + (n->dur_steps ? n->dur_steps : 0);
        if (end <= STEPS_PER_BAR) {
            n->t_end = bi->step_sample[end];
        } else {
            /* hold spilling into the next bar: extrapolate at this bar's tempo */
            uint64_t per_step = (bi->step_sample[STEPS_PER_BAR] - bi->step_sample[0]) / STEPS_PER_BAR;
            n->t_end = bi->step_sample[STEPS_PER_BAR] + per_step * (uint64_t)(end - STEPS_PER_BAR);
        }
        n->resolved = 1;
    }
}

Judgment chart_judge_offset(double off_samples, float win_scale)
{
    double ms = fabs(off_samples) * 1000.0 / (double)SAMPLE_RATE;
    if (ms <= WINDOW_PERFECT_MS * win_scale) return J_PERFECT;
    if (ms <= WINDOW_GOOD_MS * win_scale) return J_GOOD;
    return J_MISS;
}

int chart_find_hit(Chart *c, int lane, uint64_t t, float win_scale)
{
    double best = 1e18;
    int best_i = -1;
    double good_w = (double)WINDOW_GOOD_MS * win_scale * SAMPLE_RATE / 1000.0;
    for (int i = 0; i < c->count; i++) {
        Note *n = &c->n[i];
        if (!n->resolved || n->judged) continue;
        if (n->kind == NOTE_MARK) continue;
        if (lane >= 0 && n->lane != (uint8_t)lane) continue;
        double d = (double)t - (double)n->t;
        double ad = fabs(d);
        if (ad <= good_w && ad < best) { best = ad; best_i = i; }
    }
    return best_i;
}

int chart_expire(Chart *c, uint64_t now, float win_scale, int *out, int out_max)
{
    double good_w = (double)WINDOW_GOOD_MS * win_scale * SAMPLE_RATE / 1000.0;
    int n_new = 0;
    for (int i = 0; i < c->count; i++) {
        Note *n = &c->n[i];
        if (!n->resolved || n->judged) continue;
        if (n->kind == NOTE_MARK) continue;
        uint64_t deadline = (n->kind == NOTE_HOLD) ? n->t_end : n->t;
        if ((double)now > (double)deadline + good_w) {
            n->judged = J_MISS;
            if (out && n_new < out_max) out[n_new] = i;
            n_new++;
        }
    }
    return n_new;
}

void stats_add(PhraseStats *s, Judgment j)
{
    switch (j) {
    case J_PERFECT: s->perfect++; s->judged++; break;
    case J_GOOD:    s->good++;    s->judged++; break;
    case J_MISS:    s->miss++;    s->judged++; break;
    default: break;
    }
}

int stats_score(const PhraseStats *s)
{
    return s->perfect * SCORE_PERFECT + s->good * SCORE_GOOD;
}

bool chart_default_success(const PhraseStats *s)
{
    if (s->judged <= 0) return true;
    float hit_ratio = (float)(s->perfect + s->good) / (float)s->judged;
    return hit_ratio >= 0.70f && s->miss <= 2;
}
