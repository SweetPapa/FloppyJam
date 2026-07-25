#include "artkit.h"
#include "rlgl.h"
#include "save/save.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

/* ==========================================================================
 * stable noise
 * ========================================================================== */
float art_noise(int seed, int i)
{
    unsigned h = (unsigned)seed * 2654435761u ^ (unsigned)i * 2246822519u;
    h ^= h >> 15; h *= 2654435761u; h ^= h >> 13;
    return (float)(h & 0xffff) / 32767.5f - 1.0f;      /* -1 .. 1 */
}

/* ==========================================================================
 * palette — the gimmick's engine (§8.2)
 * ========================================================================== */
static int   g_stage;
static float g_band[7] = { 1, 0, 0, 0, 0, 0, 0 };
static float g_bloom_x, g_bloom_y, g_bloom_r;
static int   g_bloom_hue = -1;
static float g_last_x = VW * 0.5f, g_last_y = VH * 0.5f;

int palette_stage(void) { return g_stage; }
float palette_band(int hue) { return (hue < 0 || hue > 6) ? 1.0f : g_band[hue]; }

void palette_set_stage(int n)
{
    if (n < 0) n = 0;
    if (n > 6) n = 6;
    g_stage = n;
    for (int i = 0; i <= 6; i++) g_band[i] = (i == 0 || i <= n) ? 1.0f : 0.0f;
    g_bloom_hue = -1;
}

void palette_bloom(float x, float y, int stage)
{
    if (stage < 1 || stage > 6) return;
    g_stage = stage;
    g_bloom_x = x; g_bloom_y = y; g_bloom_r = 0.0f;
    g_bloom_hue = stage;
    for (int i = 1; i < stage; i++) g_band[i] = 1.0f;
    g_band[stage] = 0.0f;
}

bool palette_blooming(void) { return g_bloom_hue > 0; }

static void bloom_update(float dt)
{
    if (g_bloom_hue <= 0) return;
    /* the flood crosses the canvas in ~2.6s; reduce-motion snaps it instead
     * of racing it, because a slow flood is the accessible one */
    g_bloom_r += (settings()->reduce_motion ? 2600.0f : 700.0f) * dt;
    if (g_bloom_r > 2200.0f) {
        g_band[g_bloom_hue] = 1.0f;
        g_bloom_hue = -1;
    }
}

/* which hue band an authored colour belongs to */
static int hue_band(Color c)
{
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b));
    float d = mx - mn;
    if (mx <= 0.001f || d / mx < 0.14f) return HUE_BASE;   /* ink & paper */

    float h;
    if (mx == r)      h = 60.0f * fmodf((g - b) / d, 6.0f);
    else if (mx == g) h = 60.0f * ((b - r) / d + 2.0f);
    else              h = 60.0f * ((r - g) / d + 4.0f);
    if (h < 0) h += 360.0f;

    if (h < 18.0f  || h >= 345.0f) return HUE_RED;
    if (h < 45.0f)                 return HUE_GOLD;
    if (h < 78.0f)                 return HUE_YELLOW;
    if (h < 172.0f)                return HUE_GREEN;
    if (h < 268.0f)                return HUE_BLUE;
    return HUE_VIOLET;
}

Color pal_at(Color c, float x, float y)
{
    int band = hue_band(c);
    float amt = g_band[band];

    if (band == g_bloom_hue) {
        float dx = x - g_bloom_x, dy = y - g_bloom_y;
        float d = sqrtf(dx * dx + dy * dy);
        amt = 1.0f - (d - g_bloom_r) / 90.0f;            /* soft leading edge */
        if (amt < 0) amt = 0;
        if (amt > 1) amt = 1;
    }
    if (amt >= 0.999f) return c;

    /* Drained: luminance on a slightly warm paper-gray, not a dead gray, and
     * pulled down a little. Without that darkening a pale sky and pale paper
     * land on the same value and the whole town reads as a blank page —
     * gray has to keep its tonal drawing or there is nothing to restore. */
    float lum = (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) * 0.78f;
    float gr = lum * 1.03f, gg = lum * 1.00f, gb = lum * 0.93f;
    float k = amt;
    Color o;
    o.r = (unsigned char)fminf(255.0f, gr + (c.r - gr) * k);
    o.g = (unsigned char)fminf(255.0f, gg + (c.g - gg) * k);
    o.b = (unsigned char)fminf(255.0f, gb + (c.b - gb) * k);
    o.a = c.a;
    return o;
}

Color pal(Color c) { return pal_at(c, g_last_x, g_last_y); }

/* Authored slot colours. These are the *full spectrum* values; the player
 * earns them back one band at a time. */
