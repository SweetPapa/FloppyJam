/* test_physics.c — headless Definition-of-Done suite (Section 14/17.2).
 *
 * Links physics.c only: no raylib, no window, no audio device.
 *   make test
 */
#include "../src/physics.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

static void ok(int cond, const char *what, const char *detail)
{
    ++checks;
    if (cond) {
        printf("  ok   %s%s%s\n", what, detail ? " — " : "", detail ? detail : "");
    } else {
        printf("  FAIL %s%s%s\n", what, detail ? " — " : "", detail ? detail : "");
        ++failures;
    }
}

/* ---- world builders ------------------------------------------------ */

static void flat_table(BpWorld *w, float hx, float hz, int rails)
{
    BpPad *p;
    bp_world_reset(w);
    p = &w->pads[w->npads++];
    memset(p, 0, sizeof(*p));
    p->x0 = -hx; p->x1 = hx; p->z0 = -hz; p->z1 = hz;
    p->y0 = 0.0f; p->surf = SURF_FELT;
    p->rails = (unsigned char)(rails ? (PAD_W | PAD_E | PAD_N | PAD_S) : 0);
    w->nballs = 1;
    memset(&w->balls[0], 0, sizeof(BpBall));
    w->balls[0].p = v3(0.0f, BP_R, -hz + 0.5f);
    w->balls[0].home = w->balls[0].p;
    w->balls[0].kind = BALL_CUE;
    w->balls[0].state = BS_REST;
}

static int add_ball(BpWorld *w, float x, float z, int kind)
{
    int i = w->nballs++;
    memset(&w->balls[i], 0, sizeof(BpBall));
    w->balls[i].p = v3(x, BP_R, z);
    w->balls[i].home = w->balls[i].p;
    w->balls[i].kind = (unsigned char)kind;
    w->balls[i].state = BS_REST;
    return i;
}

static int run_to_rest(BpWorld *w, int max_ticks)
{
    int t = 0;
    while (t < max_ticks && !bp_settled(w)) { bp_step(w); ++t; }
    return t;
}

/* ---- 1. determinism ------------------------------------------------ */

static void test_determinism(void)
{
    BpWorld w;
    V3 ref[3];
    int i, k, same = 1;
    printf("determinism (1000 re-sims, bit-identical)\n");
    for (i = 0; i < 1000; ++i) {
        BpWorld scratch;                     /* perturb the stack between runs */
        memset(&scratch, (i * 37) & 0xff, sizeof(scratch));
        flat_table(&w, 1.6f, 6.0f, 1);
        add_ball(&w, 0.35f, 1.2f, BALL_OBJ);
        add_ball(&w, -0.5f, 3.0f, BALL_OBJ);
        bp_world_finalize(&w);
        bp_shot_begin(&w);
        bp_strike(&w, 0.14f, 0.78f, 0.5f, -0.45f);
        run_to_rest(&w, 8000);
        if (i == 0) { for (k = 0; k < 3; ++k) ref[k] = w.balls[k].p; }
        else {
            for (k = 0; k < 3; ++k) {
                if (memcmp(&ref[k], &w.balls[k].p, sizeof(V3)) != 0) same = 0;
            }
        }
        (void)scratch;
    }
    ok(same, "1000 identical shots produce bit-identical rest positions", NULL);
}

/* ---- 2. stop distance / power curve -------------------------------- */

static float shot_distance(float power, float tx, float ty)
{
    BpWorld w;
    flat_table(&w, 3.0f, 40.0f, 0);
    w.kill_y = -50.0f;
    bp_world_finalize(&w);
    bp_shot_begin(&w);
    bp_strike(&w, 0.0f, power, tx, ty);
    run_to_rest(&w, 40000);
    return w.balls[0].p.z - w.balls[0].prev.z;
}

static void test_stop_distance(void)
{
    float d25, d50, d100;
    char buf[128];
    printf("stop distance & power curve\n");
    d25 = shot_distance(0.25f, 0, 0);
    d50 = shot_distance(0.50f, 0, 0);
    d100 = shot_distance(1.00f, 0, 0);
    snprintf(buf, sizeof buf, "25%%=%.2fm 50%%=%.2fm 100%%=%.2fm", d25, d50, d100);
    ok(d25 > 0.6f && d25 < 2.5f, "soft shot is a touch shot", buf);
    ok(d50 > 4.0f && d50 < 11.0f, "50%% power stops inside a hole length", buf);
    ok(d100 > 25.0f, "full power is a smash", buf);
    ok(d25 < d50 && d50 < d100, "power curve is monotonic", NULL);
    /* repeatability: three identical shots land within a ball width */
    ok(fabsf(shot_distance(0.5f, 0, 0) - d50) < 2.0f * BP_R,
       "repeated 50%% shots land within one ball width", NULL);
}

