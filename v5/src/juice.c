#include "juice.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    V3 p, v;
    float life, life0, size, grav;
    Color col;
    unsigned char kind;   /* 0 = billboard dot, 1 = confetti flake, 2 = ring */
} Particle;

static Particle par[BP_MAX_PARTICLES];
static int      par_head = 0;

static float hitstop_t = 0.0f;
static float slowmo_t = 0.0f, slowmo_k = 1.0f;
static float flash_a = 0.0f;
static Color flash_c;

static char  ban_big[32], ban_small[64];
static float ban_t = 0.0f, ban_hold = 0.0f;
static Color ban_tint;

static char  toast_s[96];
static float toast_t = 0.0f;

typedef struct {
    V3    p;
    float life, life0;
    Color c;
    char  s[16];
    unsigned char big;
} Popup;
static Popup pop[BP_MAX_POPUPS];
static int   pop_head = 0;

static float edge_a = 0.0f;
static Color edge_c;
static float speed_a = 0.0f;

/* cosmetic PRNG — never read by the sim (5.1) */
static unsigned int rs = 0x1234567u;
static float rnd(void)
{
    rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
    return (float)(rs & 0xffffu) / 65535.0f;
}
static float rnds(void) { return rnd() * 2.0f - 1.0f; }

void bp_juice_reset(void)
{
    memset(par, 0, sizeof(par));
    memset(pop, 0, sizeof(pop));
    par_head = pop_head = 0;
    hitstop_t = slowmo_t = flash_a = 0.0f;
    edge_a = speed_a = 0.0f;
    slowmo_k = 1.0f;
    ban_t = toast_t = 0.0f;
}

static Particle *alloc_par(void)
{
    Particle *p = &par[par_head];
    par_head = (par_head + 1) % BP_MAX_PARTICLES;   /* pooled, never grows */
    memset(p, 0, sizeof(*p));
    return p;
}

void bp_burst(V3 at, int count, Color c, float speed, float life, float gravity)
{
    int i;
    for (i = 0; i < count; ++i) {
        Particle *p = alloc_par();
        V3 d = v3norm(v3(rnds(), rnd() * 0.9f + 0.1f, rnds()));
        p->p = at;
        p->v = v3mul(d, speed * (0.45f + 0.55f * rnd()));
        p->life = p->life0 = life * (0.6f + 0.6f * rnd());
        p->size = 0.012f + 0.014f * rnd();
        p->col = c;
        p->grav = gravity;
        p->kind = 0;
    }
}

void bp_confetti(V3 at, int count, float power)
{
    static const Color CONF[6] = {
        { 255, 214,  92, 255 }, { 255, 118, 122, 255 }, { 118, 226, 199, 255 },
        { 150, 176, 255, 255 }, { 255, 255, 255, 255 }, { 214, 150, 255, 255 },
    };
    int i;
    for (i = 0; i < count; ++i) {
        Particle *p = alloc_par();
        V3 d = v3norm(v3(rnds(), 1.4f + rnd(), rnds()));
        p->p = v3add(at, v3(rnds() * 0.05f, 0.02f, rnds() * 0.05f));
        p->v = v3mul(d, (1.4f + 1.9f * rnd()) * power);
        p->life = p->life0 = 1.5f + 1.4f * rnd();
        p->size = 0.020f + 0.020f * rnd();
        p->col = CONF[(int)(rnd() * 5.999f)];
        p->grav = 3.4f;
        p->kind = 1;
    }
}

/* Sparkles are drawn as tiny cubes that flare and shrink rather than fall,
 * so they read as light rather than debris. */
void bp_sparkle(V3 at, int count, Color c, float spread, float life)
{
    int i;
    for (i = 0; i < count; ++i) {
        Particle *p = alloc_par();
        p->p = v3add(at, v3(rnds() * spread, rnd() * spread * 0.8f, rnds() * spread));
        p->v = v3mul(v3norm(v3(rnds(), rnd() * 0.6f + 0.2f, rnds())),
                     0.25f + 0.55f * rnd());
        p->life = p->life0 = life * (0.55f + 0.75f * rnd());
        p->size = 0.010f + 0.016f * rnd();
        p->col = c;
        p->grav = 0.35f;
        p->kind = 4;
    }
}

void bp_popup(V3 at, const char *text, Color c, int big)
{
    Popup *p = &pop[pop_head];
    pop_head = (pop_head + 1) % BP_MAX_POPUPS;
    p->p = at;
    p->life = p->life0 = big ? 1.5f : 1.1f;
    p->c = c;
    p->big = (unsigned char)(big != 0);
    snprintf(p->s, sizeof p->s, "%s", text ? text : "");
}

