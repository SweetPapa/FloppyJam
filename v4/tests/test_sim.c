/* Headless simulation tests. Compiles sim.c + levels_gen.h with no raylib.
 * A greedy climbing bot proves every level is completable, plus targeted
 * checks for lava death, respawn, and scoring. Exit non-zero on failure. */
#include "../src/maglava.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

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

static int test_targeting(void) {
    GameSim g; sim_init(&g,1);
    LevelDef lv=*g.lv;
    const MagnetDef mag[]={{270,1000,COL_RED},{270,1040,COL_BLUE},{270,800,COL_BLUE}};
    lv.mag=mag;lv.n_mag=3;g.lv=&lv;g.n_mag=3;
    g.px=270;g.py=1028;
    if(sim_find_magnet(&g,g.px,g.py,COL_BLUE,0)!=2)return 0;
    for(int i=1;i<g.n_mag;i++)g.mag_alive[i]=0;
    sim_update(&g,DT,COL_GREEN);
    if(g.state!=PS_ATTACHED)return 0;
    puts("  ok: forward color targeting; unavailable colors preserve the tether");
    return 1;
}
static int test_checkpoint_recovery(void) {
    int tested=0;
    for(int id=1;id<=LEVEL_COUNT;id++) {
        GameSim initial;sim_init(&initial,id);
        for(int cp=0;cp<initial.lv->n_cp;cp++) {
            GameSim g=initial;g.cur_cp=cp;
            int idx=g.lv->cp[cp].respawn;
            g.cp_lava_y=g.lv->mag[idx].y-100;
            g.state=PS_DEAD;g.respawn_timer=0;
            sim_update(&g,DT,-1);
            if(g.state!=PS_ATTACHED || g.attached_idx!=idx ||
               g.lava_y-g.py<LAVA_START_OFFSET-ORBIT_REST_R-1)return 0;
            sim_update(&g,DT,-1);
            if(g.state==PS_DEAD)return 0;
            tested++;
        }
    }
    printf("  ok: %d checkpoint respawns have a safe lava margin\n",tested);
    return 1;
}
static int test_goal_and_anomalies(void) {
    GameSim g;sim_init(&g,1);
    int final=g.n_mag-1;
    g.state=PS_SWINGING;g.target_idx=final;g.attached_idx=-1;
    g.px=g.lv->mag[final].x;g.py=g.lv->mag[final].y;
    g.lava_y=g.py-1; /* collision on this same tick must not undo a win */
    sim_update(&g,DT,-1);
    if(!g.won||g.deaths||!g.ev_complete)return 0;
    for(int id=1;id<=LEVEL_COUNT;id++) {
        sim_init(&g,id);
        if(g.ai.active!=(g.lv->anomaly==ANOM_RACE))return 0;
        sim_update(&g,DT,-1);
        if((g.rot_cam!=0)!=(g.lv->anomaly==ANOM_ROLL))return 0;
    }
    puts("  ok: goal landing is final; anomalies survive campaign reordering");
    return 1;
}

/* Subspace must offer safe landings through a complete pulse cycle, even
 * with arrival spin. This catches ring/anchor overlap that a fast bot or
 * post-death immunity can otherwise conceal. */
static int test_subspace_rest_points(void) {
    int id=0;
    for(int i=0;i<LEVEL_COUNT;i++)if(!strcmp(LEVELS[i].key,"level-6b"))id=i+1;
    if(!id)return 0;
    const LevelDef *lv=&LEVELS[id-1];
    for(int i=0;i<lv->n_mag;i++) {
        const MagnetDef *m=&lv->mag[i];
        for(int j=0;j<lv->n_ob;j++) {
            float dx=m->x-lv->ob[j].x,dy=m->y-lv->ob[j].y;
            float safe=PULSE_MAX_R+PULSE_THICK*.5f+PLAYER_SIZE+ORBIT_MAX_R;
            if(lv->ob[j].type==OB_PULSE && dx*dx+dy*dy<=safe*safe) {
                printf("  FAIL: Subspace ring reaches anchor %d\n",i);return 0;
            }
        }
        GameSim g;sim_init(&g,id);
        g.attached_idx=i;g.color=(MagColor)m->color;
        g.orbit_r=ORBIT_MAX_R;g.orbit_ang=0;g.orbit_av=ORBIT_MAX_AV;
        g.px=m->x+g.orbit_r;g.py=m->y;
        for(int step=0;step<60*8;step++) {
            sim_update(&g,DT,-1);
            if(g.deaths) { printf("  FAIL: cannot wait at Subspace anchor %d\n",i);return 0; }
        }
    }
    printf("  ok: all %d Subspace landings survive a full pulse cycle with arrival spin\n",lv->n_mag);
    return 1;
}

static int test_lava_rate(void) {
    GameSim g;sim_init(&g,1);sim_set_lava_rate(&g,2);
    g.backtrack_active=1;g.backtrack_timer=.5f;g.lava_speed=g.normal_lava_speed*BACKTRACK_MULT;
    float y=g.lava_y;sim_update(&g,DT,-1);
    if(fabsf((y-g.lava_y)-g.normal_lava_speed*BACKTRACK_MULT*2*DT)>.001f)return 0;
    for(int i=0;i<40;i++)sim_update(&g,DT,-1);
    if(g.backtrack_active || g.lava_rate!=2 || g.lava_speed!=g.normal_lava_speed)return 0;
    g.state=PS_DEAD;g.respawn_timer=DT;sim_update(&g,DT,-1);
    if(g.lava_rate!=2 || g.lava_speed!=g.normal_lava_speed)return 0;
    g.lava_stopped=1;y=g.lava_y;sim_update(&g,DT,-1);if(g.lava_y!=y)return 0;
    return 1;
}

int main(void) {
    int fails = 0;
    if(!test_lava_rate()) { puts("FAIL lava multiplier/surge/respawn");fails++; }
    printf("== MagLava sim tests ==\n");

    printf("[completability]\n");
    int won = 0;
    for (int id = 1; id <= LEVEL_COUNT; id++) won += play_level(id, 1);
    printf("  %d/%d levels completed by bot\n", won, LEVEL_COUNT);
    /* Every stage must remain completable; failures cannot hide in a majority. */
    if (won != LEVEL_COUNT) { printf("  FAIL: too few levels completable\n"); fails++; }

    printf("[lava]\n");   if (!test_lava_kills()) fails++;
    printf("[scoring]\n"); if (!test_scoring())    fails++;
    printf("[tether orbit]\n"); if (!test_orbit()) fails++;

    if (!test_subspace_rest_points()) { puts("FAIL Subspace landings"); fails++; }
    if (!test_targeting()) { puts("FAIL targeting"); fails++; }
    if (!test_checkpoint_recovery()) { puts("FAIL checkpoint recovery"); fails++; }
    if (!test_goal_and_anomalies()) { puts("FAIL goal/anomaly"); fails++; }
    printf(fails ? "\nFAILED (%d)\n" : "\nALL PASS\n", fails);
    return fails ? 1 : 0;
}
