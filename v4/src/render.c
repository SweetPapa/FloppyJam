/* render.c — 3D presentation of the 2D MagLava gameplay plane.
 *
 * The game simulates on a flat plane (x,y). We project that plane into a
 * three-sided neon "shaft": the play plane sits at z=0, walls recede behind
 * it, magnets are glowing spheres, the rising lava is an emissive floor that
 * climbs toward the player, and the camera looks up the well. */
#include "app.h"
#include <math.h>

/* tube depth in game px (play plane is z=0) */
#define Z_BACK  (-120.0f)
#define Z_FRONT (40.0f)
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
}

void render_shutdown(App *a) {
    UnloadTexture(a->tex_glow);
    UnloadTexture(a->tex_spark);
    UnloadTexture(a->tex_flash);
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
    Color surf = (Color){(unsigned char)(255), (unsigned char)(120 + pulse * 60), 40, 255};
    Vector3 c = game_to_world(g_cx, ly, (Z_BACK + Z_FRONT) * 0.5f);
    float w = (g_half * 2.0f + 20.0f) * WS;
    float d = (Z_FRONT - Z_BACK + 20.0f) * WS;
    /* thin slab (visible from every angle, unlike a single-sided plane) */
    DrawCubeV((Vector3){c.x, c.y, c.z}, (Vector3){w, 16.0f * WS, d}, surf);
    /* molten body extending down */
    Vector3 body = game_to_world(g_cx, ly + 900.0f, (Z_BACK + Z_FRONT) * 0.5f);
    DrawCubeV((Vector3){body.x, body.y, body.z}, (Vector3){w, 1800.0f * WS, d}, (Color){150, 25, 10, 255});
    /* glow line along the surface */
    Vector3 gl = game_to_world(g_cx, ly, Z_FRONT);
    draw_glow(a, gl, g_half * 2.6f * WS, (Color){255, 90, 20, 180});
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
        int is_last = (i == lv->n_mag - 1);
        draw_glow(a, pos, (is_last ? 90.0f : 58.0f) * WS * pulse, glow);
        DrawSphere(pos, 22.0f * WS * pulse, main);
        DrawSphere(pos, 11.0f * WS, (Color){255,255,255,255});
        if (is_last) { /* goal marker beacon */
            Vector3 top = game_to_world(lv->mag[i].x, lv->mag[i].y - 60.0f, 0);
            DrawLine3D(pos, top, (Color){255,255,255,180});
        }
    }
}

