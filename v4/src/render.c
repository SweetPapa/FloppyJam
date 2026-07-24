/* render.c — 3D presentation of the 2D MagLava gameplay plane.
 *
 * The game simulates on a flat plane (x,y). We project that plane into a
 * three-sided neon "shaft": the play plane sits at z=0, walls recede behind
 * it, magnets are glowing spheres, the rising lava is an emissive floor that
 * climbs toward the player, and the camera looks up the well.
 *
 * Depth rule: everything environmental stays BEHIND the play plane (z<0) so
 * it can never occlude the player. Only the player, its trail, and its
 * tether live at z=0. */
#include "app.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* tube depth in game px (play plane is z=0) */
#define Z_BACK  (-120.0f)
#define Z_FRONT (40.0f)
/* lava never reaches past this, so it can't cover the player */
#define Z_LAVA_FRONT (-12.0f)
#define Y_WINDOW 1700.0f   /* draw only within this game-px of the player */

static float g_cx = 270.0f;   /* horizontal center of the level (game px) */
static float g_half = 200.0f; /* shaft half-width (game px) */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

Vector3 game_to_world(float gx, float gy, float gz) {
    Vector3 v = { (gx - g_cx) * WS, (LEVEL_HEIGHT - gy) * WS, gz * WS };
    return v;
}

Color mag_color(MagColor c, int glow) {
    switch (c) {
    case COL_RED:    return glow ? (Color){255,102,102,255} : (Color){255,68,68,255};
    case COL_BLUE:   return glow ? (Color){102,170,255,255} : (Color){68,136,255,255};
    case COL_YELLOW: return glow ? (Color){255,238,102,255} : (Color){255,221,68,255};
    default:         return glow ? (Color){102,255,170,255} : (Color){68,255,136,255};
    }
}

static Color scale_color(Color c, float s) {
    c.r = (unsigned char)clampf(c.r * s, 0, 255);
    c.g = (unsigned char)clampf(c.g * s, 0, 255);
    c.b = (unsigned char)clampf(c.b * s, 0, 255);
    return c;
}
static Color with_alpha(Color c, float a) {
    c.a = (unsigned char)clampf(a * 255.0f, 0, 255);
    return c;
}

/* ------------------------------------------------------------------ */
void render_init(App *a) {
    Image g = GenImageGradientRadial(64, 64, 0.0f, (Color){255,255,255,255}, (Color){255,255,255,0});
    a->tex_glow = LoadTextureFromImage(g);
    UnloadImage(g);
    SetTextureFilter(a->tex_glow, TEXTURE_FILTER_BILINEAR);

    Image s = GenImageGradientRadial(32, 32, 0.1f, (Color){255,255,255,255}, (Color){255,255,255,0});
    a->tex_spark = LoadTextureFromImage(s);
    UnloadImage(s);
    SetTextureFilter(a->tex_spark, TEXTURE_FILTER_BILINEAR);

    /* flashlight: transparent center fading to dark at the edge */
    Image f = GenImageGradientRadial(256, 256, 0.45f, (Color){2,2,10,0}, (Color){2,2,10,235});
    a->tex_flash = LoadTextureFromImage(f);
    UnloadImage(f);
    SetTextureFilter(a->tex_flash, TEXTURE_FILTER_BILINEAR);

    a->cam.position = (Vector3){0, 0, 8.2f};
    a->cam.target = (Vector3){0, 0, 0};
    a->cam.up = (Vector3){0, 1, 0};
    a->cam.fovy = 58.0f;
    a->cam.projection = CAMERA_PERSPECTIVE;
    a->cam_y = (LEVEL_HEIGHT - a->sim.py) * WS;
    a->cam_dist = 8.2f;
    a->time_scale = 1.0f;
}

void render_shutdown(App *a) {
    UnloadTexture(a->tex_glow);
    UnloadTexture(a->tex_spark);
    UnloadTexture(a->tex_flash);
}

void render_reset_fx(App *a) {
    memset(a->parts, 0, sizeof(a->parts));
    memset(a->popups, 0, sizeof(a->popups));
    memset(a->trail, 0, sizeof(a->trail));
    a->part_head = a->popup_head = a->trail_head = 0;
    a->trail_timer = 0;
    a->land_pop = 0;
    a->land_idx = -1;
    a->death_flash = 0;
    a->time_scale = 1.0f;
}

/* Compute per-level horizontal framing. Call after sim_init. */
static void frame_level(App *a) {
    const LevelDef *lv = a->sim.lv;
    float minx = 1e9f, maxx = -1e9f;
    for (int i = 0; i < lv->n_mag; i++) {
        if (lv->mag[i].x < minx) minx = lv->mag[i].x;
        if (lv->mag[i].x > maxx) maxx = lv->mag[i].x;
    }
    g_cx = (minx + maxx) * 0.5f;
    float span = (maxx - minx) * 0.5f + 70.0f;
    g_half = clampf(span, 150.0f, 300.0f);
}

