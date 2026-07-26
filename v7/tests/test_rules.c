/* test_rules.c — §6 rulesets, the foosball law of §5.5, and §10 replay.
 *
 * The ruleset lock is a promise to competitive players, so it gets a test
 * rather than a comment.
 */
#include "rules.h"
#include "rods.h"
#include "heat.h"
#include "replay.h"
#include "ai.h"

#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;

#define CHECK(cond, ...) do {                                   \
    checks++;                                                   \
    if (!(cond)) {                                              \
        fails++;                                                \
        printf("  FAIL %s:%d  ", __FILE__, __LINE__);           \
        printf(__VA_ARGS__); printf("\n");                      \
    }                                                           \
} while (0)

static void head(const char *s) { printf("\n%s\n", s); }

static VbSimCfg loaded_cfg(void) {
    /* every mutator on, every tilt lever pushed — the thing STANDARD must
     * refuse to accept */
    VbSimCfg c;
    memset(&c, 0, sizeof c);
    for (int i = 0; i < VB_NPLAYERS; i++) c.scheme[i] = VB_SCHEME_GRIP;
    c.mutators = VB_MUT_MULTIBALL | VB_MUT_BIG_BALL | VB_MUT_MAGNET
               | VB_MUT_MIRROR | VB_MUT_SUDDEN_HEAT | VB_MUT_MOVING_GOAL
               | VB_MUT_GOLDEN_GOAL;
    for (int t = 0; t < VB_NSIDES; t++) {
        c.tilt[t].pad_scale  = 1.40f;
        c.tilt[t].rod_speed  = 1.60f;
        c.tilt[t].assist     = 1.0f;
        c.tilt[t].goal_scale = 1.50f;
        c.tilt[t].heat_cap   = 4;
    }
    return c;
}

static void test_ruleset_lock(void) {
    head("the ruleset lock (§6)");
    VbSimCfg sc = loaded_cfg();
    static VbMatch m;
    VbMatchCfg mc = { VB_MODE_VERSUS, VB_RULES_STANDARD, 5, 1 };
    vb_match_init(&m, &mc, &sc, 1);

    CHECK(m.sim.cfg.mutators == 0, "a mutator survived into STANDARD (0x%x)",
          m.sim.cfg.mutators);
    CHECK(m.cfg.golden == 0, "Golden Goal survived into STANDARD");
    for (int t = 0; t < VB_NSIDES; t++) {
        CHECK(m.sim.cfg.tilt[t].pad_scale == 1.0f, "side %d kept a paddle handicap", t);
        CHECK(m.sim.cfg.tilt[t].rod_speed == 1.0f, "side %d kept a rod-speed handicap", t);
        CHECK(m.sim.cfg.tilt[t].heat_cap == 0, "side %d kept a heat cap", t);
        CHECK(m.sim.cfg.tilt[t].goal_scale == 1.0f, "side %d kept a goal handicap", t);
        CHECK(m.sim.goal_half[t] == VB_GOAL_HALF, "side %d's mouth is not standard", t);
    }
    /* the assist is a control scheme, not an advantage: it stays */
    CHECK(m.sim.cfg.tilt[0].assist == 1.0f, "STANDARD stripped the GLIDE assist");

    /* PARTY keeps everything it was given */
    VbMatchCfg pc = { VB_MODE_VERSUS, VB_RULES_PARTY, 5, 1 };
    vb_match_init(&m, &pc, &sc, 1);
    CHECK(m.sim.cfg.mutators == sc.mutators, "PARTY dropped a mutator");
    CHECK(m.cfg.golden == 1, "PARTY dropped Golden Goal");
    CHECK(m.sim.goal_half[0] > VB_GOAL_HALF, "PARTY dropped a Table Tilt handicap");
}

static VbSimCfg plain_cfg(void) {
    VbSimCfg c;
    memset(&c, 0, sizeof c);
    for (int i = 0; i < VB_NPLAYERS; i++) c.scheme[i] = VB_SCHEME_GRIP;
    for (int t = 0; t < VB_NSIDES; t++) {
        c.tilt[t].pad_scale = 1.0f; c.tilt[t].rod_speed = 1.0f;
        c.tilt[t].assist = 1.0f;    c.tilt[t].goal_scale = 1.0f;
    }
    return c;
}

