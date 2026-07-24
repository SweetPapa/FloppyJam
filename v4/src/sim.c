/* sim.c — authoritative MagLava game simulation (no raylib).
 *
 * Faithful port of the original Phaser gameplay: color-swing tether
 * movement, rising lava, five hazard types, checkpoints, combo scoring,
 * and the three level anomalies. Coordinates are in game-space
 * (x[0,540], y[0,5000], up = -y). Runs at a fixed dt from the caller.
 */
#include "maglava.h"
#include <math.h>

#define PI 3.14159265358979323846f
#define DEG2RAD_ (PI / 180.0f)

/* ---- deterministic RNG (LCG), so headless tests reproduce ---------- */
static float rng_f(GameSim *g) {
    g->rng = g->rng * 1664525u + 1013904223u;
    return (float)((g->rng >> 8) & 0xFFFFFF) / (float)0x1000000; /* [0,1) */
}

static float dist2(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}
static float flen(float x, float y) { return sqrtf(x * x + y * y); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

/* nearest magnet of a color within detection range, excluding one index */
int sim_find_magnet(const GameSim *g, float x, float y, MagColor c, int exclude) {
    const LevelDef *lv = g->lv;
    int best = -1;
    float bestd = DETECTION_RANGE * DETECTION_RANGE;
    for (int i = 0; i < lv->n_mag; i++) {
        if (i == exclude || !g->mag_alive[i]) continue;
        if (lv->mag[i].color != (unsigned char)c) continue;
        float d = dist2(x, y, lv->mag[i].x, lv->mag[i].y);
        if (d <= bestd) { bestd = d; best = i; }
    }
    return best;
}

/* closest distance from point to segment */
static float seg_dist(float px, float py, float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay;
    float len2 = dx * dx + dy * dy;
    float t = 0.0f;
    if (len2 > 1e-6f) {
        t = ((px - ax) * dx + (py - ay) * dy) / len2;
        if (t < 0) t = 0; else if (t > 1) t = 1;
    }
    float cx = ax + dx * t, cy = ay + dy * t;
    return flen(px - cx, py - cy);
}

/* tether-orbit helpers (defined below, used from init/attach/respawn) */
static void orbit_from_arrival(GameSim *g, int idx);
static void orbit_rest(GameSim *g, int idx);

/* ------------------------------------------------------------------ */
static void obstacle_reset(GameSim *g, int i) {
    const ObstacleDef *d = &g->lv->ob[i];
    Obstacle *o = &g->ob[i];
    o->type = d->type;
    o->x = d->x; o->y = d->y;
    o->angle = 0.0f;
    o->radius = PULSE_MIN_R;
    o->expanding = 1;
    o->timer = 0.0f;
    o->lstate = 0;
    o->alive = 1;
    o->polarity = d->polarity;
    if (d->type == OB_ROAMER) {
        o->angle = d->init_angle * DEG2RAD_;
        o->target_angle = o->angle;
        o->vx = cosf(o->angle) * d->speed;
        o->vy = sinf(o->angle) * d->speed;
    }
}

void sim_init(GameSim *g, int level_id) {
    if (level_id < 1) level_id = 1;
    if (level_id > LEVEL_COUNT) level_id = LEVEL_COUNT;
    const LevelDef *lv = &LEVELS[level_id - 1];

    /* zero everything */
    for (size_t i = 0; i < sizeof(*g); i++) ((char *)g)[i] = 0;

    g->level_id = level_id;
    g->lv = lv;
    g->rng = 0x1234567u ^ (unsigned)(level_id * 2654435761u);
    g->n_mag = lv->n_mag;
    g->n_ob = lv->n_ob;
    for (int i = 0; i < lv->n_mag; i++) g->mag_alive[i] = 1;
    for (int i = 0; i < lv->n_ob; i++) obstacle_reset(g, i);

    /* player starts attached to magnet 0 */
    g->attached_idx = 0;
    g->target_idx = -1;
    g->color = (MagColor)lv->mag[0].color;
    g->state = PS_ATTACHED;
    orbit_rest(g, 0);

    /* lava */
    g->normal_lava_speed = lv->lava_speed;
    g->lava_speed = lv->lava_speed;
    g->lava_y = lv->mag[0].y + LAVA_START_OFFSET;
    g->cp_lava_y = g->lava_y;

    /* progress */
    g->cur_cp = -1;
    g->highest_y = lv->mag[0].y;
    g->last_attach_y = lv->mag[0].y;

    /* anomalies */
    g->rot_dir = 1;
    if (level_id == LEVEL_AIRACER && lv->n_mag > 0) {
        g->ai.active = 1;
        g->ai.x = lv->mag[0].x;
        g->ai.y = lv->mag[0].y;
        g->ai.finish_y = lv->mag[lv->n_mag - 1].y;
        /* first magnet above start */
        g->ai.target_idx = 0;
        for (int i = 0; i < lv->n_mag; i++) {
            if (lv->mag[i].y < g->ai.y - 50.0f) { g->ai.target_idx = i; break; }
        }
    }
}

/* ---- landing / scoring ------------------------------------------- */
static void check_checkpoint(GameSim *g) {
    const LevelDef *lv = g->lv;
    for (int i = 0; i < lv->n_cp; i++) {
        float cy = lv->cp[i].y;
        if (g->py < cy && (g->cur_cp < 0 || cy < lv->cp[g->cur_cp].y)) {
            g->cur_cp = i;
            g->cp_lava_y = g->lava_y;
            g->score += SCORE_CHECKPOINT;
            g->ev_checkpoint = 1;
        }
    }
}

static void check_backtrack(GameSim *g) {
    if (g->py < g->highest_y) {
        g->highest_y = g->py;
    } else if (g->py > g->highest_y + BACKTRACK_THRESHOLD) {
        if (!g->backtrack_active) {
            g->backtrack_active = 1;
            g->backtrack_timer = BACKTRACK_TIME;
            g->lava_speed = g->normal_lava_speed * BACKTRACK_MULT;
        }
    }
}

static void level_complete(GameSim *g) {
    if (g->game_over) return;
    g->game_over = 1;
    g->won = 1;
    g->lava_stopped = 1;
    g->state = PS_ATTACHED;
    int bonus = 0;
    if (g->elapsed <= g->lv->par_time)
        bonus += (int)((g->lv->par_time - g->elapsed) * 10.0f); /* time bonus */
    g->score += SCORE_LEVEL_COMPLETE + bonus;
    if (g->deaths == 0) g->score += SCORE_NO_DEATH;
    g->ev_complete = 1;
}

static void do_attach(GameSim *g, int idx) {
    const LevelDef *lv = g->lv;
    /* landing on the last magnet completes the level */
    if (idx == lv->n_mag - 1) {
        g->attached_idx = idx; g->target_idx = -1;
        g->state = PS_ATTACHED;
        orbit_rest(g, idx);
        level_complete(g);
        return;
    }
    check_backtrack(g);
    g->combo++;
    int mult = g->combo < COMBO_MAX ? g->combo : COMBO_MAX;
    g->score += SCORE_LANDING * mult;
    g->combo_timer = COMBO_TIMEOUT;

    g->ev_attach = 1;
    g->ev_attach_forward = (g->py < g->last_attach_y);
    g->last_attach_y = g->py;

    g->attached_idx = idx;
    g->target_idx = -1;
    g->state = PS_ATTACHED;
    orbit_from_arrival(g, idx); /* keep the momentum: swing around the node */
    check_checkpoint(g);
}

/* ---- tether orbit ------------------------------------------------ *
 * Arriving at a node converts the tangential part of your arrival velocity
 * into spin around it. Hit it fast and side-on and you whip around; drift in
 * slowly and you just hang. */
static void orbit_from_arrival(GameSim *g, int idx) {
    const MagnetDef *m = &g->lv->mag[idx];
    float dx = g->px - m->x, dy = g->py - m->y;
    float d = flen(dx, dy);
    float ang;
    if (d < 1.0f) { ang = PI * 0.5f; d = ORBIT_REST_R; }
    else           ang = atan2f(dy, dx);
    float r = d;
    if (r < ORBIT_MIN_R) r = ORBIT_MIN_R;
    if (r > ORBIT_MAX_R) r = ORBIT_MAX_R;

    /* only the tangential component can become rotation */
    float tx = -sinf(ang), ty = cosf(ang);
    float av = (g->vx * tx + g->vy * ty) / r;
    if (av >  ORBIT_MAX_AV) av =  ORBIT_MAX_AV;
    if (av < -ORBIT_MAX_AV) av = -ORBIT_MAX_AV;

    g->orbit_r = r;
    g->orbit_ang = ang;
    g->orbit_av = av;
    g->px = m->x + cosf(ang) * r;
    g->py = m->y + sinf(ang) * r;
    g->vx = tx * av * r;
    g->vy = ty * av * r;
}

/* park calmly on a node (level start, respawn) */
static void orbit_rest(GameSim *g, int idx) {
    const MagnetDef *m = &g->lv->mag[idx];
    g->orbit_r = ORBIT_REST_R;
    g->orbit_ang = PI * 0.5f;   /* hanging directly below */
    g->orbit_av = 0.0f;
    g->px = m->x;
    g->py = m->y + ORBIT_REST_R;
    g->vx = g->vy = 0.0f;
}

static void update_orbit(GameSim *g, float dt) {
    int idx = g->attached_idx;
    if (idx < 0) return;
    const MagnetDef *m = &g->lv->mag[idx];

    /* pendulum torque about the node: a = g*cos(theta)/r, rest at theta=pi/2 */
    g->orbit_av += (PLAYER_GRAVITY * cosf(g->orbit_ang) / g->orbit_r) * dt;
    g->orbit_av -= g->orbit_av * ORBIT_DAMP * dt;
    if (g->orbit_av >  ORBIT_MAX_AV) g->orbit_av =  ORBIT_MAX_AV;
    if (g->orbit_av < -ORBIT_MAX_AV) g->orbit_av = -ORBIT_MAX_AV;
    g->orbit_ang += g->orbit_av * dt;

    /* normalise the tether length back toward its resting value */
    g->orbit_r += (ORBIT_REST_R - g->orbit_r) * (dt * 1.2f > 1.0f ? 1.0f : dt * 1.2f);

    /* settle assist: kill residual jitter once we are basically at rest,
     * so aiming is never a fight. Gated on already being near the hang, so
     * it can't yank a genuine swing out of the air. */
    if (fabsf(g->orbit_av) < 0.30f) {
        float d = g->orbit_ang - PI * 0.5f;
        while (d >  PI) d -= 2.0f * PI;
        while (d < -PI) d += 2.0f * PI;
        if (fabsf(d) < 0.7f) {
            float k = dt * 1.5f; if (k > 1.0f) k = 1.0f;
            g->orbit_ang -= d * k;
        }
    }

    g->px = m->x + cosf(g->orbit_ang) * g->orbit_r;
    g->py = m->y + sinf(g->orbit_ang) * g->orbit_r;
    float tx = -sinf(g->orbit_ang), ty = cosf(g->orbit_ang);
    g->vx = tx * g->orbit_av * g->orbit_r;
    g->vy = ty * g->orbit_av * g->orbit_r;
}

/* Soft shaft walls: keeps a wild sling inside the playfield and adds a
 * little bounce to play off. */
static void walls(GameSim *g) {
    float lo = WALL_MARGIN, hi = WORLD_WIDTH - WALL_MARGIN;
    if (g->px < lo)      { g->px = lo; if (g->vx < 0) { g->vx = -g->vx * WALL_BOUNCE; g->ev_bounce = 1; } }
    else if (g->px > hi) { g->px = hi; if (g->vx > 0) { g->vx = -g->vx * WALL_BOUNCE; g->ev_bounce = 1; } }
}

/* ---- swing / fall physics ---------------------------------------- */
static void start_swing(GameSim *g, int idx) {
    const LevelDef *lv = g->lv;
    g->attached_idx = -1;
    g->target_idx = idx;
    g->state = PS_SWINGING;
    g->ev_swing = 1;

    /* whatever you were carrying when you let go feeds the launch — this is
     * what makes release timing matter */
    float carry_x = g->vx * MOMENTUM_CARRY;
    float carry_y = g->vy * MOMENTUM_CARRY;

    float dx = lv->mag[idx].x - g->px;
    float dy = lv->mag[idx].y - g->py;
    float d = flen(dx, dy);
    if (d > 0) {
        float dirx = dx / d, diry = dy / d;
        float perpx = -diry, perpy = dirx;
        float initial = PLAYER_SWING_MAX_SPEED * ARC_INIT_MULT;
        float sign = (perpy < 0 || (fabsf(perpy) < 0.3f && perpx * dx < 0)) ? 1.0f : -1.0f;
        g->vx = perpx * initial * sign + carry_x;
        g->vy = perpy * initial * sign + carry_y;
        /* cap the launch so a big whip stays bounded and the homing steer
         * below can still always converge on the target */
        float sp = flen(g->vx, g->vy);
        float maxs = PLAYER_SWING_MAX_SPEED * ARC_SPEED_BOOST;
        if (sp > maxs) { g->vx = g->vx / sp * maxs; g->vy = g->vy / sp * maxs; }
    }
}

static void update_swing(GameSim *g, float dt) {
    if (g->target_idx < 0) { g->state = PS_FALLING; return; }
    const MagnetDef *t = &g->lv->mag[g->target_idx];
    float dx = t->x - g->px, dy = t->y - g->py;
    float d = flen(dx, dy);
    if (d < ATTACHMENT_RADIUS) { do_attach(g, g->target_idx); return; }

    float dirx = dx / d, diry = dy / d;
    float ratio = d / DETECTION_RANGE; if (ratio > 1) ratio = 1;
    float pull = 1.0f + (1.0f - ratio) * 0.5f;
    float target_speed = PLAYER_SWING_MAX_SPEED * pull * ARC_SPEED_BOOST;
    float desx = dirx * target_speed, desy = diry * target_speed;
    g->vx = lerpf(g->vx, desx, ARC_STEER);
    g->vy = lerpf(g->vy, desy, ARC_STEER);

    float speed = flen(g->vx, g->vy);
    if (speed < ARC_MIN_SPEED && speed > 0) {
        g->vx = g->vx / speed * ARC_MIN_SPEED;
        g->vy = g->vy / speed * ARC_MIN_SPEED;
    }
    float maxs = PLAYER_SWING_MAX_SPEED * ARC_SPEED_BOOST;
    if (speed > maxs) {
        g->vx = g->vx / speed * maxs;
        g->vy = g->vy / speed * maxs;
    }
    g->px += g->vx * dt;
    g->py += g->vy * dt;
    walls(g);
}

static void update_fall(GameSim *g, float dt) {
    g->vy += PLAYER_GRAVITY * dt;
    if (g->vy > PLAYER_MAX_FALL_SPEED) g->vy = PLAYER_MAX_FALL_SPEED;
    g->px += g->vx * dt;
    g->py += g->vy * dt;
    g->vx *= 0.98f;
    walls(g);
}

/* ---- obstacles ---------------------------------------------------- */
static void update_obstacles(GameSim *g, float dt) {
    for (int i = 0; i < g->n_ob; i++) {
        Obstacle *o = &g->ob[i];
        if (!o->alive) continue;
        const ObstacleDef *d = &g->lv->ob[i];
        switch (o->type) {
        case OB_SWEEPER:
            o->angle += d->speed * dt; /* deg */
            break;
        case OB_PULSE:
            if (o->expanding) {
                o->radius += d->speed * dt;
                if (o->radius >= PULSE_MAX_R) { o->radius = PULSE_MAX_R; o->expanding = 0; }
            } else {
                o->radius -= d->speed * dt;
                if (o->radius <= PULSE_MIN_R) { o->radius = PULSE_MIN_R; o->expanding = 1; }
            }
            break;
        case OB_LASER: {
            o->timer += dt * 1000.0f;
            if (o->lstate == 0) {
                if (o->timer >= d->off_ms - LASER_WARNING) { o->lstate = 1; o->timer = 0; }
            } else if (o->lstate == 1) {
                if (o->timer >= LASER_WARNING) { o->lstate = 2; o->timer = 0; }
            } else {
                if (o->timer >= d->on_ms) { o->lstate = 0; o->timer = 0; }
            }
            break;
        }
        case OB_ROAMER: {
            float minx = 100.0f, maxx = 440.0f;
            float miny = d->y - 300.0f, maxy = d->y + 300.0f;
            if (rng_f(g) < 0.01f) o->target_angle += (rng_f(g) - 0.5f) * PI;
            /* steer heading toward target */
            float diff = o->target_angle - o->angle;
            while (diff > PI) diff -= 2 * PI;
            while (diff < -PI) diff += 2 * PI;
            o->angle += diff * ROAMER_TURN_RATE * dt;
            o->vx = cosf(o->angle) * d->speed;
            o->vy = sinf(o->angle) * d->speed;
            o->x += o->vx * dt;
            o->y += o->vy * dt;
            int bounced = 0;
            if (o->x < minx) { o->x = minx; o->vx = -o->vx; bounced = 1; }
            else if (o->x > maxx) { o->x = maxx; o->vx = -o->vx; bounced = 1; }
            if (o->y < miny) { o->y = miny; o->vy = -o->vy; bounced = 1; }
            else if (o->y > maxy) { o->y = maxy; o->vy = -o->vy; bounced = 1; }
            if (bounced) { o->angle = atan2f(o->vy, o->vx); o->target_angle = o->angle; }
            break;
        }
        case OB_MINE:
            /* force field applied to player in update_mines() */
            break;
        }
    }
}

/* returns 1 if player (circle radius PLAYER_SIZE) hit a damaging obstacle */
static int obstacle_hit(GameSim *g) {
    float pr = PLAYER_SIZE;
    for (int i = 0; i < g->n_ob; i++) {
        Obstacle *o = &g->ob[i];
        if (!o->alive) continue;
        const ObstacleDef *d = &g->lv->ob[i];
        switch (o->type) {
        case OB_SWEEPER: {
            float ex = o->x + cosf(o->angle * DEG2RAD_) * d->length;
            float ey = o->y + sinf(o->angle * DEG2RAD_) * d->length;
            if (seg_dist(g->px, g->py, o->x, o->y, ex, ey) < pr + (SWEEPER_WIDTH + SWEEPER_HITPAD) * 0.5f)
                return 1;
            break;
        }
        case OB_PULSE: {
            float dd = flen(g->px - o->x, g->py - o->y);
            float inner = o->radius - PULSE_THICK * 0.5f;
            float outer = o->radius + PULSE_THICK * 0.5f;
            if ((dd + pr) > inner && (dd - pr) < outer) return 1;
            break;
        }
        case OB_LASER:
            if (o->lstate == 2) {
                if (seg_dist(g->px, g->py, d->x, d->y, d->ex, d->ey) < pr + LASER_THICK * 0.5f)
                    return 1;
            }
            break;
        case OB_ROAMER:
            if (flen(g->px - o->x, g->py - o->y) < pr + ROAMER_SIZE) return 1;
            break;
        case OB_MINE: /* never damages */
            break;
        }
    }
    return 0;
}

static void update_mines(GameSim *g, float dt) {
    if (g->state == PS_ATTACHED || g->state == PS_DEAD) return;
    for (int i = 0; i < g->n_ob; i++) {
        Obstacle *o = &g->ob[i];
        if (!o->alive || o->type != OB_MINE) continue;
        float dx = g->px - o->x, dy = g->py - o->y;
        float d = flen(dx, dy);
        if (d > MINE_FORCE_RADIUS || d < 1.0f) continue;
        float ux = dx / d, uy = dy / d;
        float mag = MINE_FORCE_STRENGTH * (1.0f - d / MINE_FORCE_RADIUS);
        int player_blue = (g->color == COL_BLUE);
        int mine_blue = (o->polarity == 0);
        float dir = (player_blue == mine_blue) ? -1.0f : 1.0f; /* same=attract */
        g->vx += ux * mag * dir * dt;
        g->vy += uy * mag * dir * dt;
    }
}

/* ---- lava --------------------------------------------------------- */
static void consume_below(GameSim *g) {
    float th = g->lava_y + LAVA_DESTROY_BUFFER;
    for (int i = 0; i < g->n_mag; i++)
        if (g->mag_alive[i] && g->lv->mag[i].y > th) g->mag_alive[i] = 0;
    for (int i = 0; i < g->n_ob; i++)
        if (g->ob[i].alive && g->ob[i].y > th) g->ob[i].alive = 0;
}

/* ---- anomalies ---------------------------------------------------- */
static void update_ai(GameSim *g, float dt) {
    AIRacer *a = &g->ai;
    if (!a->active || g->game_over) return;
    const LevelDef *lv = g->lv;
    float speed = AIRACER_SPEED * (0.95f + sinf(g->elapsed * 2.0f) * 0.1f);
    if (a->target_idx >= lv->n_mag) { a->finished = 1; return; }
    const MagnetDef *t = &lv->mag[a->target_idx];
    float dx = t->x - a->x, dy = t->y - a->y;
    float d = flen(dx, dy);
    if (d < 30.0f) {
        if (a->y <= a->finish_y) a->finished = 1;
        a->target_idx++;
        if (a->target_idx >= lv->n_mag) a->finished = 1;
    } else {
        float step = speed * dt / d; if (step > 1) step = 1;
        a->x += dx * step;
        a->y += dy * step;
    }
}

static void update_rotcam(GameSim *g, float dt) {
    if (g->level_id != LEVEL_ROTCAM) return;
    g->rot_cam += ROTCAM_SPEED * (float)g->rot_dir * dt;
    if (g->rot_cam >= ROTCAM_MAX) { g->rot_cam = ROTCAM_MAX; g->rot_dir = -1; }
    else if (g->rot_cam <= -ROTCAM_MAX) { g->rot_cam = -ROTCAM_MAX; g->rot_dir = 1; }
}

/* ---- death / respawn --------------------------------------------- */
static void die(GameSim *g) {
    if (g->state == PS_DEAD || g->game_over) return;
    g->state = PS_DEAD;
    g->vx = g->vy = 0;
    g->deaths++;
    g->combo = 0;
    g->immune = 0;
    g->respawn_timer = RESPAWN_DELAY;
    g->ev_death = 1;
}

static void respawn(GameSim *g) {
    const LevelDef *lv = g->lv;
    int midx; float rlava;
    if (g->cur_cp >= 0) {
        midx = lv->cp[g->cur_cp].respawn;
        rlava = g->cp_lava_y;
    } else {
        midx = 0;
        rlava = lv->mag[0].y + LAVA_START_OFFSET;
    }
    if (midx < 0 || midx >= lv->n_mag) midx = 0;

    /* recreate magnets/obstacles above the reset lava line */
    for (int i = 0; i < g->n_mag; i++)
        if (lv->mag[i].y < rlava) g->mag_alive[i] = 1;
    for (int i = 0; i < g->n_ob; i++)
        if (lv->ob[i].y < rlava) obstacle_reset(g, i);

    g->attached_idx = midx;
    g->target_idx = -1;
    g->color = (MagColor)lv->mag[midx].color;
    g->state = PS_ATTACHED;
    orbit_rest(g, midx);
    g->immune = IMMUNITY_TIME;

    g->lava_y = rlava;
    g->highest_y = lv->mag[midx].y;
    g->last_attach_y = lv->mag[midx].y;
    g->backtrack_active = 0;
    g->backtrack_timer = 0;
    g->lava_speed = g->normal_lava_speed;

    if (g->ai.active) {
        g->ai.x = lv->mag[midx].x;
        g->ai.y = lv->mag[midx].y;
        g->ai.finished = 0;
        g->ai.target_idx = midx;
    }
}

/* ------------------------------------------------------------------ */
void sim_update(GameSim *g, float dt, int color_pressed) {
    /* clear per-frame events */
    g->ev_attach = g->ev_attach_forward = g->ev_checkpoint = 0;
    g->ev_swing = g->ev_death = g->ev_complete = g->ev_bounce = 0;

    if (g->game_over) { update_rotcam(g, dt); return; }

    /* dying: count down to respawn */
    if (g->state == PS_DEAD) {
        g->respawn_timer -= dt;
        if (g->respawn_timer <= 0) respawn(g);
        return;
    }

    g->elapsed += dt;

    /* immunity */
    if (g->immune > 0) { g->immune -= dt; if (g->immune < 0) g->immune = 0; }

    /* combo timeout */
    if (g->combo > 0) {
        g->combo_timer -= dt;
        if (g->combo_timer <= 0) g->combo = 0;
    }

    /* race-lost delay */
    if (g->race_lost) {
        g->race_lost_timer -= dt;
        if (g->race_lost_timer <= 0) {
            g->race_lost = 0;
            g->cur_cp = -1;  /* forces respawn at bottom */
            die(g);
            return;
        }
    }

    /* --- input: swing to nearest magnet of pressed color --- */
    if (color_pressed >= 0 && color_pressed <= 3) {
        g->color = (MagColor)color_pressed;
        int exclude = g->attached_idx >= 0 ? g->attached_idx : g->target_idx;
        int idx = sim_find_magnet(g, g->px, g->py, g->color, exclude);
        if (idx >= 0) start_swing(g, idx);
        else if (g->state == PS_ATTACHED) { g->attached_idx = -1; g->state = PS_FALLING; }
    }

    /* --- player physics --- */
    switch (g->state) {
    case PS_ATTACHED:
        update_orbit(g, dt);
        break;
    case PS_SWINGING: update_swing(g, dt); break;
    case PS_FALLING:  update_fall(g, dt);  break;
    default: break;
    }

    update_mines(g, dt);
    update_obstacles(g, dt);

    /* --- collisions / death checks --- */
    if (g->state != PS_DEAD && g->immune <= 0 && obstacle_hit(g)) { die(g); return; }
    /* Lava: ignores immunity. PLAYABILITY RULE — while you are on a tether
     * the check is taken at the anchor node, not at your dangling feet. The
     * levels were authored assuming you sit on the node, so this keeps every
     * hand-tuned lava margin exactly as designed; the orbit stays a movement
     * mechanic and can never silently make a level unsurvivable. Once you
     * let go (swinging or falling) your real position is what counts. */
    float lava_test_y = g->py;
    if (g->state == PS_ATTACHED && g->attached_idx >= 0)
        lava_test_y = g->lv->mag[g->attached_idx].y;
    if (lava_test_y > g->lava_y) { die(g); return; }
    if (g->state == PS_FALLING && g->py > g->lava_y + 900.0f) { die(g); return; }

    /* --- backtrack lava surge timer --- */
    if (g->backtrack_active) {
        g->backtrack_timer -= dt;
        if (g->backtrack_timer <= 0) {
            g->backtrack_active = 0;
            g->lava_speed = g->normal_lava_speed;
        }
    }

    /* --- lava rise --- */
    if (!g->lava_stopped) {
        g->lava_y -= g->lava_speed * dt;
        consume_below(g);
    }

    /* --- anomalies --- */
    update_rotcam(g, dt);
    if (g->ai.active) {
        update_ai(g, dt);
        if (g->ai.finished && !g->race_lost && !g->game_over) {
            g->race_lost = 1;
            g->race_lost_timer = RACE_LOST_DELAY;
        }
    }
}

float sim_height(const GameSim *g) {
    float h = g->lv->mag[0].y - g->py;
    return h < 0 ? 0 : h;
}

int sim_stars(const GameSim *g) {
    if (!g->won) return 0;
    if (g->deaths == 0) return 3;
    if (g->elapsed <= g->lv->par_time) return 2;
    return 1;
}