/* ------------------------------------------------------------------ */
static void draw_glow(App *a, Vector3 pos, float size, Color col) {
    BeginBlendMode(BLEND_ADDITIVE);
    DrawBillboard(a->cam, a->tex_glow, pos, size, col);
    EndBlendMode();
}

static void draw_shaft(App *a, float py) {
    float y0 = py - Y_WINDOW, y1 = py + Y_WINDOW;
    float step = 130.0f;
    float ystart = floorf(y0 / step) * step;
    float lx = g_cx - g_half, rx = g_cx + g_half;
    Color wall = (Color){60, 70, 110, 255};
    Color rung;
    for (float gy = ystart; gy <= y1; gy += step) {
        /* brighter near the lava for tension */
        float dl = fabsf(gy - a->sim.lava_y);
        float heat = clampf(1.0f - dl / 700.0f, 0.0f, 1.0f);
        rung = (Color){(unsigned char)(60 + heat * 150), (unsigned char)(70 - heat * 30),
                       (unsigned char)(110 - heat * 70), 255};
        Vector3 blB = game_to_world(lx, gy, Z_BACK);
        Vector3 brB = game_to_world(rx, gy, Z_BACK);
        Vector3 blF = game_to_world(lx, gy, Z_FRONT);
        Vector3 brF = game_to_world(rx, gy, Z_FRONT);
        DrawLine3D(blB, brB, rung);   /* back rung */
        DrawLine3D(blB, blF, rung);   /* left rung  */
        DrawLine3D(brB, brF, rung);   /* right rung */
    }
    /* long posts */
    Vector3 p0 = game_to_world(lx, y0, Z_BACK), p1 = game_to_world(lx, y1, Z_BACK);
    DrawLine3D(p0, p1, wall);
    p0 = game_to_world(rx, y0, Z_BACK); p1 = game_to_world(rx, y1, Z_BACK);
    DrawLine3D(p0, p1, wall);
    p0 = game_to_world(lx, y0, Z_FRONT); p1 = game_to_world(lx, y1, Z_FRONT);
    DrawLine3D(p0, p1, wall);
    p0 = game_to_world(rx, y0, Z_FRONT); p1 = game_to_world(rx, y1, Z_FRONT);
    DrawLine3D(p0, p1, wall);
}

static void draw_lava(App *a) {
    float ly = a->sim.lava_y;
    float pulse = 0.5f + 0.5f * sinf(a->t * 3.0f);
    /* Lava occupies only the space BEHIND the play plane so it can never
     * cover the player, even when it is right at their feet. */
    float zmid = (Z_BACK + Z_LAVA_FRONT) * 0.5f;
    float d = (Z_LAVA_FRONT - Z_BACK) * WS;
    float w = (g_half * 2.0f + 20.0f) * WS;

    Color surf = (Color){255, (unsigned char)(120 + pulse * 60), 40, 255};
    /* wavy crust: a few slabs at slightly different heights */
    for (int i = 0; i < 5; i++) {
        float fx = g_cx - g_half + (g_half * 2.0f) * ((i + 0.5f) / 5.0f);
        float wob = sinf(a->t * 2.4f + i * 1.3f) * 5.0f;
        Vector3 sc = game_to_world(fx, ly + wob, zmid);
        DrawCubeV(sc, (Vector3){w / 5.0f + 0.02f, 14.0f * WS, d}, surf);
    }
    /* molten body extending down */
    Vector3 body = game_to_world(g_cx, ly + 900.0f, zmid);
    DrawCubeV(body, (Vector3){w, 1800.0f * WS, d}, (Color){150, 25, 10, 255});
    /* soft heat glow, kept behind the plane and modest so it never blooms
     * over the player */
    Vector3 gl = game_to_world(g_cx, ly - 10.0f, Z_LAVA_FRONT);
    draw_glow(a, gl, g_half * 1.5f * WS, (Color){255, 100, 25, 90});
}

