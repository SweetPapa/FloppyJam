/* test_sim.c — the merge gate for §5.
 *
 * Determinism, tunneling at maximum heat, the open-lane law, english, chip,
 * snap, the mercy beat, the anti-stall referee, and the AI honesty audit.
 * No window, no audio device, no clock.
 */
#include "sim.h"
#include "rods.h"
#include "heat.h"
#include "rules.h"
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

static VbSimCfg base_cfg(int scheme) {
    VbSimCfg c;
    memset(&c, 0, sizeof c);
    for (int i = 0; i < VB_NPLAYERS; i++) c.scheme[i] = scheme;
    for (int t = 0; t < VB_NSIDES; t++) {
        c.tilt[t].pad_scale = 1.0f;
        c.tilt[t].rod_speed = 1.0f;
        c.tilt[t].assist    = 1.0f;
        c.tilt[t].goal_scale = 1.0f;
        c.tilt[t].heat_cap  = 0;
    }
    return c;
}

/* ------------------------------------------------------------------ */
/* §5.1 — same seed, same inputs, bit-identical match. A thousand runs. */

static unsigned fnv(const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p;
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
    return h;
}

static unsigned play_match(unsigned seed, int ticks) {
    VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
    static VbMatch m;
    VbMatchCfg mc = { VB_MODE_VERSUS, VB_RULES_STANDARD, 5, 0 };
    vb_match_init(&m, &mc, &cfg, seed);
    VbAI a, b;
    vb_ai_init(&a, 3, 1, 0, seed + 1);
    vb_ai_init(&b, 8, 2, 2, seed + 2);
    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    for (int t = 0; t < ticks; t++) {
        in[0] = vb_ai_think(&a, &m.sim);
        in[2] = vb_ai_think(&b, &m.sim);
        vb_match_step(&m, in);
    }
    return fnv(&m, sizeof m);
}

static void test_determinism(void) {
    head("determinism (§5.1)");
    unsigned ref = play_match(0xC0FFEEu, 4000);
    int same = 1;
    for (int i = 0; i < 1000; i++)
        if (play_match(0xC0FFEEu, 4000) != ref) { same = 0; break; }
    CHECK(same, "1000 runs of the same seed did not agree");
    CHECK(play_match(0xC0FFEFu, 4000) != ref, "different seeds produced the same match");

    /* and the sim must not care what the wall clock says: no clock reaches it,
     * so a match replayed from a snapshot lands on the same hash */
    VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
    static VbSim s1, s2;
    vb_sim_init(&s1, &cfg, 7); vb_sim_serve(&s1, 0);
    memcpy(&s2, &s1, sizeof s1);
    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    for (int t = 0; t < 2000; t++) {
        in[0].axis = (float)((t / 37) % 3 - 1);
        in[0].btn  = ((t / 53) % 4 == 0) ? VB_BTN_FLICK : 0u;
        in[2].axis = (float)((t / 29) % 3 - 1);
        in[2].btn  = ((t / 61) % 5 == 0) ? VB_BTN_PIN : 0u;
        vb_sim_step(&s1, in);
        vb_sim_step(&s2, in);
    }
    CHECK(memcmp(&s1, &s2, sizeof s1) == 0, "two sims from one snapshot diverged");
}

/* ------------------------------------------------------------------ */
/* §5.2 — swept collision. At heat 12 the ball crosses several of its own
 * radii per tick; not one of them may pass through a bar. */