static void draw_checkpoints(App *a) {
    const LevelDef *lv = a->sim.lv;
    float lx = g_cx - g_half, rx = g_cx + g_half;
    for (int i = 0; i < lv->n_cp; i++) {
        if (fabsf(lv->cp[i].y - a->sim.py) > Y_WINDOW) continue;
        int passed = (a->sim.cur_cp >= 0 && lv->cp[a->sim.cur_cp].y <= lv->cp[i].y);
        Color c = passed ? (Color){80,255,170,200} : (Color){40,120,120,120};
        Vector3 l = game_to_world(lx, lv->cp[i].y, 0);
        Vector3 r = game_to_world(rx, lv->cp[i].y, 0);
        DrawLine3D(l, r, c);
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
            if (o->lstate == 2) { draw_glow(a, s, 30*WS, (Color){255,40,40,150}); draw_glow(a, e, 30*WS, (Color){255,40,40,150}); }
            break;
        }
        case OB_ROAMER: {
            Vector3 c = game_to_world(o->x, o->y, 0);
            draw_glow(a, c, 44.0f * WS, (Color){102,255,102,160});
            DrawSphere(c, ROAMER_SIZE * WS, (Color){0,230,0,255});
            DrawSphereWires(c, ROAMER_SIZE * 1.3f * WS, 5, 6, (Color){150,255,150,180});
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

static void draw_player(App *a) {
    GameSim *g = &a->sim;
    if (g->state == PS_DEAD) return;
    Vector3 pos = game_to_world(g->px, g->py, 0);
    Color glow = mag_color(g->color, 1);
    /* tether while swinging */
    if (g->state == PS_SWINGING && g->target_idx >= 0) {
        Vector3 t = game_to_world(g->lv->mag[g->target_idx].x, g->lv->mag[g->target_idx].y, 0);
        DrawCylinderEx(pos, t, 2.5f * WS, 2.5f * WS, 5, scale_color(mag_color(g->color, 0), 1.0f));
    }
    float spin = a->t * 4.0f;
    draw_glow(a, pos, 70.0f * WS, glow);
    DrawSphere(pos, PLAYER_SIZE * WS, (Color){255,255,255,255});
    DrawSphereWires(pos, PLAYER_SIZE * 1.5f * WS, 6, 8, glow);
    /* orbiting spark */
    Vector3 orb = game_to_world(g->px + cosf(spin) * 30, g->py + sinf(spin) * 30, 0);
    draw_glow(a, orb, 24.0f * WS, glow);
    if (g->immune > 0) {
        int fl = (g->immune > 3.0f) || ((int)(a->t * 8) & 1);
        if (fl) DrawSphereWires(pos, (PLAYER_SIZE + 20) * WS, 8, 10, (Color){0,255,255,150});
    }
}

static void draw_ai(App *a) {
    if (!a->sim.ai.active) return;
    Vector3 p = game_to_world(a->sim.ai.x, a->sim.ai.y, 0);
    draw_glow(a, p, 60.0f * WS, (Color){136,68,255,180});
    DrawSphere(p, 16.0f * WS, (Color){170,100,255,255});
    DrawSphereWires(p, 20.0f * WS, 6, 8, (Color){200,150,255,180});
}

/* ---- particles --------------------------------------------------- */
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

void render_update_particles(App *a, float dt) {
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
        Vector3 w = game_to_world(fx, fy, Z_FRONT * (0.3f + 0.5f * sinf(ph)));
        Color c = (Color){255, 150, 40, (unsigned char)(200 * (1.0f - rise))};
        DrawBillboard(a->cam, a->tex_spark, w, (14.0f - rise * 8.0f) * WS, c);
    }
    EndBlendMode();
}

/* ------------------------------------------------------------------ */
static void update_camera(App *a) {
    float yp = (LEVEL_HEIGHT - a->sim.py) * WS;
    a->cam_y = lerpf(a->cam_y, yp, 0.12f);
    float sx = 0, sy = 0;
    if (a->cam_shake > 0.001f) {
        sx = (sinf(a->t * 90.0f) * a->cam_shake);
        sy = (cosf(a->t * 77.0f) * a->cam_shake);
    }
    a->cam.target = (Vector3){0, a->cam_y + 1.6f, 0};
    a->cam.position = (Vector3){sx, a->cam_y - 2.6f + sy, 8.2f};
    /* rotating-camera anomaly: roll the up-vector */
    float roll = a->sim.rot_cam * 0.01745329f;
    a->cam.up = (Vector3){sinf(roll), cosf(roll), 0};
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
        draw_obstacles(a);
        draw_ai(a);
        draw_player(a);
        draw_particles(a);
    EndMode3D();

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
        /* fill outside the disc's bounding box */
        DrawRectangle(0, 0, sw, (int)(ps.y - r), dk);
        DrawRectangle(0, (int)(ps.y + r), sw, sh, dk);
        DrawRectangle(0, (int)(ps.y - r), (int)(ps.x - r), (int)(r * 2), dk);
        DrawRectangle((int)(ps.x + r), (int)(ps.y - r), sw, (int)(r * 2), dk);
        DrawTexturePro(a->tex_flash, src, dst, (Vector2){0, 0}, 0, WHITE);
    }

    /* danger vignette when lava is close */
    float prox = clampf(1.0f - (g->lava_y - g->py) / LAVA_WARNING_DIST, 0, 1);
    if (prox > 0.01f) {
        Color v = (Color){255, 40, 0, (unsigned char)(prox * 90)};
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        DrawRectangleGradientV(0, sh * 2 / 3, sw, sh / 3, (Color){0,0,0,0}, v);
    }
    if (a->lava_flash > 0.01f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){255, 68, 0, (unsigned char)(a->lava_flash * 60)});
    }
}