static void draw_magnets(App *a) {
    const LevelDef *lv = a->sim.lv;
    float py = a->sim.py;
    for (int i = 0; i < lv->n_mag; i++) {
        if (!a->sim.mag_alive[i]) continue;
        if (fabsf(lv->mag[i].y - py) > Y_WINDOW) continue;
        Vector3 pos = game_to_world(lv->mag[i].x, lv->mag[i].y, 0);
        MagColor mc = (MagColor)lv->mag[i].color;
        Color main = mag_color(mc, 0);
        Color glow = mag_color(mc, 1);
        float pulse = 1.0f + 0.12f * sinf(a->t * 6.28f + i);
        /* landing pop */
        if (i == a->land_idx) pulse += a->land_pop * 0.7f;
        int is_last = (i == lv->n_mag - 1);
        draw_glow(a, pos, (is_last ? 90.0f : 58.0f) * WS * pulse, glow);
        DrawSphere(pos, 22.0f * WS * pulse, main);
        DrawSphere(pos, 11.0f * WS, (Color){255,255,255,255});
        if (is_last) { /* goal beacon: rising rings */
            for (int k = 0; k < 3; k++) {
                float ph = fmodf(a->t * 0.6f + k * 0.333f, 1.0f);
                Vector3 rp = game_to_world(lv->mag[i].x, lv->mag[i].y - ph * 120.0f, 0);
                DrawCircle3D(rp, (26.0f + ph * 26.0f) * WS, (Vector3){0,0,1}, 0,
                             with_alpha((Color){255,255,255,255}, (1.0f - ph) * 0.7f));
            }
        }
    }
}

/* Highlight the magnet each color key would tether to right now. This is the
 * single biggest readability aid: it teaches the direction->color mapping. */
static void draw_targets(App *a) {
    GameSim *g = &a->sim;
    if (g->state == PS_DEAD || g->game_over) return;
    int exclude = g->attached_idx >= 0 ? g->attached_idx : g->target_idx;
    for (int c = 0; c < 4; c++) {
        int idx = sim_find_magnet(g, g->px, g->py, (MagColor)c, exclude);
        if (idx < 0) continue;
        Vector3 p = game_to_world(g->lv->mag[idx].x, g->lv->mag[idx].y, 0);
        float ph = fmodf(a->t * 1.4f + c * 0.25f, 1.0f);
        Color col = mag_color((MagColor)c, 1);
        /* contracting reticle ring */
        DrawCircle3D(p, (52.0f - ph * 18.0f) * WS, (Vector3){0,0,1}, 0,
                     with_alpha(col, 0.25f + 0.45f * (1.0f - ph)));
    }
}

static void draw_checkpoints(App *a) {
    const LevelDef *lv = a->sim.lv;
    float lx = g_cx - g_half, rx = g_cx + g_half;
    for (int i = 0; i < lv->n_cp; i++) {
        if (fabsf(lv->cp[i].y - a->sim.py) > Y_WINDOW) continue;
        int passed = (a->sim.cur_cp >= 0 && lv->cp[a->sim.cur_cp].y <= lv->cp[i].y);
        Color c = passed ? (Color){80,255,170,200} : (Color){40,120,120,120};
        float pulse = passed ? 1.0f : 0.6f + 0.4f * sinf(a->t * 2.0f + i);
        Vector3 l = game_to_world(lx, lv->cp[i].y, Z_LAVA_FRONT);
        Vector3 r = game_to_world(rx, lv->cp[i].y, Z_LAVA_FRONT);
        DrawLine3D(l, r, with_alpha(c, pulse));
    }
}

static void draw_obstacles(App *a) {
    for (int i = 0; i < a->sim.n_ob; i++) {
        Obstacle *o = &a->sim.ob[i];
        if (!o->alive) continue;
        if (fabsf(o->y - a->sim.py) > Y_WINDOW) continue;
        const ObstacleDef *d = &a->sim.lv->ob[i];
        switch (o->type) {
        case OB_SWEEPER: {
            float rad = o->angle * 0.01745329f;
            Vector3 pivot = game_to_world(o->x, o->y, 0);
            Vector3 end = game_to_world(o->x + cosf(rad) * d->length,
                                        o->y + sinf(rad) * d->length, 0);
            DrawCylinderEx(pivot, end, 6.0f * WS, 6.0f * WS, 6, (Color){255,102,0,255});
            DrawSphere(pivot, 10.0f * WS, (Color){255,153,51,255});
            draw_glow(a, end, 40.0f * WS, (Color){255,120,0,150});
            break;
        }
        case OB_PULSE: {
            Vector3 c = game_to_world(o->x, o->y, 0);
            for (int k = -1; k <= 1; k++)
                DrawCircle3D(c, (o->radius + k * 3) * WS, (Vector3){0,0,1}, 0,
                             (Color){255,0,255,220});
            draw_glow(a, c, o->radius * 0.9f * WS, (Color){255,0,255,50});
            break;
        }
        case OB_LASER: {
            Vector3 s = game_to_world(d->x, d->y, 0);
            Vector3 e = game_to_world(d->ex, d->ey, 0);
            Color c; float r;
            if (o->lstate == 2) { c = (Color){255,0,0,255}; r = 6.0f * WS; }
            else if (o->lstate == 1) {
                int fl = ((int)(a->t * 20) & 1);
                c = fl ? (Color){255,80,0,220} : (Color){120,30,0,160}; r = 3.5f * WS;
            } else { c = (Color){80,10,10,140}; r = 2.0f * WS; }
            DrawCylinderEx(s, e, r, r, 6, c);
            if (o->lstate == 2) {
                draw_glow(a, s, 30*WS, (Color){255,40,40,150});
                draw_glow(a, e, 30*WS, (Color){255,40,40,150});
            }
            break;
        }
        case OB_ROAMER: {
            Vector3 c = game_to_world(o->x, o->y, 0);
            draw_glow(a, c, 44.0f * WS, (Color){102,255,102,160});
            DrawSphere(c, ROAMER_SIZE * WS, (Color){0,230,0,255});
            DrawSphereWires(c, ROAMER_SIZE * (1.3f + 0.08f * sinf(a->t * 5 + i)) * WS,
                            5, 6, (Color){150,255,150,180});
            break;
        }
        case OB_MINE: {
            Vector3 c = game_to_world(o->x, o->y, 0);
            Color col = o->polarity == 0 ? (Color){0,136,255,255} : (Color){255,68,68,255};
            float pl = 1.0f + 0.2f * sinf(a->t * 7.0f + i);
            DrawSphere(c, MINE_SIZE * WS * pl, col);
            DrawCircle3D(c, MINE_FORCE_RADIUS * WS, (Vector3){0,0,1}, 0, scale_color(col, 0.5f));
            break;
        }
        }
    }
}

