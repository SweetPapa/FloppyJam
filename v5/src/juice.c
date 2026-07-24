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
    par_head = 0;
    hitstop_t = slowmo_t = flash_a = 0.0f;
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
    if (toast_t > 0.0f)   toast_t -= dt;
    if (ban_hold > 0.0f) { ban_t += dt; if (ban_t > ban_hold) ban_hold = 0.0f; }

    for (i = 0; i < BP_MAX_PARTICLES; ++i) {
        Particle *p = &par[i];
        if (p->life <= 0.0f) continue;
        p->life -= dt;
        if (p->kind == 2) continue;
        p->v.y -= p->grav * dt;
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
        } else {
            DrawCubeV((Vector3){ p->p.x, p->p.y, p->p.z },
                      (Vector3){ p->size, p->size, p->size }, c);
        }
    }
}

void bp_juice_draw_hud(int sw, int sh)
{
    if (flash_a > 0.001f) {
        Color c = flash_c;
        c.a = (unsigned char)(flash_a * 150.0f);
        DrawRectangle(0, 0, sw, sh, c);
    }
    if (ban_hold > 0.0f) {
        /* slam in, hold, fall away */
        float u = ban_t / ban_hold;
        float slam = (ban_t < 0.14f) ? (ban_t / 0.14f) : 1.0f;
        float ease = 1.0f - (1.0f - slam) * (1.0f - slam);
        int fs = 58, w, y;
        float scale = bp_lerpf(2.1f, 1.0f, ease);
        int alpha = (int)(bp_clampf((1.0f - u) * 3.0f, 0.0f, 1.0f) * 255.0f);
        fs = (int)(fs * scale);
        w = MeasureText(ban_big, fs);
        y = sh / 2 - 90 - (int)((1.0f - ease) * 30.0f);
        DrawRectangle(0, y - 10, sw, fs + 20, (Color){ 8, 12, 20, (unsigned char)(alpha * 0.55f) });
        DrawText(ban_big, sw / 2 - w / 2 + 3, y + 3, fs, (Color){ 0, 0, 0, (unsigned char)(alpha * 0.6f) });
        DrawText(ban_big, sw / 2 - w / 2, y, fs,
                 (Color){ ban_tint.r, ban_tint.g, ban_tint.b, (unsigned char)alpha });
        if (ban_small[0]) {
            int w2 = MeasureText(ban_small, 22);
            DrawText(ban_small, sw / 2 - w2 / 2, y + fs + 8, 22,
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
