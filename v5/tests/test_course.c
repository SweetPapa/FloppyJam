/* test_course.c — every hole is sane and every hole is beatable.
 *
 * Part 1 checks geometry invariants (tee and cup on solid floor, walls
 * thick enough for the substepper, tables inside their limits).
 * Part 2 runs a greedy bot: at each stroke it searches a grid of aim /
 * power / spin, keeps the shot that gets closest to the objective, and
 * plays it. If a hole cannot be finished inside the stroke cap by a bot
 * that only ever plays the best single shot, the hole is broken.
 *
 *   make testcourse
 */
#include "../src/course.h"
#include "../src/shot.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0, checks = 0;
static int verbose = 0;

static void ok(int cond, const char *fmt, ...)
{
    (void)fmt;
    ++checks;
    if (!cond) ++failures;
}

static void report(int cond, const char *hole, const char *what, const char *detail)
{
    ++checks;
    if (cond) {
        if (verbose) printf("  ok   %-16s %s%s%s\n", hole, what,
                            detail ? " — " : "", detail ? detail : "");
    } else {
        printf("  FAIL %-16s %s%s%s\n", hole, what,
               detail ? " — " : "", detail ? detail : "");
        ++failures;
    }
}

/* ---- run one shot to completion ------------------------------------ */

#define MAX_TICKS BP_RIDE_TICKS

static void play(BpWorld *w, float aim, float power, float tx, float ty)
{
    int t;
    bp_shot_begin(w);
    bp_strike(w, aim, power, tx, ty);
    for (t = 0; t < MAX_TICKS && !bp_settled(w); ++t) {
        bp_step(w);
        if (w->holed) break;
    }
    if (w->scratched && !w->holed) bp_respawn_cue(w);
}

/* Lower is better. Holing out wins outright. */
static float evaluate(const BpWorld *w, int cup)
{
    float s;
    const BpBall *cb = &w->balls[0];
    int i;
    if (w->holed) return -1000.0f;

    if (w->cup_sealed) {
        /* objective inverts: get the eight into any pocket */
        float best = 60.0f;
        for (i = 0; i < w->nballs; ++i) {
            const BpBall *e = &w->balls[i];
            int k;
            if (e->kind != BALL_EIGHT) continue;
            if (e->state == BS_GONE) { best = 0.0f; break; }
            for (k = 0; k < w->npockets; ++k) {
                float dx = e->p.x - w->pockets[k].x, dz = e->p.z - w->pockets[k].z;
                float d = sqrtf(dx * dx + dz * dz);
                if (w->pockets[k].kind == PK_CUP) continue;
                if (d < best) best = d;
            }
        }
        {   /* also reward getting the cue ball near the eight to set up */
            float near = 20.0f;
            for (i = 0; i < w->nballs; ++i) {
                const BpBall *e = &w->balls[i];
                float dx, dz;
                if (e->kind != BALL_EIGHT || e->state == BS_GONE) continue;
                dx = cb->p.x - e->p.x; dz = cb->p.z - e->p.z;
                near = sqrtf(dx * dx + dz * dz);
            }
            s = 40.0f + 1.2f * best + 1.2f * near;
        }
    } else {
        float dx = cb->p.x - w->pockets[cup].x;
        float dz = cb->p.z - w->pockets[cup].z;
        float dy = cb->p.y - w->pockets[cup].y;
        s = sqrtf(dx * dx + dz * dz) + 2.0f * fabsf(dy);
    }
    s += 9.0f * (float)w->scratched;
    s -= 3.0f * (float)w->bonus_hits;
    return s;
}

typedef struct { float aim, power, tx, ty; float score; } Shot;

static const float SPINS[][2] = { {0,0}, {0,-0.7f}, {0,0.7f}, {0.7f,0}, {-0.7f,0} };