static void test_tunneling(void) {
    head("tunneling at maximum heat (§5.2)");
    int through = 0, shots = 0;
    float top = vb_heat_speed(VB_HEAT_MAX) * VB_SNAP_SPD;
    int rod = vb_rod_of(1, VB_DEF);

    /* Fired dead at a paddle from close range, at every angle that still lands
     * on it and at every point across its face, with no wall in the path — so
     * a ball that ends up behind the bar got there by tunneling and by nothing
     * else. Also swept across bar speeds: a bar closing on the ball is the
     * case a naive overlap test misses. */
    for (int rv = -3; rv <= 3; rv++) {
        for (int a = -40; a <= 40; a++) {
            for (int y = -50; y <= 50; y++) {
                VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
                static VbSim s;
                vb_sim_init(&s, &cfg, 1);
                vb_sim_serve(&s, 0);
                for (int i = 0; i < VB_NRODS; i++) s.rods[i].off = 0.0f;

                VbRod *r = &s.rods[rod];
                float lo, hi;
                vb_pad_span(r, 1, VB_BALL_R, &lo, &hi);
                float face = (lo + hi) * 0.5f;
                float reach = (hi - lo) * 0.5f;

                float ang = (float)a * 0.5f * VB_DEG;
                VbBall *b = &s.balls[0];
                b->alive = 1;
                b->heat = VB_HEAT_MAX;
                b->v = v2(top * cosf(ang), top * sinf(ang));
                /* back-project from a point on the face so the shot lands on it */
                float land = face + (float)y / 50.0f * (reach - VB_BALL_R * 1.5f);
                float dx = 0.22f;
                b->p = v2(r->x - VB_PAD_HALF_X - VB_BALL_R - dx,
                          land - b->v.y * (dx / b->v.x));
                if (vb_absf(b->p.y) > VB_TABLE_HY - VB_BALL_R * 2.0f) continue;
                shots++;

                VbInput in[VB_NPLAYERS];
                memset(in, 0, sizeof in);
                int passed = 0;
                for (int t = 0; t < 40; t++) {
                    for (int i = 0; i < VB_NRODS; i++) {
                        s.rods[i].off = 0.0f;
                        s.rods[i].vel = (i == rod) ? (float)rv * 0.5f : 0.0f;
                    }
                    vb_sim_step(&s, in);
                    if (!s.balls[0].alive) break;
                    if (s.balls[0].p.x > r->x + VB_PAD_HALF_X) { passed = 1; break; }
                }
                through += passed;
            }
        }
    }
    CHECK(shots > 20000, "the tunneling sweep did not fire enough shots (%d)", shots);
    CHECK(through == 0, "%d of %d maximum-heat shots passed through a paddle",
          through, shots);
    printf("  %d shots at %.2f u/s (%.1f ball radii per tick), %d through\n",
           shots, top, top * VB_DT / VB_BALL_R, through);
}

/* ------------------------------------------------------------------ */
/* §2 — no rod may ever seal the table, at any offset it can reach. */

static void test_open_lane(void) {
    head("the open-lane law (§2, Pillar 4)");
    float worst = 1e9f;
    int worst_rod = -1;
    for (float scale = 0.75f; scale <= 1.45f; scale += 0.05f) {
        VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
        for (int t = 0; t < VB_NSIDES; t++) cfg.tilt[t].pad_scale = scale;
        static VbSim s;
        vb_sim_init(&s, &cfg, 1);
        for (int i = 0; i < VB_NRODS; i++) {
            for (int k = -60; k <= 60; k++) {
                s.rods[i].off = s.rods[i].travel * (float)k / 60.0f;
                float lane = vb_rod_open_lane(&s.rods[i]);
                if (lane < worst) { worst = lane; worst_rod = i; }
            }
        }
    }
    CHECK(worst >= VB_OPEN_LANE,
          "rod %d sealed to %.4f, under the %.4f law", worst_rod, worst, VB_OPEN_LANE);
    printf("  narrowest lane over every rod, offset and tilt: %.4f (law %.4f)\n",
           worst, VB_OPEN_LANE);

    /* and the goalie may never seal its own mouth */
    VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
    for (int t = 0; t < VB_NSIDES; t++) cfg.tilt[t].pad_scale = 1.45f;
    static VbSim s;
    vb_sim_init(&s, &cfg, 1);
    for (int side = 0; side < VB_NSIDES; side++) {
        const VbRod *gk = &s.rods[vb_rod_of(side, VB_GK)];
        float open = 2.0f * s.goal_half[side] - 2.0f * gk->half;
        CHECK(open >= VB_OPEN_LANE,
              "side %d goalie covers its mouth to %.4f", side, open);
    }
}

/* A gap the ball cannot fit through is not a lane, and the collision geometry
 * must close it — otherwise the ball wedges and buzzes there forever. */
static void test_no_wedge(void) {
    head("no sub-ball gaps in the collision geometry");
    VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
    static VbSim s;
    vb_sim_init(&s, &cfg, 1);
    float worst = 1e9f;
    for (int i = 0; i < VB_NRODS; i++) {
        for (int k = -80; k <= 80; k++) {
            s.rods[i].off = s.rods[i].travel * (float)k / 80.0f;
            const VbRod *r = &s.rods[i];
            float lo, hi;
            vb_pad_span(r, 0, VB_BALL_R, &lo, &hi);
            float bot = lo - (-VB_TABLE_HY);
            vb_pad_span(r, r->n - 1, VB_BALL_R, &lo, &hi);
            float top = VB_TABLE_HY - hi;
            if (bot > 0.0f && bot < worst) worst = bot;
            if (top > 0.0f && top < worst) worst = top;
        }
    }
    CHECK(worst >= VB_BALL_W,
          "a %.4f gap survives at the wall, narrower than the %.4f ball",
          worst, VB_BALL_W);
    printf("  narrowest surviving wall gap: %.4f (ball is %.4f)\n", worst, VB_BALL_W);
}