Color col_ink(void)        { return (Color){  38,  34,  40, 255 }; }
Color col_ink_soft(void)   { return (Color){  74,  69,  78, 255 }; }
Color col_paper(void)      { return (Color){ 241, 233, 218, 255 }; }
Color col_paper_dark(void) { return (Color){ 207, 195, 176, 255 }; }
Color col_accent_a(void)   { return pal((Color){ 232, 188,  58, 255 }); }  /* yellow */
Color col_accent_b(void)   { return pal((Color){ 196,  74,  70, 255 }); }  /* red    */
Color col_sky(void)        { return pal((Color){ 122, 168, 214, 255 }); }  /* blue   */
Color col_sea(void)        { return pal((Color){  68, 124, 160, 255 }); }  /* blue   */
Color col_warm(void)       { return pal((Color){ 214, 132,  74, 255 }); }  /* gold   */
Color col_cool(void)       { return pal((Color){  96, 158, 116, 255 }); }  /* green  */

/* ==========================================================================
 * frame plumbing: virtual canvas, camera, shake, fade
 * ========================================================================== */
static RenderTexture2D g_canvas;
static bool  g_have_canvas;
static float g_cam_x, g_cam_y;
static float g_shake_amt, g_shake_t, g_shake_dur;
static float g_fade;
static float g_time;

static Vector2 g_grain[1400];

void art_init(void)
{
    g_canvas = LoadRenderTexture(VW, VH);
    SetTextureFilter(g_canvas.texture, TEXTURE_FILTER_BILINEAR);
    g_have_canvas = true;
    /* Ink has no back face. Every fill in this game is an authored 2D
     * polygon and half of them wind the "wrong" way for the GPU's taste;
     * culling them is how a whole sky silently fails to be drawn. */
    rlDisableBackfaceCulling();
    for (int i = 0; i < 1400; i++) {
        g_grain[i].x = (art_noise(7717, i) * 0.5f + 0.5f) * VW;
        g_grain[i].y = (art_noise(9931, i) * 0.5f + 0.5f) * VH;
    }
    palette_set_stage(0);
}

void art_shutdown(void)
{
    if (g_have_canvas) { UnloadRenderTexture(g_canvas); g_have_canvas = false; }
}

void art_camera(float x, float y) { g_cam_x = x; g_cam_y = y; }

void art_shake(float amount, float duration)
{
    if (settings()->reduce_motion) return;               /* §5.4, honoured here */
    g_shake_amt = amount; g_shake_dur = duration; g_shake_t = 0;
}

void  art_fade_set(float a) { g_fade = a < 0 ? 0 : (a > 1 ? 1 : a); }
float art_fade_get(void)    { return g_fade; }

void art_begin_frame(float dt)
{
    g_time += dt;
    bloom_update(dt);
    if (g_shake_dur > 0) {
        g_shake_t += dt;
        if (g_shake_t >= g_shake_dur) { g_shake_dur = 0; g_shake_amt = 0; }
    }
    BeginTextureMode(g_canvas);
    /* the desk, a shade under the paper, so panels read as sheets on it */
    ClearBackground((Color){ 226, 217, 200, 255 });
    rlPushMatrix();
    rlTranslatef(-g_cam_x, -g_cam_y, 0);
}

/* letterbox transform, shared by art_end_frame and art_mouse */
static void canvas_rect(float *ox, float *oy, float *s)
{
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float sc = fminf(sw / VW, sh / VH);
    *s = sc;
    *ox = (sw - VW * sc) * 0.5f;
    *oy = (sh - VH * sc) * 0.5f;
}

void art_end_frame(void)
{
    rlPopMatrix();
    EndTextureMode();

    float ox, oy, s;
    canvas_rect(&ox, &oy, &s);
    float sx = 0, sy = 0;
    if (g_shake_dur > 0) {
        float k = 1.0f - g_shake_t / g_shake_dur;
        sx = art_noise(1234, (int)(g_shake_t * 240)) * g_shake_amt * k;
        sy = art_noise(4321, (int)(g_shake_t * 240)) * g_shake_amt * k;
    }

    BeginDrawing();
    ClearBackground((Color){ 22, 20, 24, 255 });
    Rectangle src = { 0, 0, (float)VW, -(float)VH };
    Rectangle dst = { ox + sx * s, oy + sy * s, VW * s, VH * s };
    DrawTexturePro(g_canvas.texture, src, dst, (Vector2){ 0, 0 }, 0, WHITE);
    if (g_fade > 0.001f)
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){ 20, 18, 22, (unsigned char)(g_fade * 255) });
    EndDrawing();
}

Vector2 art_mouse(void)
{
    float ox, oy, s;
    canvas_rect(&ox, &oy, &s);
    Vector2 m = GetMousePosition();
    return (Vector2){ (m.x - ox) / s + g_cam_x, (m.y - oy) / s + g_cam_y };
}

bool art_hover(Rectangle r) { return CheckCollisionPointRec(art_mouse(), r); }

