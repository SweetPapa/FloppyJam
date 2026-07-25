/* render.c — every mesh, palette and pixel is generated. No asset files.
 *
 * Geometry is drawn immediate-mode: the whole course is a few thousand
 * triangles, so batching through rlgl is cheaper than managing meshes, and
 * tilt platforms / sweepers animate for free.
 */
#include "render.h"
#include "juice.h"
#include "scenery.h"
#include <string.h>
#include <stdio.h>

/* one global directional light, baked per-face — no shaders, no shadow maps */
static const V3 LIGHT = { -0.38f, 0.86f, -0.34f };

static Color shade(Color c, float k)
{
    int r = (int)(c.r * k), g = (int)(c.g * k), b = (int)(c.b * k);
    Color o;
    o.r = (unsigned char)bp_clampi(r, 0, 255);
    o.g = (unsigned char)bp_clampi(g, 0, 255);
    o.b = (unsigned char)bp_clampi(b, 0, 255);
    o.a = c.a;
    return o;
}

static float lambert(V3 n)
{
    float d = v3dot(v3norm(n), v3norm(LIGHT));
    /* Night grading: the ambient floor drops from 0.55 to 0.42 and the direct
     * term picks up the slack, so faces have more range against each other and
     * nothing on the table competes with a neon tube for brightness. */
    return 0.42f + 0.58f * bp_clampf(d, 0.0f, 1.0f);
}

static Vector3 rv(V3 v) { Vector3 o; o.x = v.x; o.y = v.y; o.z = v.z; return o; }

/* ------------------------------------------------------------------ */
/* palette: a hue walk across the eighteen (12.2)                      */

static Color hsv(float h, float s, float v)
{
    Vector3 c = ColorToHSV((Color){ 255, 0, 0, 255 });
    (void)c;
    return ColorFromHSV(h, s, v);
}

BpPalette bp_palette(int hole)
{
    BpPalette p;
    /* Night edition. The felt still walks green -> twilight blue across the
     * eighteen (12.2), but it now sits under a city sky, so values come down
     * and saturation goes up: a lit table in the dark, not a table in the sun.
     * The accent walks the neon wheel independently and lands on a different
     * tube colour every hole — it drives the flag, the cup beacon, the plaza
     * rings and the HUD trim, so one number re-themes the whole hole. */
    float u = bp_clampf((float)hole / (float)(BP_NHOLES - 1), 0.0f, 1.0f);
    float hue = bp_lerpf(140.0f, 214.0f, u * u * 0.85f + u * 0.15f);
    float sat = bp_lerpf(0.58f, 0.64f, u);
    float val = bp_lerpf(0.33f, 0.26f, u);
    /* neon wheel: mint -> cyan -> violet -> magenta -> amber, wrapped */
    float ah = fmodf(bp_lerpf(158.0f, 470.0f, u), 360.0f);
    p.felt    = hsv(hue, sat, val);
    p.felt2   = hsv(hue + 5.0f, sat * 0.92f, val * 0.86f);
    p.rim     = hsv(hue, sat * 0.6f, val * 1.5f);
    p.rail    = hsv(hue - 26.0f, sat * 0.70f, bp_lerpf(0.20f, 0.16f, u));
    p.railTop = hsv(hue - 26.0f, sat * 0.50f, bp_lerpf(0.34f, 0.27f, u));
    p.sky     = hsv(bp_lerpf(252.0f, 274.0f, u), bp_lerpf(0.62f, 0.66f, u),
                    bp_lerpf(0.13f, 0.10f, u));
    p.horizon = hsv(bp_lerpf(342.0f, 288.0f, u), 0.62f, bp_lerpf(0.34f, 0.26f, u));
    p.accent  = hsv(ah, 0.78f, 1.00f);
    p.ink     = (Color){ 8, 10, 18, 255 };
    return p;
}

/* Surface-zone colours are FIXED across every palette so a player can read
 * the floor at a glance from orbit distance (6.3). */
static Color surf_color(int surf, const BpPalette *pal, int alt)
{
    switch (surf) {
    case SURF_ROUGH: return alt ? (Color){ 38,  76,  46, 255 } : (Color){ 32,  66,  40, 255 };
    case SURF_ICE:   return alt ? (Color){ 176, 226, 244, 255 } : (Color){ 158, 212, 236, 255 };
    case SURF_SAND:  return alt ? (Color){ 214, 190, 128, 255 } : (Color){ 200, 176, 114, 255 };
    case SURF_KICK:  return alt ? (Color){ 226, 132,  46, 255 } : (Color){ 208, 116,  36, 255 };
    default:         return alt ? pal->felt : pal->felt2;
    }
}

static const Color BALL_COL[8] = {
    { 246, 200,  52, 255 },   /* 1 yellow */
    { 72,  126, 232, 255 },   /* 2 blue   */
    { 226,  74,  70, 255 },   /* 3 red    */
    { 156,  96, 216, 255 },   /* 4 purple */
    { 240, 140,  56, 255 },   /* 5 orange */
    { 60,  178, 118, 255 },   /* 6 green  */
    { 150,  60,  60, 255 },   /* 7 maroon */
    { 26,   28,  34, 255 },   /* 8 black  */
};

/* ------------------------------------------------------------------ */
/* ball orientation (cosmetic — spin must READ, 10.5)                  */

static float ballrot[BP_MAX_BALLS][9];
static float flagwave = 0.0f;

static void mat_identity(float *m)
{
    memset(m, 0, sizeof(float) * 9);
    m[0] = m[4] = m[8] = 1.0f;
}

static void mat_mul(const float *a, const float *b, float *out)
{
    int r, c, k;
    float t[9];
    for (r = 0; r < 3; ++r)
        for (c = 0; c < 3; ++c) {
            float s = 0.0f;
            for (k = 0; k < 3; ++k) s += a[r * 3 + k] * b[k * 3 + c];
            t[r * 3 + c] = s;
        }
    memcpy(out, t, sizeof(t));
}

static void mat_axis_angle(V3 ax, float ang, float *m)
{
    float c = cosf(ang), s = sinf(ang), t = 1.0f - c;
    float x = ax.x, y = ax.y, z = ax.z;
    m[0] = t*x*x + c;   m[1] = t*x*y - s*z; m[2] = t*x*z + s*y;
    m[3] = t*x*y + s*z; m[4] = t*y*y + c;   m[5] = t*y*z - s*x;
    m[6] = t*x*z - s*y; m[7] = t*y*z + s*x; m[8] = t*z*z + c;
}

static V3 mat_apply(const float *m, V3 v)
{
    return v3(m[0]*v.x + m[1]*v.y + m[2]*v.z,
              m[3]*v.x + m[4]*v.y + m[5]*v.z,
              m[6]*v.x + m[7]*v.y + m[8]*v.z);
}

void bp_render_init(void) { bp_render_hole_begin(); }

void bp_render_hole_begin(void)
{
    int i;
    for (i = 0; i < BP_MAX_BALLS; ++i) mat_identity(ballrot[i]);
    flagwave = 0.0f;
}

void bp_render_update(const BpWorld *w, float dt)
{
    int i;
    flagwave += dt;
    for (i = 0; i < w->nballs; ++i) {
        const BpBall *b = &w->balls[i];
        float ws = v3len(b->w);
        if (b->state == BS_GONE || ws < 1e-3f) continue;
        {
            float d[9];
            mat_axis_angle(v3mul(b->w, 1.0f / ws), ws * dt, d);
            mat_mul(d, ballrot[i], ballrot[i]);
        }
    }
}

/* ------------------------------------------------------------------ */
/* primitives                                                          */

static void quad3(V3 a, V3 b, V3 c, V3 d, Color col)
{
    DrawTriangle3D(rv(a), rv(b), rv(c), col);
    DrawTriangle3D(rv(a), rv(c), rv(d), col);
    DrawTriangle3D(rv(c), rv(b), rv(a), col);   /* double-sided: ledges read */
    DrawTriangle3D(rv(d), rv(c), rv(a), col);
}