/* put the ball through a mouth on purpose */
static void force_goal(VbMatch *m, int scoring_side, int heat) {
    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    while (m->phase != VB_PH_LIVE && m->winner < 0) vb_match_step(m, in);

    /* set it up once and let it fly — resetting the ball every tick would
     * simply hold it in place */
    VbBall *b = &m->sim.balls[0];
    float wall = scoring_side == 0 ? VB_TABLE_HX : -VB_TABLE_HX;
    b->alive = 1;
    b->heat = heat;
    b->mercy = 0;
    b->z = 0.0f; b->vz = 0.0f;
    b->curve = v2zero(); b->curve_ticks = 0;
    b->p = v2(wall - vb_signf(wall) * 0.12f, m->sim.goal_y[1 - scoring_side]);
    b->v = v2(vb_signf(wall) * 3.0f, 0.0f);

    for (int t = 0; t < 600; t++) {
        /* pull the goalie off its mouth so the shot is actually on */
        int gk = vb_rod_of(1 - scoring_side, VB_GK);
        m->sim.rods[gk].off = m->sim.rods[gk].travel;
        m->sim.rods[gk].vel = 0.0f;
        vb_match_step(m, in);
        if (m->phase != VB_PH_LIVE) break;
    }
}

static void test_scoring_and_serve(void) {
    head("scoring, loser serves, scorchers (§5.5, §6)");
    VbSimCfg sc = plain_cfg();
    static VbMatch m;
    VbMatchCfg mc = { VB_MODE_VERSUS, VB_RULES_STANDARD, 3, 0 };
    vb_match_init(&m, &mc, &sc, 5);

    CHECK(m.serve_to == 0, "the first serve did not go to side 0");

    force_goal(&m, 0, 5);
    CHECK(m.score[0] == 1, "a goal did not score (%d-%d)", m.score[0], m.score[1]);
    CHECK(m.serve_to == 1, "the loser does not serve");
    CHECK(m.stats[0].goals == 1, "the stat card missed the goal");
    CHECK(m.stats[0].scorchers == 0, "a heat-5 goal was called a scorcher");
    CHECK(m.phase == VB_PH_GOAL, "the game did not pause after a goal");

    /* between points is 1.2 s, max: this game respects the run-it-back reflex */
    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    int pause = 0;
    while (m.phase == VB_PH_GOAL) { vb_match_step(&m, in); pause++; }
    CHECK(pause <= VB_TICKS(1.25f), "the pause after a goal ran %d ticks", pause);

    /* the serve is protected, and it comes off the conceding side's goalie */
    while (m.phase == VB_PH_SERVE) vb_match_step(&m, in);
    CHECK(m.sim.serve_side == 1, "the serve came from the wrong side");

    force_goal(&m, 0, 11);
    CHECK(m.stats[0].scorchers == 1, "a heat-11 goal was not a SCORCHER");
    CHECK(m.scorcher_crack == 1, "a scorcher did not crack the glass");
    CHECK(m.score[0] == 2, "the scorcher was worth more than one point");

    force_goal(&m, 0, 4);
    CHECK(m.winner == 0, "first to 3 did not end the match (%d-%d, winner %d)",
          m.score[0], m.score[1], m.winner);
    CHECK(m.phase == VB_PH_OVER, "the match did not end");
}

static void test_golden_goal(void) {
    head("Golden Goal (§6, PARTY only)");
    VbSimCfg sc = plain_cfg();
    static VbMatch m;
    VbMatchCfg mc = { VB_MODE_VERSUS, VB_RULES_PARTY, 5, 1 };
    vb_match_init(&m, &mc, &sc, 9);
    m.score[0] = 4;
    m.score[1] = 4;
    force_goal(&m, 1, 3);
    CHECK(m.winner == 1, "Golden Goal did not finish it (winner %d)", m.winner);
}

static void test_rally_mode(void) {
    head("RALLY: no goals, a shared streak, a gentle reset (§6)");
    VbSimCfg sc = plain_cfg();
    static VbMatch m;
    VbMatchCfg mc = { VB_MODE_RALLY, VB_RULES_CO_OP, 0, 0 };
    vb_match_init(&m, &mc, &sc, 3);

    VbAI a, b;
    vb_ai_init(&a, 2, 1, 0, 1);
    vb_ai_init(&b, 2, 1, 2, 2);
    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    for (int t = 0; t < 240 * 90; t++) {
        in[0] = vb_ai_think(&a, &m.sim);
        in[2] = vb_ai_think(&b, &m.sim);
        vb_match_step(&m, in);
    }
    CHECK(m.score[0] == 0 && m.score[1] == 0, "RALLY scored points (%d-%d)",
          m.score[0], m.score[1]);
    CHECK(m.winner < 0, "RALLY produced a winner");
    int best = m.best_streak > m.streak ? m.best_streak : m.streak;
    CHECK(best > 5, "RALLY never built a streak (best %d)", best);
    printf("  best co-op streak in 90s: %d\n", best);
}