/* ==========================================================================
 * ink vocabulary (§8.1)
 * ========================================================================== */
void ink_stroke(const Vector2 *pts, int n, float w, float wobble, int seed, Color c)
{
    if (n < 2) return;
    float cx = 0, cy = 0;
    for (int i = 0; i < n; i++) { cx += pts[i].x; cy += pts[i].y; }
    g_last_x = cx / n; g_last_y = cy / n;
    Color k = pal_at(c, g_last_x, g_last_y);

    Vector2 prev = { 0 };
    for (int i = 0; i < n; i++) {
        Vector2 p = pts[i];
        p.x += art_noise(seed, i * 2) * wobble;
        p.y += art_noise(seed, i * 2 + 1) * wobble;
        if (i > 0) {
            /* nib pressure: the line breathes along its length */
            float pw = w * (0.78f + 0.34f * (art_noise(seed + 91, i) * 0.5f + 0.5f));
            DrawLineEx(prev, p, pw, k);
            DrawCircleV(p, pw * 0.5f, k);
        } else {
            DrawCircleV(p, w * 0.5f, k);
        }
        prev = p;
    }
}

void ink_line(float x1, float y1, float x2, float y2, float w, float wobble,
              int seed, Color c)
{
    Vector2 p[5];
    for (int i = 0; i < 5; i++) {
        float t = i / 4.0f;
        p[i].x = x1 + (x2 - x1) * t;
        p[i].y = y1 + (y2 - y1) * t;
    }
    ink_stroke(p, 5, w, wobble, seed, c);
}

void ink_rect(Rectangle r, float w, float wobble, int seed, Color c)
{
    Vector2 p[5] = {
        { r.x, r.y }, { r.x + r.width, r.y },
        { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height }, { r.x, r.y }
    };
    ink_stroke(p, 5, w, wobble, seed, c);
}

void ink_circle(float x, float y, float rad, float w, float wobble, int seed, Color c)
{
    Vector2 p[19];
    for (int i = 0; i < 19; i++) {
        float a = (float)i / 18.0f * 2 * PI;
        p[i].x = x + cosf(a) * rad;
        p[i].y = y + sinf(a) * rad;
    }
    ink_stroke(p, 19, w, wobble, seed, c);
}

/* even-odd scanline used by both hatching and dot fills */
static bool point_in_poly(const Vector2 *p, int n, float x, float y)
{
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if ((p[i].y > y) != (p[j].y > y) &&
            x < (p[j].x - p[i].x) * (y - p[i].y) / (p[j].y - p[i].y) + p[i].x)
            in = !in;
    }
    return in;
}

/* intersections of the infinite line (o + d*t) with the polygon, sorted */
static int poly_cuts(const Vector2 *p, int n, Vector2 o, Vector2 d, float *out, int cap)
{
    int m = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        Vector2 a = p[j], b = p[i];
        Vector2 e = { b.x - a.x, b.y - a.y };
        float den = d.x * e.y - d.y * e.x;
        if (fabsf(den) < 1e-6f) continue;
        float u = ((a.x - o.x) * e.y - (a.y - o.y) * e.x) / den;
        float v = ((a.x - o.x) * d.y - (a.y - o.y) * d.x) / den;
        if (v < 0 || v > 1) continue;
        if (m < cap) out[m++] = u;
    }
    for (int i = 1; i < m; i++) {                 /* insertion sort, m is tiny */
        float k = out[i]; int j = i - 1;
        while (j >= 0 && out[j] > k) { out[j + 1] = out[j]; j--; }
        out[j + 1] = k;
    }
    return m;
}

static void hatch_lines(const Vector2 *p, int n, Rectangle bb, float ang,
                        float gap, float w, Color k)
{
    Vector2 d = { cosf(ang), sinf(ang) };
    Vector2 nvec = { -d.y, d.x };
    float diag = sqrtf(bb.width * bb.width + bb.height * bb.height);
    Vector2 mid = { bb.x + bb.width * 0.5f, bb.y + bb.height * 0.5f };
    for (float s = -diag * 0.5f; s <= diag * 0.5f; s += gap) {
        Vector2 o = { mid.x + nvec.x * s, mid.y + nvec.y * s };
        float cuts[16];
        int m = poly_cuts(p, n, o, d, cuts, 16);
        for (int i = 0; i + 1 < m; i += 2) {
            Vector2 a = { o.x + d.x * cuts[i],     o.y + d.y * cuts[i] };
            Vector2 b = { o.x + d.x * cuts[i + 1], o.y + d.y * cuts[i + 1] };
            DrawLineEx(a, b, w, k);
        }
    }
}