/* yaw-oriented box with per-face flat shading */
static void box3(V3 c, V3 h, float yaw, Color col, Color top)
{
    float cs = cosf(yaw), sn = sinf(yaw);
    V3 ex = v3(cs * h.x, 0.0f, sn * h.x);
    V3 ez = v3(-sn * h.z, 0.0f, cs * h.z);
    V3 ey = v3(0.0f, h.y, 0.0f);
    V3 p[8];
    int i;
    for (i = 0; i < 8; ++i) {
        float sx = (i & 1) ? 1.0f : -1.0f;
        float sy = (i & 2) ? 1.0f : -1.0f;
        float sz = (i & 4) ? 1.0f : -1.0f;
        p[i] = v3add(c, v3add(v3add(v3mul(ex, sx), v3mul(ey, sy)), v3mul(ez, sz)));
    }
    /* +Y */
    quad3(p[2], p[3], p[7], p[6], shade(top, lambert(v3(0, 1, 0))));
    /* -Y */
    quad3(p[0], p[4], p[5], p[1], shade(col, 0.45f));
    /* +X / -X */
    quad3(p[1], p[5], p[7], p[3], shade(col, lambert(v3(cs, 0, sn))));
    quad3(p[0], p[2], p[6], p[4], shade(col, lambert(v3(-cs, 0, -sn))));
    /* +Z / -Z */
    quad3(p[4], p[6], p[7], p[5], shade(col, lambert(v3(-sn, 0, cs))));
    quad3(p[0], p[1], p[3], p[2], shade(col, lambert(v3(sn, 0, -cs))));
}

static void ring3(V3 c, float r0, float r1, Color col, int seg)
{
    int i;
    for (i = 0; i < seg; ++i) {
        float a0 = BP_TAU * (float)i / (float)seg;
        float a1 = BP_TAU * (float)(i + 1) / (float)seg;
        V3 q0 = v3(c.x + cosf(a0) * r0, c.y, c.z + sinf(a0) * r0);
        V3 q1 = v3(c.x + cosf(a1) * r0, c.y, c.z + sinf(a1) * r0);
        V3 q2 = v3(c.x + cosf(a1) * r1, c.y, c.z + sinf(a1) * r1);
        V3 q3 = v3(c.x + cosf(a0) * r1, c.y, c.z + sinf(a0) * r1);
        quad3(q0, q1, q2, q3, col);
    }
}

/* a hole in the floor: dark shaft plus a lit rim */
static void pit3(V3 c, float r, float depth, Color rim, Color inner)
{
    int i, seg = 16;
    ring3(v3(c.x, c.y + 0.002f, c.z), r, r * 1.34f, rim, seg);
    for (i = 0; i < seg; ++i) {
        float a0 = BP_TAU * (float)i / (float)seg;
        float a1 = BP_TAU * (float)(i + 1) / (float)seg;
        V3 t0 = v3(c.x + cosf(a0) * r, c.y, c.z + sinf(a0) * r);
        V3 t1 = v3(c.x + cosf(a1) * r, c.y, c.z + sinf(a1) * r);
        V3 b0 = v3(t0.x, c.y - depth, t0.z);
        V3 b1 = v3(t1.x, c.y - depth, t1.z);
        quad3(t0, t1, b1, b0, shade(inner, 0.35f + 0.25f * (float)i / (float)seg));
    }
    DrawCylinderEx(rv(v3(c.x, c.y - depth, c.z)), rv(v3(c.x, c.y - depth - 0.004f, c.z)),
                   r, r, seg, shade(inner, 0.2f));
}

/* ------------------------------------------------------------------ */
/* world                                                               */

static void draw_pads(const BpWorld *w, const BpPalette *pal)
{
    int i;
    for (i = 0; i < w->npads; ++i) {
        const BpPad *p = &w->pads[i];
        V3 n = bp_pad_normal_at(w, i);
        float lam = lambert(n);
        float wx = p->x1 - p->x0, wz = p->z1 - p->z0;
        int nx = bp_clampi((int)(wx / 0.75f + 0.5f), 1, 24);
        int nz = bp_clampi((int)(wz / 0.75f + 0.5f), 1, 32);
        int a, b;
        for (a = 0; a < nx; ++a) {
            for (b = 0; b < nz; ++b) {
                float x0 = p->x0 + wx * (float)a / (float)nx;
                float x1 = p->x0 + wx * (float)(a + 1) / (float)nx;
                float z0 = p->z0 + wz * (float)b / (float)nz;
                float z1 = p->z0 + wz * (float)(b + 1) / (float)nz;
                Color c = shade(surf_color(p->surf, pal, (a + b) & 1), lam);
                quad3(v3(x0, bp_pad_height(w, i, x0, z0), z0),
                      v3(x1, bp_pad_height(w, i, x1, z0), z0),
                      v3(x1, bp_pad_height(w, i, x1, z1), z1),
                      v3(x0, bp_pad_height(w, i, x0, z1), z1), c);
            }
        }
        /* zone outline so surface changes read from orbit distance */
        if (p->surf != SURF_FELT) {
            Color e = shade(surf_color(p->surf, pal, 1), 1.5f);
            float y0 = bp_pad_height(w, i, p->x0, p->z0) + 0.004f;
            float y1 = bp_pad_height(w, i, p->x1, p->z0) + 0.004f;
            float y2 = bp_pad_height(w, i, p->x1, p->z1) + 0.004f;
            float y3 = bp_pad_height(w, i, p->x0, p->z1) + 0.004f;
            DrawLine3D(rv(v3(p->x0, y0, p->z0)), rv(v3(p->x1, y1, p->z0)), e);
            DrawLine3D(rv(v3(p->x1, y1, p->z0)), rv(v3(p->x1, y2, p->z1)), e);
            DrawLine3D(rv(v3(p->x1, y2, p->z1)), rv(v3(p->x0, y3, p->z1)), e);
            DrawLine3D(rv(v3(p->x0, y3, p->z1)), rv(v3(p->x0, y0, p->z0)), e);
        }
        /* kicker arrow */
        if (p->surf == SURF_KICK) {
            float cx = 0.5f * (p->x0 + p->x1), cz = 0.5f * (p->z0 + p->z1);
            float y = bp_pad_height(w, i, cx, cz) + 0.008f;
            V3 d = v3(sinf(p->ka), 0.0f, cosf(p->ka));
            V3 s = v3(d.z, 0.0f, -d.x);
            float len = 0.32f;
            quad3(v3(cx + d.x * len, y, cz + d.z * len),
                  v3(cx + s.x * 0.16f - d.x * 0.05f, y, cz + s.z * 0.16f - d.z * 0.05f),
                  v3(cx - d.x * 0.05f, y, cz - d.z * 0.05f),
                  v3(cx - s.x * 0.16f - d.x * 0.05f, y, cz - s.z * 0.16f - d.z * 0.05f),
                  (Color){ 255, 236, 190, 255 });
        }
    }
}

/* A tube of light laid along the top of a rail. Two passes: a wide dim halo
 * and a narrow hot core, which is how you fake bloom without a shader. */
static void neon_cap(V3 c, V3 h, float yaw, Color k, float glow)
{
    float cs = cosf(yaw), sn = sinf(yaw);
    int pass;
    for (pass = 0; pass < 2; ++pass) {
        float inset = pass ? 0.55f : 1.30f;
        float lift  = pass ? 0.004f : 0.002f;
        float a     = (pass ? 0.95f : 0.26f) * glow;
        float hx = (h.x > h.z) ? h.x * 0.98f : h.x * inset;
        float hz = (h.x > h.z) ? h.z * inset : h.z * 0.98f;
        V3 ex = v3(cs * hx, 0.0f, sn * hx);
        V3 ez = v3(-sn * hz, 0.0f, cs * hz);
        V3 o  = v3(c.x, c.y + h.y + lift, c.z);
        Color col = k;
        col.a = (unsigned char)bp_clampi((int)(a * 255.0f), 0, 255);
        quad3(v3sub(v3sub(o, ex), ez), v3sub(v3add(o, ex), ez),
              v3add(v3add(o, ex), ez), v3add(v3sub(o, ex), ez), col);
    }
}