/* ---- 3. draw / follow / stun --------------------------------------- */

static float cue_travel_after_hit(float gap, float power, float ty)
{
    BpWorld w;
    float start, obz;
    flat_table(&w, 3.0f, 30.0f, 0);
    w.kill_y = -50.0f;
    start = w.balls[0].p.z;
    obz = start + gap;
    add_ball(&w, 0.0f, obz, BALL_OBJ);
    bp_world_finalize(&w);
    bp_shot_begin(&w);
    bp_strike(&w, 0.0f, power, 0.0f, ty);
    run_to_rest(&w, 40000);
    /* signed distance from the contact point (object ball start) */
    return w.balls[0].p.z - obz;
}

static void test_spin(void)
{
    float draw, stun, follow;
    char buf[160];
    printf("draw / follow / stun (40%% power, full ball, 1.6 m)\n");
    draw = cue_travel_after_hit(1.6f, 0.40f, -BP_TIP_MAX);
    stun = cue_travel_after_hit(1.6f, 0.40f, 0.0f);
    follow = cue_travel_after_hit(1.6f, 0.40f, BP_TIP_MAX);
    snprintf(buf, sizeof buf, "draw=%.2fm stun=%.2fm follow=%.2fm", draw, stun, follow);
    ok(draw < -0.4f, "max draw pulls the cue ball back off the object ball", buf);
    ok(fabsf(stun) < 0.30f, "centre ball stuns close to dead", buf);
    ok(follow > 0.8f, "follow carries the cue ball forward", buf);
    ok(draw < stun && stun < follow, "draw < stun < follow", NULL);

    /* draw bleeds off with distance (5.2 / Hole 3's lesson) */
    {
        float near_draw = cue_travel_after_hit(1.2f, 0.55f, -BP_TIP_MAX);
        float far_draw  = cue_travel_after_hit(4.0f, 0.55f, -BP_TIP_MAX);
        snprintf(buf, sizeof buf, "1.2m gap=%.2fm  4.0m gap=%.2fm (same 55%% power)",
                 near_draw, far_draw);
        ok(far_draw > near_draw, "backspin bleeds off: long-range draw is weaker", buf);
    }
}

/* ---- 4. cushion english -------------------------------------------- */

static float bank_angle(float tx, float *exit_speed)
{
    /* Bank off the +X rail at 45 degrees and measure the outgoing heading. */
    BpWorld w;
    int t;
    float ang;
    bp_world_reset(&w);
    {
        BpPad *p = &w.pads[w.npads++];
        memset(p, 0, sizeof(*p));
        p->x0 = -4.0f; p->x1 = 2.0f; p->z0 = -4.0f; p->z1 = 14.0f;
        p->surf = SURF_FELT;
        p->rails = PAD_E;
    }
    w.nballs = 1;
    memset(&w.balls[0], 0, sizeof(BpBall));
    w.balls[0].p = v3(-1.6f, BP_R, -1.6f);
    w.balls[0].kind = BALL_CUE;
    w.balls[0].state = BS_REST;
    w.kill_y = -50.0f;
    bp_world_finalize(&w);
    bp_shot_begin(&w);
    bp_strike(&w, 45.0f * BP_DEG, 0.62f, tx, 0.0f);
    for (t = 0; t < 4000; ++t) {
        bp_step(&w);
        if (w.wall_hits > 0 && w.tick > 4) break;
    }
    ang = atan2f(w.balls[0].v.x, w.balls[0].v.z) / BP_DEG;
    if (exit_speed) *exit_speed = v3len(w.balls[0].v);
    return ang;
}

static void test_english(void)
{
    float run, check, plain, spread;
    float vrun = 0.0f, vcheck = 0.0f, vplain = 0.0f;
    char buf[192];
    printf("cushion english (45 degree bank)\n");
    plain = bank_angle(0.0f, &vplain);
    run   = bank_angle(BP_TIP_MAX, &vrun);
    check = bank_angle(-BP_TIP_MAX, &vcheck);
    spread = fabsf(run - check);
    snprintf(buf, sizeof buf, "check=%.1f deg plain=%.1f deg running=%.1f deg spread=%.1f deg",
             check, plain, run, spread);
    ok(spread >= 15.0f, "running vs check english differ by >= 15 degrees", buf);
    ok(check < plain - 3.0f && run > plain + 3.0f,
       "check narrows the rebound, running widens it, either side of centre", buf);
    snprintf(buf, sizeof buf, "exit speed check=%.2f plain=%.2f running=%.2f m/s",
             vcheck, vplain, vrun);
    ok(vrun > vplain && vcheck < vplain,
       "running english preserves speed, check english kills it", buf);
}