void ink_fill(const Vector2 *pts, int n, int hatch, int seed, Color c)
{
    if (n < 3) return;
    float cx = 0, cy = 0;
    Rectangle bb = { pts[0].x, pts[0].y, 0, 0 };
    float x0 = pts[0].x, y0 = pts[0].y, x1 = x0, y1 = y0;
    for (int i = 0; i < n; i++) {
        cx += pts[i].x; cy += pts[i].y;
        if (pts[i].x < x0) x0 = pts[i].x;
        if (pts[i].x > x1) x1 = pts[i].x;
        if (pts[i].y < y0) y0 = pts[i].y;
        if (pts[i].y > y1) y1 = pts[i].y;
    }
    bb = (Rectangle){ x0, y0, x1 - x0, y1 - y0 };
    g_last_x = cx / n; g_last_y = cy / n;
    Color k = pal_at(c, g_last_x, g_last_y);

    if (hatch == HATCH_NONE || hatch == HATCH_SOLID) {
        /* fan from the centroid: our shapes are authored star-shaped */
        Vector2 fan[66];
        int m = n > 64 ? 64 : n;
        fan[0] = (Vector2){ g_last_x, g_last_y };
        float area = 0;
        for (int i = 0, j = m - 1; i < m; j = i++)
            area += pts[j].x * pts[i].y - pts[i].x * pts[j].y;
        for (int i = 0; i < m; i++) fan[1 + i] = area < 0 ? pts[m - 1 - i] : pts[i];
        fan[1 + m] = fan[1];
        DrawTriangleFan(fan, m + 2, k);
        return;
    }

    /* washed base so hatching reads as tone, not as line art on nothing */
    Color wash = k; wash.a = (unsigned char)(k.a * 0.16f);
    Vector2 fan[66];
    int m = n > 64 ? 64 : n;
    fan[0] = (Vector2){ g_last_x, g_last_y };
    float area = 0;
    for (int i = 0, j = m - 1; i < m; j = i++)
        area += pts[j].x * pts[i].y - pts[i].x * pts[j].y;
    for (int i = 0; i < m; i++) fan[1 + i] = area < 0 ? pts[m - 1 - i] : pts[i];
    fan[1 + m] = fan[1];
    DrawTriangleFan(fan, m + 2, wash);

    float jig = art_noise(seed, 3) * 0.12f;
    /* Hatching is tone, and tone is per-area. A gap that reads as shading on
     * a roof reads as solid black across a whole hillside, so the pen opens
     * up as the shape grows. */
    float g = (bb.width > 420.0f || bb.height > 300.0f) ? 2.6f : 1.0f;
    switch (hatch) {
    case HATCH_DIAG:  hatch_lines(pts, n, bb, -0.72f + jig, 7.0f * g, 1.6f, k); break;
    case HATCH_VERT:  hatch_lines(pts, n, bb, 1.5708f + jig, 6.0f * g, 1.6f, k); break;
    case HATCH_CROSS: hatch_lines(pts, n, bb, -0.72f + jig, 8.0f * g, 1.5f, k);
                      hatch_lines(pts, n, bb,  0.72f + jig, 8.0f * g, 1.5f, k); break;
    case HATCH_DOT: {
        for (float y = bb.y; y < bb.y + bb.height; y += 7.0f * g)
            for (float x = bb.x; x < bb.x + bb.width; x += 7.0f * g) {
                float jx = art_noise(seed, (int)(x + y * 3)) * 2.4f;
                float jy = art_noise(seed + 5, (int)(x * 3 + y)) * 2.4f;
                if (point_in_poly(pts, n, x + jx, y + jy))
                    DrawCircle((int)(x + jx), (int)(y + jy), 1.4f, k);
            }
        break;
    }
    default: break;
    }
}

void ink_blob(float x, float y, float r, float squash, int seed, Color c)
{
    Vector2 p[22];
    for (int i = 0; i < 22; i++) {
        float a = (float)i / 22.0f * 2 * PI;
        float rr = r * (1.0f + art_noise(seed, i) * 0.13f);
        p[i].x = x + cosf(a) * rr;
        p[i].y = y + sinf(a) * rr * squash;
    }
    ink_fill(p, 22, HATCH_NONE, seed, c);
}

