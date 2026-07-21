#include "draw.h"
#include "rng.h"

/* ------------------------------------------------------------------ */
/* font: 8x8, authored as art so it stays verifiable in source.        */
/* ASCII 32..95 (text is upper-cased at draw time).                    */
/* ------------------------------------------------------------------ */
static const char *FONT_ART[64][8] = {
/*   */ {"........","........","........","........","........","........","........","........"},
/* ! */ {"..#.....","..#.....","..#.....","..#.....","..#.....","........","..#.....","........"},
/* " */ {".#.#....",".#.#....","........","........","........","........","........","........"},
/* # */ {".#.#....","#####...",".#.#....","#####...",".#.#....","........","........","........"},
/* $ */ {"..#.....",".####...","#.#.....",".###....","..#.#...","####....","..#.....","........"},
/* % */ {"##..#...","##.#....","..#.....",".#......","#..##...","...##...","........","........"},
/* & */ {".##.....","#..#....",".##.....","#..#.#..","#...#...",".###.#..","........","........"},
/* ' */ {"..#.....","..#.....","........","........","........","........","........","........"},
/* ( */ {"...#....","..#.....","..#.....","..#.....","..#.....","..#.....","...#....","........"},
/* ) */ {".#......","..#.....","..#.....","..#.....","..#.....","..#.....",".#......","........"},
/* * */ {"........","#.#.#...",".###....","#####...",".###....","#.#.#...","........","........"},
/* + */ {"........","..#.....","..#.....","#####...","..#.....","..#.....","........","........"},
/* , */ {"........","........","........","........","........","..#.....",".#......","........"},
/* - */ {"........","........","........","#####...","........","........","........","........"},
/* . */ {"........","........","........","........","........","........","..#.....","........"},
/* / */ {"....#...","....#...","...#....","..#.....",".#......","#.......","#.......","........"},
/* 0 */ {".###....","#...#...","#..##...","#.#.#...","##..#...","#...#...",".###....","........"},
/* 1 */ {"..#.....",".##.....","..#.....","..#.....","..#.....","..#.....",".###....","........"},
/* 2 */ {".###....","#...#...","....#...","...#....","..#.....",".#......","#####...","........"},
/* 3 */ {"#####...","...#....","..##....","....#...","....#...","#...#...",".###....","........"},
/* 4 */ {"...##...","..#.#...",".#..#...","#...#...","#####...","....#...","....#...","........"},
/* 5 */ {"#####...","#.......","####....","....#...","....#...","#...#...",".###....","........"},
/* 6 */ {"..##....",".#......","#.......","####....","#...#...","#...#...",".###....","........"},
/* 7 */ {"#####...","....#...","...#....","..#.....",".#......",".#......",".#......","........"},
/* 8 */ {".###....","#...#...","#...#...",".###....","#...#...","#...#...",".###....","........"},
/* 9 */ {".###....","#...#...","#...#...",".####...","....#...","...#....",".##.....","........"},
/* : */ {"........","..#.....","........","........","........","..#.....","........","........"},
/* ; */ {"........","..#.....","........","........","..#.....",".#......","........","........"},
/* < */ {"...#....","..#.....",".#......","#.......",".#......","..#.....","...#....","........"},
/* = */ {"........","........","#####...","........","#####...","........","........","........"},
/* > */ {".#......","..#.....","...#....","....#...","...#....","..#.....",".#......","........"},
/* ? */ {".###....","#...#...","....#...","...#....","..#.....","........","..#.....","........"},
/* @ */ {".###....","#...#...","#.###...","#.#.#...","#.##....","#.......",".###....","........"},
/* A */ {".###....","#...#...","#...#...","#####...","#...#...","#...#...","#...#...","........"},
/* B */ {"####....","#...#...","#...#...","####....","#...#...","#...#...","####....","........"},
/* C */ {".###....","#...#...","#.......","#.......","#.......","#...#...",".###....","........"},
/* D */ {"###.....","#..#....","#...#...","#...#...","#...#...","#..#....","###.....","........"},
/* E */ {"#####...","#.......","#.......","####....","#.......","#.......","#####...","........"},
/* F */ {"#####...","#.......","#.......","####....","#.......","#.......","#.......","........"},
/* G */ {".###....","#...#...","#.......","#..##...","#...#...","#...#...",".###....","........"},
/* H */ {"#...#...","#...#...","#...#...","#####...","#...#...","#...#...","#...#...","........"},
/* I */ {".###....","..#.....","..#.....","..#.....","..#.....","..#.....",".###....","........"},
/* J */ {"..###...","...#....","...#....","...#....","...#....","#..#....",".##.....","........"},
/* K */ {"#...#...","#..#....","#.#.....","##......","#.#.....","#..#....","#...#...","........"},
/* L */ {"#.......","#.......","#.......","#.......","#.......","#.......","#####...","........"},
/* M */ {"#...#...","##.##...","#.#.#...","#.#.#...","#...#...","#...#...","#...#...","........"},
/* N */ {"#...#...","##..#...","#.#.#...","#.#.#...","#..##...","#...#...","#...#...","........"},
/* O */ {".###....","#...#...","#...#...","#...#...","#...#...","#...#...",".###....","........"},
/* P */ {"####....","#...#...","#...#...","####....","#.......","#.......","#.......","........"},
/* Q */ {".###....","#...#...","#...#...","#...#...","#.#.#...","#..#....",".##.#...","........"},
/* R */ {"####....","#...#...","#...#...","####....","#.#.....","#..#....","#...#...","........"},
/* S */ {".####...","#.......","#.......",".###....","....#...","....#...","####....","........"},
/* T */ {"#####...","..#.....","..#.....","..#.....","..#.....","..#.....","..#.....","........"},
/* U */ {"#...#...","#...#...","#...#...","#...#...","#...#...","#...#...",".###....","........"},
/* V */ {"#...#...","#...#...","#...#...","#...#...","#...#...",".#.#....","..#.....","........"},
/* W */ {"#...#...","#...#...","#...#...","#.#.#...","#.#.#...","##.##...","#...#...","........"},
/* X */ {"#...#...","#...#...",".#.#....","..#.....",".#.#....","#...#...","#...#...","........"},
/* Y */ {"#...#...","#...#...",".#.#....","..#.....","..#.....","..#.....","..#.....","........"},
/* Z */ {"#####...","....#...","...#....","..#.....",".#......","#.......","#####...","........"},
/* [ */ {".###....",".#......",".#......",".#......",".#......",".#......",".###....","........"},
/* \ */ {"#.......","#.......",".#......","..#.....","...#....","....#...","....#...","........"},
/* ] */ {".###....","...#....","...#....","...#....","...#....","...#....",".###....","........"},
/* ^ */ {"..#.....",".#.#....","#...#...","........","........","........","........","........"},
/* _ */ {"........","........","........","........","........","........","........","#####..."}
};