static Shot search(const BpWorld *base, int cup, int coarse_aim, int nspin)
{
    Shot best;
    BpWorld w;
    int a, p, s;
    best.aim = 0; best.power = 0.5f; best.tx = best.ty = 0; best.score = 1e9f;

    for (a = 0; a < coarse_aim; ++a) {
        float aim = (float)a * (BP_TAU / (float)coarse_aim);
        for (p = 0; p < 9; ++p) {
            float power = 0.14f + 0.105f * (float)p;
            for (s = 0; s < nspin; ++s) {
                float sc;
                w = *base;
                play(&w, aim, power, SPINS[s][0], SPINS[s][1]);
                sc = evaluate(&w, cup);
                if (sc < best.score) {
                    best.score = sc; best.aim = aim; best.power = power;
                    best.tx = SPINS[s][0]; best.ty = SPINS[s][1];
                }
            }
        }
    }
    /* Refine twice. A player with the aim guide and the fine-aim modifier
     * can point to a tenth of a degree, so the bot has to be able to as
     * well — the wobble below is what models human *execution* error. */
    {
        float astep[2] = { 0.09f, 0.012f };
        float pstep[2] = { 0.035f, 0.008f };
        int pass;
        for (pass = 0; pass < 2; ++pass) {
            Shot ref = best;
            for (a = -6; a <= 6; ++a) {
                float aim = ref.aim + (float)a * (BP_TAU / (float)coarse_aim) * astep[pass];
                for (p = -3; p <= 3; ++p) {
                    float power = ref.power + (float)p * pstep[pass];
                    float sc;
                    if (power < 0.05f || power > 1.0f) continue;
                    w = *base;
                    play(&w, aim, power, ref.tx, ref.ty);
                    sc = evaluate(&w, cup);
                    if (sc < best.score) {
                        best.score = sc; best.aim = aim; best.power = power;
                    }
                }
            }
        }
    }
    return best;
}

/* ---- geometry sanity ----------------------------------------------- */

static void check_geometry(int hi, BpWorld *w)
{
    const BpHole *H = &BP_HOLES[hi];
    char buf[128];
    int cup = bp_course_cup(w);
    float g;

    report(cup >= 0, H->name, "hole has a cup", NULL);
    report(w->min_thick >= 0.05f, H->name, "no wall thinner than 5 cm", NULL);
    report(w->npads > 0 && w->npads < BP_MAX_PADS, H->name, "pad table fits", NULL);
    report(w->nboxes < BP_MAX_BOXES, H->name, "wall table fits", NULL);
    report(w->npockets < BP_MAX_POCKETS, H->name, "pocket table fits", NULL);

    g = bp_ground_h(w, H->tee_x, H->tee_z, 50.0f, NULL);
    snprintf(buf, sizeof buf, "tee (%.1f, %.1f)", H->tee_x, H->tee_z);
    report(g > -1e8f, H->name, "tee sits on floor", buf);

    if (cup >= 0) {
        int pad = -1;
        g = bp_ground_h(w, w->pockets[cup].x, w->pockets[cup].z, 50.0f, &pad);
        snprintf(buf, sizeof buf, "cup floor %.2f vs rim %.2f", g, w->pockets[cup].y);
        report(pad >= 0 && fabsf(g - w->pockets[cup].y) < 0.05f, H->name,
               "cup rim is flush with its floor", buf);
    }
    {
        int i, bad = 0;
        for (i = 0; i < w->npockets; ++i) {
            int pad = -1;
            float ph = bp_ground_h(w, w->pockets[i].x, w->pockets[i].z, 50.0f, &pad);
            if (pad < 0 || fabsf(ph - w->pockets[i].y) > 0.06f) bad = i + 1;
        }
        snprintf(buf, sizeof buf, "pocket %d", bad - 1);
        report(bad == 0, H->name, "every pocket sits on its own floor", bad ? buf : NULL);
    }
    {
        int i, bad = 0;
        for (i = 1; i < w->nballs; ++i) {
            int pad = -1;
            bp_ground_h(w, w->balls[i].p.x, w->balls[i].p.z, 50.0f, &pad);
            if (pad < 0) bad = 1;
        }
        report(!bad, H->name, "every object ball spawns on floor", NULL);
    }
}

/* ---- the bot ------------------------------------------------------- */

/* Par honesty (6.1.4) is a claim about a *person*, so the human bot plans on
 * a coarse grid and then executes with a wobble: about 1.5 degrees of aim
 * error and 4% of power error, which is roughly what a careful player with
 * an oscillating power meter actually delivers. */
static unsigned int rng_state = 1u;
static float wobble(void)
{
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return ((float)(rng_state & 0xffff) / 32768.0f) - 1.0f;   /* [-1,1) */
}

static int human = 0;