static void draw_boxes(const BpWorld *w, const BpPalette *pal, float t)
{
    int i;
    for (i = 0; i < w->nboxes; ++i) {
        const BpBox *b = &w->boxes[i];
        V3 c; float yaw;
        Color side = pal->rail, top = pal->railTop, tube = pal->accent;
        float glow = 0.55f + 0.12f * sinf(t * 1.6f + (float)i * 0.7f);
        if (!bp_box_visible(w, i)) continue;
        bp_box_pose(w, i, &c, &yaw);
        if (b->kind == BOX_BUMPER_WALL) {
            side = (Color){ 92, 22, 52, 255 }; top = (Color){ 150, 44, 88, 255 };
            tube = (Color){ 255, 110, 180, 255 };
            glow = 0.75f + 0.25f * sinf(t * 4.0f + (float)i);
        } else if (b->kind == BOX_GATE) {
            side = (Color){ 88, 24, 26, 255 };  top = (Color){ 148, 46, 44, 255 };
            tube = (Color){ 255, 96, 84, 255 };
            glow = 0.70f + 0.30f * sinf(t * 5.5f);
        } else if (b->kind == BOX_CAP) {
            side = (Color){ 44, 46, 56, 255 };  top = (Color){ 74, 78, 90, 255 };
            tube = (Color){ 150, 158, 176, 255 }; glow = 0.35f;
        } else if (b->mover) {
            side = (Color){ 92, 56, 18, 255 };  top = (Color){ 140, 92, 34, 255 };
            tube = (Color){ 255, 186, 84, 255 };
            glow = 0.80f + 0.20f * sinf(t * 3.0f + (float)i);
        }
        box3(c, v3(b->hx, b->hy, b->hz), yaw, side, top);
        neon_cap(c, v3(b->hx, b->hy, b->hz), yaw, tube, glow);
    }
}

static void draw_posts(const BpWorld *w, const BpPalette *pal, float t)
{
    int i;
    for (i = 0; i < w->nposts; ++i) {
        const BpPost *po = &w->posts[i];
        V3 c;
        Color col = pal->rail, cap = pal->railTop, ring = pal->accent;
        float pulse = 0.5f + 0.5f * sinf(t * 2.2f + (float)i * 0.9f);
        bp_post_pose(w, i, &c);
        if (po->kind == POST_BUMPER) {
            col = (Color){ 148, 34, 78, 255 };
            cap = (Color){ 255, 168, 206, 255 };
            ring = (Color){ 255, 120, 186, 255 };
            pulse = 0.55f + 0.45f * sinf(t * 3.4f + (float)i);
        } else if (po->kind == POST_TARGET) {
            col = po->lit ? (Color){ 40, 132, 96, 255 } : (Color){ 152, 122, 34, 255 };
            cap = po->lit ? (Color){ 150, 255, 200, 255 } : (Color){ 255, 240, 150, 255 };
            ring = cap;
            /* an unlit target begs to be hit: it strobes, a lit one settles */
            pulse = po->lit ? 0.55f : (0.35f + 0.65f * (0.5f + 0.5f * sinf(t * 6.0f)));
        } else if (po->mover) {
            col = (Color){ 118, 74, 24, 255 };
            cap = (Color){ 250, 190, 90, 255 };
            ring = cap;
        }
        DrawCylinderEx(rv(v3(c.x, c.y, c.z)), rv(v3(c.x, c.y + po->h, c.z)),
                       po->r, po->r * (po->kind == POST_BUMPER ? 1.06f : 1.0f), 14,
                       shade(col, 0.9f));
        if (po->kind == POST_BUMPER) {
            DrawSphere(rv(v3(c.x, c.y + po->h, c.z)), po->r * 1.02f, shade(cap, lambert(v3(0, 1, 0))));
        } else {
            DrawCylinderEx(rv(v3(c.x, c.y + po->h, c.z)), rv(v3(c.x, c.y + po->h + 0.02f, c.z)),
                           po->r * 1.1f, po->r * 1.1f, 14, cap);
        }
        /* a light band round the waist and a pool of it on the floor */
        {
            Color k = ring;
            k.a = (unsigned char)(90.0f + 130.0f * pulse);
            ring3(v3(c.x, c.y + po->h * 0.62f, c.z), po->r * 1.02f, po->r * 1.24f, k, 12);
            k.a = (unsigned char)(28.0f + 46.0f * pulse);
            ring3(v3(c.x, c.y + 0.004f, c.z), po->r * 1.1f, po->r * 2.6f, k, 14);
        }
    }
}

static void draw_pockets(const BpWorld *w, const BpPalette *pal, float t)
{
    int i;
    for (i = 0; i < w->npockets; ++i) {
        const BpPocket *pk = &w->pockets[i];
        V3 c = v3(pk->x, pk->y, pk->z);
        switch (pk->kind) {
        case PK_CUP:
            if (w->cup_sealed) {
                ring3(v3(c.x, c.y + 0.004f, c.z), BP_CUP_R * 0.2f, BP_CUP_R * 1.35f,
                      (Color){ 96, 100, 112, 255 }, 16);
                break;
            }
            pit3(c, BP_CUP_R, BP_CUP_DEPTH, (Color){ 236, 240, 248, 255 },
                 (Color){ 20, 24, 32, 255 });
            /* flagstick */
            DrawCylinderEx(rv(v3(c.x, c.y, c.z)), rv(v3(c.x, c.y + 0.52f, c.z)),
                           0.006f, 0.006f, 6, (Color){ 240, 244, 250, 255 });
            {   /* the flag streams downwind and flutters harder in a breeze */
                float ws = bp_wind_strength(), wa = bp_wind_angle();
                V3 wd = v3(sinf(wa), 0.0f, cosf(wa));
                V3 side = v3(wd.z, 0.0f, -wd.x);
                float flut = sinf(flagwave * (3.0f + 5.0f * ws)) * (0.015f + 0.045f * ws);
                float len = 0.11f + 0.15f * ws;
                V3 a = v3(c.x, c.y + 0.52f, c.z);
                V3 d = v3(c.x, c.y + 0.37f, c.z);
                V3 b = v3add(v3add(v3lerp(a, d, 0.45f), v3mul(wd, len)),
                             v3add(v3mul(side, flut), v3(0.0f, flut * 0.5f, 0.0f)));
                DrawTriangle3D(rv(a), rv(b), rv(d), pal->accent);
                DrawTriangle3D(rv(d), rv(b), rv(a), pal->accent);
            }
            break;
        case PK_SCRATCH:
            pit3(c, BP_POCKET_R, BP_CUP_DEPTH, (Color){ 178, 186, 200, 255 },
                 (Color){ 14, 16, 22, 255 });
            break;
        case PK_BONUS:
            pit3(c, BP_POCKET_R, BP_CUP_DEPTH, (Color){ 255, 208, 74, 255 },
                 (Color){ 32, 24, 8, 255 });
            ring3(v3(c.x, c.y + 0.006f, c.z), BP_POCKET_R * 1.36f,
                  BP_POCKET_R * (1.5f + 0.10f * sinf(t * 2.4f)),
                  (Color){ 255, 226, 130, 150 }, 16);
            break;
        case PK_WARP:
        default:
            pit3(c, BP_POCKET_R, BP_CUP_DEPTH, (Color){ 96, 232, 255, 255 },
                 (Color){ 6, 26, 40, 255 });
            ring3(v3(c.x, c.y + 0.006f, c.z),
                  BP_POCKET_R * (1.05f + 0.30f * (0.5f + 0.5f * sinf(t * 3.0f))),
                  BP_POCKET_R * (1.20f + 0.30f * (0.5f + 0.5f * sinf(t * 3.0f))),
                  (Color){ 120, 240, 255, 130 }, 16);
            break;
        }
    }
}