static uint8_t s_font[64][8];

/* ------------------------------------------------------------------ */
/* icons: authored 8x8, expanded to the 16x16 1-bit sheet at init.     */
/* One entry per microgame, in roster order (§5.2).                    */
/* ------------------------------------------------------------------ */
static const char *ICON_ART[ICON_COUNT][8] = {
/* 0 TAP       */ {"........","..####..",".#....#.","#..##..#","#.####.#","#......#",".######.","........"},
/* 1 UPBEAT    */ {"...##...","..####..",".##..##.","##....##","...##...","...##...","...##...","........"},
/* 2 ALTERNATE */ {"..#..#..",".##..##.","###..###","..#..#..","..#..#..","###..###",".##..##.","........"},
/* 3 HOLD      */ {"..####..",".#....#.",".#....#.",".#....#.",".#....#.","..####..","...##...","..####.."},
/* 4 REST      */ {"#......#",".#....#.","..####..",".######.","..####..",".#....#.","#......#","........"},
/* 5 SNIPE     */ {"...##...","..####..",".##..##.","##.##.##",".##..##.","..####..","...##...","........"},
/* 6 DOUBLE    */ {"##....##","##....##","##....##","########","##....##","##....##","##....##","........"},
/* 7 STAIRS    */ {"......##","....####","..######","########","........","........","........","........"},
/* 8 RAPID     */ {"#.#.#.#.",".#.#.#.#","#.#.#.#.",".#.#.#.#","#.#.#.#.",".#.#.#.#","#.#.#.#.","........"},
/* 9 ECHO      */ {"..#.....",".###....","#####...",".###....","..#..##.","..#.####","..#..##.","........"},
/*10 BLIND     */ {"..####..",".#....#.","#..##..#","#.####.#","#..##..#",".#....#.","..####..","##....##"},
/*11 SWAP      */ {".######.","#......#","#.####.#","...##...","..####..",".######.","........","........"},
/*12 MEASURE   */ {"########","#......#","#.####.#","#.####.#","#.####.#","#......#","########","........"},
/*13 CALLBACK  */ {"...##...","..####..",".######.","########","..####..","...##...","..#..#..",".#....#."},
/*14 FINALE    */ {"#.#..#.#",".#.##.#.","..####..","########","..####..",".#.##.#.","#.#..#.#","........"},
/*15 SPARE     */ {"........",".######.",".#....#.",".#....#.",".#....#.",".#....#.",".######.","........"}
};