/* Motion ribbon: connect the trail samples with tapering segments. Reads far
 * better than stacked billboards (which fight the depth buffer). */
static void draw_trail(App *a) {
    if (a->sim.state == PS_DEAD) return;
    Color base = mag_color(a->sim.color, 1);
    Vector3 prev; int have_prev = 0;
    for (int k = 0; k < TRAIL_LEN; k++) {
        int i = (a->trail_head - 1 - k + TRAIL_LEN * 2) % TRAIL_LEN;
        if (!a->trail[i].used) { have_prev = 0; continue; }
        Vector3 p = { a->trail[i].x, a->trail[i].y, a->trail[i].z };
        float f = 1.0f - (float)k / (float)TRAIL_LEN;
        if (have_prev) {
            /* swells into a comet at speed, thins to almost nothing at rest */
            float mo = 0.35f + a->speed_norm * 1.5f;
            float r = (1.5f + 6.0f * f) * WS * mo;
            /* skip degenerate segments (raylib dislikes zero-length cylinders) */
            float dx = p.x - prev.x, dy = p.y - prev.y, dz = p.z - prev.z;
            if (dx*dx + dy*dy + dz*dz > 1e-8f)
                DrawCylinderEx(prev, p, r, r * 0.85f, 4,
                               with_alpha(base, f * f * (0.25f + a->speed_norm * 0.6f)));
        }
        prev = p; have_prev = 1;
    }
}

static void draw_player(App *a) {
    GameSim *g = &a->sim;
    if (g->state == PS_DEAD) return;
    Vector3 pos = game_to_world(g->px, g->py, 0);
    Color glow = mag_color(g->color, 1);
    /* tether while swinging: taut beam with a bright core */
    if (g->state == PS_SWINGING && g->target_idx >= 0) {
        Vector3 t = game_to_world(g->lv->mag[g->target_idx].x, g->lv->mag[g->target_idx].y, 0);
        DrawCylinderEx(pos, t, 3.5f * WS, 1.5f * WS, 6, with_alpha(mag_color(g->color, 0), 0.55f));
        DrawCylinderEx(pos, t, 1.2f * WS, 0.6f * WS, 5, (Color){255,255,255,200});
    }
    float spin = a->t * 4.0f;
    float speed = sqrtf(g->vx * g->vx + g->vy * g->vy);
    float stretch = 1.0f + clampf(speed / 900.0f, 0, 0.35f);
    /* Softer halo than a magnet's so the bright white core stays the thing
     * your eye locks onto — the player must never read as a node. */
    draw_glow(a, pos, 54.0f * WS * stretch, with_alpha(glow, 0.7f));
    DrawSphere(pos, PLAYER_SIZE * WS, (Color){255,255,255,255});
    DrawSphereWires(pos, PLAYER_SIZE * 1.5f * WS, 6, 8, glow);
    /* orbiting spark shows the armed color */
    Vector3 orb = game_to_world(g->px + cosf(spin) * 30, g->py + sinf(spin) * 30, 0);
    draw_glow(a, orb, 24.0f * WS, glow);
    if (g->immune > 0) {
        int fl = (g->immune > 3.0f) || ((int)(a->t * 8) & 1);
        if (fl) {
            DrawSphereWires(pos, (PLAYER_SIZE + 20 + 3 * sinf(a->t * 6)) * WS, 8, 10,
                            (Color){0,255,255,150});
        }
    }
}