/* ------------------------------------------------------------------ */
/* the ball stays on the table, always, whatever the bars do to it */

static void test_containment(void) {
    head("nothing leaves the table except through a mouth");
    VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
    static VbSim s;
    vb_sim_init(&s, &cfg, 1);
    vb_sim_serve(&s, 0);
    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    unsigned rng = 12345;
    int escapes = 0;
    for (int t = 0; t < 240 * 120; t++) {
        /* thrash the bars: full-speed slams, both sides, every direction */
        in[0].axis = vb_rands(&rng);
        in[2].axis = vb_rands(&rng);
        in[0].btn = (vb_randf(&rng) < 0.05f) ? VB_BTN_FLICK : 0u;
        in[2].btn = (vb_randf(&rng) < 0.05f) ? VB_BTN_PIN : 0u;
        vb_sim_step(&s, in);
        if (s.goal_side >= 0) { vb_sim_serve(&s, 1 - s.goal_side); continue; }
        for (int i = 0; i < s.nballs; i++) {
            const VbBall *b = &s.balls[i];
            if (!b->alive) continue;
            if (vb_absf(b->p.x) > VB_TABLE_HX - b->radius + 1e-3f ||
                vb_absf(b->p.y) > VB_TABLE_HY - b->radius + 1e-3f) escapes++;
        }
    }
    CHECK(escapes == 0, "the ball was outside the table on %d ticks", escapes);
}

/* ------------------------------------------------------------------ */
/* The table is fair.
 *
 * Mirror the whole match through x -> -x and the two sides swap: side 0's
 * bars land exactly on side 1's, with the same roles and the same paddle
 * counts, because the interleave of §2 is symmetric. So the same inputs
 * handed to the other player must produce the mirrored ball, tick for tick.
 * STANDARD is a promise of a fair table, and this is what makes it one. */

static void test_mirror(void) {
    head("the table is fair under mirroring (§2, Pillar 3)");
    VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
    static VbSim a, b;
    vb_sim_init(&a, &cfg, 4242);
    vb_sim_init(&b, &cfg, 4242);
    vb_sim_serve(&a, 0);
    vb_sim_serve(&b, 1);

    VbInput ia[VB_NPLAYERS], ib[VB_NPLAYERS];
    memset(ia, 0, sizeof ia);
    memset(ib, 0, sizeof ib);

    float worst_x = 0.0f, worst_y = 0.0f;
    unsigned rng = 90210u;
    int diverged = -1;

    for (int t = 0; t < 240 * 45; t++) {
        /* one stream of play, handed to player 1 in A and to player 2 in B.
         * Mirroring flips x and leaves y alone, so the stick is unchanged. */
        VbInput p, q;
        p.axis = vb_rands(&rng);
        p.btn  = (vb_randf(&rng) < 0.04f ? VB_BTN_FLICK : 0u)
               | (vb_randf(&rng) < 0.03f ? VB_BTN_PIN : 0u)
               | (vb_randf(&rng) < 0.02f ? VB_BTN_CHIP : 0u);
        q.axis = vb_rands(&rng);
        q.btn  = (vb_randf(&rng) < 0.04f ? VB_BTN_FLICK : 0u)
               | (vb_randf(&rng) < 0.03f ? VB_BTN_PIN : 0u);

        ia[0] = p; ia[2] = q;
        ib[2] = p; ib[0] = q;

        vb_sim_step(&a, ia);
        vb_sim_step(&b, ib);

        if (a.goal_side >= 0 || b.goal_side >= 0) {
            CHECK(a.goal_side >= 0 && b.goal_side >= 0,
                  "a goal went in on one table and not its mirror (tick %d)", t);
            if (a.goal_side >= 0 && b.goal_side >= 0)
                CHECK(a.goal_side == 1 - b.goal_side,
                      "the mirrored goal was scored by the wrong side");
            vb_sim_serve(&a, 0);
            vb_sim_serve(&b, 1);
            continue;
        }

        const VbBall *ba = &a.balls[0], *bb = &b.balls[0];
        if (!ba->alive || !bb->alive) continue;
        float dx = vb_absf(ba->p.x + bb->p.x);      /* mirrored: x = -x     */
        float dy = vb_absf(ba->p.y - bb->p.y);      /* and y is unchanged   */
        if (dx > worst_x) worst_x = dx;
        if (dy > worst_y) worst_y = dy;
        if ((dx > 0.02f || dy > 0.02f) && diverged < 0) diverged = t;

        if (ba->heat != bb->heat && diverged < 0) diverged = t;
        if (diverged >= 0) break;
    }

    CHECK(diverged < 0,
          "the two ends of the table diverged at tick %d (dx %.4f, dy %.4f) — "
          "identical play does not produce identical results on both sides",
          diverged, worst_x, worst_y);
    printf("  45 s of mirrored play: worst drift %.5f in x, %.5f in y\n",
           worst_x, worst_y);
}