static uint16_t s_icons[ICON_COUNT][16];

/* ------------------------------------------------------------------ */
/* palettes (§9.1): bg, dim, main, accent, hot                          */
/* ------------------------------------------------------------------ */
static const Palette PALETTES[4] = {
    /* 0 midnight  */ {{ 8, 10, 22, 255}, { 40, 46, 78, 255}, {126,150,232,255}, {255,120,180,255}, {255,238,120,255}},
    /* 1 ember     */ {{ 18, 8, 10, 255}, { 62, 34, 34, 255}, {226,128, 86,255}, {255,196, 74,255}, {255,246,214,255}},
    /* 2 acid      */ {{  6, 16, 12, 255}, { 30, 60, 42, 255}, {110,222,150,255}, {236,255, 96,255}, {255,255,255,255}},
    /* 3 violet    */ {{ 14,  6, 24, 255}, { 52, 30, 76, 255}, {186,124,246,255}, {104,232,236,255}, {255,224,255,255}}
};

static Palette s_pal;
static int     s_pal_idx = 0;
static float   s_desat = 0.0f;

/* ------------------------------------------------------------------ */
static RenderTexture2D s_trail;
static bool  s_trail_ok = false;
static float s_shake = 0.0f;
static float s_shake_t = 0.0f;
static float s_hitstop = 0.0f;

#define MAX_PARTICLES 512
typedef struct { float x, y, vx, vy, life, life0, size; Color c; int active; } Particle;
static Particle s_particles[MAX_PARTICLES];

#define MAX_POPS 32
typedef struct { char s[24]; float x, y, life, life0; int scale; Color c; int active; } Pop;
static Pop s_pops[MAX_POPS];

static Rng s_cos_rng;

/* ------------------------------------------------------------------ */
Color cmix(Color a, Color b, float t)
{
    t = g_clampf(t, 0.0f, 1.0f);
    Color o;
    o.r = (unsigned char)(a.r + (b.r - a.r) * t);
    o.g = (unsigned char)(a.g + (b.g - a.g) * t);
    o.b = (unsigned char)(a.b + (b.b - a.b) * t);
    o.a = (unsigned char)(a.a + (b.a - a.a) * t);
    return o;
}

Color pal_mix(Color c, float t) { return cmix(c, s_pal.dim, g_clampf(t + s_desat, 0.0f, 1.0f)); }

void draw_set_desat(float amount) { s_desat = g_clampf(amount, 0.0f, 1.0f); }
const Palette *pal(void) { return &s_pal; }
void draw_set_palette(int idx) { s_pal_idx = ((idx % 4) + 4) % 4; s_pal = PALETTES[s_pal_idx]; }