static void draw_ball(const BpWorld *w, int i, const BpPalette *pal)
{
    const BpBall *b = &w->balls[i];
    Color base;
    V3 p = b->p;
    float g;
    if (b->state == BS_GONE) return;

    base = (b->kind == BALL_CUE) ? (Color){ 246, 246, 240, 255 }
                                 : BALL_COL[b->color & 7];

    /* blob shadow (12.1) */
    g = bp_ground_h(w, p.x, p.z, p.y, NULL);
    if (g > -1e8f) {
        float lift = bp_clampf((p.y - BP_R - g) * 3.0f, 0.0f, 1.0f);
        float r = BP_R * bp_lerpf(1.15f, 1.9f, lift);
        unsigned char a = (unsigned char)(bp_lerpf(120.0f, 34.0f, lift));
        DrawCircle3D(rv(v3(p.x, g + 0.004f, p.z)), r, rv(v3(1, 0, 0)), 90.0f,
                     (Color){ 0, 0, 0, a });
    }

    DrawSphereEx(rv(p), BP_R, 10, 12, base);

    /* Two-tone marks so spin reads at a glance (10.5). */
    {
        const float *m = ballrot[i];
        V3 up = mat_apply(m, v3(0, 1, 0));
        V3 rt = mat_apply(m, v3(1, 0, 0));
        Color mark = (b->kind == BALL_CUE) ? (Color){ 220, 60, 60, 255 }
                                           : (Color){ 248, 248, 244, 255 };
        DrawSphereEx(rv(v3add(p, v3mul(up,  BP_R * 0.80f))), BP_R * 0.40f, 6, 8, mark);
        DrawSphereEx(rv(v3add(p, v3mul(up, -BP_R * 0.80f))), BP_R * 0.40f, 6, 8, mark);
        if (b->kind != BALL_CUE) {
            DrawSphereEx(rv(v3add(p, v3mul(rt,  BP_R * 0.86f))), BP_R * 0.26f, 6, 8,
                         (Color){ 250, 250, 250, 255 });
        }
        if (b->kind == BALL_EIGHT) {
            DrawSphereEx(rv(v3add(p, v3mul(rt, BP_R * 0.84f))), BP_R * 0.34f, 6, 8,
                         (Color){ 250, 250, 250, 255 });
        }
    }
    /* idle sheen */
    if (b->state == BS_REST) {
        DrawSphereEx(rv(v3add(p, v3(-0.010f, BP_R * 0.62f, -0.010f))), BP_R * 0.16f, 5, 6,
                     (Color){ 255, 255, 255, 150 });
    }

    /* Night rim light. Two translucent shells pick the ball out against dark
     * felt without washing out the two-tone spin marks underneath, and a pool
     * of the same colour on the floor keeps it readable from orbit height. */
    {
        Color k = (b->kind == BALL_CUE) ? (Color){ 210, 232, 255, 255 } : base;
        DrawSphereEx(rv(p), BP_R * 1.22f, 8, 10, (Color){ k.r, k.g, k.b, 26 });
        DrawSphereEx(rv(p), BP_R * 1.55f, 6, 8, (Color){ k.r, k.g, k.b, 12 });
        if (g > -1e8f && p.y - BP_R - g < 0.05f)
            ring3(v3(p.x, g + 0.003f, p.z), BP_R * 1.3f, BP_R * 3.4f,
                  (Color){ k.r, k.g, k.b, 22 }, 12);
    }
    (void)pal;
}

/* ------------------------------------------------------------------ */
/* cue + aim guide                                                     */

static void draw_cue(const BpWorld *w, const BpShot *sh)
{
    const BpBall *cb = &w->balls[0];
    V3 dir = v3(sinf(sh->aim), 0.0f, cosf(sh->aim));
    V3 up = v3(0, 1, 0);
    V3 right = v3cross(dir, up);
    /* the tip sits where the player dialled the english */
    V3 tip = v3add(cb->p, v3add(v3mul(right, sh->tx * BP_R), v3mul(up, sh->ty * BP_R)));
    float pull = 0.07f + sh->cue_pull * 0.34f + sh->strike_anim * -0.07f;
    V3 butt;
    tip = v3sub(tip, v3mul(dir, BP_R * 1.15f + pull));
    butt = v3sub(tip, v3mul(dir, 0.62f));
    DrawCylinderEx(rv(tip), rv(butt), 0.005f, 0.009f, 8, (Color){ 214, 176, 116, 230 });
    DrawCylinderEx(rv(tip), rv(v3sub(tip, v3mul(dir, 0.04f))), 0.0062f, 0.0062f, 8,
                   (Color){ 60, 180, 200, 255 });
    DrawCylinderEx(rv(v3sub(butt, v3mul(dir, -0.22f))), rv(butt), 0.0104f, 0.0116f, 8,
                   (Color){ 32, 36, 48, 255 });
}

static void draw_guide(const BpWorld *w, const BpShot *sh)
{
    const BpBall *cb = &w->balls[0];
    const BpGuideHit *g = &sh->guide;
    V3 dir = v3(sinf(sh->aim), 0.0f, cosf(sh->aim));
    float d = bp_clampf(g->dist, 0.0f, 30.0f);
    float step = 0.085f, t;
    Color dot = (Color){ 250, 250, 255, 210 };

    for (t = BP_R * 1.6f; t < d; t += step) {
        V3 p = v3add(cb->p, v3mul(dir, t));
        DrawCube(rv(v3(p.x, cb->p.y, p.z)), 0.016f, 0.016f, 0.016f, dot);
    }

    if (g->kind == 2) {
        /* ghost ball + the object ball's departure line (4.3) */
        V3 ghost = v3(g->point.x, cb->p.y, g->point.z);
        DrawSphereWires(rv(ghost), BP_R, 6, 8, (Color){ 255, 255, 255, 120 });
        for (t = 0.0f; t < 0.55f; t += step) {
            V3 p = v3add(w->balls[g->ball].p, v3mul(g->objdir, BP_R * 2.0f + t));
            DrawCube(rv(p), 0.014f, 0.014f, 0.014f, (Color){ 255, 214, 92, 220 });
        }
    } else if (g->kind == 1) {
        /* one short centre-ball rebound stub — deliberately spin-blind */
        float stub = BP_R * 3.0f;
        for (t = 0.0f; t < stub; t += step * 0.7f) {
            V3 p = v3add(v3(g->point.x, cb->p.y, g->point.z), v3mul(g->rebound, t));
            DrawCube(rv(p), 0.013f, 0.013f, 0.013f, (Color){ 150, 220, 255, 190 });
        }
    } else if (g->kind == 3) {
        V3 p = g->point;
        DrawCircle3D(rv(v3(p.x, p.y + 0.01f, p.z)), BP_POCKET_R * 1.5f, rv(v3(1, 0, 0)), 90.0f,
                     (Color){ 255, 230, 120, 220 });
    } else if (g->kind == 4) {
        V3 p = v3(g->point.x, cb->p.y, g->point.z);
        DrawCircle3D(rv(p), 0.06f, rv(v3(1, 0, 0)), 90.0f, (Color){ 255, 90, 90, 220 });
    }
}

void bp_render_world(const BpWorld *w, int hole, const BpShot *sh,
                     int show_guide, int show_cue, float t)
{
    BpPalette pal = bp_palette(hole);
    int i;
    draw_pads(w, &pal);
    draw_pockets(w, &pal, t);
    draw_boxes(w, &pal, t);
    draw_posts(w, &pal, t);
    bp_scenery_draw_near(w, &pal, t);
    for (i = 0; i < w->nballs; ++i) draw_ball(w, i, &pal);
    if (show_guide && sh) draw_guide(w, sh);
    if (show_cue && sh) draw_cue(w, sh);
    bp_juice_draw_world();
}

/* ------------------------------------------------------------------ */
/* HUD                                                                 */

const char *bp_score_name(int strokes, int par)
{
    int d = strokes - par;
    if (strokes == 1) return "ACE";
    if (strokes >= BP_STROKE_CAP) return "RACKED";
    if (d <= -3) return "ALBATROSS";
    if (d == -2) return "EAGLE";
    if (d == -1) return "BIRDIE";
    if (d ==  0) return "PAR";
    if (d ==  1) return "BOGEY";
    if (d ==  2) return "DOUBLE BOGEY";
    return "TRIPLE BOGEY";
}

const char *bp_score_flair(int strokes, int par)
{
    int d = strokes - par;
    if (strokes == 1) return "one shot, one hole. clean break.";
    if (strokes >= BP_STROKE_CAP) return "the table won that one.";
    if (d <= -2) return "you hustled the whole hole.";
    if (d == -1) return "position play. lovely.";
    if (d ==  0) return "textbook. next.";
    if (d ==  1) return "one loose stroke. it happens.";
    if (d ==  2) return "the felt is not your friend today.";
    return "let's call that a learning hole.";
}

static void draw_meter(int x, int y, int hgt, float p, int charging)
{
    int i, seg = 26;
    DrawRectangle(x - 4, y - 4, 26, hgt + 8, (Color){ 6, 8, 16, 200 });
    DrawRectangleLines(x - 4, y - 4, 26, hgt + 8, (Color){ 50, 60, 80, 220 });
    for (i = 0; i < seg; ++i) {
        float u = (float)i / (float)(seg - 1);
        int yy = y + hgt - (int)(u * hgt) - 4;
        Color c = (u < 0.55f) ? (Color){ 90, 235, 150, 255 }
                : (u < 0.82f) ? (Color){ 255, 210, 80, 255 }
                              : (Color){ 255, 90, 110, 255 };
        if (u > p) {
            DrawRectangle(x, yy, 18, 4, (Color){ 28, 32, 44, 255 });
            continue;
        }
        /* lit segments get a halo, so a full meter genuinely glows */
        DrawRectangle(x - 3, yy - 1, 24, 6, (Color){ c.r, c.g, c.b, 45 });
        DrawRectangle(x, yy, 18, 4, c);
    }
    if (charging) {
        int yy = y + hgt - (int)(p * hgt) - 5;
        DrawRectangle(x - 10, yy - 1, 38, 4, (Color){ 255, 255, 255, 70 });
        DrawRectangle(x - 8, yy, 34, 2, (Color){ 255, 255, 255, 240 });
    }
    DrawText("PWR", x - 3, y + hgt + 8, 12, (Color){ 150, 165, 185, 255 });
}