static void test_tilt(void) {
    head("Table Tilt is real and visible (§6)");
    VbSimCfg sc = plain_cfg();
    sc.tilt[0].pad_scale  = 0.70f;
    sc.tilt[1].pad_scale  = 1.30f;
    sc.tilt[0].goal_scale = 1.40f;
    sc.tilt[1].heat_cap   = 6;
    static VbMatch m;
    VbMatchCfg mc = { VB_MODE_VERSUS, VB_RULES_PARTY, 5, 0 };
    vb_match_init(&m, &mc, &sc, 2);

    const VbRod *small = &m.sim.rods[vb_rod_of(0, VB_ATK)];
    const VbRod *big   = &m.sim.rods[vb_rod_of(1, VB_ATK)];
    CHECK(small->half < big->half, "the paddle handicap did nothing");
    CHECK(m.sim.goal_half[0] > m.sim.goal_half[1],
          "the wider-goal handicap did nothing");
    CHECK(vb_heat_up(6, m.sim.cfg.tilt[1].heat_cap) == 6,
          "the personal heat cap did nothing");
    /* and the law still holds under the biggest paddles anyone can ask for */
    for (int i = 0; i < VB_NRODS; i++) {
        m.sim.rods[i].off = m.sim.rods[i].travel;
        CHECK(vb_rod_open_lane(&m.sim.rods[i]) >= VB_OPEN_LANE,
              "rod %d sealed the table under Table Tilt", i);
    }
}

static void test_replay(void) {
    head("replay is the rally, run again (§5.1, §10)");
    VbSimCfg sc = plain_cfg();
    static VbSim s;
    vb_sim_init(&s, &sc, 77);
    vb_sim_serve(&s, 0);

    static VbReplay r;
    vb_replay_reset(&r);
    vb_replay_begin(&r, &s);

    VbAI a, b;
    vb_ai_init(&a, 5, 2, 0, 3);
    vb_ai_init(&b, 6, 2, 2, 4);
    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    int n = 1200;
    for (int t = 0; t < n; t++) {
        in[0] = vb_ai_think(&a, &s);
        in[2] = vb_ai_think(&b, &s);
        vb_replay_record(&r, in);
        vb_sim_step(&s, in);
        if (s.goal_side >= 0) { n = t + 1; break; }
    }
    vb_replay_end(&r, s.balls[0].contacts, s.balls[0].heat, 0, s.goal_side);

    int best = vb_replay_best(&r);
    CHECK(best >= 0, "no rally was kept");
    vb_replay_play(&r, best);
    CHECK(r.playing, "playback would not start");

    int steps = 0;
    while (vb_replay_step(&r)) steps++;
    CHECK(steps == n, "playback ran %d ticks against %d recorded", steps, n);
    /* the axis is quantised to a byte on the way in, so the replay tracks the
     * original closely rather than bit-exactly; the ball must still end up in
     * the same place on the table */
    float dx = vb_absf(r.play_sim.balls[0].p.x - s.balls[0].p.x);
    float dy = vb_absf(r.play_sim.balls[0].p.y - s.balls[0].p.y);
    CHECK(dx < 0.08f && dy < 0.08f,
          "the replay diverged from the rally by (%.3f, %.3f)", dx, dy);
    CHECK(r.play_sim.tick == s.tick, "the replay ran a different number of ticks");
    printf("  %d ticks replayed, ball within (%.4f, %.4f)\n", steps, dx, dy);
}