/* ---- 5. the cup ---------------------------------------------------- */

static int putt_result(float power)
{
    BpWorld w;
    BpPocket *pk;
    flat_table(&w, 2.0f, 6.0f, 1);
    pk = &w.pockets[w.npockets++];
    memset(pk, 0, sizeof(*pk));
    pk->x = 0.0f; pk->z = -5.5f + 2.5f; pk->y = 0.0f; pk->kind = PK_CUP; pk->link = -1;
    bp_world_finalize(&w);
    bp_shot_begin(&w);
    bp_strike(&w, 0.0f, power, 0.0f, 0.0f);
    run_to_rest(&w, 20000);
    return w.holed;
}

static void test_cup(void)
{
    int i, holed = 0, missed = 0;
    char buf[96];
    printf("the cup (2.5 m straight putt, honest capture)\n");
    ok(putt_result(0.41f) == 1, "a dead-weight putt drops", NULL);
    for (i = 0; i < 40; ++i) {
        float p = 0.41f + 0.010f * (float)i;   /* 41% .. 80% */
        if (putt_result(p)) ++holed; else ++missed;
    }
    snprintf(buf, sizeof buf, "%d/%d dropped across 41-80%% power", holed, holed + missed);
    ok(holed > 0, "some firm putts still drop", buf);
    ok(missed > 0, "hard putts rim out or fly the cup — no magnetism", buf);
    ok(putt_result(1.0f) == 0, "a full-power smash flies straight over the cup", NULL);
}

/* ---- 6. tunnelling ------------------------------------------------- */

static void test_tunnelling(void)
{
    int trial, escaped = 0;
    BpWorld w;
    printf("tunnelling (10000 max-power shots into the thinnest wall)\n");
    for (trial = 0; trial < 10000; ++trial) {
        float aim = (float)trial * (BP_TAU / 10000.0f);
        int t;
        flat_table(&w, 1.0f, 1.0f, 1);          /* a 2x2 m box of thin rails */
        bp_world_finalize(&w);
        bp_shot_begin(&w);
        bp_strike(&w, aim, 1.0f, 0.0f, 0.0f);
        for (t = 0; t < 900; ++t) {
            bp_step(&w);
            if (fabsf(w.balls[0].p.x) > 1.30f || fabsf(w.balls[0].p.z) > 1.30f ||
                w.balls[0].state == BS_GONE) { ++escaped; break; }
        }
    }
    {
        char buf[64];
        snprintf(buf, sizeof buf, "%d escapes", escaped);
        ok(escaped == 0, "no ball ever passes through a rail", buf);
    }
}

/* ---- 7. hazards & pool systems ------------------------------------- */