/* ------------------------------------------------------------------ */
/* §5.3, §5.4 — the strike vocabulary actually does what it says */

/* Drive one rod into a ball arriving at it and report the outgoing ball.
 * `axis` steers the bar through the contact, which is the only way to get
 * lateral bar velocity at the moment of impact — and lateral bar velocity is
 * what english is made of. */
static VbBall strike_once_axis(int kind, float axis, float dist, int heat,
                               int *heat_out) {
    VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
    static VbSim s;
    vb_sim_init(&s, &cfg, 1);
    vb_sim_serve(&s, 0);
    int rod = vb_rod_of(1, VB_ATK);
    for (int i = 0; i < VB_NRODS; i++) s.rods[i].off = 0.0f;
    /* start the bar at one end of its travel so it is still accelerating
     * across the ball's path when the ball gets there */
    s.rods[rod].off = (axis > 0.0f) ? -s.rods[rod].travel
                    : (axis < 0.0f) ?  s.rods[rod].travel : 0.0f;
    VbBall *b = &s.balls[0];
    b->alive = 1;
    b->heat = heat;
    b->p = v2(s.rods[rod].x + dist, 0.0f);
    b->v = v2(-vb_heat_speed(heat), 0.0f);

    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    /* the defending side is slot 2 */
    int pin_seen = -1;
    for (int t = 0; t < 400; t++) {
        s.active_rod[2] = rod;
        s.handoff_lock[2] = 1000;
        in[2].axis = axis;
        in[2].btn = 0;

        /* press when the ball is close enough that the windup lands on it,
         * which is what a player does and what the rod machine expects */
        float gap = vb_absf(s.balls[0].p.x - s.rods[rod].x);
        int near = (gap < 0.11f);
        switch (kind) {
            case VB_STRIKE_FLICK: if (near) in[2].btn = VB_BTN_FLICK; break;
            case VB_STRIKE_CHIP:  if (near) in[2].btn = VB_BTN_FLICK | VB_BTN_CHIP; break;
            case VB_STRIKE_BUNT:
                /* a tap of the catch, released well before contact */
                if (gap < 0.24f && gap > 0.17f) in[2].btn = VB_BTN_PIN;
                break;
            case VB_STRIKE_SNAP:
                if (pin_seen < 0) { if (gap < 0.16f) in[2].btn = VB_BTN_PIN; }
                else if (t - pin_seen < VB_TICKS(0.35f)) in[2].btn = VB_BTN_PIN;
                break;
            default: break;
        }

        vb_sim_step(&s, in);
        for (int e = 0; e < s.nev; e++) {
            int ty = s.ev[e].type;
            if (ty == VB_EV_PIN && pin_seen < 0) pin_seen = t;
            if (ty == VB_EV_SNAP && kind == VB_STRIKE_SNAP) {
                if (heat_out) *heat_out = s.balls[0].heat;
                return s.balls[0];
            }
            if ((ty == VB_EV_HIT || ty == VB_EV_CHIP) && s.ev[e].i == kind) {
                if (heat_out) *heat_out = s.balls[0].heat;
                return s.balls[0];
            }
        }
    }
    if (heat_out) *heat_out = -1;
    VbBall none;
    memset(&none, 0, sizeof none);
    return none;
}

static VbBall strike_once(int kind, int heat, int *heat_out) {
    return strike_once_axis(kind, 0.0f, 0.30f, heat, heat_out);
}