void bp_edge_pulse(Color c, float amount)
{
    edge_c = c;
    if (amount > edge_a) edge_a = bp_clampf(amount, 0.0f, 1.0f);
}

void bp_speedlines(float amount) { speed_a = bp_clampf(amount, 0.0f, 1.0f); }

void bp_shockwave(V3 at, Color c, float r)
{
    Particle *p = alloc_par();
    p->p = at;
    p->v = v3zero();
    p->life = p->life0 = 0.28f;
    p->size = r;
    p->col = c;
    p->kind = 2;
}

void bp_flash(Color c, float amount)
{
    flash_c = c;
    if (amount > flash_a) flash_a = amount;
}

/* ---- wind: atmosphere, never physics ------------------------------- */

static float wind_ang = 0.7f, wind_str = 0.5f, mote_acc = 0.0f;

void bp_wind_set(float angle, float strength)
{
    wind_ang = angle;
    wind_str = bp_clampf(strength, 0.0f, 1.0f);
}
float bp_wind_angle(void)    { return wind_ang; }
float bp_wind_strength(void) { return wind_str; }

/* Drifting motes seeded around the camera's interest point. They are just
 * particles with the breeze baked into their velocity. */
void bp_wind_motes(V3 centre, float dt)
{
    V3 dir = v3(sinf(wind_ang), 0.0f, cosf(wind_ang));
    mote_acc += dt * (5.0f + 16.0f * wind_str);
    while (mote_acc >= 1.0f) {
        Particle *p = alloc_par();
        mote_acc -= 1.0f;
        p->p = v3add(centre, v3(rnds() * 3.4f, 0.05f + rnd() * 1.15f, rnds() * 3.4f));
        p->v = v3add(v3mul(dir, (0.35f + 1.5f * wind_str) * (0.6f + 0.7f * rnd())),
                     v3(rnds() * 0.10f, rnds() * 0.06f, rnds() * 0.10f));
        p->life = p->life0 = 1.4f + 1.6f * rnd();
        p->size = 0.007f + 0.008f * rnd();
        p->col = (Color){ 236, 244, 226, 110 };
        p->grav = 0.0f;
        p->kind = 3;
    }
}

void bp_hitstop(float seconds) { if (seconds > hitstop_t) hitstop_t = seconds; }
float bp_hitstop_left(void) { return hitstop_t; }

void bp_slowmo(float seconds, float scale)
{
    slowmo_t = seconds;
    slowmo_k = scale;
}

float bp_timescale(void) { return (slowmo_t > 0.0f) ? slowmo_k : 1.0f; }

void bp_banner(const char *big, const char *small, float hold, Color tint)
{
    snprintf(ban_big, sizeof ban_big, "%s", big ? big : "");
    snprintf(ban_small, sizeof ban_small, "%s", small ? small : "");
    ban_t = 0.0f;
    ban_hold = hold;
    ban_tint = tint;
}

void bp_toast(const char *text, float hold)
{
    snprintf(toast_s, sizeof toast_s, "%s", text ? text : "");
    toast_t = hold;
}

void bp_juice_update(float dt)
{
    int i;
    if (hitstop_t > 0.0f) hitstop_t -= dt;
    if (slowmo_t > 0.0f)  slowmo_t  -= dt;
    if (flash_a > 0.0f)   flash_a = bp_clampf(flash_a - dt * 3.2f, 0.0f, 1.0f);
    if (edge_a > 0.0f)    edge_a  = bp_clampf(edge_a - dt * 2.1f, 0.0f, 1.0f);
    if (toast_t > 0.0f)   toast_t -= dt;
    if (ban_hold > 0.0f) { ban_t += dt; if (ban_t > ban_hold) ban_hold = 0.0f; }

    for (i = 0; i < BP_MAX_POPUPS; ++i) {
        if (pop[i].life <= 0.0f) continue;
        pop[i].life -= dt;
        pop[i].p.y += dt * 0.55f;      /* drifts up off the felt */
    }

    for (i = 0; i < BP_MAX_PARTICLES; ++i) {
        Particle *p = &par[i];
        if (p->life <= 0.0f) continue;
        p->life -= dt;
        if (p->kind == 2) continue;
        p->v.y -= p->grav * dt;
        if (p->kind == 1 || p->kind == 3) {
            /* confetti and motes get pushed around by the breeze */
            V3 wd = v3(sinf(wind_ang), 0.0f, cosf(wind_ang));
            float push = (p->kind == 3 ? 0.9f : 0.45f) * wind_str * dt;
            p->v = v3add(p->v, v3mul(wd, push));
        }
        p->p = v3add(p->p, v3mul(p->v, dt));
    }
}