static void test_doubles(void) {
    head("doubles: GK+DEF to one player, ATK to the other (§3.3)");
    VbSimCfg sc = plain_cfg();
    sc.doubles = 1;
    static VbSim s;
    vb_sim_init(&s, &sc, 4);
    CHECK(vb_rod_owner_slot(&s, vb_rod_of(0, VB_GK))  == 0, "side 0 GK went astray");
    CHECK(vb_rod_owner_slot(&s, vb_rod_of(0, VB_DEF)) == 0, "side 0 DEF went astray");
    CHECK(vb_rod_owner_slot(&s, vb_rod_of(0, VB_ATK)) == 1, "side 0 ATK went astray");
    CHECK(vb_rod_owner_slot(&s, vb_rod_of(1, VB_GK))  == 2, "side 1 GK went astray");
    CHECK(vb_rod_owner_slot(&s, vb_rod_of(1, VB_ATK)) == 3, "side 1 ATK went astray");

    /* the ATK player's stick must not move their teammate's bars */
    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    in[1].axis = 1.0f;
    float gk = s.rods[vb_rod_of(0, VB_GK)].off;
    for (int t = 0; t < 60; t++) vb_sim_step(&s, in);
    CHECK(vb_absf(s.rods[vb_rod_of(0, VB_GK)].off - gk) < 1e-6f,
          "the ATK player dragged their teammate's goalie");
    CHECK(vb_absf(s.rods[vb_rod_of(0, VB_ATK)].off) > 0.01f,
          "the ATK player could not move their own bar");

    /* and partners can trade roles between points */
    s.cfg.swap[0] = 1;
    CHECK(vb_rod_owner_slot(&s, vb_rod_of(0, VB_GK))  == 1, "the swap missed the GK");
    CHECK(vb_rod_owner_slot(&s, vb_rod_of(0, VB_DEF)) == 1, "the swap missed the DEF");
    CHECK(vb_rod_owner_slot(&s, vb_rod_of(0, VB_ATK)) == 0, "the swap missed the ATK");
    CHECK(vb_rod_owner_slot(&s, vb_rod_of(1, VB_GK))  == 2,
          "swapping one side swapped the other too");
    s.cfg.swap[0] = 0;

    /* in singles the swap is meaningless and must not move anything */
    s.cfg.doubles = 0;
    s.cfg.swap[0] = 1;
    CHECK(vb_rod_owner_slot(&s, vb_rod_of(0, VB_ATK)) == 0,
          "a doubles swap leaked into a singles match");
}

/* Every mode has to survive a long unattended match without the sim wedging,
 * losing the ball, or running out of events. */
static void test_modes_survive(void) {
    head("every mode runs unattended");
    const int modes[] = { VB_MODE_VERSUS, VB_MODE_VERSUS, VB_MODE_RALLY,
                          VB_MODE_GAUNTLET, VB_MODE_TRICKSHOT };
    const int rules[] = { VB_RULES_STANDARD, VB_RULES_PARTY, VB_RULES_CO_OP,
                          VB_RULES_STANDARD, VB_RULES_STANDARD };
    const char *name[] = { "STANDARD", "PARTY", "RALLY", "GAUNTLET", "TRICKSHOT" };
    for (int i = 0; i < 5; i++) {
        VbSimCfg sc = plain_cfg();
        if (rules[i] == VB_RULES_PARTY)
            sc.mutators = VB_MUT_MULTIBALL | VB_MUT_MOVING_GOAL | VB_MUT_MAGNET
                        | VB_MUT_SUDDEN_HEAT;
        static VbMatch m;
        VbMatchCfg mc = { modes[i], rules[i], 5, rules[i] == VB_RULES_PARTY };
        vb_match_init(&m, &mc, &sc, 100 + (unsigned)i);
        VbAI a, b;
        vb_ai_init(&a, i, 1, 0, 11u + (unsigned)i);
        vb_ai_init(&b, 9 - i, 2, 2, 22u + (unsigned)i);
        VbInput in[VB_NPLAYERS];
        memset(in, 0, sizeof in);
        int dead = 0, overran = 0;
        for (int t = 0; t < 240 * 120; t++) {
            in[0] = vb_ai_think(&a, &m.sim);
            in[2] = vb_ai_think(&b, &m.sim);
            vb_match_step(&m, in);
            if (m.sim.nev > VB_MAXEV) overran++;
            if (m.winner >= 0) break;
            if (m.phase == VB_PH_LIVE) {
                int alive = 0;
                for (int k = 0; k < m.sim.nballs; k++) if (m.sim.balls[k].alive) alive = 1;
                if (!alive) dead++;
            }
        }
        CHECK(overran == 0, "%s overran the event buffer on %d ticks", name[i], overran);
        CHECK(dead == 0, "%s left the table with no live ball for %d ticks", name[i], dead);
        printf("  %-9s %d-%d, longest rally %d, top heat %d, saves %d/%d\n",
               name[i], m.score[0], m.score[1],
               m.stats[0].longest_rally > m.stats[1].longest_rally
                 ? m.stats[0].longest_rally : m.stats[1].longest_rally,
               m.stats[0].top_heat > m.stats[1].top_heat
                 ? m.stats[0].top_heat : m.stats[1].top_heat,
               m.stats[0].saves, m.stats[1].saves);
    }
}

int main(void) {
    printf("VOLLEYBAR — rules suite\n");
    test_ruleset_lock();
    test_scoring_and_serve();
    test_golden_goal();
    test_rally_mode();
    test_tilt();
    test_replay();
    test_doubles();
    test_modes_survive();
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