static void test_systems(void)
{
    BpWorld w;
    printf("pool systems (scratch, bonus, warp, rack)\n");

    /* scratch pocket penalises the cue ball */
    flat_table(&w, 2.0f, 6.0f, 1);
    {
        BpPocket *pk = &w.pockets[w.npockets++];
        memset(pk, 0, sizeof(*pk));
        pk->z = -5.5f + 2.0f; pk->kind = PK_SCRATCH; pk->link = -1;
    }
    bp_world_finalize(&w);
    bp_shot_begin(&w);
    bp_strike(&w, 0.0f, 0.40f, 0.0f, 0.0f);
    run_to_rest(&w, 20000);
    ok(w.scratched == 1, "cue ball in a scratch pocket sets the scratch flag", NULL);

    /* bonus pocket rewards a potted object ball */
    flat_table(&w, 2.0f, 6.0f, 1);
    add_ball(&w, 0.0f, -5.5f + 1.2f, BALL_OBJ);
    {
        BpPocket *pk = &w.pockets[w.npockets++];
        memset(pk, 0, sizeof(*pk));
        pk->z = -5.5f + 2.4f; pk->kind = PK_BONUS; pk->link = -1;
    }
    bp_world_finalize(&w);
    bp_shot_begin(&w);
    bp_strike(&w, 0.0f, 0.46f, 0.0f, 0.0f);
    run_to_rest(&w, 20000);
    ok(w.bonus_hits == 1, "object ball potted in a bonus pocket counts", NULL);

    /* rack hole: potting the eight unseals the cup */
    flat_table(&w, 2.0f, 6.0f, 1);
    add_ball(&w, 0.0f, -5.5f + 1.2f, BALL_EIGHT);
    {
        BpPocket *pk = &w.pockets[w.npockets++];
        memset(pk, 0, sizeof(*pk));
        pk->z = -5.5f + 2.4f; pk->kind = PK_SCRATCH; pk->link = -1;
    }
    w.cup_sealed = 1;
    bp_world_finalize(&w);
    bp_shot_begin(&w);
    bp_strike(&w, 0.0f, 0.46f, 0.0f, 0.0f);
    run_to_rest(&w, 20000);
    ok(w.eight_potted == 1 && w.cup_sealed == 0, "potting the eight uncaps the cup", NULL);

    /* warp preserves speed */
    flat_table(&w, 2.0f, 6.0f, 1);
    {
        BpPocket *a = &w.pockets[w.npockets++];
        BpPocket *b = &w.pockets[w.npockets++];
        memset(a, 0, sizeof(*a)); memset(b, 0, sizeof(*b));
        a->z = -5.5f + 1.5f; a->kind = PK_WARP; a->link = 1;
        b->z =  3.5f;        b->kind = PK_WARP; b->link = 0;
    }
    bp_world_finalize(&w);
    bp_shot_begin(&w);
    bp_strike(&w, 0.0f, 0.44f, 0.0f, 0.0f);
    {
        int t, warped = 0; float before = 0.0f, after = 0.0f;
        for (t = 0; t < 20000 && !bp_settled(&w); ++t) {
            V3 was = w.balls[0].p;
            V3 hv = w.balls[0].v;
            float sp = sqrtf(hv.x * hv.x + hv.z * hv.z);
            bp_step(&w);
            if (v3len(v3sub(w.balls[0].p, was)) > 1.0f && !warped) {
                hv = w.balls[0].v;
                warped = 1; before = sp;
                after = sqrtf(hv.x * hv.x + hv.z * hv.z);
            }
        }
        {
            char b2[96];
            snprintf(b2, sizeof b2, "%.2f m/s in, %.2f m/s out (horizontal)", before, after);
            ok(warped, "warp pocket ejects the ball at its partner", b2);
            ok(warped && fabsf(before - after) < 0.02f, "warp preserves speed", b2);
        }
    }

    /* void costs a stroke */
    bp_world_reset(&w);
    {
        BpPad *p = &w.pads[w.npads++];
        memset(p, 0, sizeof(*p));
        p->x0 = -1.0f; p->x1 = 1.0f; p->z0 = -1.0f; p->z1 = 1.0f;
        p->surf = SURF_FELT;
    }
    w.nballs = 1;
    memset(&w.balls[0], 0, sizeof(BpBall));
    w.balls[0].p = v3(0.0f, BP_R, -0.8f);
    w.balls[0].kind = BALL_CUE;
    bp_world_finalize(&w);
    bp_shot_begin(&w);
    bp_strike(&w, 0.0f, 0.5f, 0.0f, 0.0f);
    run_to_rest(&w, 20000);
    ok(w.scratched == 1, "falling off the world is a penalty, not a crash", NULL);
}

/* ---- 8. surface zones ---------------------------------------------- */

static float surf_distance(int surf)
{
    BpWorld w;
    BpPad *p;
    bp_world_reset(&w);
    p = &w.pads[w.npads++];
    memset(p, 0, sizeof(*p));
    p->x0 = -3.0f; p->x1 = 3.0f; p->z0 = -40.0f; p->z1 = 40.0f;
    p->surf = (unsigned char)surf;
    w.nballs = 1;
    memset(&w.balls[0], 0, sizeof(BpBall));
    w.balls[0].p = v3(0.0f, BP_R, -20.0f);
    w.balls[0].kind = BALL_CUE;
    w.kill_y = -50.0f;
    bp_world_finalize(&w);
    bp_shot_begin(&w);
    bp_strike(&w, 0.0f, 0.5f, 0.0f, 0.0f);
    run_to_rest(&w, 60000);
    return w.balls[0].p.z + 20.0f;
}

static void test_surfaces(void)
{
    float felt, rough, ice, sand;
    char buf[160];
    printf("surface zones\n");
    felt = surf_distance(SURF_FELT);
    rough = surf_distance(SURF_ROUGH);
    ice = surf_distance(SURF_ICE);
    sand = surf_distance(SURF_SAND);
    snprintf(buf, sizeof buf, "felt=%.1fm rough=%.1fm ice=%.1fm sand=%.1fm",
             felt, rough, ice, sand);
    ok(rough < felt, "rough kills a shot faster than felt", buf);
    ok(ice > felt, "ice carries further than felt", buf);
    ok(sand < rough, "sand is the harshest", buf);
}

/* ---- main ---------------------------------------------------------- */

int main(void)
{
    printf("BREAK PAR — physics suite\n\n");
    test_determinism();
    test_stop_distance();
    test_spin();
    test_english();
    test_cup();
    test_systems();
    test_surfaces();
    test_tunnelling();
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