static const char *power_word(float p)
{
    if (p < 0.20f) return "TAP";
    if (p < 0.40f) return "SOFT";
    if (p < 0.62f) return "FIRM";
    if (p < 0.82f) return "HARD";
    return "SMASH";
}

/* How hard did I just hit that? Shown right where the meter was, so the
 * number and the word land in the same place your eye already is. */
static void draw_strike_readout(int x, int y, float flash, float power)
{
    char buf[32];
    int a, fs, w;
    float pop;
    if (flash <= 0.001f) return;
    a = (int)(bp_clampf(flash * 1.6f, 0.0f, 1.0f) * 255.0f);
    pop = 1.0f + (1.0f - bp_clampf(flash * 3.0f, 0.0f, 1.0f)) * 0.0f +
          bp_clampf((flash - 0.75f) * 4.0f, 0.0f, 1.0f) * 0.45f;
    fs = (int)(26.0f * pop);
    /* left-aligned: "SMASH" at the pop scale is wider than the meter column
     * and would run off the edge of the screen if it were centred on it */
    snprintf(buf, sizeof buf, "%s", power_word(power));
    DrawText(buf, x, y - fs, fs,
             (Color){ 255, (unsigned char)bp_lerpf(240.0f, 150.0f, power),
                      (unsigned char)bp_lerpf(210.0f, 110.0f, power), (unsigned char)a });
    snprintf(buf, sizeof buf, "%d%%", (int)(power * 100.0f + 0.5f));
    DrawText(buf, x, y + 2, 16, (Color){ 226, 234, 250, (unsigned char)a });
    (void)w;
}

/* the cue-ball face widget: this is where english lives (4.4) */
static void draw_spin_widget(int cx, int cy, int r, float tx, float ty)
{
    int dx = (int)(tx * (float)r);
    int dy = (int)(-ty * (float)r);
    Color tint = (Color){ 235, 235, 240, 255 };
    if (ty >  0.15f) tint = (Color){ 255, 206, 150, 255 };   /* follow = warm  */
    if (ty < -0.15f) tint = (Color){ 160, 200, 255, 255 };   /* draw   = cool  */
    if (fabsf(tx) > 0.35f && fabsf(ty) < 0.3f) tint = (Color){ 170, 245, 190, 255 };

    DrawCircle(cx, cy, (float)r + 6.0f, (Color){ 10, 14, 22, 200 });
    DrawCircle(cx, cy, (float)r, (Color){ 236, 236, 232, 255 });
    DrawCircleLines(cx, cy, (float)r, (Color){ 80, 92, 110, 255 });
    DrawCircleLines(cx, cy, (float)r * BP_TIP_MAX, (Color){ 150, 160, 180, 160 });
    DrawLine(cx - r, cy, cx + r, cy, (Color){ 190, 196, 210, 90 });
    DrawLine(cx, cy - r, cx, cy + r, (Color){ 190, 196, 210, 90 });
    DrawCircle(cx + dx, cy + dy, 6.0f, tint);
    DrawCircleLines(cx + dx, cy + dy, 6.0f, (Color){ 20, 24, 32, 255 });
    DrawText("ENGLISH", cx - 26, cy + r + 10, 12, (Color){ 150, 165, 185, 255 });
}

static void draw_minimap(const BpWorld *w, int x, int y, int size, Color accent)
{
    V3 lo, hi;
    float sx, sz, sc;
    int i;
    bp_course_bounds(w, &lo, &hi);
    sx = hi.x - lo.x; sz = hi.z - lo.z;
    sc = (float)size / ((sx > sz ? sx : sz) + 0.6f);

    DrawRectangle(x - 6, y - 6, size + 12, size + 12, (Color){ 5, 7, 15, 195 });
    for (i = 0; i < w->npads; ++i) {
        const BpPad *p = &w->pads[i];
        int px = x + (int)((p->x0 - lo.x) * sc);
        int py = y + (int)((p->z0 - lo.z) * sc);
        int pw = (int)((p->x1 - p->x0) * sc);
        int ph = (int)((p->z1 - p->z0) * sc);
        Color c = (Color){ 46, 96, 62, 220 };
        if (p->surf == SURF_ROUGH) c = (Color){ 30, 62, 38, 220 };
        if (p->surf == SURF_ICE)   c = (Color){ 120, 175, 200, 220 };
        if (p->surf == SURF_SAND)  c = (Color){ 172, 152, 100, 220 };
        if (p->surf == SURF_KICK)  c = (Color){ 200, 110, 40, 220 };
        DrawRectangle(px, py, pw < 1 ? 1 : pw, ph < 1 ? 1 : ph, c);
    }
    for (i = 0; i < w->npockets; ++i) {
        const BpPocket *pk = &w->pockets[i];
        int px = x + (int)((pk->x - lo.x) * sc);
        int py = y + (int)((pk->z - lo.z) * sc);
        Color c = (Color){ 200, 200, 210, 255 };
        if (pk->kind == PK_CUP)   c = w->cup_sealed ? (Color){ 120, 124, 134, 255 }
                                                    : (Color){ 255, 255, 255, 255 };
        if (pk->kind == PK_BONUS) c = (Color){ 255, 208, 74, 255 };
        if (pk->kind == PK_WARP)  c = (Color){ 96, 232, 255, 255 };
        if (pk->kind == PK_SCRATCH) c = (Color){ 235, 96, 96, 255 };
        DrawCircle(px, py, pk->kind == PK_CUP ? 4.0f : 3.0f, c);
    }
    for (i = 0; i < w->nballs; ++i) {
        const BpBall *b = &w->balls[i];
        int px, py;
        if (b->state == BS_GONE) continue;
        px = x + (int)((b->p.x - lo.x) * sc);
        py = y + (int)((b->p.z - lo.z) * sc);
        DrawCircle(px, py, i == 0 ? 3.5f : 2.5f,
                   i == 0 ? (Color){ 255, 255, 255, 255 } : BALL_COL[b->color & 7]);
    }
    DrawRectangleLines(x - 6, y - 6, size + 12, size + 12, (Color){ 50, 60, 80, 220 });
    DrawRectangle(x - 6, y + size + 4, size + 12, 2, accent);
}

/* Every HUD box is the same object: smoked glass with one lit edge. Doing it
 * once keeps the overlay looking like one machine rather than six widgets. */
static void hud_panel(int x, int y, int w, int hgt, Color k, int edge)
{
    DrawRectangle(x, y, w, hgt, (Color){ 6, 8, 16, 180 });
    DrawRectangleLines(x, y, w, hgt, (Color){ 40, 48, 66, 190 });
    if (edge & 1) DrawRectangle(x, y + hgt - 2, w, 2, k);            /* bottom */
    if (edge & 2) DrawRectangle(x, y, 2, hgt, k);                    /* left   */
    if (edge & 4) DrawRectangle(x + w - 2, y, 2, hgt, k);            /* right  */
    if (edge & 8) DrawRectangle(x, y, w, 2, k);                      /* top    */
    /* the bloom the tube throws onto the glass */
    DrawRectangleGradientV(x, y + hgt - 12, w, 12,
                           (Color){ k.r, k.g, k.b, 0 }, (Color){ k.r, k.g, k.b, 44 });
}