void paper_panel(Rectangle r, float torn, int seed)
{
    /* The step has to scale with the panel, or a wide panel runs out of
     * vertices halfway round and the fill closes itself with a diagonal
     * straight across the page. Ask how it was found out. */
    Vector2 p[64];
    const int CAP = 60;
    float perim = 2.0f * (r.width + r.height);
    float step = fmaxf(18.0f, perim / (float)(CAP - 6));
    int n = 0;

    for (float x = r.x; x < r.x + r.width && n < CAP; x += step, n++)
        p[n] = (Vector2){ x, r.y + art_noise(seed, n) * torn };
    for (float y = r.y; y < r.y + r.height && n < CAP; y += step, n++)
        p[n] = (Vector2){ r.x + r.width + art_noise(seed, n) * torn, y };
    for (float x = r.x + r.width; x > r.x && n < CAP; x -= step, n++)
        p[n] = (Vector2){ x, r.y + r.height + art_noise(seed, n) * torn };
    for (float y = r.y + r.height; y > r.y && n < CAP; y -= step, n++)
        p[n] = (Vector2){ r.x + art_noise(seed, n) * torn, y };
    if (n < 3) return;

    Vector2 sh[64];
    for (int i = 0; i < n; i++) sh[i] = (Vector2){ p[i].x + 7, p[i].y + 9 };
    ink_fill(sh, n, HATCH_NONE, seed, (Color){ 52, 46, 50, 60 });
    ink_fill(p, n, HATCH_NONE, seed, (Color){ 248, 242, 229, 255 });
    Vector2 outline[65];
    memcpy(outline, p, sizeof(Vector2) * (size_t)n);
    outline[n] = p[0];
    ink_stroke(outline, n + 1, 2.0f, 0.7f, seed + 3, col_ink_soft());
}

void paper_grain(float intensity)
{
    unsigned char a = (unsigned char)(intensity * 26.0f);
    if (a == 0) return;
    Color c1 = { 120, 108, 92, a };
    Color c2 = { 255, 250, 236, a };
    for (int i = 0; i < 1400; i++)
        DrawCircleV(g_grain[i], (i & 3) ? 1.0f : 1.8f, (i & 1) ? c1 : c2);
}

void vignette(float amt)
{
    unsigned char a = (unsigned char)(amt * 110.0f);
    Color e = { 48, 42, 40, a };
    Color z = { 48, 42, 40, 0 };
    DrawRectangleGradientH(0, 0, 190, VH, e, z);
    DrawRectangleGradientH(VW - 190, 0, 190, VH, z, e);
    DrawRectangleGradientV(0, 0, VW, 150, e, z);
    DrawRectangleGradientV(0, VH - 150, VW, 150, z, e);
}

void wind_lines(float t, float amt, Color c)
{
    if (settings()->reduce_motion) t = 0;
    Color k = pal_at(c, VW * 0.5f, VH * 0.5f);
    k.a = (unsigned char)(60 * amt);
    for (int i = 0; i < 9; i++) {
        float y = 70 + i * 63.0f + art_noise(31, i) * 24.0f;
        float ph = fmodf(t * (52.0f + i * 7.0f) + i * 190.0f, VW + 340.0f) - 170.0f;
        float len = 60 + art_noise(57, i) * 40.0f;
        Vector2 p[4];
        for (int j = 0; j < 4; j++) {
            float u = j / 3.0f;
            p[j].x = ph + u * len;
            p[j].y = y + sinf(u * 3.1f + i) * 6.0f;
        }
        ink_stroke(p, 4, 1.8f, 0.5f, 900 + i, k);
    }
}

/* ==========================================================================
 * text
 * ========================================================================== */
float art_text(const char *s, float x, float y, float size, Color c)
{
    float sz = size * text_scale();
    Font f = GetFontDefault();
    Vector2 m = MeasureTextEx(f, s, sz, sz / 10.0f);
    DrawTextEx(f, s, (Vector2){ x, y }, sz, sz / 10.0f, pal_at(c, x, y));
    return m.x;
}

float art_text_w(const char *s, float size)
{
    float sz = size * text_scale();
    return MeasureTextEx(GetFontDefault(), s, sz, sz / 10.0f).x;
}

int art_text_flow(const char *s, int start, float x, float y, float wide,
                  float maxh, float size, Color c, int reveal, bool draw,
                  float *out_h)
{
    float sz = size * text_scale();
    Font f = GetFontDefault();
    float lh = sz * 1.42f;
    float spacing = sz / 10.0f;
    int n = (int)strlen(s);
    if (start < 0) start = 0;
    if (out_h) *out_h = 0;
    if (start >= n) return -1;

    char probe[TEXT_MAX];
    float ty = y;
    int i = start;

    while (i < n) {
        while (i < n && s[i] == ' ') i++;            /* eat the wrap's space */
        if (i >= n) break;
        if (ty + lh > y + maxh + 0.5f) {             /* no room for this line */
            if (out_h) *out_h = ty - y;
            return i;
        }

        /* how much of the paragraph fits on one line */
        int line_start = i, end = -1, j = i;
        while (j < n && s[j] != '\n') {
            int k = j;
            while (k < n && s[k] != ' ' && s[k] != '\n') k++;
            int len = k - line_start;
            if (len > TEXT_MAX - 1) len = TEXT_MAX - 1;
            memcpy(probe, s + line_start, (size_t)len);
            probe[len] = 0;
            if (end >= 0 && MeasureTextEx(f, probe, sz, spacing).x > wide) break;
            end = k;
            j = k;
            while (j < n && s[j] == ' ') j++;
        }
        if (end < 0) {                               /* one unbreakable word */
            end = i;
            while (end < n && s[end] != ' ' && s[end] != '\n') end++;
            if (end == i) end = i + 1;
        }

        int len = end - line_start;
        if (len > TEXT_MAX - 1) len = TEXT_MAX - 1;
        int since_start = line_start - start;
        if (draw && (reveal < 0 || since_start < reveal)) {
            int cut = len;
            if (reveal >= 0 && since_start + len > reveal) cut = reveal - since_start;
            if (cut > 0) {
                memcpy(probe, s + line_start, (size_t)cut);
                probe[cut] = 0;
                DrawTextEx(f, probe, (Vector2){ x, ty }, sz, spacing, c);
            }
        }
        ty += lh;
        i = end;
        if (i < n && s[i] == '\n') i++;
    }
    if (out_h) *out_h = ty - y;
    return -1;
}

