/* rules.c — Section 6 and the foosball law of §5.5.
 *
 * Loser serves. Pins hard-cap at two seconds. Possession that never advances
 * gets a referee pulse and then a heat-0 pop toward the middle. None of these
 * are difficulty settings; they are the shape of the game.
 */
#include "rules.h"
#include "rods.h"
#include "heat.h"

#define GOAL_PAUSE  VB_TICKS(1.20f)   /* §10: between points, 1.2 s max     */
#define SERVE_HOLD  VB_TICKS(0.55f)

void vb_match_init(VbMatch *m, const VbMatchCfg *mc, const VbSimCfg *sc,
                   unsigned seed) {
    for (size_t i = 0; i < sizeof(*m); i++) ((char *)m)[i] = 0;
    m->cfg = *mc;
    if (m->cfg.target <= 0) m->cfg.target = 5;

    VbSimCfg cfg = *sc;
    /* The ruleset lock (§6). STANDARD is mutator-free by construction, not by
     * the menu remembering to grey a checkbox out. */
    if (m->cfg.ruleset == VB_RULES_STANDARD) {
        cfg.mutators = 0;
        m->cfg.golden = 0;
        for (int t = 0; t < VB_NSIDES; t++) {
            cfg.tilt[t].pad_scale = 1.0f;
            cfg.tilt[t].rod_speed = 1.0f;
            cfg.tilt[t].heat_cap  = 0;
            cfg.tilt[t].goal_scale = 1.0f;
            /* assist stays: it is a control scheme, not an advantage — a
             * GLIDE player needs it to have a scheme at all (§3.1). */
        }
    }
    if (m->cfg.ruleset == VB_RULES_CO_OP) cfg.mutators &= ~VB_MUT_MULTIBALL;

    vb_sim_init(&m->sim, &cfg, seed);
    m->winner = -1;
    m->serve_to = 0;
    m->phase = VB_PH_SERVE;
    m->phase_ticks = SERVE_HOLD;
    vb_sim_serve(&m->sim, m->serve_to);
}

int vb_match_live(const VbMatch *m) { return m->phase == VB_PH_LIVE; }

int vb_match_is_match_point(const VbMatch *m) {
    if (m->cfg.mode == VB_MODE_RALLY) return 0;
    return (m->score[0] >= m->cfg.target - 1) || (m->score[1] >= m->cfg.target - 1);
}

/* Rally mode has no goals: the ball reaching a mouth is a gentle reset, and
 * that is the whole failure state. Zero pressure, by design (§6). */
static void rally_reset(VbMatch *m) {
    if (m->streak > m->best_streak) m->best_streak = m->streak;
    m->streak = 0;
    m->rally = 0;
    m->rally_heat = 0;
    m->phase = VB_PH_SERVE;
    m->phase_ticks = SERVE_HOLD;
    vb_sim_serve(&m->sim, m->serve_to);
}

static void score_goal(VbMatch *m, int side, int heat) {
    VbStats *st = &m->stats[side];
    st->goals++;
    if (vb_heat_scorcher(heat)) st->scorchers++;
    m->score[side]++;
    m->last_goal_side = side;
    m->last_goal_heat = heat;
    /* a SCORCHER cosmetically cracks the glass for the next rally (§8) */
    m->scorcher_crack = vb_heat_scorcher(heat) ? 1 : 0;

    /* loser serves, from their goalie, with a protected possession */
    m->serve_to = 1 - side;

    int done = m->score[side] >= m->cfg.target;
    if (m->cfg.golden && m->score[0] >= m->cfg.target - 1
                      && m->score[1] >= m->cfg.target - 1)
        done = 1;                              /* Golden Goal finale        */
    if (done) {
        m->winner = side;
        m->phase = VB_PH_OVER;
        m->phase_ticks = 0;
    } else {
        m->phase = VB_PH_GOAL;
        m->phase_ticks = GOAL_PAUSE;
    }
    m->match_point = vb_match_is_match_point(m);
}

static void drain_events(VbMatch *m) {
    VbSim *s = &m->sim;
    for (int i = 0; i < s->nev; i++) {
        const VbEvent *e = &s->ev[i];
        int side, heat;
        switch (e->type) {
            case VB_EV_HIT:
            case VB_EV_SNAP:
                side = s->rods[e->a].side;
                m->rally++;
                if (m->cfg.mode == VB_MODE_RALLY) m->streak++;
                heat = (e->b >= 0) ? s->balls[e->b].heat : 0;
                if (heat > m->rally_heat) m->rally_heat = heat;
                if (heat > m->stats[side].top_heat) m->stats[side].top_heat = heat;
                if (m->rally > m->stats[side].longest_rally)
                    m->stats[side].longest_rally = m->rally;
                break;
            case VB_EV_PIN:
                m->stats[s->rods[e->a].side].pins++;
                break;
            case VB_EV_CHIP:
                m->stats[s->rods[e->a].side].chips++;
                break;
            case VB_EV_SAVE:
                m->stats[s->rods[e->a].side].saves++;
                break;
            case VB_EV_WALL:
                m->stats[0].banks++;   /* the rally's bank count, side-blind */
                break;
            case VB_EV_STALL:
                /* the referee pulse already went out as the event; the pop is
                 * ours to order */
                vb_sim_pop_center(s);
                m->stall_warned = 1;
                break;
            default: break;
        }
    }
}

void vb_match_step(VbMatch *m, const VbInput in[VB_NPLAYERS]) {
    VbSim *s = &m->sim;

    switch (m->phase) {
        case VB_PH_SERVE:
            /* the ball is on the goalie and the rods already answer the stick:
             * you can be moving before the whistle, exactly like the real
             * thing. The sim runs; the ball simply is not launched yet. */
            s->nev = 0;
            vb_rods_update(s, in);
            s->tick++;
            if (--m->phase_ticks <= 0) {
                m->phase = VB_PH_LIVE;
                m->rally = 0;
                m->rally_heat = 0;
                m->stall_warned = 0;
            }
            return;

        case VB_PH_GOAL:
            s->nev = 0;
            vb_rods_update(s, in);
            s->tick++;
            if (--m->phase_ticks <= 0) {
                vb_sim_serve(s, m->serve_to);
                m->phase = VB_PH_SERVE;
                m->phase_ticks = SERVE_HOLD;
            }
            return;

        case VB_PH_OVER:
            m->phase_ticks++;
            return;

        default: break;
    }

    vb_sim_step(s, in);
    drain_events(m);

    if (s->goal_side >= 0) {
        if (m->cfg.mode == VB_MODE_RALLY) rally_reset(m);
        else score_goal(m, s->goal_side, s->goal_heat);
        return;
    }

    /* every ball gone without a goal (multiball ends, a dead ball): serve on */
    int alive = 0;
    for (int i = 0; i < s->nballs; i++) if (s->balls[i].alive) alive = 1;
    if (!alive) {
        if (m->cfg.mode == VB_MODE_RALLY) rally_reset(m);
        else {
            m->phase = VB_PH_GOAL;
            m->phase_ticks = GOAL_PAUSE / 2;
        }
    }
}