/* the strongest english this bar can put on the ball, over every approach */
static float best_curve(float axis) {
    float best = 0.0f;
    for (int d = 0; d < 24; d++) {
        VbBall b = strike_once_axis(VB_STRIKE_FLICK, axis,
                                    0.10f + (float)d * 0.012f, 4, NULL);
        if (b.curve_ticks > 0 && v2len(b.curve) > best) best = v2len(b.curve);
    }
    return best;
}

static void test_strikes(void) {
    head("the strike vocabulary (§5.3)");

    int h = -1;
    VbBall flick = strike_once(VB_STRIKE_FLICK, 4, &h);
    CHECK(h == 5, "a flick did not raise heat one step (got %d)", h);
    CHECK(flick.v.x > 0.0f, "a flick sent the ball the wrong way");
    float flick_sp = v2len(flick.v);
    CHECK(flick_sp > vb_heat_speed(5) * 0.8f,
          "a flick at heat 5 left at %.2f, well under the ladder", flick_sp);

    /* english: lateral bar velocity at contact becomes curve */
    float still = best_curve(0.0f);
    float swept = best_curve(1.0f);
    CHECK(swept > 0.0f, "a moving bar imparted no english at all");
    CHECK(swept > still * 2.0f + 0.05f,
          "english did not scale with bar velocity (still %.3f, swept %.3f)",
          still, swept);
    printf("  english: still bar %.3f, swept bar %.3f\n", still, swept);

    /* the snap: the fastest shot in the game, and nearly straight */
    int hs = -1;
    VbBall snap = strike_once(VB_STRIKE_SNAP, 4, &hs);
    CHECK(hs >= 0, "the pin never released into a snap");
    if (hs >= 0) {
        CHECK(v2len(snap.v) > flick_sp,
              "the snap (%.2f) was not faster than the flick (%.2f)",
              v2len(snap.v), flick_sp);
        CHECK(v2len(snap.curve) < swept,
              "the snap should be laser-straight; its counter is positioning");
    }

    /* the bunt cools the ball and drops it short */
    int hb = -1;
    VbBall bunt = strike_once(VB_STRIKE_BUNT, 6, &hb);
    if (hb >= 0) {
        CHECK(hb == 5, "a bunt did not take a step of heat off (got %d)", hb);
        CHECK(v2len(bunt.v) < flick_sp, "a bunt was not softer than a flick");
    } else {
        CHECK(0, "the bunt never connected");
    }

    /* the chip leaves the glass and comes back down */
    VbBall chip = strike_once(VB_STRIKE_CHIP, 4, NULL);
    CHECK(chip.vz > 0.0f || chip.z > 0.0f, "the chip never left the table");
    CHECK(chip.chip_pass >= 0, "the chip was not licensed to clear a rod");
}

static void test_heat_curve(void) {
    head("the heat ladder (§4, §5.4)");
    CHECK(vb_heat_speed(0) < vb_heat_speed(12), "the ladder does not climb");
    float t0 = 2.0f * VB_TABLE_HX / vb_heat_speed(0);
    float t12 = 2.0f * VB_TABLE_HX / vb_heat_speed(VB_HEAT_MAX);
    CHECK(t0 > 1.8f && t0 < 2.4f, "step 0 crosses the table in %.2fs, not ~2.1s", t0);
    CHECK(t12 > 0.45f && t12 < 0.70f, "step 12 crosses in %.2fs, not ~0.55s", t12);
    printf("  step 0 crosses in %.2fs, step 12 in %.2fs\n", t0, t12);

    CHECK(vb_heat_speed(13) == vb_heat_speed(12), "the heat cap does not hold");
    CHECK(vb_heat_up(VB_HEAT_MAX, 0) == VB_HEAT_MAX, "heat climbed past the cap");
    CHECK(vb_heat_up(5, 6) == 6 && vb_heat_up(6, 6) == 6,
          "a personal heat cap from Table Tilt was ignored");
    CHECK(vb_heat_down(0) == 0, "heat went negative");

    /* the mercy beat arms at step 8 and not before */
    CHECK(vb_heat_mercy(7) == 0, "the mercy beat armed below step 8");
    CHECK(vb_heat_mercy(8) == VB_MERCY_TICKS, "the mercy beat is missing at step 8");
    CHECK(vb_heat_scorcher(9) && !vb_heat_scorcher(8), "the scorcher line moved");

    /* and a struck ball at step 8+ really does hold its breath */
    int h = -1;
    VbBall hot = strike_once(VB_STRIKE_FLICK, 8, &h);
    CHECK(hot.mercy > 0 || v2len(hot.v) > 0.0f, "the hot strike vanished");
    printf("  mercy beat at step 9: %d ticks held\n", hot.mercy);
}