void bp_juice_draw_world(void)
{
    int i;
    for (i = 0; i < BP_MAX_PARTICLES; ++i) {
        Particle *p = &par[i];
        float u;
        Color c;
        if (p->life <= 0.0f) continue;
        u = p->life / p->life0;
        c = p->col;
        c.a = (unsigned char)(bp_clampf(u * 1.6f, 0.0f, 1.0f) * 255.0f);
        if (p->kind == 2) {
            float r = p->size * (1.0f - u) * 3.0f + p->size * 0.2f;
            DrawCircle3D((Vector3){ p->p.x, p->p.y + 0.004f, p->p.z }, r,
                         (Vector3){ 1, 0, 0 }, 90.0f, c);
        } else if (p->kind == 1) {
            DrawCubeV((Vector3){ p->p.x, p->p.y, p->p.z },
                      (Vector3){ p->size, p->size * 0.35f, p->size }, c);
        } else if (p->kind == 4) {
            /* flare out then shrink to nothing, with a halo either side */
            float f = (u > 0.7f) ? (1.0f - u) / 0.3f : u / 0.7f;
            float s = p->size * (0.5f + 1.9f * f);
            DrawCubeV((Vector3){ p->p.x, p->p.y, p->p.z },
                      (Vector3){ s * 3.2f, s * 0.35f, s * 0.35f }, c);
            DrawCubeV((Vector3){ p->p.x, p->p.y, p->p.z },
                      (Vector3){ s * 0.35f, s * 3.2f, s * 0.35f }, c);
            c.a = (unsigned char)(c.a / 4);
            DrawCubeV((Vector3){ p->p.x, p->p.y, p->p.z },
                      (Vector3){ s * 2.0f, s * 2.0f, s * 2.0f }, c);
        } else {
            DrawCubeV((Vector3){ p->p.x, p->p.y, p->p.z },
                      (Vector3){ p->size, p->size, p->size }, c);
        }
    }
}

/* Text pinned to a spot on the table. Behind the camera, GetWorldToScreenEx
 * folds the point back into the frame, so anything that lands outside a
 * generous margin is dropped rather than drawn in the wrong place. */
void bp_juice_draw_popups(Camera3D cam, int sw, int sh)
{
    int i;
    for (i = 0; i < BP_MAX_POPUPS; ++i) {
        Popup *p = &pop[i];
        Vector2 s;
        float u, rise, a;
        int fs, w;
        if (p->life <= 0.0f || !p->s[0]) continue;
        u = 1.0f - p->life / p->life0;
        s = GetWorldToScreenEx((Vector3){ p->p.x, p->p.y, p->p.z }, cam, sw, sh);
        if (s.x < -200.0f || s.x > (float)sw + 200.0f ||
            s.y < -200.0f || s.y > (float)sh + 200.0f) continue;
        /* overshoot on the way in, then ease out and fade */
        rise = (u < 0.16f) ? (u / 0.16f) * 1.18f : 1.0f + 0.18f * (1.0f - (u - 0.16f) / 0.84f);
        a = bp_clampf((1.0f - u) * 2.4f, 0.0f, 1.0f);
        fs = (int)((p->big ? 34.0f : 22.0f) * (0.55f + 0.45f * rise));
        w = MeasureText(p->s, fs);
        DrawText(p->s, (int)s.x - w / 2 + 2, (int)s.y - fs / 2 + 2, fs,
                 (Color){ 0, 0, 0, (unsigned char)(a * 150.0f) });
        DrawText(p->s, (int)s.x - w / 2, (int)s.y - fs / 2, fs,
                 (Color){ p->c.r, p->c.g, p->c.b, (unsigned char)(a * 255.0f) });
    }
}

