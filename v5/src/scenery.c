/* scenery.c — the neon city the table sits in.
 *
 * Seeded entirely off the hole index: same hole, same skyline, every time.
 * Drawn immediate-mode like the rest of the renderer, with three distance
 * rings of buildings so the parallax reads as you orbit, and a light-emitting
 * pass (windows, signs, the wheel, the beacon) that ignores the directional
 * light because at two in the morning the city IS the light.
 *
 * Budget note: the whole backdrop is roughly five thousand triangles with the
 * far rings on strips instead of window grids, which keeps the integrated-GPU
 * floor in Section 13 intact.
 */
#include "scenery.h"
#include "course.h"
#include "juice.h"      /* the breeze: read for sway, never written here */
#include <string.h>

/* ------------------------------------------------------------------ */
/* deterministic hash — cosmetic only, never touched by the sim (5.1)  */

static unsigned int hsh(unsigned int x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
static float hf(unsigned int x) { return (float)(hsh(x) & 0xffffffu) / 16777215.0f; }
static float hs(unsigned int x) { return hf(x) * 2.0f - 1.0f; }

/* ------------------------------------------------------------------ */
/* colour helpers                                                      */

static Color cmul(Color c, float k)
{
    Color o;
    o.r = (unsigned char)bp_clampi((int)(c.r * k), 0, 255);
    o.g = (unsigned char)bp_clampi((int)(c.g * k), 0, 255);
    o.b = (unsigned char)bp_clampi((int)(c.b * k), 0, 255);
    o.a = c.a;
    return o;
}
static Color clerp(Color a, Color b, float t)
{
    Color o;
    o.r = (unsigned char)bp_lerpf(a.r, b.r, t);
    o.g = (unsigned char)bp_lerpf(a.g, b.g, t);
    o.b = (unsigned char)bp_lerpf(a.b, b.b, t);
    o.a = (unsigned char)bp_lerpf(a.a, b.a, t);
    return o;
}
static Color calpha(Color c, float a)
{
    c.a = (unsigned char)bp_clampi((int)(a * 255.0f), 0, 255);
    return c;
}

static Vector3 rv(V3 v) { Vector3 o; o.x = v.x; o.y = v.y; o.z = v.z; return o; }

/* Double-sided quad. The city is seen from inside its own ring and from
 * above, so paying two extra triangles beats reasoning about winding. */
static void quad(V3 a, V3 b, V3 c, V3 d, Color col)
{
    DrawTriangle3D(rv(a), rv(b), rv(c), col);
    DrawTriangle3D(rv(a), rv(c), rv(d), col);
    DrawTriangle3D(rv(c), rv(b), rv(a), col);
    DrawTriangle3D(rv(d), rv(c), rv(a), col);
}

/* Filled disc in the plane spanned by u and v. */
static void disc(V3 c, V3 u, V3 v, float r, Color col, int seg)
{
    int i;
    for (i = 0; i < seg; ++i) {
        float a0 = BP_TAU * (float)i / (float)seg;
        float a1 = BP_TAU * (float)(i + 1) / (float)seg;
        V3 p0 = v3add(c, v3add(v3mul(u, cosf(a0) * r), v3mul(v, sinf(a0) * r)));
        V3 p1 = v3add(c, v3add(v3mul(u, cosf(a1) * r), v3mul(v, sinf(a1) * r)));
        DrawTriangle3D(rv(c), rv(p0), rv(p1), col);
        DrawTriangle3D(rv(p1), rv(p0), rv(c), col);
    }
}

/* Flat annulus on the plaza — the neon floor rings. */
static void annulus(V3 c, float r0, float r1, Color col, int seg)
{
    int i;
    for (i = 0; i < seg; ++i) {
        float a0 = BP_TAU * (float)i / (float)seg;
        float a1 = BP_TAU * (float)(i + 1) / (float)seg;
        quad(v3(c.x + cosf(a0) * r0, c.y, c.z + sinf(a0) * r0),
             v3(c.x + cosf(a1) * r0, c.y, c.z + sinf(a1) * r0),
             v3(c.x + cosf(a1) * r1, c.y, c.z + sinf(a1) * r1),
             v3(c.x + cosf(a0) * r1, c.y, c.z + sinf(a0) * r1), col);
    }
}

/* ------------------------------------------------------------------ */
/* tables                                                              */

#define CITY_MAX   56
#define STAR_MAX   200
#define CLOUD_MAX  12
#define BEAM_MAX    3

typedef struct {
    float x, z;            /* footprint centre                            */
    float hw, hd;          /* half width / half depth                     */
    float h;               /* height above the plaza                      */
    float yaw;
    float dist;            /* to the course centre, for haze + LOD        */
    Color body, win, neon;
    unsigned char ring;    /* 0 near, 1 mid, 2 far                        */
    unsigned char rows, cols;
    unsigned char sign;    /* 1 = wears a neon sign band                  */
    unsigned char ant;     /* 1 = antenna with a blinking aircraft light  */
    unsigned int  seed;
} Bldg;

typedef struct { V3 p; float sz, ph, rate; Color c; } Star;
typedef struct { float x, y, z, len, wid, drift; Color c; } Cloud;
typedef struct { float x, z, ph, rate; Color c; } Beam;

static struct {
    int   built;
    int   hole;
    float cx, cz;          /* course centre                                */
    float py;              /* plaza plane                                  */
    float ext;             /* course half-extent                           */
    float downtown;        /* azimuth of the bright part of the skyline     */

    Bldg  b[CITY_MAX];   int nb;
    Star  s[STAR_MAX];   int ns;
    Cloud cl[CLOUD_MAX]; int ncl;
    Beam  bm[BEAM_MAX];  int nbm;

    /* funfair */
    float wheel_x, wheel_z, wheel_y, wheel_r, wheel_a;
    Color wheel_c;
    float blimp_a, blimp_r, blimp_y;
    float t;               /* local animation clock                        */
} S;

/* The neon set the whole city signs in — six hues, all high-value so they
 * survive the haze lerp toward the horizon. */
static const Color NEON[6] = {
    { 255,  92, 168, 255 },   /* hot pink   */
    { 104, 236, 255, 255 },   /* cyan       */
    { 255, 196,  84, 255 },   /* amber      */
    { 168, 132, 255, 255 },   /* violet     */
    {  96, 255, 176, 255 },   /* mint       */
    { 255, 128,  96, 255 },   /* coral      */
};

/* Window glow: a warm office set plus the odd cold fluorescent floor. */
static const Color WINCOL[4] = {
    { 255, 214, 138, 255 },
    { 255, 236, 190, 255 },
    { 176, 224, 255, 255 },
    { 255, 178, 120, 255 },
};

/* ------------------------------------------------------------------ */

void bp_scenery_build(const BpWorld *w, int hole)
{
    V3 lo, hi;
    unsigned int sd = hsh(0xB4EA4u + (unsigned int)hole * 2654435761u);
    float ex, ez;
    int i, ring, n;

    memset(&S, 0, sizeof(S));
    S.hole = hole;
    bp_course_bounds(w, &lo, &hi);
    S.cx = 0.5f * (lo.x + hi.x);
    S.cz = 0.5f * (lo.z + hi.z);
    ex = 0.5f * (hi.x - lo.x);
    ez = 0.5f * (hi.z - lo.z);
    S.ext = (ex > ez ? ex : ez) + 1.5f;
    S.py = lo.y - 0.55f;
    S.downtown = hf(sd + 11u) * BP_TAU;

    /* ---- skyline: three rings, each further out and taller ---------- */
    {
        static const int   RN[3]   = { 16, 20, 18 };
        static const float R0[3]   = { 15.0f, 30.0f, 56.0f };
        static const float R1[3]   = { 25.0f, 48.0f, 88.0f };
        static const float H0[3]   = {  4.0f, 10.0f, 22.0f };
        static const float H1[3]   = { 15.0f, 32.0f, 62.0f };
        int k = 0;
        for (ring = 0; ring < 3; ++ring) {
            for (n = 0; n < RN[ring] && k < CITY_MAX; ++n, ++k) {
                unsigned int q = sd + (unsigned int)(ring * 97 + n) * 7919u;
                float a  = BP_TAU * ((float)n + 0.35f * hs(q + 1u)) / (float)RN[ring];
                float r  = S.ext + bp_lerpf(R0[ring], R1[ring], hf(q + 2u));
                /* the downtown side gets the tall stuff */
                float face = 0.5f + 0.5f * cosf(a - S.downtown);
                float hgt  = bp_lerpf(H0[ring], H1[ring], hf(q + 3u) * 0.55f + face * 0.45f);
                Bldg *b = &S.b[k];
                b->x = S.cx + cosf(a) * r;
                b->z = S.cz + sinf(a) * r;
                b->yaw = a + hs(q + 4u) * 0.35f;
                b->hw = bp_lerpf(1.6f, 4.2f, hf(q + 5u)) * (1.0f + 0.35f * (float)ring);
                b->hd = bp_lerpf(1.6f, 4.2f, hf(q + 6u)) * (1.0f + 0.35f * (float)ring);
                b->h  = hgt;
                b->dist = r;
                b->ring = (unsigned char)ring;
                b->seed = q;
                b->win  = WINCOL[hsh(q + 7u) & 3];
                b->neon = NEON[hsh(q + 8u) % 6u];
                /* concrete at night is barely a colour: a cool near-black that
                 * the haze lerp pushes toward the horizon glow with distance */
                b->body = (Color){ (unsigned char)(16 + (int)(hf(q + 9u) * 14.0f)),
                                   (unsigned char)(16 + (int)(hf(q + 10u) * 12.0f)),
                                   (unsigned char)(30 + (int)(hf(q + 11u) * 18.0f)), 255 };
                b->rows = (unsigned char)bp_clampi((int)(b->h / 1.5f), 2, 9);
                b->cols = (unsigned char)bp_clampi((int)(b->hw * 1.4f), 2, 5);
                b->sign = (unsigned char)(hf(q + 12u) < (ring == 0 ? 0.55f : 0.25f));
                b->ant  = (unsigned char)(b->h > 18.0f && hf(q + 13u) < 0.6f);
            }
        }
        S.nb = k;
    }

    /* ---- stars: a dome of them, brighter away from downtown ---------- */
    S.ns = STAR_MAX;
    for (i = 0; i < S.ns; ++i) {
        unsigned int q = sd + (unsigned int)i * 40503u;
        float az = hf(q + 1u) * BP_TAU;
        float el = 0.10f + hf(q + 2u) * hf(q + 20u) * 1.36f;   /* bias upward */
        float cr, dim;
        if (el > 1.45f) el = 1.45f;
        cr = cosf(el);
        S.s[i].p = v3(S.cx + sinf(az) * cr * 220.0f,
                      S.py + sinf(el) * 220.0f,
                      S.cz + cosf(az) * cr * 220.0f);
        /* city glow washes out anything low over downtown */
        dim = bp_clampf(0.35f + 0.65f * sinf(el) +
                        0.25f * (1.0f - cosf(az - S.downtown)), 0.15f, 1.0f);
        S.s[i].sz = (0.55f + 1.5f * hf(q + 3u) * hf(q + 4u)) * dim;
        S.s[i].ph = hf(q + 5u) * BP_TAU;
        S.s[i].rate = 0.7f + 2.6f * hf(q + 6u);
        S.s[i].c = clerp((Color){ 220, 232, 255, 255 },
                         (Color){ 255, 226, 190, 255 }, hf(q + 7u));
    }

    /* ---- smog banks, lit from underneath by the city ----------------- */
    S.ncl = CLOUD_MAX;
    for (i = 0; i < S.ncl; ++i) {
        unsigned int q = sd + (unsigned int)i * 2246822519u;
        float az = hf(q + 1u) * BP_TAU;
        float r  = S.ext + 40.0f + hf(q + 2u) * 120.0f;
        S.cl[i].x = S.cx + cosf(az) * r;
        S.cl[i].z = S.cz + sinf(az) * r;
        S.cl[i].y = S.py + 34.0f + hf(q + 3u) * 42.0f;
        S.cl[i].len = 22.0f + hf(q + 4u) * 46.0f;
        S.cl[i].wid = 9.0f + hf(q + 5u) * 16.0f;
        S.cl[i].drift = 0.3f + hf(q + 6u) * 0.8f;
        S.cl[i].c = clerp((Color){ 54, 34, 74, 90 }, (Color){ 96, 46, 78, 110 },
                          hf(q + 7u));
    }

    /* ---- searchlights raking the sky over the fairground ------------- */
    S.nbm = BEAM_MAX;
    for (i = 0; i < S.nbm; ++i) {
        unsigned int q = sd + (unsigned int)i * 374761393u;
        float az = hf(q + 1u) * BP_TAU;
        float r  = S.ext + 26.0f + hf(q + 2u) * 40.0f;
        S.bm[i].x = S.cx + cosf(az) * r;
        S.bm[i].z = S.cz + sinf(az) * r;
        S.bm[i].ph = hf(q + 3u) * BP_TAU;
        S.bm[i].rate = 0.22f + hf(q + 4u) * 0.30f;
        S.bm[i].c = NEON[hsh(q + 5u) % 6u];
    }

    /* ---- the wheel: the one thing that says funfair at a glance ------ */
    {
        float az = S.downtown + 2.1f + hs(sd + 31u) * 0.5f;
        S.wheel_r = 8.0f + hf(sd + 32u) * 4.0f;
        S.wheel_x = S.cx + cosf(az) * (S.ext + 30.0f + hf(sd + 33u) * 16.0f);
        S.wheel_z = S.cz + sinf(az) * (S.ext + 30.0f + hf(sd + 33u) * 16.0f);
        S.wheel_y = S.py + S.wheel_r + 2.5f;
        S.wheel_c = NEON[hsh(sd + 34u) % 6u];
    }
    S.blimp_r = S.ext + 34.0f;
    S.blimp_y = S.py + 26.0f + hf(sd + 41u) * 10.0f;
    S.blimp_a = hf(sd + 42u) * BP_TAU;
    S.built = 1;
}

void bp_scenery_update(float dt)
{
    if (!S.built) return;
    S.t += dt;
    S.wheel_a += dt * 0.28f;
    S.blimp_a += dt * 0.035f;
}

/* ------------------------------------------------------------------ */
/* sky dome                                                            */

/* Six bands from just under the horizon to the zenith. The lowest two carry
 * the sodium-and-neon bounce that makes a city sky orange instead of black. */
static void draw_sky(const BpPalette *pal)
{
    static const float ELEV[7] = { -0.22f, 0.02f, 0.10f, 0.22f, 0.42f, 0.70f, 1.05f };
    static const Color BAND[7] = {
        {  36,  14,  30, 255 },
        {  84,  32,  50, 255 },
        {  70,  32,  74, 255 },
        {  42,  26,  76, 255 },
        {  22,  20,  58, 255 },
        {  12,  14,  38, 255 },
        {   7,   9,  24, 255 },
    };
    const int SEG = 20;
    const float R = 240.0f;
    int i, j;
    for (j = 0; j < 6; ++j) {
        float e0 = ELEV[j], e1 = ELEV[j + 1];
        float y0 = S.py + sinf(e0) * R, y1 = S.py + sinf(e1) * R;
        float r0 = cosf(e0) * R,        r1 = cosf(e1) * R;
        for (i = 0; i < SEG; ++i) {
            float a0 = BP_TAU * (float)i / (float)SEG;
            float a1 = BP_TAU * (float)(i + 1) / (float)SEG;
            /* warm the horizon toward downtown, cool it away from it */
            float g0 = 0.5f + 0.5f * cosf(a0 - S.downtown);
            float lo = 1.0f - (float)j / 6.0f;
            Color c0 = clerp(BAND[j], BAND[j + 1], 0.5f);
            Color c1 = clerp(c0, pal->accent, 0.10f * g0 * lo * lo);
            c1 = clerp(c1, (Color){ 190, 88, 60, 255 }, 0.30f * g0 * lo * lo * lo);
            quad(v3(S.cx + cosf(a0) * r0, y0, S.cz + sinf(a0) * r0),
                 v3(S.cx + cosf(a1) * r0, y0, S.cz + sinf(a1) * r0),
                 v3(S.cx + cosf(a1) * r1, y1, S.cz + sinf(a1) * r1),
                 v3(S.cx + cosf(a0) * r1, y1, S.cz + sinf(a0) * r1), c1);
        }
    }
}

static void draw_stars(float t)
{
    int i;
    for (i = 0; i < S.ns; ++i) {
        const Star *s = &S.s[i];
        float tw = 0.62f + 0.38f * sinf(t * s->rate + s->ph);
        float sz = s->sz * (0.75f + 0.45f * tw);
        Color c = calpha(s->c, 0.30f + 0.70f * tw);
        DrawCubeV(rv(s->p), (Vector3){ sz, sz, sz }, c);
    }
    /* the moon, with three halo discs so it blooms into the smog */
    {
        float az = S.downtown + 2.6f, el = 0.62f;
        V3 p = v3(S.cx + cosf(az) * cosf(el) * 210.0f,
                  S.py + sinf(el) * 210.0f,
                  S.cz + sinf(az) * cosf(el) * 210.0f);
        V3 u = v3norm(v3(-sinf(az), 0.0f, cosf(az)));
        V3 n = v3norm(v3sub(v3(S.cx, S.py, S.cz), p));
        V3 v = v3norm(v3cross(n, u));
        int k;
        for (k = 3; k >= 1; --k) {
            V3 q = v3add(p, v3mul(n, (float)k * 0.6f));
            disc(q, u, v, 7.0f + (float)k * 5.5f,
                 (Color){ 255, 226, 176, (unsigned char)(26 - k * 6) }, 18);
        }
        disc(v3add(p, v3mul(n, 3.0f)), u, v, 6.4f,
             (Color){ 255, 242, 214, 255 }, 22);
        /* one crater smudge so it is not a flat coin */
        disc(v3add(v3add(p, v3mul(n, 3.4f)), v3add(v3mul(u, 1.8f), v3mul(v, 1.2f))),
             u, v, 2.1f, (Color){ 232, 216, 196, 255 }, 12);
    }
}

static void draw_clouds(float t)
{
    int i;
    for (i = 0; i < S.ncl; ++i) {
        const Cloud *c = &S.cl[i];
        float dx = cosf(t * 0.012f * c->drift + (float)i) * 14.0f;
        float dz = sinf(t * 0.012f * c->drift + (float)i) * 14.0f;
        V3 a = v3(c->x + dx - c->len, c->y, c->z + dz - c->wid);
        V3 b = v3(c->x + dx + c->len, c->y, c->z + dz - c->wid);
        V3 d = v3(c->x + dx + c->len, c->y, c->z + dz + c->wid);
        V3 e = v3(c->x + dx - c->len, c->y, c->z + dz + c->wid);
        quad(a, b, d, e, c->c);
        /* underside catches the sodium bounce */
        quad(v3(a.x, a.y - 2.2f, a.z), v3(b.x, b.y - 2.2f, b.z),
             v3(d.x, d.y - 2.2f, d.z), v3(e.x, e.y - 2.2f, e.z),
             (Color){ 120, 52, 60, 46 });
    }
}

/* ------------------------------------------------------------------ */
/* plaza                                                               */

/* Ring roads with something driving on them. Forty light dots moving at
 * walking pace is the cheapest "this city is awake" signal there is, and it
 * gives the eye something to track while a shot is being lined up. */
static void draw_traffic(float t)
{
    int i;
    for (i = 0; i < 42; ++i) {
        unsigned int q = 0x7a1c3u + (unsigned int)i * 26699u;
        int lane = i % 3;
        float dirn = (lane & 1) ? -1.0f : 1.0f;
        float r = S.ext + 13.0f + (float)lane * 9.5f + hs(q) * 0.9f;
        float a = hf(q + 1u) * BP_TAU + t * (0.045f + hf(q + 2u) * 0.035f) * dirn;
        float x = S.cx + cosf(a) * r, z = S.cz + sinf(a) * r;
        Color k = (dirn > 0.0f) ? (Color){ 255, 240, 206, 255 }
                                : (Color){ 255, 88, 70, 255 };
        DrawCubeV((Vector3){ x, S.py + 0.09f, z },
                  (Vector3){ 0.30f, 0.14f, 0.30f }, calpha(k, 0.95f));
        DrawCubeV((Vector3){ x, S.py + 0.09f, z },
                  (Vector3){ 1.10f, 0.40f, 1.10f }, calpha(k, 0.085f));
    }
}

static void draw_plaza(const BpPalette *pal, float t)
{
    V3 c = v3(S.cx, S.py, S.cz);
    int i;
    /* wet asphalt, out to where the sky dome lands */
    disc(c, v3(1, 0, 0), v3(0, 0, 1), 240.0f, (Color){ 9, 10, 20, 255 }, 24);
    disc(v3(S.cx, S.py + 0.008f, S.cz), v3(1, 0, 0), v3(0, 0, 1),
         S.ext + 40.0f, (Color){ 15, 15, 30, 255 }, 24);

    /* Polar paving right around the table. The camera spends most of the
     * round looking down at this, so it gets a readable grid rather than the
     * flat black it used to be: arcs every 1.6 m and sixteen radial seams. */
    for (i = 0; i < 7; ++i) {
        float r = S.ext + 1.2f + (float)i * 1.6f;
        float fade = 1.0f - (float)i / 8.0f;
        annulus(v3(S.cx, S.py + 0.012f, S.cz), r, r + 0.06f,
                calpha((Color){ 132, 168, 226, 255 }, 0.10f * fade + 0.03f), 28);
    }
    for (i = 0; i < 16; ++i) {
        float a = BP_TAU * (float)i / 16.0f;
        float ca = cosf(a), sa = sinf(a);
        float px = -sa * 0.05f, pz = ca * 0.05f;
        float r0 = S.ext + 1.2f, r1 = S.ext + 34.0f;
        Color k = calpha((Color){ 120, 150, 210, 255 }, 0.075f);
        quad(v3(S.cx + ca * r0 + px, S.py + 0.014f, S.cz + sa * r0 + pz),
             v3(S.cx + ca * r1 + px, S.py + 0.014f, S.cz + sa * r1 + pz),
             v3(S.cx + ca * r1 - px, S.py + 0.014f, S.cz + sa * r1 - pz),
             v3(S.cx + ca * r0 - px, S.py + 0.014f, S.cz + sa * r0 - pz), k);
    }
    /* the three ring roads themselves, one shade off the paving */
    for (i = 0; i < 3; ++i) {
        float r = S.ext + 13.0f + (float)i * 9.5f;
        annulus(v3(S.cx, S.py + 0.010f, S.cz), r - 1.1f, r + 1.1f,
                (Color){ 21, 21, 36, 255 }, 30);
        annulus(v3(S.cx, S.py + 0.013f, S.cz), r + 1.1f, r + 1.22f,
                calpha((Color){ 150, 180, 240, 255 }, 0.13f), 30);
    }

    /* neon floor rings pulsing outward from the table */
    for (i = 0; i < 3; ++i) {
        float ph = t * 0.5f - (float)i * 0.33f;
        float glow = 0.45f + 0.55f * (0.5f + 0.5f * sinf(ph * BP_TAU));
        float r = S.ext + 3.4f + (float)i * 5.5f;
        Color k = (i == 1) ? pal->accent : NEON[(S.hole + i) % 6];
        annulus(v3(S.cx, S.py + 0.018f, S.cz), r, r + 0.30f,
                calpha(k, 0.22f + 0.58f * glow), 40);
        annulus(v3(S.cx, S.py + 0.016f, S.cz), r - 0.55f, r + 0.85f,
                calpha(k, 0.05f + 0.10f * glow), 40);
    }
    draw_traffic(t);
}

/* Wet-street light streaks: one quad per building, smeared toward the
 * viewer's side of the plaza. Sells "it rained an hour ago" for 56 quads. */
static void draw_reflections(void)
{
    int i;
    for (i = 0; i < S.nb; ++i) {
        const Bldg *b = &S.b[i];
        float dx = S.cx - b->x, dz = S.cz - b->z;
        float l = sqrtf(dx * dx + dz * dz);
        float wdt, len;
        if (l < 1e-3f) continue;
        dx /= l; dz /= l;
        wdt = b->hw * 0.75f;
        len = bp_clampf(b->h * 0.55f, 2.0f, 16.0f);
        quad(v3(b->x - dz * wdt, S.py + 0.03f, b->z + dx * wdt),
             v3(b->x + dz * wdt, S.py + 0.03f, b->z - dx * wdt),
             v3(b->x + dz * wdt * 0.35f + dx * len, S.py + 0.03f,
                b->z - dx * wdt * 0.35f + dz * len),
             v3(b->x - dz * wdt * 0.35f + dx * len, S.py + 0.03f,
                b->z + dx * wdt * 0.35f + dz * len),
             calpha(b->win, 0.055f));
    }
}

/* ------------------------------------------------------------------ */
/* skyline                                                             */

static void draw_windows(const Bldg *b, V3 fwd, V3 side, Color haze, float t)
{
    float bot = S.py + 0.6f, top = S.py + b->h - 0.5f;
    int r, c;
    if (top <= bot) return;

    if (b->ring == 0) {
        /* near ring: a real grid, because you can count the floors */
        float wq = (b->h - 1.1f) / (float)b->rows;
        for (r = 0; r < b->rows; ++r) {
            for (c = 0; c < b->cols; ++c) {
                unsigned int q = b->seed + (unsigned int)(r * 31 + c) * 6151u;
                float on = hf(q);
                float y0, y1, u0, u1;
                Color col;
                if (on > 0.72f) continue;                 /* dark apartment */
                /* one in eight is on a timer and blinks over the round */
                if (on < 0.09f && sinf(t * (0.5f + hf(q + 1u)) + hf(q + 2u) * 6.0f) < 0.0f)
                    continue;
                y0 = bot + (float)r * wq + wq * 0.22f;
                y1 = bot + (float)r * wq + wq * 0.72f;
                u0 = -b->hw * 0.78f + (2.0f * b->hw * 0.78f) * ((float)c + 0.16f) / (float)b->cols;
                u1 = -b->hw * 0.78f + (2.0f * b->hw * 0.78f) * ((float)c + 0.84f) / (float)b->cols;
                col = clerp(b->win, haze, bp_clampf(b->dist / 190.0f, 0.0f, 0.55f));
                col = cmul(col, 0.65f + 0.55f * hf(q + 3u));
                quad(v3add(v3add(v3mul(fwd, 0.02f), v3mul(side, u0)),
                           v3(b->x, y0, b->z)),
                     v3add(v3add(v3mul(fwd, 0.02f), v3mul(side, u1)),
                           v3(b->x, y0, b->z)),
                     v3add(v3add(v3mul(fwd, 0.02f), v3mul(side, u1)),
                           v3(b->x, y1, b->z)),
                     v3add(v3add(v3mul(fwd, 0.02f), v3mul(side, u0)),
                           v3(b->x, y1, b->z)), col);
            }
        }
    } else {
        /* mid and far: lit floor strips. At that distance a grid is mush
         * anyway, and this is a sixth of the triangles. */
        float wq = (b->h - 1.1f) / (float)b->rows;
        for (r = 0; r < b->rows; ++r) {
            unsigned int q = b->seed + (unsigned int)r * 7717u;
            float on = hf(q);
            float y0, y1;
            Color col;
            if (on > 0.80f) continue;
            y0 = bot + (float)r * wq + wq * 0.26f;
            y1 = bot + (float)r * wq + wq * 0.70f;
            col = clerp(b->win, haze, bp_clampf(b->dist / 190.0f, 0.0f, 0.60f));
            col = cmul(col, 0.62f + 0.48f * hf(q + 1u));
            quad(v3add(v3add(v3mul(fwd, 0.02f), v3mul(side, -b->hw * 0.80f)),
                       v3(b->x, y0, b->z)),
                 v3add(v3add(v3mul(fwd, 0.02f), v3mul(side,  b->hw * 0.80f)),
                       v3(b->x, y0, b->z)),
                 v3add(v3add(v3mul(fwd, 0.02f), v3mul(side,  b->hw * 0.80f)),
                       v3(b->x, y1, b->z)),
                 v3add(v3add(v3mul(fwd, 0.02f), v3mul(side, -b->hw * 0.80f)),
                       v3(b->x, y1, b->z)), col);
        }
    }
}

static void draw_city(const BpPalette *pal, float t)
{
    Color haze = clerp((Color){ 86, 38, 62, 255 }, pal->horizon, 0.35f);
    int i;
    for (i = 0; i < S.nb; ++i) {
        const Bldg *b = &S.b[i];
        float cs = cosf(b->yaw), sn = sinf(b->yaw);
        V3 ex = v3(cs * b->hw, 0.0f, sn * b->hw);
        V3 ez = v3(-sn * b->hd, 0.0f, cs * b->hd);
        V3 base = v3(b->x, S.py, b->z);
        V3 topc = v3(b->x, S.py + b->h, b->z);
        float fade = bp_clampf(b->dist / 200.0f, 0.0f, 1.0f);
        Color body = clerp(b->body, haze, fade * 0.62f);
        V3 p0, p1, p2, p3, q0, q1, q2, q3;

        p0 = v3sub(v3sub(base, ex), ez);
        p1 = v3sub(v3add(base, ex), ez);
        p2 = v3add(v3add(base, ex), ez);
        p3 = v3add(v3sub(base, ex), ez);
        q0 = v3add(p0, v3(0, b->h, 0));
        q1 = v3add(p1, v3(0, b->h, 0));
        q2 = v3add(p2, v3(0, b->h, 0));
        q3 = v3add(p3, v3(0, b->h, 0));

        quad(p0, p1, q1, q0, body);
        quad(p1, p2, q2, q1, cmul(body, 0.82f));
        quad(p2, p3, q3, q2, cmul(body, 0.70f));
        quad(p3, p0, q0, q3, cmul(body, 0.90f));
        quad(q0, q1, q2, q3, cmul(body, 1.25f));

        /* windows on the two faces that look back at the table */
        {
            V3 toc = v3norm(v3(S.cx - b->x, 0.0f, S.cz - b->z));
            V3 fa = v3norm(v3(cs, 0.0f, sn));      /* +X face normal */
            V3 fb = v3norm(v3(-sn, 0.0f, cs));     /* +Z face normal */
            V3 sa = v3(-sn, 0.0f, cs), sb = v3(cs, 0.0f, sn);
            float da = v3dot(fa, toc), db = v3dot(fb, toc);
            V3 f1 = (da < 0.0f) ? fa : v3mul(fa, -1.0f);
            V3 f2 = (db < 0.0f) ? fb : v3mul(fb, -1.0f);
            Bldg tmp = *b;
            /* the helper works in "face" space: hw is the face half-width */
            tmp.hw = b->hd; tmp.x = b->x + f1.x * b->hw; tmp.z = b->z + f1.z * b->hw;
            draw_windows(&tmp, f1, sa, haze, t);
            if (b->ring < 2) {
                tmp = *b;
                tmp.hw = b->hw; tmp.x = b->x + f2.x * b->hd; tmp.z = b->z + f2.z * b->hd;
                draw_windows(&tmp, f2, sb, haze, t);
            }
        }

        /* rooftop neon band — the thing that makes a skyline look alive */
        if (b->sign) {
            float ph = t * (0.8f + hf(b->seed + 21u) * 1.4f) + hf(b->seed + 22u) * 6.0f;
            float on = (sinf(ph) > -0.55f) ? 1.0f : 0.18f;   /* lazy flicker */
            float y = S.py + b->h - 0.9f - hf(b->seed + 23u) * 1.4f;
            Color k = calpha(clerp(b->neon, haze, fade * 0.35f), 0.55f + 0.45f * on);
            int j, bars = 3 + (int)(hf(b->seed + 24u) * 3.0f);
            V3 toc = v3norm(v3(S.cx - b->x, 0.0f, S.cz - b->z));
            V3 sd  = v3(toc.z, 0.0f, -toc.x);
            V3 o   = v3add(v3(b->x, y, b->z), v3mul(toc, -(b->hw + b->hd) * 0.5f - 0.06f));
            for (j = 0; j < bars; ++j) {
                float u = ((float)j / (float)(bars - 1) - 0.5f) * b->hw * 1.5f;
                float hgt = 0.5f + hf(b->seed + 30u + (unsigned int)j) * 0.9f;
                V3 c0 = v3add(o, v3mul(sd, u));
                quad(v3add(c0, v3mul(sd, -0.10f)),
                     v3add(c0, v3mul(sd,  0.10f)),
                     v3add(v3add(c0, v3mul(sd,  0.10f)), v3(0, hgt, 0)),
                     v3add(v3add(c0, v3mul(sd, -0.10f)), v3(0, hgt, 0)), k);
            }
        }

        /* antenna and its aircraft warning light */
        if (b->ant) {
            float ah = 2.0f + hf(b->seed + 40u) * 3.5f;
            float blink = (sinf(t * 2.2f + hf(b->seed + 41u) * 6.0f) > 0.4f) ? 1.0f : 0.12f;
            DrawCylinderEx(rv(topc), rv(v3(topc.x, topc.y + ah, topc.z)),
                           0.05f, 0.02f, 5, cmul(body, 1.5f));
            DrawCubeV((Vector3){ topc.x, topc.y + ah + 0.18f, topc.z },
                      (Vector3){ 0.28f, 0.28f, 0.28f },
                      (Color){ 255, 70, 60, (unsigned char)(60 + 195 * blink) });
        }
    }
}

/* ------------------------------------------------------------------ */
/* funfair                                                             */

static void draw_wheel(float t)
{
    V3 c = v3(S.wheel_x, S.wheel_y, S.wheel_z);
    V3 toc = v3norm(v3(S.cx - S.wheel_x, 0.0f, S.cz - S.wheel_z));
    V3 u = v3(toc.z, 0.0f, -toc.x);       /* wheel plane, horizontal axis */
    V3 v = v3(0, 1, 0);
    const int SPOKES = 14;
    int i;
    float R = S.wheel_r;

    /* legs */
    DrawCylinderEx(rv(v3(c.x + u.x * R * 0.55f, S.py, c.z + u.z * R * 0.55f)),
                   rv(c), 0.20f, 0.10f, 6, (Color){ 34, 34, 52, 255 });
    DrawCylinderEx(rv(v3(c.x - u.x * R * 0.55f, S.py, c.z - u.z * R * 0.55f)),
                   rv(c), 0.20f, 0.10f, 6, (Color){ 34, 34, 52, 255 });

    /* rim: a chasing colour runs round it */
    for (i = 0; i < SPOKES * 2; ++i) {
        float a0 = BP_TAU * (float)i / (float)(SPOKES * 2) + S.wheel_a;
        float a1 = BP_TAU * (float)(i + 1) / (float)(SPOKES * 2) + S.wheel_a;
        float chase = 0.5f + 0.5f * sinf(t * 3.0f - (float)i * 0.55f);
        Color k = calpha(clerp(S.wheel_c, NEON[(i + S.hole) % 6], 0.5f),
                         0.35f + 0.65f * chase);
        V3 p0 = v3add(c, v3add(v3mul(u, cosf(a0) * R), v3mul(v, sinf(a0) * R)));
        V3 p1 = v3add(c, v3add(v3mul(u, cosf(a1) * R), v3mul(v, sinf(a1) * R)));
        DrawCylinderEx(rv(p0), rv(p1), 0.30f, 0.30f, 4, k);
        DrawCylinderEx(rv(p0), rv(p1), 0.85f, 0.85f, 4, calpha(k, 0.10f));
    }
    /* spokes and cabins */
    for (i = 0; i < SPOKES; ++i) {
        float a = BP_TAU * (float)i / (float)SPOKES + S.wheel_a;
        V3 rim = v3add(c, v3add(v3mul(u, cosf(a) * R), v3mul(v, sinf(a) * R)));
        V3 cab = v3add(c, v3add(v3mul(u, cosf(a) * (R - 0.55f)),
                                v3mul(v, sinf(a) * (R - 0.55f))));
        DrawLine3D(rv(c), rv(rim), calpha((Color){ 190, 210, 255, 255 }, 0.5f));
        DrawCubeV(rv(v3(cab.x, cab.y - 0.45f, cab.z)),
                  (Vector3){ 0.65f, 0.55f, 0.65f },
                  calpha(NEON[(i + 2) % 6], 0.9f));
    }
    disc(c, u, v, 0.9f, calpha((Color){ 255, 244, 210, 255 }, 0.85f), 12);
}

static void draw_beams(float t)
{
    int i, k;
    for (i = 0; i < S.nbm; ++i) {
        const Beam *b = &S.bm[i];
        float a = b->ph + t * b->rate;
        float tilt = 0.72f + 0.22f * sinf(t * b->rate * 1.7f + b->ph);
        V3 base = v3(b->x, S.py + 0.6f, b->z);
        V3 dir = v3norm(v3(cosf(a) * cosf(tilt), sinf(tilt), sinf(a) * cosf(tilt)));
        V3 side = v3norm(v3(-dir.z, 0.0f, dir.x));
        for (k = 0; k < 4; ++k) {
            float t0 = 14.0f * (float)k, t1 = 14.0f * (float)(k + 1);
            float w0 = 0.5f + t0 * 0.055f, w1 = 0.5f + t1 * 0.055f;
            V3 c0 = v3add(base, v3mul(dir, t0));
            V3 c1 = v3add(base, v3mul(dir, t1));
            float al = 0.13f / (1.0f + (float)k * 0.85f);
            quad(v3add(c0, v3mul(side, -w0)), v3add(c0, v3mul(side, w0)),
                 v3add(c1, v3mul(side,  w1)), v3add(c1, v3mul(side, -w1)),
                 calpha(b->c, al));
        }
    }
}

static void draw_blimp(float t)
{
    float a = S.blimp_a;
    V3 c = v3(S.cx + cosf(a) * S.blimp_r, S.blimp_y, S.cz + sinf(a) * S.blimp_r);
    V3 fwd = v3(-sinf(a), 0.0f, cosf(a));
    V3 sd = v3(fwd.z, 0.0f, -fwd.x);
    Color hull = (Color){ 46, 44, 70, 255 };
    int i;
    DrawCylinderEx(rv(v3sub(c, v3mul(fwd, 3.2f))), rv(v3sub(c, v3mul(fwd, 1.2f))),
                   0.05f, 1.05f, 8, hull);
    DrawCylinderEx(rv(v3sub(c, v3mul(fwd, 1.2f))), rv(v3add(c, v3mul(fwd, 1.2f))),
                   1.05f, 1.05f, 8, hull);
    DrawCylinderEx(rv(v3add(c, v3mul(fwd, 1.2f))), rv(v3add(c, v3mul(fwd, 3.4f))),
                   1.05f, 0.10f, 8, hull);
    /* the ad board: a scrolling row of bulbs, because of course it is */
    for (i = 0; i < 12; ++i) {
        float u = ((float)i / 11.0f - 0.5f) * 4.0f;
        float on = 0.5f + 0.5f * sinf(t * 4.0f - (float)i * 0.7f);
        V3 p = v3add(v3add(c, v3mul(fwd, u)), v3mul(sd, 1.02f));
        DrawCubeV(rv(v3(p.x, p.y + 0.15f, p.z)), (Vector3){ 0.22f, 0.22f, 0.22f },
                  calpha(NEON[(i + S.hole) % 6], 0.35f + 0.65f * on));
    }
    DrawCubeV(rv(v3(c.x, c.y - 1.25f, c.z)), (Vector3){ 0.7f, 0.4f, 0.7f },
              (Color){ 28, 28, 44, 255 });
}

/* ------------------------------------------------------------------ */

void bp_scenery_draw(const BpCam *cam, const BpPalette *pal, float t)
{
    (void)cam;
    if (!S.built) return;
    draw_sky(pal);
    draw_stars(t);
    draw_clouds(t);
    draw_plaza(pal, t);
    draw_reflections();
    draw_city(pal, t);
    draw_wheel(t);
    draw_blimp(t);
    draw_beams(t);
}

/* ------------------------------------------------------------------ */
/* near dressing: the stuff that lights the table itself               */

/* The table is an island of light dropped on the plaza: a dark skirt down to
 * the ground with a lit base line, and an inlaid neon border round it. At the
 * aiming camera angle this and the festoon are most of what you actually see
 * of the city, so it gets the detail budget. */
static void draw_apron(const BpWorld *w, const BpPalette *pal, float t)
{
    V3 lo, hi;
    float x0, x1, z0, z1, y;
    int i;
    bp_course_bounds(w, &lo, &hi);
    x0 = lo.x - 0.55f; x1 = hi.x + 0.55f;
    z0 = lo.z - 0.55f; z1 = hi.z + 0.55f;
    y  = lo.y - 0.06f;

    /* apron deck, then the skirt down to the plaza */
    quad(v3(x0, y, z0), v3(x1, y, z0), v3(x1, y, z1), v3(x0, y, z1),
         (Color){ 20, 20, 34, 255 });
    quad(v3(x0, y, z0), v3(x1, y, z0), v3(x1, S.py, z0), v3(x0, S.py, z0),
         (Color){ 13, 13, 24, 255 });
    quad(v3(x1, y, z1), v3(x0, y, z1), v3(x0, S.py, z1), v3(x1, S.py, z1),
         (Color){ 13, 13, 24, 255 });
    quad(v3(x0, y, z1), v3(x0, y, z0), v3(x0, S.py, z0), v3(x0, S.py, z1),
         (Color){ 10, 10, 20, 255 });
    quad(v3(x1, y, z0), v3(x1, y, z1), v3(x1, S.py, z1), v3(x1, S.py, z0),
         (Color){ 10, 10, 20, 255 });

    /* under-lighting: the table appears to float on its own glow */
    for (i = 0; i < 2; ++i) {
        float e = 0.10f + (float)i * 0.55f;
        float a = (i ? 0.10f : 0.30f) * (0.85f + 0.15f * sinf(t * 1.5f));
        quad(v3(x0 - e, S.py + 0.012f + 0.002f * (float)i, z0 - e),
             v3(x1 + e, S.py + 0.012f + 0.002f * (float)i, z0 - e),
             v3(x1 + e, S.py + 0.012f + 0.002f * (float)i, z1 + e),
             v3(x0 - e, S.py + 0.012f + 0.002f * (float)i, z1 + e),
             calpha(pal->accent, a));
    }
    /* inlaid tube along the top lip of the apron */
    {
        float g = 0.55f + 0.20f * sinf(t * 1.3f);
        Color k = calpha(pal->accent, 0.30f + 0.50f * g);
        float e = 0.055f;
        quad(v3(x0, y + 0.004f, z0), v3(x1, y + 0.004f, z0),
             v3(x1, y + 0.004f, z0 + e), v3(x0, y + 0.004f, z0 + e), k);
        quad(v3(x0, y + 0.004f, z1 - e), v3(x1, y + 0.004f, z1 - e),
             v3(x1, y + 0.004f, z1), v3(x0, y + 0.004f, z1), k);
        quad(v3(x0, y + 0.004f, z0), v3(x0 + e, y + 0.004f, z0),
             v3(x0 + e, y + 0.004f, z1), v3(x0, y + 0.004f, z1), k);
        quad(v3(x1 - e, y + 0.004f, z0), v3(x1, y + 0.004f, z0),
             v3(x1, y + 0.004f, z1), v3(x1 - e, y + 0.004f, z1), k);
    }
}

/* A point on one swaying span of bulb string, u running 0..1 pole to pole.
 *
 * The flag has always leaned on the breeze while the bulb strings hung dead
 * still, which quietly told the player the weather was a decal. Now the same
 * wind walks the strings: a span swings most at its middle and not at all at
 * the poles, rises a little as it swings out the way a real bob does, and each
 * span carries its own phase so the whole run ripples rather than sliding
 * sideways in one piece. Wire and bulbs come through here together, so they
 * cannot disagree about where the string is. */
static V3 festoon_pt(float px0, float pz0, float px1, float pz1,
                     float y, float sag, float u, float phase, float t)
{
    float ws = bp_wind_strength(), wa = bp_wind_angle();
    float droop = sinf(u * BP_PI);
    float swing = droop * (0.022f + 0.085f * ws)
                * sinf(t * (1.1f + 1.3f * ws) - phase);
    V3 q;
    q.x = bp_lerpf(px0, px1, u) + sinf(wa) * swing;
    q.z = bp_lerpf(pz0, pz1, u) + cosf(wa) * swing;
    q.y = y - droop * sag + fabsf(swing) * 0.22f;
    return q;
}

/* One pole roughly every three metres, all the way round, so wherever the
 * camera is parked there is a string of bulbs in the frame. */
static void draw_festoon(const BpWorld *w, float t)
{
    V3 lo, hi;
    int side, seg, i;
    bp_course_bounds(w, &lo, &hi);
    {
        float x0 = lo.x - 0.95f, x1 = hi.x + 0.95f;
        float z0 = lo.z - 0.95f, z1 = hi.z + 0.95f;
        float y = hi.y + 1.55f;
        float run[4][4] = {
            { x0, z0, x1, z0 }, { x1, z0, x1, z1 },
            { x1, z1, x0, z1 }, { x0, z1, x0, z0 },
        };
        int idx = 0;
        for (side = 0; side < 4; ++side) {
            float ax = run[side][0], az = run[side][1];
            float bx = run[side][2], bz = run[side][3];
            float len = sqrtf((bx - ax) * (bx - ax) + (bz - az) * (bz - az));
            int poles = bp_clampi((int)(len / 3.0f + 0.5f), 1, 8);
            for (seg = 0; seg < poles; ++seg) {
                float u0 = (float)seg / (float)poles;
                float u1 = (float)(seg + 1) / (float)poles;
                float px0 = bp_lerpf(ax, bx, u0), pz0 = bp_lerpf(az, bz, u0);
                float px1 = bp_lerpf(ax, bx, u1), pz1 = bp_lerpf(az, bz, u1);
                float sag = 0.14f + 0.06f * (len / (float)poles);
                float phase = (float)(side * 8 + seg) * 0.62f;
                /* pole, with a lit collar just under the string */
                DrawCylinderEx((Vector3){ px0, lo.y - 0.4f, pz0 },
                               (Vector3){ px0, y + 0.10f, pz0 },
                               0.038f, 0.024f, 6, (Color){ 38, 40, 58, 255 });
                DrawCubeV((Vector3){ px0, y + 0.14f, pz0 },
                          (Vector3){ 0.09f, 0.05f, 0.09f },
                          calpha(NEON[(side + S.hole) % 6], 0.85f));
                for (i = 0; i <= 5; ++i) {
                    float u = (float)i / 5.0f;
                    V3 b = festoon_pt(px0, pz0, px1, pz1, y, sag, u, phase, t);
                    float chase = 0.5f + 0.5f * sinf(t * 2.4f - (float)idx * 0.55f);
                    Color k = NEON[(idx + S.hole) % 6];
                    if (i < 5) {
                        V3 b2 = festoon_pt(px0, pz0, px1, pz1, y, sag,
                                           (float)(i + 1) / 5.0f, phase, t);
                        DrawLine3D((Vector3){ b.x, b.y, b.z },
                                   (Vector3){ b2.x, b2.y, b2.z },
                                   (Color){ 28, 30, 44, 255 });
                    }
                    if (i == 5) break;            /* last bulb belongs to the next span */
                    DrawCubeV((Vector3){ b.x, b.y - 0.05f, b.z },
                              (Vector3){ 0.05f, 0.05f, 0.05f },
                              calpha(k, 0.5f + 0.5f * chase));
                    DrawCubeV((Vector3){ b.x, b.y - 0.05f, b.z },
                              (Vector3){ 0.15f, 0.15f, 0.15f },
                              calpha(k, 0.05f + 0.09f * chase));
                    ++idx;
                }
            }
        }
    }
}

/* Fairground furniture at the corners: a booth with a striped awning and a
 * neon sign, and a stack of speakers. Small, but it turns "a table in the
 * dark" into "a table at a night market". */
static void draw_props(const BpWorld *w, float t)
{
    V3 lo, hi;
    int c, i;
    bp_course_bounds(w, &lo, &hi);
    for (c = 0; c < 4; ++c) {
        float bx = (c & 1) ? hi.x + 1.9f : lo.x - 1.9f;
        float bz = (c & 2) ? hi.z + 1.9f : lo.z - 1.9f;
        float by = S.py;
        unsigned int q = (unsigned int)(S.hole * 13 + c) * 2654435761u;
        Color k = NEON[hsh(q) % 6u];
        if ((c & 1) == (c >> 1)) {
            /* booth: body, counter, awning stripes, sign */
            DrawCubeV((Vector3){ bx, by + 0.45f, bz }, (Vector3){ 1.05f, 0.90f, 0.85f },
                      (Color){ 26, 26, 42, 255 });
            DrawCubeV((Vector3){ bx, by + 0.92f, bz }, (Vector3){ 1.25f, 0.06f, 1.05f },
                      (Color){ 40, 40, 60, 255 });
            for (i = 0; i < 5; ++i) {
                float u = ((float)i / 4.0f - 0.5f) * 1.2f;
                DrawCubeV((Vector3){ bx + u, by + 0.97f, bz },
                          (Vector3){ 0.22f, 0.05f, 1.02f },
                          (i & 1) ? (Color){ 226, 226, 236, 255 }
                                  : (Color){ 190, 52, 62, 255 });
            }
            /* the sign, blinking on its own lazy timer */
            {
                float on = (sinf(t * 1.6f + (float)c) > -0.4f) ? 1.0f : 0.2f;
                DrawCubeV((Vector3){ bx, by + 1.28f, bz }, (Vector3){ 0.95f, 0.30f, 0.06f },
                          calpha(k, 0.45f + 0.55f * on));
                DrawCubeV((Vector3){ bx, by + 1.28f, bz }, (Vector3){ 1.20f, 0.55f, 0.20f },
                          calpha(k, 0.05f + 0.09f * on));
            }
            /* light spilling onto the ground under it */
            disc(v3(bx, by + 0.01f, bz), v3(1, 0, 0), v3(0, 0, 1), 1.5f,
                 calpha(k, 0.055f), 12);
        } else {
            /* speaker stack — this is where the trio is coming from */
            for (i = 0; i < 3; ++i) {
                float yy = by + 0.22f + (float)i * 0.42f;
                float sc = 1.0f - (float)i * 0.16f;
                float thump = 1.0f + 0.05f * sinf(t * 6.2f - (float)i * 0.6f);
                DrawCubeV((Vector3){ bx, yy, bz },
                          (Vector3){ 0.62f * sc, 0.40f, 0.52f * sc },
                          (Color){ 22, 22, 34, 255 });
                DrawCubeV((Vector3){ bx, yy, bz + 0.27f * sc },
                          (Vector3){ 0.34f * sc * thump, 0.24f * thump, 0.03f },
                          calpha(k, 0.55f));
            }
            DrawCylinderEx((Vector3){ bx, by, bz }, (Vector3){ bx, by + 1.9f, bz },
                           0.035f, 0.022f, 6, (Color){ 34, 36, 52, 255 });
            /* a par-can on a stand, aimed back at the table */
            {
                V3 head = v3(bx, by + 1.95f, bz);
                V3 aim = v3norm(v3(S.cx - bx, -1.4f, S.cz - bz));
                DrawCubeV(rv(head), (Vector3){ 0.18f, 0.18f, 0.18f },
                          (Color){ 40, 42, 58, 255 });
                for (i = 0; i < 3; ++i) {
                    float t0 = (float)i * 0.9f, t1 = (float)(i + 1) * 0.9f;
                    DrawCylinderEx(rv(v3add(head, v3mul(aim, t0))),
                                   rv(v3add(head, v3mul(aim, t1))),
                                   0.09f + t0 * 0.10f, 0.09f + t1 * 0.10f, 8,
                                   calpha(k, 0.045f / (1.0f + (float)i)));
                }
            }
        }
    }
}

static void draw_cup_beacon(const BpWorld *w, const BpPalette *pal, float t)
{
    int c = bp_course_cup(w), i;
    V3 p;
    float close;
    if (c < 0 || w->cup_sealed) return;
    p = v3(w->pockets[c].x, w->pockets[c].y, w->pockets[c].z);

    /* How close the cue ball is, 0 (miles away) to 1 (on the lip). The beacon
     * used to breathe at one fixed rate whatever was happening, which meant the
     * tensest moment on the table looked exactly like the calmest. Now the ring
     * draws in and quickens as the ball closes — the table itself holds its
     * breath, with nothing added to the HUD to say so. */
    {
        const BpBall *b = &w->balls[0];
        float dx = b->p.x - p.x, dz = b->p.z - p.z;
        float d = sqrtf(dx * dx + dz * dz);
        close = (b->state == BS_GONE) ? 0.0f
              : 1.0f - bp_clampf(d / (BP_CUP_R * 11.0f), 0.0f, 1.0f);
        close *= close;                  /* stays calm until it is genuinely near */
    }

    /* a soft column so the cup is findable from anywhere on the table */
    for (i = 0; i < 5; ++i) {
        float u0 = (float)i / 5.0f, u1 = (float)(i + 1) / 5.0f;
        float h = 1.30f;
        float pulse = 0.5f + 0.5f * sinf(t * bp_lerpf(2.0f, 5.0f, close) - u0 * 3.0f);
        float r0 = BP_CUP_R * bp_lerpf(1.05f, 0.35f, u0);
        float r1 = BP_CUP_R * bp_lerpf(1.05f, 0.35f, u1);
        float a = (0.16f - u0 * 0.026f) * (0.6f + 0.4f * pulse) * (1.0f + 0.55f * close);
        DrawCylinderEx((Vector3){ p.x, p.y + h * u0, p.z },
                       (Vector3){ p.x, p.y + h * u1, p.z },
                       r0, r1, 10, calpha(pal->accent, a));
    }
    /* and a breathing ring on the felt around the lip */
    {
        float rate = bp_lerpf(2.6f, 9.0f, close);
        float k = 1.0f + 0.10f * sinf(t * rate);
        float rad = BP_CUP_R * bp_lerpf(1.70f, 1.28f, close) * k;
        DrawCircle3D((Vector3){ p.x, p.y + 0.006f, p.z }, rad,
                     (Vector3){ 1, 0, 0 }, 90.0f,
                     calpha(pal->accent, 0.55f + 0.40f * close));
        /* a second ring only shows up on the approach, chasing the first in */
        if (close > 0.05f)
            DrawCircle3D((Vector3){ p.x, p.y + 0.005f, p.z },
                         rad * bp_lerpf(1.55f, 1.12f, close),
                         (Vector3){ 1, 0, 0 }, 90.0f,
                         calpha(pal->accent, 0.34f * close));
    }
}

void bp_scenery_draw_near(const BpWorld *w, const BpPalette *pal, float t)
{
    if (!S.built) return;
    draw_apron(w, pal, t);
    draw_props(w, t);
    draw_festoon(w, t);
    draw_cup_beacon(w, pal, t);
}

/* ------------------------------------------------------------------ */
/* screen-space wash                                                   */

void bp_scenery_draw_overlay(int sw, int sh, const BpPalette *pal, float t)
{
    int i;
    /* corner vignette: four gradients, no shader, no render texture */
    DrawRectangleGradientV(0, 0, sw, sh / 5, (Color){ 4, 4, 14, 110 },
                           (Color){ 4, 4, 14, 0 });
    DrawRectangleGradientV(0, sh - sh / 4, sw, sh / 4, (Color){ 4, 4, 14, 0 },
                           (Color){ 6, 4, 16, 140 });
    DrawRectangleGradientH(0, 0, sw / 6, sh, (Color){ 4, 4, 14, 90 },
                           (Color){ 4, 4, 14, 0 });
    DrawRectangleGradientH(sw - sw / 6, 0, sw / 6, sh, (Color){ 4, 4, 14, 0 },
                           (Color){ 4, 4, 14, 90 });

    /* out-of-focus city bokeh drifting across the lens */
    for (i = 0; i < 9; ++i) {
        unsigned int q = 0x51ed2u + (unsigned int)i * 26699u;
        float sp = 0.008f + hf(q + 1u) * 0.02f;
        float x = fmodf(hf(q) * (float)sw + t * sp * (float)sw, (float)sw);
        float y = (0.10f + hf(q + 2u) * 0.34f) * (float)sh +
                  sinf(t * 0.25f + hf(q + 3u) * 6.0f) * 9.0f;
        float r = 7.0f + hf(q + 4u) * 22.0f;
        Color k = NEON[hsh(q + 5u) % 6u];
        DrawCircle((int)x, (int)y, r, calpha(k, 0.030f));
        DrawCircle((int)x, (int)y, r * 0.45f, calpha(k, 0.035f));
    }
    (void)pal;
}
