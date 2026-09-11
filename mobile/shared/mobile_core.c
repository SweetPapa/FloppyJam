#include "mobile_core.h"
#include "maglava.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct MLGame {
    GameSim sim, previous;
    double accumulator;
    float camera, old_camera, visual_time, hit_x, hit_y, hit_age;
    int paused, events, queue[8], head, count;
};
static const double tick = 1.0 / 60.0;
MLGame *ml_create(void) { MLGame *g = calloc(1, sizeof(*g)); if (g) ml_start(g, 1); return g; }
void ml_destroy(MLGame *g) { free(g); }
void ml_start(MLGame *g, int level) {
    if (!g) return;
    memset(g, 0, sizeof(*g)); sim_init(&g->sim, level);
    g->hit_age = -1;
    g->previous = g->sim;
    g->camera = g->old_camera = g->sim.py - 150;
}
void ml_pause(MLGame *g, int paused) {
    if (!g) return;
    g->paused = !!paused; g->accumulator = 0; g->count = g->head = 0;
    g->previous = g->sim; g->old_camera = g->camera; g->events = 0;
}
void ml_input(MLGame *g, int color) {
    if (!g || g->paused || g->sim.game_over || g->sim.state == PS_DEAD || color < 0 || color > 3) return;
    if (g->count < 8) { g->queue[(g->head + g->count) % 8] = color; g->count++; }
}
void ml_advance(MLGame *g, double seconds) {
    if (!g) return;
    g->events = 0;
    if (g->paused || !isfinite(seconds) || seconds <= 0) return;
    /* A suspended app never fast-forwards hazards. Maximum six catch-up ticks. */
    if (seconds > .1) seconds = .1;
    g->accumulator += seconds;
    while (g->accumulator + 1e-10 >= tick) {
        int input = -1;
        if (g->count) { input = g->queue[g->head]; g->head = (g->head + 1) % 8; g->count--; }
        g->previous = g->sim; g->old_camera = g->camera;
        sim_update(&g->sim, (float)tick, input);
        GameSim *s = &g->sim;
        g->visual_time += (float)tick;
        if (g->hit_age >= 0) g->hit_age += (float)tick;
        if (s->ev_death) { g->hit_x=s->px; g->hit_y=s->py; g->hit_age=0; }
        g->events |= (s->ev_attach ? ML_ATTACH : 0) | (s->ev_checkpoint ? ML_CHECKPOINT : 0)
            | (s->ev_swing ? ML_SWING : 0) | (s->ev_death ? ML_DEATH : 0) | (s->ev_complete ? ML_COMPLETE : 0);
        if (s->state == PS_DEAD) g->count = 0;
        if (g->previous.state == PS_DEAD && s->state != PS_DEAD) {
            g->events |= ML_RESPAWN; g->previous = *s;
            g->camera = g->old_camera = s->py - 150;
        } else g->camera += (s->py - 150 - g->camera) * (1 - expf(-6.0f / 60));
        g->accumulator -= tick;
    }
}
static float mix(float a, float b, float t) { return a + (b-a)*t; }
void ml_snapshot(const MLGame *g, float *o) {
    if (!g || !o) return;
    memset(o, 0, sizeof(float)*ML_SNAPSHOT_SIZE);
    const GameSim *s=&g->sim, *p=&g->previous;
    float t = (float)fmax(0, fmin(1, g->accumulator/tick));
    o[0]=mix(p->px,s->px,t); o[1]=mix(p->py,s->py,t); o[2]=mix(p->lava_y,s->lava_y,t);
    o[3]=mix(g->old_camera,g->camera,t); o[4]=s->state; o[5]=s->won; o[6]=s->score;
    o[7]=s->combo; o[8]=s->deaths; o[9]=s->elapsed; o[10]=s->cur_cp; o[11]=s->immune;
    o[12]=mix(p->rot_cam,s->rot_cam,t); o[13]=s->n_mag; o[14]=s->n_ob; o[15]=s->lv->n_cp;
    o[16]=sim_stars(s); o[17]=sim_height(s); o[18]=s->ai.active;
    o[19]=mix(p->ai.x,s->ai.x,t); o[20]=mix(p->ai.y,s->ai.y,t); o[21]=s->race_lost;
    o[22]=g->events; o[23]=s->attached_idx; o[24]=s->target_idx; o[25]=s->color;
    float distance=s->lv->mag[0].y-s->lv->mag[s->n_mag-1].y;
    o[26]=fmaxf(0,fminf(1,sim_height(s)/fmaxf(1,distance))); o[27]=s->lv->par_time;
    o[28]=s->lv->anomaly;
    o[29]=s->lava_y-(s->state==PS_ATTACHED ? s->lv->mag[s->attached_idx].y : s->py);
    for(int c=0;c<4;c++) o[30+c]=sim_find_magnet(s,s->px,s->py,(MagColor)c,s->attached_idx>=0?s->attached_idx:s->target_idx);
    o[34]=s->level_id;
    /* Persistent impact survives dropped render frames; clock also runs during death. */
    o[35]=g->hit_x; o[36]=g->hit_y; o[37]=g->hit_age; o[38]=g->visual_time;
    for(int i=0;i<s->n_mag;i++) {
        int k=ML_MAG_OFFSET+i*6; const MagnetDef *m=&s->lv->mag[i];
        o[k]=m->x; o[k+1]=m->y; o[k+2]=m->color; o[k+3]=s->mag_alive[i];
        o[k+4]=i==s->n_mag-1; o[k+5]=i==s->attached_idx || i==s->target_idx;
    }
    for(int i=0;i<s->n_ob;i++) {
        int k=ML_OB_OFFSET+i*10; const Obstacle *b=&s->ob[i], *a=&p->ob[i]; const ObstacleDef *d=&s->lv->ob[i];
        o[k]=b->type; o[k+1]=mix(a->x,b->x,t); o[k+2]=mix(a->y,b->y,t); o[k+3]=b->alive;
        /* Sweeper angle is wrapped in the simulation: interpolate by the shortest arc. */
        float angleDelta=b->angle-a->angle;
        if(b->type==OB_SWEEPER) angleDelta=remainderf(angleDelta,360);
        o[k+4]=a->angle+angleDelta*t; o[k+5]=mix(a->radius,b->radius,t);
        o[k+6]=b->lstate; o[k+7]=d->length; o[k+8]=d->ex; o[k+9]=b->polarity;
    }
    for(int i=0;i<s->lv->n_cp && i<64;i++) {
        int k=ML_CP_OFFSET+i*3; o[k]=s->lv->cp[i].y; o[k+1]=i<=s->cur_cp; o[k+2]=s->lv->cp[i].respawn;
    }
}
int ml_level_count(void) { return LEVEL_COUNT; }
static int valid(int n) { return n>=1 && n<=LEVEL_COUNT; }
const char *ml_level_key(int n) { return valid(n)?LEVELS[n-1].key:""; }
const char *ml_level_name(int n) { return valid(n)?LEVELS[n-1].name:""; }
const char *ml_level_hint(int n) { return valid(n)?LEVELS[n-1].hint:""; }