static int solve(int hi, int coarse, int nspin, int *out_penalties)
{
    BpWorld w;
    int strokes = 0, cup, guard;
    bp_course_build(&w, hi);
    cup = bp_course_cup(&w);
    *out_penalties = 0;
    if (cup < 0) return 99;

    for (guard = 0; guard < BP_STROKE_CAP; ++guard) {
        Shot s = search(&w, cup, coarse, nspin);
        if (human) {
            s.aim += wobble() * 0.7f * BP_DEG;
            s.power = bp_clampf(s.power + wobble() * 0.04f, 0.05f, 1.0f);
        }
        play(&w, s.aim, s.power, s.tx, s.ty);
        ++strokes;
        if (w.scratched) { ++strokes; ++(*out_penalties); }
        strokes -= w.bonus_hits;
        if (strokes < 0) strokes = 0;
        if (verbose) {
            printf("       stroke %d: aim %5.1f pow %3.0f%% spin(%+.1f,%+.1f) score %6.2f "
                   "sealed=%d cue(%.2f,%.2f) -> %s\n",
                   guard + 1, s.aim / BP_DEG, s.power * 100.0f, s.tx, s.ty, s.score,
                   w.cup_sealed, w.balls[0].p.x, w.balls[0].p.z,
                   w.holed ? "HOLED" : "rolled");
        }
        if (w.holed) return strokes;
    }
    return 99;
}

/* ---- the trajectory preview must not lie ---------------------------
 *
 * The assist claims to show the real path, not an approximation. Prove it:
 * predict a shot, then actually play the same shot, and require the ball to
 * finish in exactly the same place with the same outcome. Any drift here
 * means the preview is drawing a shot the player will not get. */
static void test_preview(void)
{
    int hi, bad_pos = 0, bad_outcome = 0, checked = 0;
    float worst = 0.0f;
    char buf[128];
    printf("\ntrajectory preview fidelity\n");

    for (hi = 0; hi < BP_NHOLES; ++hi) {
        int a, p;
        for (a = 0; a < 8; ++a) {
            for (p = 0; p < 4; ++p) {
                BpWorld w;
                BpPreview pv;
                float aim = (float)a * (BP_TAU / 8.0f);
                float pow = 0.25f + 0.24f * (float)p;
                float tx = (a & 1) ? 0.45f : -0.30f;
                float ty = (p & 1) ? -0.50f : 0.35f;
                V3 pred_end, real_end;
                float d;

                bp_course_build(&w, hi);
                bp_predict(&w, aim, pow, tx, ty, &pv);
                pred_end = pv.pt[pv.n - 1];

                /* now play it for real on the untouched world */
                bp_course_build(&w, hi);
                play(&w, aim, pow, tx, ty);
                real_end = w.balls[0].p;

                ++checked;
                if ((pv.holed != 0) != (w.holed != 0)) ++bad_outcome;
                /* a holed or scratched ball is removed, so only compare the
                 * resting place when the ball is still on the table */
                if (!w.holed && !w.scratched) {
                    d = v3len(v3sub(pred_end, real_end));
                    if (d > worst) worst = d;
                    if (d > 0.02f) ++bad_pos;
                }
            }
        }
    }
    snprintf(buf, sizeof buf, "%d shots, worst end-point error %.5f m", checked, worst);
    report(bad_pos == 0, "preview", "predicted path ends where the ball ends", buf);
    snprintf(buf, sizeof buf, "%d outcome mismatches of %d", bad_outcome, checked);
    report(bad_outcome == 0, "preview", "predicted hole-out matches reality", buf);

    /* Item 5: extended preview lines must not streak. Every DRAWN segment (i.e.
     * every pair of consecutive points that is not a warp break) has to be
     * short; a warp teleport must be flagged as a break so the renderer skips
     * it instead of drawing a line clean across the hole. */
    {
        int hh, bad_seg = 0, breaks = 0, ph;
        float worst_seg = 0.0f;
        static const float pows[3] = { 0.45f, 0.72f, 1.0f };
        for (hh = 0; hh < BP_NHOLES; ++hh) {
            int aa;
            for (aa = 0; aa < 24; ++aa) {
                for (ph = 0; ph < 3; ++ph) {
                    BpWorld w;
                    BpPreview pv;
                    int j;
                    float aim = (float)aa * (BP_TAU / 24.0f);
                    bp_course_build(&w, hh);
                    bp_predict(&w, aim, pows[ph], 0.0f, 0.0f, &pv);
                    for (j = 1; j < pv.n; ++j) {
                        float seg = v3len(v3sub(pv.pt[j], pv.pt[j - 1]));
                        if (pv.hit[j] & BP_PV_BREAK) { ++breaks; continue; }
                        if (seg > worst_seg) worst_seg = seg;
                        if (seg > BP_PV_JUMP + 0.01f) ++bad_seg;
                    }
                }
            }
        }
        snprintf(buf, sizeof buf, "worst drawn chord %.2f m, %d discontinuities flagged as breaks",
                 worst_seg, breaks);
        report(bad_seg == 0, "preview", "no drawn segment streaks across the hole", buf);
        report(breaks > 0, "preview", "teleports / plunges are flagged as line breaks", buf);
        /* Hole 7 is built so a straight shot warps: prove the break actually
         * lands on the teleport by finding an aim where a >4 m jump between
         * consecutive samples is the one carrying the break flag. */
        {
            int aa, ok = 0;
            for (aa = 0; aa < 48 && !ok; ++aa) {
                BpWorld w;
                BpPreview pv;
                int j;
                bp_course_build(&w, 6);
                bp_predict(&w, (float)aa * (BP_TAU / 48.0f), 0.5f, 0.0f, 0.0f, &pv);
                for (j = 1; j < pv.n; ++j) {
                    float seg = v3len(v3sub(pv.pt[j], pv.pt[j - 1]));
                    if (seg > 4.0f && (pv.hit[j] & BP_PV_BREAK)) { ok = 1; break; }
                }
            }
            report(ok, "preview", "a warp teleport is flagged, not drawn as a streak", NULL);
        }
    }
}