void draw_init(void)
{
    for (int c = 0; c < 64; c++)
        for (int row = 0; row < 8; row++) {
            uint8_t bits = 0;
            const char *r = FONT_ART[c][row];
            for (int x = 0; x < 8; x++)
                if (r[x] == '#') bits |= (uint8_t)(1u << x);
            s_font[c][row] = bits;
        }

    for (int i = 0; i < ICON_COUNT; i++)
        for (int row = 0; row < 8; row++) {
            const char *r = ICON_ART[i][row];
            uint16_t bits = 0;
            for (int x = 0; x < 8; x++)
                if (r[x] == '#') bits |= (uint16_t)(3u << (x * 2));  /* x2 horizontally */
            s_icons[i][row * 2 + 0] = bits;
            s_icons[i][row * 2 + 1] = bits;
        }

    draw_set_palette(0);
    rng_seed(&s_cos_rng, 0xC05131Cu);
    s_trail = LoadRenderTexture(SCREEN_W, SCREEN_H);
    s_trail_ok = true;
    BeginTextureMode(s_trail);
    ClearBackground(s_pal.bg);
    EndTextureMode();
}

void draw_shutdown(void)
{
    if (s_trail_ok) UnloadRenderTexture(s_trail);
    s_trail_ok = false;
}

/* ---- text ---------------------------------------------------------- */
static int char_index(char ch)
{
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    int i = (int)ch - 32;
    if (i < 0 || i >= 64) return 0;
    return i;
}

int gtext_width(const char *s, int scale)
{
    int n = 0;
    for (const char *p = s; *p; p++) n++;
    return n * 6 * scale;
}

void gtext(const char *s, int x, int y, int scale, Color c)
{
    int cx = x;
    for (const char *p = s; *p; p++) {
        int gi = char_index(*p);
        for (int row = 0; row < 8; row++) {
            uint8_t bits = s_font[gi][row];
            if (!bits) continue;
            int run = 0;
            for (int col = 0; col < 8; col++) {
                int on = (bits >> col) & 1;
                if (on) { if (run == 0) run = 1; else run++; }
                if (!on || col == 7) {
                    if (run > 0) {
                        int endc = on ? col + 1 : col;
                        DrawRectangle(cx + (endc - run) * scale, y + row * scale,
                                      run * scale, scale, c);
                        run = 0;
                    }
                }
            }
        }
        cx += 6 * scale;
    }
}

void gtext_shadow(const char *s, int x, int y, int scale, Color c)
{
    Color sh = s_pal.bg; sh.a = 190;
    gtext(s, x + scale, y + scale, scale, sh);
    gtext(s, x, y, scale, c);
}

void gtext_center(const char *s, int cx, int y, int scale, Color c)
{
    gtext(s, cx - gtext_width(s, scale) / 2, y, scale, c);
}

/* ---- icons --------------------------------------------------------- */
void gicon(int icon_id, int x, int y, int scale, Color c)
{
    if (icon_id < 0 || icon_id >= ICON_COUNT) icon_id = ICON_COUNT - 1;
    for (int row = 0; row < 16; row++) {
        uint16_t bits = s_icons[icon_id][row];
        if (!bits) continue;
        int run = 0;
        for (int col = 0; col < 16; col++) {
            int on = (bits >> col) & 1;
            if (on) run++;
            if (!on || col == 15) {
                if (run > 0) {
                    int endc = on ? col + 1 : col;
                    DrawRectangle(x + (endc - run) * scale, y + row * scale, run * scale, scale, c);
                    run = 0;
                }
            }
        }
    }
}

void gkeycap(const char *label, int x, int y, int scale, Color c, bool lit)
{
    int w = 10 * scale, h = 10 * scale;
    Color fill = lit ? c : s_pal.bg;
    Color edge = lit ? s_pal.hot : c;
    DrawRectangle(x, y, w, h, fill);
    DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)w, (float)h},
                         (float)(scale > 2 ? 3 : 2), edge);
    Color tc = lit ? s_pal.bg : c;
    gtext(label, x + (w - gtext_width(label, scale)) / 2 + scale / 2, y + h / 2 - 4 * scale, scale, tc);
}

/* ---- frame --------------------------------------------------------- */
void draw_frame_begin(float dt)
{
    shake_update(dt);
    hitstop_update(dt);
    particles_update(dt);
    pops_update(dt);
}

void draw_trail_begin(float fade)
{
    BeginTextureMode(s_trail);
    Color f = s_pal.bg;
    f.a = (unsigned char)g_clampf(fade * 255.0f, 0.0f, 255.0f);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, f);
}

void draw_trail_end(void) { EndTextureMode(); }