static void draw_ai(App *a) {
    if (!a->sim.ai.active) return;
    Vector3 p = game_to_world(a->sim.ai.x, a->sim.ai.y, 0);
    draw_glow(a, p, 60.0f * WS, (Color){136,68,255,180});
    DrawSphere(p, 16.0f * WS, (Color){170,100,255,255});
    DrawSphereWires(p, 20.0f * WS, 6, 8, (Color){200,150,255,180});
}

/* ---- particles / trail / popups ---------------------------------- */
void render_spawn_burst(App *a, float gx, float gy, Color col, int n, float spd) {
    for (int i = 0; i < n; i++) {
        Particle *p = &a->parts[a->part_head];
        a->part_head = (a->part_head + 1) % MAX_PARTICLES;
        Vector3 w = game_to_world(gx, gy, 0);
        p->x = w.x; p->y = w.y; p->z = w.z;
        unsigned int r = (a->part_head * 2654435761u) ^ (i * 40503u) ^ (unsigned)(gx * 13);
        float ang = (float)(r & 1023) / 1024.0f * 6.2832f;
        float el = (float)((r >> 10) & 1023) / 1024.0f * 3.1416f;
        float sp = spd * (0.4f + (float)((r >> 20) & 255) / 255.0f) * WS;
        p->vx = cosf(ang) * sinf(el) * sp;
        p->vy = fabsf(cosf(el)) * sp;
        p->vz = sinf(ang) * sinf(el) * sp * 0.5f;
        p->life = p->max_life = 0.4f + (float)((r >> 5) & 63) / 63.0f * 0.5f;
        p->size = (6.0f + (float)((r >> 12) & 15)) * WS;
        p->color = col;
        p->active = 1;
    }
}

void render_push_popup(App *a, float gx, float gy, const char *text, Color c) {
    Popup *p = &a->popups[a->popup_head];
    a->popup_head = (a->popup_head + 1) % MAX_POPUPS;
    p->gx = gx; p->gy = gy;
    p->life = p->max_life = 1.1f;
    snprintf(p->text, sizeof p->text, "%s", text);
    p->color = c;
    p->active = 1;
}

void render_update_fx(App *a, float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &a->parts[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0) { p->active = 0; continue; }
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->z += p->vz * dt;
        p->vy -= 2.2f * dt; /* gentle gravity in world space */
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        Popup *p = &a->popups[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0) p->active = 0;
    }
    /* sample the player position into the trail */
    a->trail_timer -= dt;
    if (a->trail_timer <= 0 && a->sim.state != PS_DEAD) {
        a->trail_timer = 0.016f;
        Vector3 w = game_to_world(a->sim.px, a->sim.py, 0);
        TrailPt *tp = &a->trail[a->trail_head];
        tp->x = w.x; tp->y = w.y; tp->z = w.z; tp->used = 1;
        a->trail_head = (a->trail_head + 1) % TRAIL_LEN;
    }
    /* ---- momentum tracking -------------------------------------------
     * The curve deliberately hugs zero at low speed so hanging on a node
     * feels like a heavy, static drag, then ramps hard once you commit to a
     * swing — the contrast is what sells the acceleration. */
    float speed = sqrtf(a->sim.vx * a->sim.vx + a->sim.vy * a->sim.vy);
    if (a->sim.state == PS_ATTACHED || a->sim.state == PS_DEAD) speed = 0.0f;
    float raw_n = clampf((speed - 190.0f) / 460.0f, 0.0f, 1.0f);
    raw_n = raw_n * raw_n; /* stays low, then rises fast */
    /* fast attack, slow release: motion blooms instantly and lingers */
    float rate = (raw_n > a->speed_norm) ? 12.0f : 3.0f;
    a->speed_norm += (raw_n - a->speed_norm) * fminf(1.0f, dt * rate);

    /* a hard shove of acceleration fires the anime speed lines */
    float accel = (speed - a->prev_speed);
    if (accel > 55.0f && speed > 240.0f) {
        float amt = clampf(accel / 260.0f, 0.25f, 1.0f);
        if (amt > a->speed_burst) a->speed_burst = amt;
    }
    a->prev_speed = speed;
    a->speed_burst = fmaxf(0.0f, a->speed_burst - dt * 3.2f); /* short and sharp */

    /* decay pops/flashes */
    a->land_pop = fmaxf(0.0f, a->land_pop - dt * 3.5f);
    a->death_flash = fmaxf(0.0f, a->death_flash - dt * 2.0f);
    if (a->fade > 0) a->fade = fmaxf(0.0f, a->fade - dt * 2.2f);
    a->intro_t += dt;
    /* ease the slow-motion back to normal speed */
    a->time_scale = lerpf(a->time_scale, 1.0f, dt * 2.5f);
    if (a->time_scale > 0.995f) a->time_scale = 1.0f;
}