/* ------------------------------------------------------------------ */
/* §5.5 — the foosball law */

static void test_stall(void) {
    head("the anti-stall referee and the pin clock (§5.5)");
    VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
    static VbSim s;
    vb_sim_init(&s, &cfg, 1);
    vb_sim_serve(&s, 0);

    /* Sit on the ball in our own half and never advance it — the referee is
     * counting possession, so the ball is held there every tick rather than
     * merely started slow (a slow ball just accelerates off the floor and
     * leaves, which is correctly not a stall). */
    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    int pulsed = -1;
    for (int t = 0; t < VB_TICKS(6.0f); t++) {
        s.balls[0].p = v2(-0.70f, 0.0f);
        s.balls[0].v = v2(-0.05f, 0.02f);
        s.balls[0].last_side = 0;
        vb_sim_step(&s, in);
        for (int e = 0; e < s.nev; e++)
            if (s.ev[e].type == VB_EV_STALL && pulsed < 0) pulsed = t;
        if (pulsed >= 0) break;
    }
    CHECK(pulsed >= 0, "the referee never pulsed on a stalled ball");
    if (pulsed >= 0)
        CHECK(pulsed <= VB_STALL_TICKS + 4,
              "the referee took %d ticks, past the %d-tick law", pulsed, VB_STALL_TICKS);

    /* the pin hard-caps at two seconds even if the button is never released */
    vb_sim_init(&s, &cfg, 2);
    vb_sim_serve(&s, 0);
    int rod = vb_rod_of(1, VB_DEF);
    s.rods[rod].off = 0.0f;
    s.balls[0].p = v2(s.rods[rod].x - 0.20f, vb_pad_y(&s.rods[rod], 1));
    s.balls[0].v = v2(1.2f, 0.0f);
    s.balls[0].heat = 4;
    memset(in, 0, sizeof in);
    int pinned_at = -1, released_at = -1;
    for (int t = 0; t < VB_TICKS(6.0f); t++) {
        in[2].btn = VB_BTN_PIN;               /* never let go */
        s.active_rod[2] = rod;
        s.handoff_lock[2] = 1000;
        for (int i = 0; i < VB_NRODS; i++) if (i != rod) s.rods[i].off = 0.42f;
        vb_sim_step(&s, in);
        for (int e = 0; e < s.nev; e++) {
            if (s.ev[e].type == VB_EV_PIN && pinned_at < 0) pinned_at = t;
            if (s.ev[e].type == VB_EV_SNAP && released_at < 0) released_at = t;
        }
        if (released_at >= 0) break;
    }
    CHECK(pinned_at >= 0, "the ball was never pinned");
    CHECK(released_at >= 0, "a held pin was never forced out by the shot clock");
    if (pinned_at >= 0 && released_at >= 0) {
        int held = released_at - pinned_at;
        CHECK(held <= VB_PIN_CLOCK + 4, "the pin held %d ticks, past the %d cap",
              held, VB_PIN_CLOCK);
        printf("  pin held %d ticks (cap %d)\n", held, VB_PIN_CLOCK);
    }
}

/* ------------------------------------------------------------------ */
/* §3.1 — GLIDE is one axis and one button, and it will not hand you your own
 * goal while the assist is on. */