void bp_render_hud(const BpWorld *w, const BpHud *h, int sw, int sh)
{
    BpPalette pal = bp_palette(h->hole);
    char buf[128];
    int i;

    /* top-left: hole card */
    hud_panel(0, 0, 352, 80, pal.accent, 1);
    snprintf(buf, sizeof buf, "HOLE %d", h->hole + 1);
    DrawText(buf, 14, 10, 22, (Color){ 236, 242, 252, 255 });
    DrawText(h->name, 112, 12, 20,
             (Color){ pal.accent.r, pal.accent.g, pal.accent.b, 255 });
    snprintf(buf, sizeof buf, "PAR %d", h->par);
    DrawText(buf, 14, 40, 18, (Color){ 150, 200, 160, 255 });
    snprintf(buf, sizeof buf, "STROKES %d", h->strokes);
    DrawText(buf, 96, 40, 18, (Color){ 240, 226, 160, 255 });

    /* top-right: running total against par, coloured by which side of it */
    {
        int d = h->running - h->running_par;
        Color c = (d < 0) ? (Color){ 130, 240, 170, 255 }
                : (d == 0) ? (Color){ 236, 242, 252, 255 }
                           : (Color){ 250, 172, 130, 255 };
        snprintf(buf, sizeof buf, "%+d", d);
        hud_panel(sw - 128, 0, 128, 56, c, 1);
        DrawText("TOTAL", sw - 114, 8, 13, (Color){ 130, 145, 165, 255 });
        DrawText(buf, sw - 114, 24, 24, c);
    }

    /* brief line */
    if (h->brief) {
        int bw = MeasureText(h->brief, 16);
        hud_panel(sw / 2 - bw / 2 - 14, 8, bw + 28, 30, pal.accent, 1);
        DrawText(h->brief, sw / 2 - bw / 2, 15, 16, (Color){ 200, 214, 234, 255 });
    }

    if (h->sealed) {
        const char *s = "CUP SEALED — POT THE 8";
        int bw = MeasureText(s, 20);
        DrawRectangle(sw / 2 - bw / 2 - 12, 44, bw + 24, 30, (Color){ 40, 10, 14, 190 });
        DrawText(s, sw / 2 - bw / 2, 50, 20, (Color){ 255, 180, 170, 255 });
    }

    draw_meter(26, sh - 230, 150, h->charging ? h->power : 0.0f, h->charging);
    draw_strike_readout(16, sh - 252, h->pow_flash, h->pow_value);
    draw_spin_widget(120, sh - 118, 44, h->tx, h->ty);
    draw_minimap(w, sw - 178, sh - 178, 160, pal.accent);

    /* aim detent: the compass ticks over as you swing the cue */
    {
        int cx = 252, cy = sh - 104, i;
        float base = h->aim_tick;
        DrawText("AIM", cx - 14, cy - 30, 12, (Color){ 150, 165, 185, 255 });
        for (i = -6; i <= 6; ++i) {
            int tx2 = cx + i * 11;
            int hgt = (i % 3 == 0) ? 11 : 6;
            unsigned char al = (unsigned char)(i == 0 ? 255 : 110);
            DrawRectangle(tx2, cy - hgt / 2, 2, hgt,
                          (Color){ 190, 205, 228, al });
        }
        if (base > 0.01f) {
            unsigned char al = (unsigned char)(bp_clampf(base, 0.0f, 1.0f) * 255.0f);
            DrawRectangle(cx - 1, cy - 9, 4, 18, (Color){ 255, 226, 130, al });
        }
    }

    /* controls hint strip */
    if (!h->riding) {
        const char *k = "LEFT/RIGHT aim   SHIFT fine   SPACE power   A/D/W/S english   "
                        "UP focus hole   DOWN aim at hole   MOUSE/QE cam   TAB card   ESC pause";
        DrawText(k, 14, sh - 26, 15, (Color){ 128, 146, 170, 205 });
    } else {
        DrawText("R  skip to rest", 14, sh - 26, 15, (Color){ 128, 146, 170, 205 });
    }

    /* strokes as pips */
    for (i = 0; i < BP_STROKE_CAP; ++i) {
        Color c = (i < h->strokes) ? (Color){ 240, 210, 120, 255 }
                                   : (Color){ 46, 54, 68, 255 };
        DrawRectangle(14 + i * 14, 66, 10, 6, c);
    }
}

/* ------------------------------------------------------------------ */
/* screens                                                             */

/* One panel style for every full-screen page: smoked glass, a hairline, and
 * a lit strip along the top so the pages belong to the same sign shop as the
 * HUD and the title. */
void bp_render_panel(int x, int y, int w, int h)
{
    Color k = (Color){ 255, 206, 120, 255 };
    DrawRectangle(x + 4, y + 5, w, h, (Color){ 0, 0, 0, 120 });
    DrawRectangle(x, y, w, h, (Color){ 8, 10, 20, 232 });
    DrawRectangleLines(x, y, w, h, (Color){ 60, 72, 96, 255 });
    DrawRectangle(x, y, w, 2, k);
    DrawRectangleGradientV(x, y + 2, w, 14, (Color){ k.r, k.g, k.b, 40 },
                           (Color){ k.r, k.g, k.b, 0 });
}

void bp_render_scorecard(const BpHud *h, int sw, int sh, int final_page)
{
    int x = sw / 2 - 330, y = 64, i;
    int tot = 0, par = 0;
    char buf[96];
    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 170 });
    bp_render_panel(x, y, 660, 446);
    DrawText(final_page ? "FINAL CARD" : "SCORECARD", x + 22, y + 16, 30,
             (Color){ 246, 240, 210, 255 });

    for (i = 0; i < BP_NHOLES; ++i) {
        int col = (i < 9) ? 0 : 1;
        int row = i % 9;
        int cx = x + 22 + col * 330;
        int cy = y + 62 + row * 34;
        int played = (i >= h->from && i <= h->to && h->scores[i] > 0);
        Color c = played ? (Color){ 236, 242, 252, 255 } : (Color){ 96, 108, 128, 255 };
        snprintf(buf, sizeof buf, "%2d  %-16s", i + 1, BP_HOLES[i].name);
        DrawText(buf, cx, cy, 17, c);
        snprintf(buf, sizeof buf, "%d", BP_HOLES[i].par);
        DrawText(buf, cx + 218, cy, 17, (Color){ 130, 180, 145, 255 });
        if (played) {
            int d = h->scores[i] - BP_HOLES[i].par;
            Color sc = (d < 0) ? (Color){ 130, 230, 160, 255 }
                     : (d == 0) ? (Color){ 236, 242, 252, 255 }
                                : (Color){ 240, 170, 130, 255 };
            snprintf(buf, sizeof buf, "%d", h->scores[i]);
            DrawText(buf, cx + 252, cy, 17, sc);
            tot += h->scores[i];
            par += BP_HOLES[i].par;
        } else if (i >= h->from && i <= h->to) {
            DrawText("-", cx + 252, cy, 17, (Color){ 96, 108, 128, 255 });
        }
        if (h->bests[i] > 0) {
            snprintf(buf, sizeof buf, "b%d", h->bests[i]);
            DrawText(buf, cx + 282, cy + 2, 13, (Color){ 120, 140, 175, 255 });
        }
    }

    snprintf(buf, sizeof buf, "TOTAL %d    PAR %d    %+d", tot, par, tot - par);
    DrawText(buf, x + 22, y + 374, 24, (Color){ 246, 240, 210, 255 });
    if (h->restarts > 0) {
        snprintf(buf, sizeof buf, "restarts used: %d", h->restarts);
        DrawText(buf, x + 420, y + 380, 16, (Color){ 190, 160, 120, 255 });
    }
    DrawText(final_page ? "ENTER  back to title" : "TAB / ENTER  continue",
             x + 22, y + 414, 15, (Color){ 130, 148, 175, 255 });
}

/* Text with a soft coloured bloom behind it: four offset copies at low alpha
 * fake the halo a real neon tube throws onto the wall. */
static void neon_text(const char *s, int x, int y, int fs, Color core, Color glow,
                      float amount)
{
    static const int OFF[8][2] = { { 2,0 },{ -2,0 },{ 0,2 },{ 0,-2 },
                                   { 3,3 },{ -3,3 },{ 3,-3 },{ -3,-3 } };
    int i;
    Color g = glow;
    DrawText(s, x + 3, y + 4, fs, (Color){ 0, 0, 0, 130 });
    for (i = 0; i < 8; ++i) {
        g.a = (unsigned char)(bp_clampf(amount, 0.0f, 1.0f) * (i < 4 ? 46.0f : 26.0f));
        DrawText(s, x + OFF[i][0], y + OFF[i][1], fs, g);
    }
    DrawText(s, x, y, fs, core);
}