static void draw_particles(App *a) {
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &a->parts[i];
        if (!p->active) continue;
        float t = p->life / p->max_life;
        Color c = p->color; c.a = (unsigned char)(255 * clampf(t, 0, 1));
        DrawBillboard(a->cam, a->tex_spark, (Vector3){p->x, p->y, p->z}, p->size * (0.5f + t), c);
    }
    EndBlendMode();
}

/* ---- lava embers ------------------------------------------------- */
static void draw_embers(App *a) {
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < 26; i++) {
        float ph = a->t * 0.6f + i * 0.618f;
        float fx = g_cx - g_half + fmodf(i * 71.3f, g_half * 2.0f);
        float rise = fmodf(ph, 1.0f);
        float fy = a->sim.lava_y - rise * 300.0f;
        if (fabsf(fy - a->sim.py) > Y_WINDOW) continue;
        /* keep embers behind the play plane too */
        Vector3 w = game_to_world(fx, fy, Z_LAVA_FRONT * (0.3f + 0.6f * sinf(ph)));
        Color c = (Color){255, 150, 40, (unsigned char)(200 * (1.0f - rise))};
        DrawBillboard(a->cam, a->tex_spark, w, (14.0f - rise * 8.0f) * WS, c);
    }
    EndBlendMode();
}

/* In-world rush streaks: short bright dashes scrolling past the player,
 * aligned to the direction of travel. Gives the shaft itself a sense of
 * rushing by, so speed is felt in the world and not just the HUD. */
static void draw_rush(App *a) {
    float sn = a->speed_norm;
    if (sn < 0.04f) return;
    float vx = a->sim.vx, vy = a->sim.vy;
    float sp = sqrtf(vx * vx + vy * vy);
    if (sp < 1.0f) return;
    float ux = vx / sp, uy = vy / sp;

    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < 30; i++) {
        unsigned int h = (unsigned int)i * 2246822519u + 12345u;
        float ox = (float)((h >> 8) & 511) / 511.0f * 2.0f - 1.0f;
        float oy = (float)((h >> 17) & 511) / 511.0f * 2.0f - 1.0f;
        /* scroll each dash backwards along the travel axis */
        float scroll = fmodf(a->t * (1.2f + sn * 3.0f) + (float)i * 0.37f, 1.0f);
        float back = scroll * 640.0f - 320.0f;
        float gx = a->sim.px + ox * 300.0f - ux * back;
        float gy = a->sim.py + oy * 340.0f - uy * back;
        float len = 26.0f + 110.0f * sn;
        Vector3 p0 = game_to_world(gx, gy, -24.0f);
        Vector3 p1 = game_to_world(gx - ux * len, gy - uy * len, -24.0f);
        /* fade in at the ends of the scroll so dashes don't pop */
        float fade = sinf(scroll * 3.1416f);
        Color c = (Color){190, 215, 255, (unsigned char)(clampf(120.0f * sn * fade, 0, 255))};
        DrawLine3D(p0, p1, c);
    }
    EndBlendMode();
}

/* Anime speed lines: screen-space streaks radiating from a focus point just
 * ahead of the player. They re-scatter ~18x/sec for that hand-inked,
 * frame-by-frame look, and only really bloom on a burst of acceleration. */