static void test_assist(void) {
    head("GLIDE assist and the no-own-goal rule (§3.1, §5.5)");
    VbSimCfg cfg = base_cfg(VB_SCHEME_GLIDE);
    static VbSim s;
    vb_sim_init(&s, &cfg, 1);
    vb_sim_serve(&s, 0);
    int rod = vb_rod_of(0, VB_DEF);
    s.balls[0].p = v2(-0.55f, 0.0f);

    /* a flick aimed squarely at our own goal, from our own half */
    V2 back = v2(-1.0f, 0.0f);
    V2 fixed = vb_assist_dir(&s, rod, 0, back, 1.0f);
    CHECK(fixed.x > 0.0f, "the assist let a GLIDE player shoot at their own goal");

    /* the bend is capped: assist helps the shot you took, it does not take it */
    V2 wide = v2norm(v2(1.0f, 0.6f));
    V2 bent = vb_assist_dir(&s, rod, 0, wide, 1.0f);
    float a0 = atan2f(wide.y, wide.x), a1 = atan2f(bent.y, bent.x);
    float d = a1 - a0;
    while (d >  VB_PI) d -= VB_TAU;
    while (d < -VB_PI) d += VB_TAU;
    CHECK(vb_absf(d) <= VB_ASSIST_BEND + 1e-3f,
          "the assist bent the shot by %.1f degrees, past the %.1f cap",
          vb_absf(d) / VB_DEG, VB_ASSIST_BEND / VB_DEG);

    /* with the assist off it does nothing at all */
    V2 none = vb_assist_dir(&s, rod, 0, wide, 0.0f);
    CHECK(vb_absf(none.x - wide.x) < 1e-6f && vb_absf(none.y - wide.y) < 1e-6f,
          "assist 0 still moved the shot");
}

/* ------------------------------------------------------------------ */
/* §7 — the honesty audit. */

static void test_ai_honesty(void) {
    head("AI honesty audit (§7)");

    /* 1. it emits nothing but a VbInput a pad could emit */
    VbSimCfg cfg = base_cfg(VB_SCHEME_GRIP);
    static VbSim s;
    vb_sim_init(&s, &cfg, 3);
    vb_sim_serve(&s, 0);
    VbAI a;
    vb_ai_init(&a, 4, 2, 2, 99);
    VbInput in[VB_NPLAYERS];
    memset(in, 0, sizeof in);
    int bad_axis = 0, bad_btn = 0;
    const unsigned legal = VB_BTN_FLICK | VB_BTN_PIN | VB_BTN_CHIP
                         | VB_BTN_PREV | VB_BTN_NEXT;
    for (int t = 0; t < 240 * 30; t++) {
        in[2] = vb_ai_think(&a, &s);
        if (in[2].axis < -1.0f || in[2].axis > 1.0f) bad_axis++;
        if (in[2].btn & ~legal) bad_btn++;
        vb_sim_step(&s, in);
        if (s.goal_side >= 0) vb_sim_serve(&s, 1 - s.goal_side);
    }
    CHECK(bad_axis == 0, "the AI pushed the stick past its stops %d times", bad_axis);
    CHECK(bad_btn == 0, "the AI pressed a button that does not exist %d times", bad_btn);

    /* 2. it never touches a rod it does not own */
    vb_sim_init(&s, &cfg, 4);
    vb_sim_serve(&s, 0);
    vb_ai_init(&a, 9, 2, 2, 5);
    float before[VB_NRODS];
    int moved_enemy = 0;
    memset(in, 0, sizeof in);
    for (int t = 0; t < 240 * 20; t++) {
        for (int i = 0; i < VB_NRODS; i++) before[i] = s.rods[i].off;
        in[0].axis = 0.0f; in[0].btn = 0;
        in[2] = vb_ai_think(&a, &s);
        vb_sim_step(&s, in);
        for (int i = 0; i < VB_NRODS; i++)
            if (s.rods[i].side == 0 && vb_absf(s.rods[i].off - before[i]) > 1e-6f)
                moved_enemy++;
        if (s.goal_side >= 0) vb_sim_serve(&s, 1 - s.goal_side);
    }
    CHECK(moved_enemy == 0, "the AI moved a rod belonging to the other side (%d ticks)",
          moved_enemy);

    /* 3. Difficulty tunes latency and noise only — never the physics. Checked
     * for EVERY personality, because a per-persona reaction time that gets
     * clamped can quietly flatten the whole ladder onto one speed. */
    int lo_ms = 9999, hi_ms = 0;
    for (int p = 0; p < VB_NPERSONAS; p++) {
        VbAI t0, t1, t2;
        vb_ai_init(&t0, p, 0, 0, 1);
        vb_ai_init(&t1, p, 1, 0, 1);
        vb_ai_init(&t2, p, 2, 0, 1);
        CHECK(t0.lag >= t1.lag && t1.lag >= t2.lag,
              "%s does not get quicker with difficulty (%d/%d/%d ticks)",
              VB_PERSONAS[p].name, t0.lag, t1.lag, t2.lag);
        CHECK(t0.lag > t2.lag, "%s reacts identically on easy and hard",
              VB_PERSONAS[p].name);
        CHECK(t0.noise_mul > t2.noise_mul, "%s aims the same on easy and hard",
              VB_PERSONAS[p].name);
        int ms0 = t0.lag * 1000 / VB_TICK_HZ, ms2 = t2.lag * 1000 / VB_TICK_HZ;
        CHECK(ms2 >= 179 && ms0 <= 321,
              "%s reacts in %d..%d ms, outside the 180-320 ms model",
              VB_PERSONAS[p].name, ms2, ms0);
        if (ms2 < lo_ms) lo_ms = ms2;
        if (ms0 > hi_ms) hi_ms = ms0;
    }
    CHECK(lo_ms <= 185 && hi_ms >= 310,
          "the ladder only spans %d..%d ms of the 180-320 ms model", lo_ms, hi_ms);
    printf("  reaction latency across the ladder: %d..%d ms\n", lo_ms, hi_ms);

    /* the same persona is quicker than a slower one at the same tier */
    VbAI wall, finale;
    vb_ai_init(&wall, 0, 1, 0, 1);
    vb_ai_init(&finale, 9, 1, 0, 1);
    CHECK(finale.lag < wall.lag,
          "the finale does not read the table faster than the Wall at one tier");

    /* 4. every personality is present, named, and distinguishable */
    for (int i = 0; i < VB_NPERSONAS; i++) {
        CHECK(VB_PERSONAS[i].name && VB_PERSONAS[i].name[0], "persona %d has no name", i);
        CHECK(VB_PERSONAS[i].style && VB_PERSONAS[i].style[0], "persona %d has no style", i);
        CHECK(VB_PERSONAS[i].banter[0] && VB_PERSONAS[i].banter[1],
              "persona %d is missing its two lines", i);
        for (int j = i + 1; j < VB_NPERSONAS; j++)
            CHECK(strcmp(VB_PERSONAS[i].name, VB_PERSONAS[j].name) != 0,
                  "two personalities share the name %s", VB_PERSONAS[i].name);
    }
}