float art_text_wrap(const char *s, float x, float y, float wide, float size,
                    Color c, int reveal)
{
    float h = 0;
    art_text_flow(s, 0, x, y, wide, 1.0e6f, size, c, reveal, true, &h);
    return h;
}

float art_text_size_for(const char *s, float wide, float size)
{
    float sz = size;
    while (sz > 9.0f && art_text_w(s, sz) > wide) sz -= 1.0f;
    return sz;
}

float art_text_fit(const char *s, Rectangle box, float size, Color c)
{
    float sz = size;
    for (int step = 0; step < 8; step++) {
        if (art_text_flow(s, 0, box.x, box.y, box.width, box.height, sz, c,
                          -1, false, NULL) < 0)
            break;                        /* it all fits at this size */
        sz -= 1.0f;
        if (sz < 9.0f) { sz = 9.0f; break; }
    }
    art_text_flow(s, 0, box.x, box.y, box.width, box.height, sz, c, -1, true, NULL);
    return sz;
}

/* ==========================================================================
 * sparkle — the interactable tell (§1.2.1: default ON)
 * ========================================================================== */
void sparkle(float x, float y, float r, float t)
{
    if (!settings()->highlight) return;
    if (settings()->reduce_motion) t = 0.35f;
    Color c = pal_at((Color){ 232, 188, 58, 255 }, x, y);
    Color orbit = c;
    orbit.a = 82;
    DrawEllipseLines((int)x, (int)y, r * 0.62f, r * 0.43f, orbit);
    DrawEllipseLines((int)(x + 1), (int)y, r * 0.48f, r * 0.33f,
                     (Color){ orbit.r, orbit.g, orbit.b, 48 });
    for (int i = 0; i < 6; i++) {
        float a = t * 1.2f + i * (PI / 3.0f);
        float rr = r * (0.72f + 0.24f * sinf(t * 2.4f + i));
        float px = x + cosf(a) * rr, py = y + sinf(a) * rr * 0.7f;
        float s = 4.1f + sinf(t * 3.0f + i * 2.1f) * 1.2f;
        DrawLineEx((Vector2){ px - s, py }, (Vector2){ px + s, py }, 2.0f, c);
        DrawLineEx((Vector2){ px, py - s }, (Vector2){ px, py + s }, 2.0f, c);
    }
}

/* ==========================================================================
 * doodles (§8.1) — parametric, never sprites
 * ========================================================================== */
static const char *g_doodle_names[D_COUNT] = {
    "lantern", "fish", "gear", "teacup", "feather", "key", "prism", "bird",
    "flower", "bread", "book", "clock", "spool", "bee", "star", "wave",
    "house", "tree", "cloud", "envelope", "anchor", "lock"
};

const char *doodle_name(int id)
{
    return (id >= 0 && id < D_COUNT) ? g_doodle_names[id] : "?";
}

/* helpers that work in a local unit box (-1..1), rotated and placed */
static float g_dx, g_dy, g_ds, g_dr;
static Vector2 dp(float x, float y)
{
    float cs = cosf(g_dr), sn = sinf(g_dr);
    return (Vector2){ g_dx + (x * cs - y * sn) * g_ds,
                      g_dy + (x * sn + y * cs) * g_ds };
}
static void dline(float x1, float y1, float x2, float y2, float w, Color c, int seed)
{
    Vector2 a = dp(x1, y1), b = dp(x2, y2);
    ink_line(a.x, a.y, b.x, b.y, w * g_ds * 0.06f + 1.0f, 0.5f, seed, c);
}
static void dpoly(const float *xy, int n, int hatch, Color c, int seed)
{
    Vector2 p[24];
    if (n > 24) n = 24;
    for (int i = 0; i < n; i++) p[i] = dp(xy[i * 2], xy[i * 2 + 1]);
    ink_fill(p, n, hatch, seed, c);
}
static void dcircle(float x, float y, float r, Color c, int seed)
{
    Vector2 a = dp(x, y);
    ink_blob(a.x, a.y, r * g_ds, 1.0f, seed, c);
}