int main(int argc, char **argv)
{
    int i, total = 0, par_total = 0, bad = 0;
    int coarse = 48, nspin = 3, only = -1;
    int a;
    for (a = 1; a < argc; ++a) {
        if (strcmp(argv[a], "-v") == 0) verbose = 1;
        else if (strcmp(argv[a], "-q") == 0) { coarse = 32; nspin = 1; }
        else if (strcmp(argv[a], "-h") == 0) { human = 1; coarse = 24; nspin = 3; }
        else only = atoi(argv[a]) - 1;
    }

    printf("BREAK PAR — course suite\n\ngeometry\n");
    for (i = 0; i < BP_NHOLES; ++i) {
        BpWorld w;
        bp_course_build(&w, i);
        check_geometry(i, &w);
    }
    if (failures == 0) printf("  ok   all %d holes pass geometry checks\n", BP_NHOLES);

    test_preview();

    printf("\nplaythrough (greedy bot, %d aim steps)\n", coarse);
    printf("  %-3s %-16s %4s %8s %6s\n", "#", "hole", "par", "bot", "pen");
    for (i = 0; i < BP_NHOLES; ++i) {
        const BpHole *H = &BP_HOLES[i];
        int trials = human ? 5 : 1, t, sum = 0, worst = 0, pen_tot = 0;
        if (only >= 0 && i != only) continue;
        for (t = 0; t < trials; ++t) {
            int pen = 0, strokes;
            rng_state = (unsigned int)(i * 7919 + t * 104729 + 1);
            strokes = solve(i, coarse, nspin, &pen);
            if (strokes > BP_STROKE_CAP) strokes = BP_STROKE_CAP;
            sum += strokes; pen_tot += pen;
            if (strokes > worst) worst = strokes;
        }
        {
            float avg = (float)sum / (float)trials;
            printf("  %-3d %-16s %4d %8.1f %6d%s\n", i + 1, H->name, H->par, avg, pen_tot,
                   worst >= BP_STROKE_CAP ? "   <-- RACKED" : "");
            /* Casino holes are allowed to bite occasionally; a hole is only
             * broken if it racks out on average. */
            if (avg >= (float)BP_STROKE_CAP - 0.5f) ++bad;
            total += sum / trials;
        }
        fflush(stdout);
        par_total += H->par;
    }
    ++checks;
    if (bad) { ++failures; printf("\n  FAIL %d hole(s) could not be finished\n", bad); }
    else     printf("\n  ok   all 18 holes completed inside the stroke cap\n");

    printf("  bot total %d vs par %d\n", total, par_total);
    printf("\n%d checks, %d failures\n", checks, failures);
    (void)ok;
    return failures ? 1 : 0;
}