static void draw_speed_lines(App *a) {
    /* Weighted hard toward the burst: sustained flight keeps only a whisper
     * of streaking, so the lines punctuate acceleration instead of becoming
     * permanent screen furniture. */
    float intensity = a->speed_norm * 0.20f + a->speed_burst * 0.95f;
    if (intensity < 0.03f) return;
    intensity = clampf(intensity, 0.0f, 1.1f);

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Vector2 focus = GetWorldToScreen(game_to_world(a->sim.px, a->sim.py, 0), a->cam);
    /* push the vanishing point ahead of travel so the world streaks past */
    float sp = sqrtf(a->sim.vx * a->sim.vx + a->sim.vy * a->sim.vy);
    if (sp > 1.0f) {
        focus.x += (a->sim.vx / sp) * 70.0f;
        focus.y += (-a->sim.vy / sp) * -70.0f; /* game y is inverted on screen */
    }

    float maxr = sqrtf((float)(sw * sw + sh * sh)) * 0.62f;
    /* the clear centre shrinks as we accelerate, tightening the tunnel */
    float inner = 190.0f - 90.0f * intensity;
    Color tint = mag_color(a->sim.color, 1);
    int tick = (int)(a->t * 18.0f); /* re-scatter for hand-drawn shimmer */
    int n = 14 + (int)(intensity * 30.0f);

    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < n; i++) {
        unsigned int h = ((unsigned int)i * 2654435761u) ^ ((unsigned int)tick * 40503u);
        float ang = (float)((h >> 8) & 1023) / 1024.0f * 6.28318f;
        float len = 0.30f + (float)((h >> 18) & 255) / 255.0f * 0.70f;
        float r0 = inner + (float)((h >> 3) & 63);
        float r1 = r0 + maxr * len * (0.45f + intensity * 0.75f);
        float ca = cosf(ang), sa = sinf(ang);
        Vector2 p0 = {focus.x + ca * r0, focus.y + sa * r0};
        Vector2 p1 = {focus.x + ca * r1, focus.y + sa * r1};
        float thick = 1.0f + 3.0f * intensity * (0.35f + len * 0.65f);
        /* mostly white with a wash of the armed colour */
        Color c = {(unsigned char)((tint.r + 510) / 3), (unsigned char)((tint.g + 510) / 3),
                   (unsigned char)((tint.b + 510) / 3),
                   (unsigned char)clampf(170.0f * intensity * (0.35f + len * 0.65f), 0, 255)};
        DrawLineEx(p0, p1, thick, c);
    }
    EndBlendMode();
}

/* ------------------------------------------------------------------ */
static void update_camera(App *a) {
    GameSim *g = &a->sim;
    float yp = (LEVEL_HEIGHT - g->py) * WS;
    /* velocity look-ahead makes fast swings feel anticipated */
    float lead = clampf(-g->vy * WS * 0.18f, -1.2f, 1.6f);
    a->cam_y = lerpf(a->cam_y, yp + lead, 0.11f);

    float xp = (g->px - g_cx) * WS;
    a->cam_x = lerpf(a->cam_x, xp * 0.45f, 0.07f);

    /* Dolly + FOV both widen with momentum: pulled in and tight when you're
     * hanging still, kicked wide the instant you launch. */
    float want = 7.9f + a->speed_norm * 1.5f + a->speed_burst * 0.5f;
    a->cam_dist = lerpf(a->cam_dist, want, 0.08f);
    float want_fov = a->speed_norm * 7.0f + a->speed_burst * 9.0f;
    a->fov_extra = lerpf(a->fov_extra, want_fov, 0.14f);
    a->cam.fovy = 58.0f + a->fov_extra;

    float sx = 0, sy = 0;
    if (a->cam_shake > 0.001f) {
        sx = sinf(a->t * 90.0f) * a->cam_shake;
        sy = cosf(a->t * 77.0f) * a->cam_shake;
    }
    /* high-speed micro-jitter: engine rumble, not a hit */
    if (a->speed_norm > 0.25f) {
        float j = (a->speed_norm - 0.25f) * 0.05f;
        sx += sinf(a->t * 61.0f) * j;
        sy += cosf(a->t * 53.0f) * j;
    }
    a->cam.target = (Vector3){a->cam_x, a->cam_y + 1.6f, 0};
    a->cam.position = (Vector3){a->cam_x + sx, a->cam_y - 2.6f + sy, a->cam_dist};
    /* rotating-camera anomaly: roll the up-vector */
    float roll = g->rot_cam * 0.01745329f;
    a->cam.up = (Vector3){sinf(roll), cosf(roll), 0};
}

/* screen-space overlays that need the 3D camera */
static void draw_overlays(App *a) {
    GameSim *g = &a->sim;

    /* key hints floating on the candidate magnets */
    if (g->state != PS_DEAD && !g->game_over) {
        static const char *KEYS[4] = {"W", "S", "A", "D"};
        int exclude = g->attached_idx >= 0 ? g->attached_idx : g->target_idx;
        Vector2 pscr = GetWorldToScreen(game_to_world(g->px, g->py, 0), a->cam);
        for (int c = 0; c < 4; c++) {
            int idx = sim_find_magnet(g, g->px, g->py, (MagColor)c, exclude);
            if (idx < 0) continue;
            Vector3 p = game_to_world(g->lv->mag[idx].x, g->lv->mag[idx].y, 0);
            Vector2 s = GetWorldToScreen(p, a->cam);
            if (s.x < -50 || s.y < -50 || s.x > GetScreenWidth() + 50 ||
                s.y > GetScreenHeight() + 50) continue;
            /* don't stamp a badge on top of the player */
            float ddx = s.x - pscr.x, ddy = s.y - pscr.y;
            if (ddx * ddx + ddy * ddy < 34.0f * 34.0f) continue;
            Color col = mag_color((MagColor)c, 1);
            DrawCircle((int)s.x, (int)s.y, 11, (Color){8, 8, 16, 190});
            DrawCircleLines((int)s.x, (int)s.y, 11, col);
            int w = MeasureText(KEYS[c], 14);
            DrawText(KEYS[c], (int)s.x - w / 2, (int)s.y - 7, 14, col);
        }
    }

    /* floating score popups */
    for (int i = 0; i < MAX_POPUPS; i++) {
        Popup *p = &a->popups[i];
        if (!p->active) continue;
        float f = p->life / p->max_life;
        Vector3 w = game_to_world(p->gx, p->gy, 0);
        Vector2 s = GetWorldToScreen(w, a->cam);
        int sz = 22;
        int tw = MeasureText(p->text, sz);
        float rise = (1.0f - f) * 46.0f;
        Color c = with_alpha(p->color, f);
        DrawText(p->text, (int)s.x - tw / 2 + 1, (int)(s.y - rise) + 1, sz,
                 with_alpha((Color){0,0,0,255}, f * 0.6f));
        DrawText(p->text, (int)s.x - tw / 2, (int)(s.y - rise), sz, c);
    }
}