void doodle(int id, float x, float y, float scale, float rot, Color c)
{
    g_dx = x; g_dy = y; g_ds = scale; g_dr = rot;
    int s = id * 97 + 11;
    Color ink = col_ink();

    switch (id) {
    case D_LANTERN: {
        const float body[] = { -.5f,-.45f, .5f,-.45f, .38f,.55f, -.38f,.55f };
        dpoly(body, 4, HATCH_NONE, c, s);
        dline(-.55f,-.45f, .55f,-.45f, 3, ink, s);
        dline(-.42f,.55f, .42f,.55f, 3, ink, s);
        dline(0,-.45f, 0,-.8f, 2, ink, s);
        dcircle(0, .05f, .17f, col_accent_a(), s + 1);
        break;
    }
    case D_FISH: {
        const float body[] = { -.7f,0, -.2f,-.35f, .45f,-.22f, .7f,0, .45f,.22f, -.2f,.35f };
        dpoly(body, 6, HATCH_DIAG, c, s);
        const float tail[] = { .62f,0, .95f,-.32f, .95f,.32f };
        dpoly(tail, 3, HATCH_NONE, c, s + 2);
        dcircle(-.42f, -.06f, .06f, ink, s + 3);
        break;
    }
    case D_GEAR: {
        for (int i = 0; i < 8; i++) {
            float a = i / 8.0f * 2 * PI;
            dline(cosf(a) * .55f, sinf(a) * .55f, cosf(a) * .85f, sinf(a) * .85f, 5, c, s + i);
        }
        Vector2 ctr = dp(0, 0);
        ink_circle(ctr.x, ctr.y, .6f * scale, 4.0f, 0.6f, s, c);
        ink_circle(ctr.x, ctr.y, .22f * scale, 3.0f, 0.5f, s + 1, c);
        break;
    }
    case D_TEACUP: {
        const float cup[] = { -.5f,-.25f, .5f,-.25f, .34f,.45f, -.34f,.45f };
        dpoly(cup, 4, HATCH_NONE, c, s);
        dline(.5f,-.15f, .78f,.1f, 3, ink, s + 1);
        dline(.78f,.1f, .48f,.3f, 3, ink, s + 2);
        dline(-.62f,.5f, .62f,.5f, 3, ink, s + 3);
        break;
    }
    case D_FEATHER: {
        dline(0,-.9f, 0,.9f, 3, ink, s);
        for (int i = 0; i < 7; i++) {
            float t = -0.75f + i * 0.24f;
            float w = 0.42f * (1.0f - fabsf(t) * 0.7f);
            dline(0, t, -w, t + 0.16f, 2, c, s + i);
            dline(0, t,  w, t + 0.16f, 2, c, s + i + 20);
        }
        break;
    }
    case D_KEY: {
        Vector2 ctr = dp(-.55f, 0);
        ink_circle(ctr.x, ctr.y, .32f * scale, 4.0f, 0.6f, s, c);
        dline(-.25f,0, .85f,0, 4, c, s + 1);
        dline(.62f,0, .62f,.34f, 4, c, s + 2);
        dline(.85f,0, .85f,.26f, 4, c, s + 3);
        break;
    }
    case D_PRISM: {
        const float tri[] = { 0,-.85f, .78f,.6f, -.78f,.6f };
        dpoly(tri, 3, HATCH_NONE, c, s);
        dline(0,-.85f, 0,.6f, 2, col_paper(), s + 1);
        dline(0,-.2f, .5f,.1f, 2, col_paper(), s + 2);
        break;
    }
    case D_BIRD: {
        const float body[] = { -.6f,.1f, -.1f,-.3f, .5f,-.1f, .7f,.2f, .1f,.42f, -.45f,.36f };
        dpoly(body, 6, HATCH_NONE, c, s);
        const float wing[] = { -.15f,-.1f, .38f,-.05f, .05f,.26f };
        dpoly(wing, 3, HATCH_DIAG, ink, s + 1);
        dline(.66f,.16f, .95f,.06f, 2, col_accent_a(), s + 2);
        dcircle(.5f, -.02f, .05f, ink, s + 3);
        break;
    }
    case D_FLOWER: {
        for (int i = 0; i < 6; i++) {
            float a = i / 6.0f * 2 * PI;
            dcircle(cosf(a) * .42f, sinf(a) * .42f, .26f, c, s + i);
        }
        dcircle(0, 0, .2f, col_accent_a(), s + 9);
        break;
    }
    case D_BREAD: {
        const float loaf[] = { -.75f,.35f, -.6f,-.3f, 0,-.5f, .6f,-.3f, .75f,.35f };
        dpoly(loaf, 5, HATCH_DOT, c, s);
        for (int i = 0; i < 3; i++)
            dline(-.35f + i * .35f, -.34f, -.2f + i * .35f, -.02f, 2, ink, s + i);
        break;
    }
    case D_BOOK: {
        const float cover[] = { -.7f,-.5f, .7f,-.5f, .7f,.5f, -.7f,.5f };
        dpoly(cover, 4, HATCH_NONE, c, s);
        dline(0,-.5f, 0,.5f, 3, ink, s + 1);
        dline(-.55f,-.28f, -.12f,-.28f, 2, ink, s + 2);
        dline(.12f,-.28f, .55f,-.28f, 2, ink, s + 3);
        break;
    }
    case D_CLOCK: {
        Vector2 ctr = dp(0, 0);
        ink_circle(ctr.x, ctr.y, .8f * scale, 4.0f, 0.7f, s, c);
        dline(0,0, 0,-.55f, 3, ink, s + 1);
        dline(0,0, .38f,.16f, 3, ink, s + 2);
        break;
    }
    case D_SPOOL: {
        const float b[] = { -.4f,-.5f, .4f,-.5f, .4f,.5f, -.4f,.5f };
        dpoly(b, 4, HATCH_VERT, c, s);
        dline(-.62f,-.55f, .62f,-.55f, 4, ink, s + 1);
        dline(-.62f,.55f, .62f,.55f, 4, ink, s + 2);
        break;
    }
    case D_BEE: {
        dcircle(0, 0, .45f, col_accent_a(), s);
        dline(-.2f,-.36f, -.2f,.36f, 3, ink, s + 1);
        dline(.12f,-.34f, .12f,.34f, 3, ink, s + 2);
        const float w[] = { -.05f,-.35f, .35f,-.8f, .45f,-.4f };
        dpoly(w, 3, HATCH_NONE, (Color){ 240, 246, 250, 190 }, s + 3);
        break;
    }
    case D_STAR: {
        Vector2 p[10];
        for (int i = 0; i < 10; i++) {
            float a = i / 10.0f * 2 * PI - PI * 0.5f;
            float r = (i & 1) ? 0.36f : 0.9f;
            p[i] = dp(cosf(a) * r, sinf(a) * r);
        }
        ink_fill(p, 10, HATCH_NONE, s, c);
        break;
    }
    case D_WAVE: {
        Vector2 p[9];
        for (int i = 0; i < 9; i++)
            p[i] = dp(-0.9f + i * 0.225f, sinf(i * 0.9f) * 0.25f);
        ink_stroke(p, 9, 3.0f * (scale * 0.05f) + 1.5f, 0.6f, s, c);
        break;
    }
    case D_HOUSE: {
        const float w[] = { -.6f,0, .6f,0, .6f,.6f, -.6f,.6f };
        dpoly(w, 4, HATCH_NONE, c, s);
        const float roof[] = { -.75f,0, 0,-.65f, .75f,0 };
        dpoly(roof, 3, HATCH_DIAG, ink, s + 1);
        const float door[] = { -.15f,.18f, .15f,.18f, .15f,.6f, -.15f,.6f };
        dpoly(door, 4, HATCH_NONE, ink, s + 2);
        break;
    }
    case D_TREE: {
        dline(0,.8f, 0,-.1f, 5, ink, s);
        dcircle(0, -.35f, .5f, c, s + 1);
        dcircle(-.34f, -.12f, .34f, c, s + 2);
        dcircle(.34f, -.12f, .34f, c, s + 3);
        break;
    }
    case D_CLOUD: {
        dcircle(-.4f, .1f, .34f, c, s);
        dcircle(0, -.06f, .45f, c, s + 1);
        dcircle(.42f, .12f, .32f, c, s + 2);
        break;
    }
    case D_ENVELOPE: {
        const float e[] = { -.7f,-.42f, .7f,-.42f, .7f,.42f, -.7f,.42f };
        dpoly(e, 4, HATCH_NONE, c, s);
        dline(-.7f,-.42f, 0,.08f, 2, ink, s + 1);
        dline(.7f,-.42f, 0,.08f, 2, ink, s + 2);
        break;
    }
    case D_ANCHOR: {
        dline(0,-.7f, 0,.7f, 4, c, s);
        dline(-.42f,-.4f, .42f,-.4f, 4, c, s + 1);
        dline(-.6f,.3f, 0,.75f, 4, c, s + 2);
        dline(.6f,.3f, 0,.75f, 4, c, s + 3);
        break;
    }
    case D_LOCK: {
        const float b[] = { -.5f,-.05f, .5f,-.05f, .5f,.6f, -.5f,.6f };
        dpoly(b, 4, HATCH_NONE, c, s);
        Vector2 ctr = dp(0, -.2f);
        ink_circle(ctr.x, ctr.y, .32f * scale, 4.5f, 0.5f, s + 1, c);
        dcircle(0, .26f, .09f, ink, s + 2);
        break;
    }
    default: break;
    }
}