void draw_trail_blit(float rotation, float scale)
{
    Vector2 sh = shake_offset();
    Rectangle src = { 0, 0, (float)SCREEN_W, -(float)SCREEN_H };
    Rectangle dst = { SCREEN_W * 0.5f + sh.x, SCREEN_H * 0.5f + sh.y,
                      SCREEN_W * scale, SCREEN_H * scale };
    Vector2 origin = { SCREEN_W * scale * 0.5f, SCREEN_H * scale * 0.5f };
    DrawTexturePro(s_trail.texture, src, dst, origin, rotation, WHITE);
}

void draw_frame_end(void) { }

/* ---- juice --------------------------------------------------------- */
void shake_add(float amount) { s_shake = g_clampf(s_shake + amount, 0.0f, 1.0f); }

void shake_update(float dt)
{
    s_shake_t += dt * 60.0f;
    s_shake = g_approach(s_shake, 0.0f, 9.0f, dt);
    if (s_shake < 0.001f) s_shake = 0.0f;
}

Vector2 shake_offset(void)
{
    /* decaying noise offset, 4 px max (§9.2) */
    float a = s_shake * 4.0f;
    Vector2 v;
    v.x = sinf(s_shake_t * 3.1f) * a + sinf(s_shake_t * 7.7f) * a * 0.5f;
    v.y = cosf(s_shake_t * 4.3f) * a + cosf(s_shake_t * 9.1f) * a * 0.5f;
    return v;
}

void hitstop_add(float sec) { if (sec > s_hitstop) s_hitstop = sec; }
bool hitstop_active(void) { return s_hitstop > 0.0f; }
void hitstop_update(float dt) { s_hitstop -= dt; if (s_hitstop < 0.0f) s_hitstop = 0.0f; }

void particles_burst(float x, float y, int n, Color c, float speed, float life)
{
    for (int i = 0; i < n; i++) {
        Particle *p = NULL;
        for (int k = 0; k < MAX_PARTICLES; k++)
            if (!s_particles[k].active) { p = &s_particles[k]; break; }
        if (!p) return;
        float ang = rng_f01(&s_cos_rng) * 6.2831853f;
        float sp = speed * (0.35f + rng_f01(&s_cos_rng));
        p->active = 1;
        p->x = x; p->y = y;
        p->vx = cosf(ang) * sp;
        p->vy = sinf(ang) * sp;
        p->life = p->life0 = life * (0.6f + rng_f01(&s_cos_rng) * 0.7f);
        p->size = 2.0f + rng_f01(&s_cos_rng) * 3.0f;
        p->c = c;
    }
}

void particles_update(float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &s_particles[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.0f) { p->active = 0; continue; }
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vy += 260.0f * dt;
        p->vx *= 1.0f - 1.2f * dt;
    }
}

void particles_draw(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &s_particles[i];
        if (!p->active) continue;
        float a = g_clampf(p->life / p->life0, 0.0f, 1.0f);
        Color c = p->c;
        c.a = (unsigned char)(a * 255.0f);
        float s = p->size * (0.4f + a * 0.9f);
        DrawRectangle((int)(p->x - s * 0.5f), (int)(p->y - s * 0.5f), (int)s, (int)s, c);
    }
}

void particles_clear(void) { memset(s_particles, 0, sizeof s_particles); }

void pop_text(const char *s, float x, float y, int scale, Color c, float life)
{
    Pop *p = NULL;
    for (int i = 0; i < MAX_POPS; i++) if (!s_pops[i].active) { p = &s_pops[i]; break; }
    if (!p) p = &s_pops[0];
    memset(p, 0, sizeof *p);
    p->active = 1;
    snprintf(p->s, sizeof p->s, "%s", s);
    p->x = x; p->y = y; p->scale = scale; p->c = c;
    p->life = p->life0 = life;
}

void pops_update(float dt)
{
    for (int i = 0; i < MAX_POPS; i++) {
        Pop *p = &s_pops[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.0f) { p->active = 0; continue; }
        p->y -= 46.0f * dt;
    }
}