void render_world(App *a) {
    GameSim *g = &a->sim;
    if (g->level_id) frame_level(a); /* cheap; keeps framing correct */
    update_camera(a);

    /* 2D background gradient from the level color */
    unsigned int bg = g->lv->bg_color;
    Color top = (Color){(bg >> 16) & 0xFF, (bg >> 8) & 0xFF, bg & 0xFF, 255};
    Color bot = scale_color(top, 2.2f);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), scale_color(top, 0.6f), bot);

    BeginMode3D(a->cam);
        draw_shaft(a, g->py);
        draw_checkpoints(a);
        draw_lava(a);
        draw_embers(a);
        draw_magnets(a);
        draw_targets(a);
        draw_obstacles(a);
        draw_rush(a);
        draw_ai(a);
        draw_trail(a);
        draw_player(a);
        draw_particles(a);
    EndMode3D();

    draw_speed_lines(a);
    draw_overlays(a);

    /* flashlight anomaly (level 12): darken all but a disc around player */
    if (g->level_id == LEVEL_FLASHLIGHT && g->state != PS_DEAD) {
        Vector3 pw = game_to_world(g->px, g->py, 0);
        Vector2 ps = GetWorldToScreen(pw, a->cam);
        int mn = GetScreenWidth() < GetScreenHeight() ? GetScreenWidth() : GetScreenHeight();
        float r = mn * 0.40f;
        Rectangle src = {0, 0, (float)a->tex_flash.width, (float)a->tex_flash.height};
        Rectangle dst = {ps.x - r, ps.y - r, r * 2, r * 2};
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        Color dk = (Color){2, 2, 10, 235};
        DrawRectangle(0, 0, sw, (int)(ps.y - r), dk);
        DrawRectangle(0, (int)(ps.y + r), sw, sh, dk);
        DrawRectangle(0, (int)(ps.y - r), (int)(ps.x - r), (int)(r * 2), dk);
        DrawRectangle((int)(ps.x + r), (int)(ps.y - r), sw, (int)(r * 2), dk);
        DrawTexturePro(a->tex_flash, src, dst, (Vector2){0, 0}, 0, WHITE);
    }

    /* danger vignette when lava is close — edges only, never the center */
    float prox = clampf(1.0f - (g->lava_y - g->py) / LAVA_WARNING_DIST, 0, 1);
    if (prox > 0.01f) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float beat = 0.75f + 0.25f * sinf(a->t * 9.0f);
        unsigned char al = (unsigned char)(prox * beat * 130);
        Color v = (Color){255, 40, 0, al};
        Color clear = (Color){255, 40, 0, 0};
        DrawRectangleGradientV(0, sh - sh / 3, sw, sh / 3, clear, v);
        DrawRectangleGradientV(0, 0, sw, sh / 6, (Color){255,40,0,(unsigned char)(al*0.5f)}, clear);
        DrawRectangleGradientH(0, 0, sw / 8, sh, (Color){255,40,0,(unsigned char)(al*0.6f)}, clear);
        DrawRectangleGradientH(sw - sw / 8, 0, sw / 8, sh, clear, (Color){255,40,0,(unsigned char)(al*0.6f)});
    }
    if (a->lava_flash > 0.01f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){255, 68, 0, (unsigned char)(a->lava_flash * 55)});
    }
    if (a->death_flash > 0.01f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){255, 0, 0, (unsigned char)(a->death_flash * 110)});
    }
    if (a->fade > 0.001f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){0, 0, 0, (unsigned char)(clampf(a->fade, 0, 1) * 255)});
    }
}