void bp_juice_draw_hud(int sw, int sh)
{
    /* Speed streaks: cheap radial motion blur while the ball is quick. */
    if (speed_a > 0.02f) {
        int i;
        float cx = (float)sw * 0.5f, cy = (float)sh * 0.5f;
        for (i = 0; i < 26; ++i) {
            float a = BP_TAU * (float)i / 26.0f + (float)(i & 3) * 0.09f;
            float r0 = (float)sw * (0.34f - 0.06f * speed_a);
            float r1 = r0 + (float)sw * (0.10f + 0.22f * speed_a);
            float ca = cosf(a), sa = sinf(a);
            unsigned char al = (unsigned char)(speed_a * 46.0f);
            DrawLineEx((Vector2){ cx + ca * r0, cy + sa * r0 * 0.62f },
                       (Vector2){ cx + ca * r1, cy + sa * r1 * 0.62f },
                       2.0f + 2.0f * speed_a,
                       (Color){ 220, 236, 255, al });
        }
    }
    if (edge_a > 0.004f) {
        /* a wash that hugs the frame instead of flooding it */
        int b = sh / 5, s = sw / 6;
        Color c0 = edge_c, c1 = edge_c;
        c0.a = (unsigned char)(edge_a * 170.0f);
        c1.a = 0;
        DrawRectangleGradientV(0, 0, sw, b, c0, c1);
        DrawRectangleGradientV(0, sh - b, sw, b, c1, c0);
        DrawRectangleGradientH(0, 0, s, sh, c0, c1);
        DrawRectangleGradientH(sw - s, 0, s, sh, c1, c0);
    }
    if (flash_a > 0.001f) {
        Color c = flash_c;
        c.a = (unsigned char)(flash_a * 105.0f);
        DrawRectangle(0, 0, sw, sh, c);
    }
    if (ban_hold > 0.0f) {
        /* slam in, hold, fall away — now with a light bar that wipes out from
         * the centre behind the word and a tube glow around the letters */
        float u = ban_t / ban_hold;
        float slam = (ban_t < 0.14f) ? (ban_t / 0.14f) : 1.0f;
        float ease = 1.0f - (1.0f - slam) * (1.0f - slam);
        float wipe = bp_clampf(ban_t / 0.30f, 0.0f, 1.0f);
        int fs = 58, w, y, j;
        float scale = bp_lerpf(2.1f, 1.0f, ease);
        int alpha = (int)(bp_clampf((1.0f - u) * 3.0f, 0.0f, 1.0f) * 255.0f);
        int bw;
        fs = (int)(fs * scale);
        w = MeasureText(ban_big, fs);
        y = sh / 2 - 90 - (int)((1.0f - ease) * 30.0f);
        bw = (int)(wipe * (float)sw);
        DrawRectangle(sw / 2 - bw / 2, y - 10, bw, fs + 20,
                      (Color){ 8, 12, 20, (unsigned char)(alpha * 0.62f) });
        DrawRectangle(sw / 2 - bw / 2, y - 12, bw, 2,
                      (Color){ ban_tint.r, ban_tint.g, ban_tint.b,
                               (unsigned char)(alpha * 0.85f) });
        DrawRectangle(sw / 2 - bw / 2, y + fs + 10, bw, 2,
                      (Color){ ban_tint.r, ban_tint.g, ban_tint.b,
                               (unsigned char)(alpha * 0.85f) });
        DrawText(ban_big, sw / 2 - w / 2 + 3, y + 4, fs,
                 (Color){ 0, 0, 0, (unsigned char)(alpha * 0.6f) });
        for (j = 0; j < 4; ++j) {
            static const int O[4][2] = { { 3,0 },{ -3,0 },{ 0,3 },{ 0,-3 } };
            DrawText(ban_big, sw / 2 - w / 2 + O[j][0], y + O[j][1], fs,
                     (Color){ ban_tint.r, ban_tint.g, ban_tint.b,
                              (unsigned char)(alpha * 0.22f) });
        }
        DrawText(ban_big, sw / 2 - w / 2, y, fs,
                 (Color){ 255, 250, 242, (unsigned char)alpha });
        if (ban_small[0]) {
            int w2 = MeasureText(ban_small, 22);
            DrawText(ban_small, sw / 2 - w2 / 2, y + fs + 20, 22,
                     (Color){ 226, 232, 240, (unsigned char)alpha });
        }
    }
    if (toast_t > 0.0f) {
        int w = MeasureText(toast_s, 19);
        int a = (int)(bp_clampf(toast_t, 0.0f, 1.0f) * 235.0f);
        DrawRectangle(sw / 2 - w / 2 - 14, sh - 96, w + 28, 32,
                      (Color){ 10, 14, 22, (unsigned char)(a * 0.8f) });
        DrawRectangleLines(sw / 2 - w / 2 - 14, sh - 96, w + 28, 32,
                           (Color){ 120, 200, 255, (unsigned char)a });
        DrawText(toast_s, sw / 2 - w / 2, sh - 89, 19,
                 (Color){ 210, 235, 255, (unsigned char)a });
    }
}