/* A marquee: a run of bulbs chasing round a rectangle. Pure showbiz. */
static void marquee(int x, int y, int w, int h, float t, Color c)
{
    int i, n = (w + h) / 22;
    if (n < 8) n = 8;
    for (i = 0; i < n; ++i) {
        float u = (float)i / (float)n * 4.0f;
        int side = (int)u;
        float f = u - (float)side;
        int px = 0, py = 0;
        float on = 0.5f + 0.5f * sinf(t * 5.0f - (float)i * 0.8f);
        Color k = c;
        switch (side) {
        case 0: px = x + (int)(f * w);     py = y;         break;
        case 1: px = x + w;                py = y + (int)(f * h); break;
        case 2: px = x + w - (int)(f * w); py = y + h;     break;
        default: px = x;                   py = y + h - (int)(f * h); break;
        }
        k.a = (unsigned char)(60.0f + 100.0f * on);
        DrawCircle(px, py, 6.0f, k);
        k.a = (unsigned char)(150.0f + 105.0f * on);
        DrawCircle(px, py, 2.5f, k);
    }
}

void bp_render_title(int sw, int sh, int sel, float t, const BpHud *h)
{
    static const char *ITEMS[4] = { "PLAY", "OPTIONS", "HOW TO PLAY", "QUIT" };
    BpPalette pal = bp_palette(0);
    int i;
    char buf[96];

    /* the live city is behind us; knock it back just enough that type reads */
    DrawRectangle(0, 0, sw, sh, (Color){ 6, 6, 18, 70 });
    DrawRectangleGradientV(0, (int)(sh * 0.08f), sw, (int)(sh * 0.24f),
                           (Color){ 8, 6, 24, 0 }, (Color){ 8, 6, 24, 130 });
    DrawRectangleGradientV(0, (int)(sh * 0.32f), sw, (int)(sh * 0.22f),
                           (Color){ 8, 6, 24, 130 }, (Color){ 8, 6, 24, 0 });

    {
        int w1 = MeasureText("BREAK", 96), w2 = MeasureText("PAR", 96);
        int total = w1 + w2 + 26;
        int x = sw / 2 - total / 2;
        int y = (int)(sh * 0.15f) + (int)(sinf(t * 1.4f) * 3.0f);
        /* the sign has a dodgy tube: BREAK stutters once every few seconds */
        float flick = (sinf(t * 1.7f) > 0.985f || sinf(t * 11.0f) > 0.995f) ? 0.25f : 1.0f;
        marquee(x - 34, y - 22, total + 68, 130, t, (Color){ 255, 206, 120, 255 });
        neon_text("BREAK", x, y, 96,
                  (Color){ 244, 246, 252, (unsigned char)(255.0f * flick) },
                  (Color){ 120, 220, 255, 255 }, flick);
        neon_text("PAR", x + w1 + 26, y, 96, (Color){ 255, 226, 150, 255 },
                  (Color){ 255, 110, 170, 255 }, 1.0f);
        (void)w2;
    }
    {
        const char *tag = "mini golf with a cue stick";
        int w = MeasureText(tag, 22);
        DrawText(tag, sw / 2 - w / 2, (int)(sh * 0.15f) + 116, 22,
                 (Color){ 188, 206, 232, 235 });
        DrawRectangle(sw / 2 - w / 2 - 30, (int)(sh * 0.15f) + 128, w + 60, 2,
                      (Color){ pal.accent.r, pal.accent.g, pal.accent.b, 150 });
    }
    for (i = 0; i < 4; ++i) {
        int w = MeasureText(ITEMS[i], 30);
        int y = (int)(sh * 0.54f) + i * 48;
        if (i == sel) {
            float p = 0.5f + 0.5f * sinf(t * 4.5f);
            DrawRectangle(sw / 2 - w / 2 - 30, y - 8, w + 60, 44,
                          (Color){ pal.accent.r, pal.accent.g, pal.accent.b,
                                   (unsigned char)(34 + 26 * p) });
            DrawRectangle(sw / 2 - w / 2 - 30, y - 8, 3, 44, pal.accent);
            DrawRectangle(sw / 2 + w / 2 + 27, y - 8, 3, 44, pal.accent);
            neon_text(ITEMS[i], sw / 2 - w / 2, y, 30, (Color){ 255, 244, 220, 255 },
                      pal.accent, 0.8f + 0.2f * p);
        } else {
            DrawText(ITEMS[i], sw / 2 - w / 2, y, 30, (Color){ 168, 184, 208, 235 });
        }
    }
    snprintf(buf, sizeof buf, "holes in one: %d", h->aces);
    DrawText(buf, 16, sh - 28, 16, (Color){ 120, 138, 168, 235 });
    DrawText("v1.0  original code, art and sound", sw - 300, sh - 28, 16,
             (Color){ 120, 138, 168, 235 });
}

void bp_render_select(int sw, int sh, int sel, float t)
{
    static const char *ITEMS[3] = { "FRONT NINE", "BACK NINE", "FULL EIGHTEEN" };
    static const char *SUB[3] = { "holes 1-9, par 24", "holes 10-18, par 30",
                                  "the whole card, par 54" };
    BpPalette pal = bp_palette(0);
    int i;
    DrawRectangle(0, 0, sw, sh, (Color){ 6, 6, 18, 175 });
    {
        const char *s = "CHOOSE YOUR ROUND";
        int w = MeasureText(s, 40);
        neon_text(s, sw / 2 - w / 2, (int)(sh * 0.16f), 40,
                  (Color){ 244, 246, 252, 255 }, pal.accent, 0.9f);
    }
    for (i = 0; i < 3; ++i) {
        int w = MeasureText(ITEMS[i], 34);
        int y = (int)(sh * 0.36f) + i * 88;
        if (i == sel) {
            float p = 0.5f + 0.5f * sinf(t * 4.5f);
            DrawRectangle(sw / 2 - 250, y - 12, 500, 70,
                          (Color){ pal.accent.r, pal.accent.g, pal.accent.b,
                                   (unsigned char)(30 + 24 * p) });
            marquee(sw / 2 - 250, y - 12, 500, 70, t, pal.accent);
            neon_text(ITEMS[i], sw / 2 - w / 2, y, 34, (Color){ 255, 244, 220, 255 },
                      pal.accent, 0.9f);
        } else {
            DrawText(ITEMS[i], sw / 2 - w / 2, y, 34, (Color){ 168, 184, 208, 235 });
        }
        {
            int w2 = MeasureText(SUB[i], 17);
            DrawText(SUB[i], sw / 2 - w2 / 2, y + 40, 17, (Color){ 140, 160, 190, 235 });
        }
    }
    DrawText("ESC  back", 16, sh - 30, 17, (Color){ 120, 138, 168, 235 });
}

void bp_render_pause(int sw, int sh, int sel, const BpHud *h)
{
    static const char *ITEMS[4] = { "RESUME", "RESTART HOLE", "OPTIONS", "QUIT TO TITLE" };
    int i, x = sw / 2 - 190, y = sh / 2 - 160;
    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 180 });
    bp_render_panel(x, y, 380, 320);
    DrawText("PAUSED", x + 24, y + 20, 34, (Color){ 246, 240, 210, 255 });
    {
        char buf[64];
        snprintf(buf, sizeof buf, "hole %d, %d stroke%s", h->hole + 1, h->strokes,
                 h->strokes == 1 ? "" : "s");
        DrawText(buf, x + 24, y + 60, 17, (Color){ 140, 160, 190, 255 });
    }
    for (i = 0; i < 4; ++i) {
        int yy = y + 108 + i * 44;
        if (i == sel) DrawRectangle(x + 14, yy - 8, 352, 38, (Color){ 255, 206, 84, 34 });
        DrawText(ITEMS[i], x + 32, yy, 24,
                 i == sel ? (Color){ 255, 236, 180, 255 } : (Color){ 176, 192, 214, 255 });
    }
    DrawText("restarting resets this hole's strokes to zero", x + 24, y + 288, 13,
             (Color){ 120, 138, 165, 255 });
}