void pops_draw(void)
{
    for (int i = 0; i < MAX_POPS; i++) {
        Pop *p = &s_pops[i];
        if (!p->active) continue;
        float t = p->life / p->life0;
        float e = g_ease_out(1.0f - t);
        int scale = (int)(p->scale * (1.0f + 0.5f * (1.0f - e)));
        if (scale < 1) scale = 1;
        Color c = p->c;
        c.a = (unsigned char)(g_clampf(t * 1.6f, 0.0f, 1.0f) * 255.0f);
        gtext_center(p->s, (int)p->x, (int)p->y, scale, c);
    }
}

void pops_clear(void) { memset(s_pops, 0, sizeof s_pops); }

/* ---- cracks: 3 authored polyline sets (§9.2) ------------------------ */
static const float CRACK0[] = { 0,180, 90,240, 40,320, 150,380, 90,470, 210,540, 160,700, -1 };
static const float CRACK1[] = { 1280,140, 1170,210, 1240,300, 1110,360, 1200,450, 1080,520, 1150,650, -1 };
static const float CRACK2[] = { 420,0, 470,90, 380,150, 520,220, 460,300, 600,360, 540,470, -1 };
static const float *CRACKS[3] = { CRACK0, CRACK1, CRACK2 };

void draw_cracks(int count, float alpha, Color c)
{
    for (int k = 0; k < count && k < 3; k++) {
        const float *pts = CRACKS[k];
        Color cc = c;
        cc.a = (unsigned char)(g_clampf(alpha, 0.0f, 1.0f) * 255.0f);
        for (int i = 0; pts[i] >= 0.0f && pts[i + 2] >= 0.0f; i += 2)
            DrawLineEx((Vector2){pts[i], pts[i + 1]}, (Vector2){pts[i + 2], pts[i + 3]}, 3.0f, cc);
    }
}

/* ---- attract harmonograph (§9.3) ------------------------------------ */
void harmonograph_draw(uint32_t seed_code, float t, float alpha)
{
    Rng r = rng_stream(seed_code, RNG_STREAM_ATTRACT);
    float f1 = 1.0f + (float)rng_range(&r, 1, 5);
    float f2 = 1.0f + (float)rng_range(&r, 1, 5);
    float f3 = 1.0f + (float)rng_range(&r, 1, 7);
    float f4 = 1.0f + (float)rng_range(&r, 1, 7);
    float p1 = rng_f01(&r) * 6.2831853f;
    float p2 = rng_f01(&r) * 6.2831853f;
    float d1 = 0.004f + rng_f01(&r) * 0.010f;
    float d2 = 0.004f + rng_f01(&r) * 0.010f;

    const int N = 900;
    float span = 26.0f;
    Vector2 prev = { 0 };
    for (int i = 0; i < N; i++) {
        float u = (float)i / (float)N;
        float tt = u * span + t * 0.35f;
        float damp1 = expf(-d1 * tt * 12.0f);
        float damp2 = expf(-d2 * tt * 12.0f);
        float x = SCREEN_W * 0.5f + 260.0f * damp1 * sinf(f1 * tt + p1)
                                  + 130.0f * damp2 * sinf(f3 * tt * 0.5f);
        float y = SCREEN_H * 0.5f + 200.0f * damp1 * sinf(f2 * tt + p2)
                                  + 100.0f * damp2 * cosf(f4 * tt * 0.5f);
        Vector2 cur = { x, y };
        if (i > 0) {
            Color c = cmix(s_pal.main, s_pal.accent, u);
            c.a = (unsigned char)(g_clampf(alpha * (0.15f + 0.85f * (1.0f - u)), 0.0f, 1.0f) * 255.0f);
            DrawLineEx(prev, cur, 2.0f, c);
        }
        prev = cur;
    }
}

/* ---- primitives ----------------------------------------------------- */
void ring(float cx, float cy, float r, float thick, Color c)
{
    if (r <= 0.5f) return;
    DrawRing((Vector2){cx, cy}, r - thick * 0.5f, r + thick * 0.5f, 0.0f, 360.0f, 48, c);
}

void draw_bar_meter(float x, float y, float w, float h, float v, Color fg, Color bg)
{
    DrawRectangle((int)x, (int)y, (int)w, (int)h, bg);
    DrawRectangle((int)x, (int)y, (int)(w * g_clampf(v, 0.0f, 1.0f)), (int)h, fg);
}