/* ------------------------------------------------------------------ */
/* §14.1 — the gray-box gate, measured rather than asserted: two AIs at the
 * same table must produce goals AND rallies, not a stalemate either way. */

static void test_rally_shape(void) {
    head("rally shape (§14.1, §14.3)");
    for (int scheme = 0; scheme < 2; scheme++) {
        VbSimCfg cfg = base_cfg(scheme);
        static VbMatch m;
        VbMatchCfg mc = { VB_MODE_VERSUS, VB_RULES_STANDARD, 10, 0 };
        vb_match_init(&m, &mc, &cfg, 4242);
        VbAI a, b;
        vb_ai_init(&a, 0, 1, 0, 11);
        vb_ai_init(&b, 9, 1, 2, 12);
        VbInput in[VB_NPLAYERS];
        memset(in, 0, sizeof in);
        int rallies = 0, contacts = 0, longest = 0, top = 0;
        int secs = 300;
        for (int t = 0; t < 240 * secs; t++) {
            in[0] = vb_ai_think(&a, &m.sim);
            in[2] = vb_ai_think(&b, &m.sim);
            int was = m.rally, wasg = m.score[0] + m.score[1];
            vb_match_step(&m, in);
            if (m.rally_heat > top) top = m.rally_heat;
            if (m.score[0] + m.score[1] > wasg) {
                rallies++; contacts += was;
                if (was > longest) longest = was;
            }
            if (m.winner >= 0) { secs = t / 240 + 1; break; }
        }
        printf("  %-5s %d-%d, %d points in %ds, mean rally %.0f contacts, longest %d, top heat %d\n",
               scheme ? "GRIP" : "GLIDE", m.score[0], m.score[1], rallies, secs,
               rallies ? (double)contacts / rallies : 0.0, longest, top);
        CHECK(rallies >= 3, "%s produced only %d points in %ds — the table is sealed",
              scheme ? "GRIP" : "GLIDE", rallies, secs);
        CHECK(longest >= 8, "%s never produced a rally worth watching (longest %d)",
              scheme ? "GRIP" : "GLIDE", longest);
        CHECK(top >= 5, "%s never got the ball hot (top heat %d)",
              scheme ? "GRIP" : "GLIDE", top);
    }
}

int main(void) {
    printf("VOLLEYBAR — sim suite\n");
    test_determinism();
    test_tunneling();
    test_open_lane();
    test_no_wedge();
    test_containment();
    test_mirror();
    test_strikes();
    test_heat_curve();
    test_stall();
    test_assist();
    test_ai_honesty();
    test_rally_shape();
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
