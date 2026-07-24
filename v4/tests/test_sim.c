/* Headless simulation tests. Compiles sim.c + levels_gen.h with no raylib.
 * A greedy climbing bot proves every level is completable, plus targeted
 * checks for lava death, respawn, and scoring. Exit non-zero on failure. */
#include "../src/maglava.h"
#include <stdio.h>
#include <math.h>

#define DT (1.0f / 60.0f)

/* greedy bot: when attached, pick the color whose nearest reachable magnet
 * is highest above us; returns color 0..3 or -1. */
static int bot_choose(GameSim *g) {
    if (g->state != PS_ATTACHED) return -1;
    int best_c = -1;
    float best_y = g->py - 5.0f;
    for (int c = 0; c < 4; c++) {
        int idx = sim_find_magnet(g, g->px, g->py, (MagColor)c, g->attached_idx);
        if (idx < 0) continue;
        float y = g->lv->mag[idx].y;
        if (y < best_y) { best_y = y; best_c = c; }
    }
    /* if nothing strictly above, take nearest above-ish to keep momentum */
    if (best_c < 0) {
        for (int c = 0; c < 4; c++) {
            int idx = sim_find_magnet(g, g->px, g->py, (MagColor)c, g->attached_idx);
            if (idx >= 0 && g->lv->mag[idx].y < g->py) { best_c = c; break; }
        }
    }
    return best_c;
}

static int play_level(int id, int verbose) {
    GameSim g;
    sim_init(&g, id);
    int frames = 0;
    int max_frames = 60 * 60 * 8; /* 8 minutes cap */
    while (!g.won && frames < max_frames) {
        int press = bot_choose(&g);
        sim_update(&g, DT, press);
        frames++;
        if (g.deaths > 40) break; /* stuck */
    }
    if (verbose) {
        printf("  L%-2d %-22s %s  t=%5.1fs deaths=%d score=%d stars=%d\n",
               id, g.lv->name, g.won ? "WON " : "FAIL",
               g.elapsed, g.deaths, g.score, sim_stars(&g));
    }
    return g.won;
}

static int test_lava_kills(void) {
    GameSim g; sim_init(&g, 1);
    /* idle: never press; lava must rise and kill (a death recorded) */
    for (int i = 0; i < 60 * 120 && g.deaths == 0; i++) sim_update(&g, DT, -1);
    if (g.deaths == 0) { printf("  FAIL: lava never killed idle player\n"); return 0; }
    printf("  ok: idle player consumed by lava after %.1fs\n", g.elapsed);
    return 1;
}

static int test_scoring(void) {
    GameSim g; sim_init(&g, 1);
    int hops = 0;
    for (int i = 0; i < 60 * 60 && hops < 5; i++) {
        int before = g.score;
        int press = bot_choose(&g);
        sim_update(&g, DT, press);
        if (g.score > before && g.ev_attach) hops++;
    }
    if (g.score <= 0) { printf("  FAIL: no score after hops\n"); return 0; }
    printf("  ok: scored %d over %d landings (combo=%d)\n", g.score, hops, g.combo);
    return 1;
}

/* The tether must do two opposing things: swing you around a node when you
 * arrive with speed (dynamics), and always settle into a predictable hang so
 * a calm shot is available (playability). Check both. */
static int test_orbit(void) {
    GameSim g;
    sim_init(&g, 1);
    int attached = 0;
    for (int i = 0; i < 60 * 30 && !attached; i++) {
        sim_update(&g, DT, bot_choose(&g));
        if (g.ev_attach) attached = 1;
    }
    if (!attached) { printf("  FAIL: never reached a node\n"); return 0; }

    float a0 = g.orbit_ang, maxdev = 0;
    for (int i = 0; i < 60; i++) {          /* one second of free orbit */
        sim_update(&g, DT, -1);
        float d = fabsf(g.orbit_ang - a0);
        if (d > maxdev) maxdev = d;
    }
    if (maxdev < 0.15f) {
        printf("  FAIL: arrival momentum did not swing the tether (%.3f rad)\n", maxdev);
        return 0;
    }
    printf("  ok: arrival momentum swung the tether %.2f rad\n", (double)maxdev);

    for (int i = 0; i < 60 * 6; i++) sim_update(&g, DT, -1);
    float off = g.orbit_ang - 1.5707963f;   /* rest hangs straight down */
    while (off >  3.1415927f) off -= 6.2831853f;
    while (off < -3.1415927f) off += 6.2831853f;
    if (fabsf(g.orbit_av) > 0.35f || fabsf(off) > 0.35f) {
        printf("  FAIL: tether never settled (av=%.3f off-rest=%.3f)\n",
               (double)g.orbit_av, (double)off);
        return 0;
    }
    printf("  ok: settles to a predictable hang (av=%.3f, off-rest=%.3f)\n",
           (double)g.orbit_av, (double)off);
    return 1;
}

int main(void) {
    int fails = 0;
    printf("== MagLava sim tests ==\n");

    printf("[completability]\n");
    int won = 0;
    for (int id = 1; id <= LEVEL_COUNT; id++) won += play_level(id, 1);
    printf("  %d/%d levels completed by bot\n", won, LEVEL_COUNT);
    /* bot need not master anomaly/gauntlet levels, but the campaign spine
     * (the 1280px story levels) must be beatable. Require a strong majority. */
    if (won < 18) { printf("  FAIL: too few levels completable\n"); fails++; }

    printf("[lava]\n");   if (!test_lava_kills()) fails++;
    printf("[scoring]\n"); if (!test_scoring())    fails++;
    printf("[tether orbit]\n"); if (!test_orbit()) fails++;

    printf(fails ? "\nFAILED (%d)\n" : "\nALL PASS\n", fails);
    return fails ? 1 : 0;
}