void bp_render_options(int sw, int sh, int sel, int vm, int vmu, int vs, int fs,
                       int preview, int invert)
{
    static const char *ITEMS[7] = { "MASTER", "MUSIC", "SFX", "FULLSCREEN",
                                    "TRAJECTORY", "INVERT AIM", "BACK" };
    int vals[3];
    int i, x = sw / 2 - 220, y = sh / 2 - 210;
    vals[0] = vm; vals[1] = vmu; vals[2] = vs;
    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 180 });
    bp_render_panel(x, y, 440, 438);
    DrawText("OPTIONS", x + 24, y + 20, 34, (Color){ 246, 240, 210, 255 });
    for (i = 0; i < 7; ++i) {
        int yy = y + 76 + i * 44;
        Color c = (i == sel) ? (Color){ 255, 236, 180, 255 } : (Color){ 176, 192, 214, 255 };
        if (i == sel) DrawRectangle(x + 14, yy - 6, 412, 36, (Color){ 255, 206, 84, 34 });
        DrawText(ITEMS[i], x + 32, yy, 22, c);
        if (i < 3) {
            int j;
            for (j = 0; j < 10; ++j)
                DrawRectangle(x + 200 + j * 20, yy + 4, 14, 14,
                              j < vals[i] ? (Color){ 120, 210, 160, 255 }
                                          : (Color){ 44, 52, 66, 255 });
        } else if (i == 3) {
            DrawText(fs ? "ON" : "OFF", x + 200, yy, 22,
                     fs ? (Color){ 120, 210, 160, 255 } : (Color){ 130, 146, 172, 255 });
        } else if (i == 4) {
            DrawText(preview ? "ON" : "OFF", x + 200, yy, 22,
                     preview ? (Color){ 120, 210, 160, 255 }
                             : (Color){ 130, 146, 172, 255 });
        } else if (i == 5) {
            DrawText(invert ? "ON" : "OFF", x + 200, yy, 22,
                     invert ? (Color){ 120, 210, 160, 255 }
                            : (Color){ 130, 146, 172, 255 });
        }
    }
    DrawText("INVERT AIM swaps which arrow turns the cue left.",
             x + 24, y + 388, 13, (Color){ 120, 138, 165, 255 });
    DrawText("LEFT / RIGHT to change        P toggles preview in play",
             x + 24, y + 408, 14, (Color){ 120, 138, 165, 255 });
}

/* ------------------------------------------------------------------ */
/* 10.5 — a faint trail tinted by the english that is still on the ball.
 * Subtle by design: it is a readability aid, not a light show. */
void bp_render_trail(const BpWorld *w, float tx, float ty, float dt)
{
    static float acc = 0.0f;
    const BpBall *b = &w->balls[0];
    float sp;
    Color c;
    if (b->state == BS_GONE || b->state == BS_REST) { acc = 0.0f; return; }
    sp = v3len(b->v);
    if (sp < 0.45f) return;
    acc += dt;
    if (acc < 0.028f) return;
    acc = 0.0f;
    if (ty < -0.15f)                      c = (Color){ 130, 180, 255, 255 };  /* draw   */
    else if (ty > 0.15f)                  c = (Color){ 255, 190, 130, 255 };  /* follow */
    else if (fabsf(tx) > 0.20f)           c = (Color){ 140, 240, 180, 255 };  /* side   */
    else                                  c = (Color){ 225, 232, 244, 255 };  /* stun   */
    bp_burst(b->p, 1, c, 0.05f, 0.34f, 0.4f);
}

/* ------------------------------------------------------------------ */
/* trajectory preview (optional assist)                                */

/* Drawn as a fading ribbon rather than a flat line: bright and warm where
 * the ball is now, cooling as it runs out, so the direction of travel and
 * roughly how long the shot lasts both read at a glance. Contact points get
 * a marker; the resting place gets a ring. */
void bp_render_preview(const BpWorld *w, const BpPreview *pv, float t)
{
    int i;
    if (!pv || !pv->valid || pv->n < 2) return;

    /* object-ball paths first, underneath, and dimmer */
    for (i = 0; i < pv->nobj; ++i) {
        Color base = BALL_COL[pv->obj[i].color & 7];
        int j;
        for (j = 1; j < pv->obj[i].n; ++j) {
            V3 a = pv->obj[i].pt[j - 1], b = pv->obj[i].pt[j];
            Color c = base;
            c.a = 130;
            DrawLine3D(rv(v3(a.x, a.y + 0.010f, a.z)),
                       rv(v3(b.x, b.y + 0.010f, b.z)), c);
        }
        if (pv->obj[i].n > 1) {
            V3 e = pv->obj[i].pt[pv->obj[i].n - 1];
            DrawCircle3D(rv(v3(e.x, e.y + 0.012f, e.z)), BP_R * 1.5f,
                         rv(v3(1, 0, 0)), 90.0f, base);
        }
    }

    for (i = 1; i < pv->n; ++i) {
        float u = (float)i / (float)(pv->n - 1);
        V3 a = pv->pt[i - 1], b = pv->pt[i];
        int mark = pv->hit[i] & BP_PV_MARK;
        Color c;
        c.r = (unsigned char)bp_lerpf(255.0f, 90.0f, u);
        c.g = (unsigned char)bp_lerpf(238.0f, 170.0f, u);
        c.b = (unsigned char)bp_lerpf(150.0f, 255.0f, u);
        c.a = (unsigned char)bp_lerpf(235.0f, 90.0f, u);
        /* a break point is where the ball warped: draw a ring at the exit but
         * no line across the gap, so the preview never streaks over the hole */
        if (pv->hit[i] & BP_PV_BREAK) {
            DrawCircle3D(rv(v3(b.x, b.y + 0.014f, b.z)), BP_R * 1.7f,
                         rv(v3(1, 0, 0)), 90.0f, (Color){ 120, 240, 255, 220 });
            continue;
        }
        DrawLine3D(rv(v3(a.x, a.y + 0.013f, a.z)),
                   rv(v3(b.x, b.y + 0.013f, b.z)), c);
        /* a second, offset strand makes the line readable against the felt
         * without needing a thick-line shader */
        DrawLine3D(rv(v3(a.x, a.y + 0.019f, a.z)),
                   rv(v3(b.x, b.y + 0.019f, b.z)), (Color){ c.r, c.g, c.b, (unsigned char)(c.a / 2) });

        if (mark) {
            Color m = (mark == 2) ? (Color){ 255, 150, 200, 235 }
                    : (mark == 3) ? (Color){ 255, 214,  92, 235 }
                                  : (Color){ 190, 226, 255, 235 };
            DrawCube(rv(v3(b.x, b.y + 0.016f, b.z)), 0.030f, 0.030f, 0.030f, m);
        }
    }

    /* where it comes to rest, and what happens there */
    {
        V3 e = pv->pt[pv->n - 1];
        float pulse = 1.0f + 0.16f * sinf(t * 6.0f);
        Color ring = pv->holed     ? (Color){ 120, 255, 170, 255 }
                   : pv->scratched ? (Color){ 255,  90,  90, 255 }
                                   : (Color){ 210, 232, 255, 235 };
        DrawCircle3D(rv(v3(e.x, e.y + 0.014f, e.z)), BP_R * 2.0f * pulse,
                     rv(v3(1, 0, 0)), 90.0f, ring);
        DrawCircle3D(rv(v3(e.x, e.y + 0.014f, e.z)), BP_R * 2.9f * pulse,
                     rv(v3(1, 0, 0)), 90.0f,
                     (Color){ ring.r, ring.g, ring.b, 110 });
    }
    (void)w;
}

/* One line of plain English about what the preview says will happen. */
void bp_render_preview_readout(const BpPreview *pv, int sw, int sh, int live)
{
    char buf[96];
    const char *verdict;
    Color c;
    int wdt;
    if (!pv || !pv->valid) return;

    if (pv->holed)          { verdict = "IN THE CUP"; c = (Color){ 120, 255, 170, 255 }; }
    else if (pv->scratched) { verdict = "SCRATCH";    c = (Color){ 255, 110, 110, 255 }; }
    else {
        snprintf(buf, sizeof buf, "%.1f m short", pv->rest_dist);
        verdict = buf;
        c = (Color){ 214, 230, 250, 255 };
    }
    {
        char line[128];
        snprintf(line, sizeof line, "PREVIEW  %3d%%  %s%s",
                 (int)(pv->power * 100.0f + 0.5f), verdict,
                 pv->bonus ? "  +GOLD" : "");
        wdt = MeasureText(line, 18);
        DrawRectangle(sw / 2 - wdt / 2 - 12, sh - 66, wdt + 24, 28,
                      (Color){ 8, 12, 20, 190 });
        DrawRectangleLines(sw / 2 - wdt / 2 - 12, sh - 66, wdt + 24, 28,
                           live ? (Color){ 255, 226, 130, 200 }
                                : (Color){ 90, 110, 140, 200 });
        DrawText(line, sw / 2 - wdt / 2, sh - 60, 18, c);
    }
}
