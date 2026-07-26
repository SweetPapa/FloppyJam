/* fx.c — §8, "Light Stadium Over the Void".
 *
 * A slab of smoked glass suspended in darkness; everything that matters is
 * made of light. The order of business in vb_fx_draw is the whole readability
 * argument: crowd, then glass, then markings, then bars, then trails, then the
 * ball, then the detonations. The gameplay layer is drawn last and drawn over
 * colours that have already been dimmed to let it through.
 */
#include "fx.h"
#include "shaders.h"
#include "heat.h"
#include "rods.h"

#include <string.h>

/* ---- colour plumbing --------------------------------------------------- */

static Color col_of(VbRGB c, float a) {
    Color o;
    o.r = (unsigned char)(vb_clampf(c.r, 0, 1) * 255.0f);
    o.g = (unsigned char)(vb_clampf(c.g, 0, 1) * 255.0f);
    o.b = (unsigned char)(vb_clampf(c.b, 0, 1) * 255.0f);
    o.a = (unsigned char)(vb_clampf(a, 0, 1) * 255.0f);
    return o;
}
static VbRGB mix(VbRGB a, VbRGB b, float t) {
    return rgb(vb_lerpf(a.r, b.r, t), vb_lerpf(a.g, b.g, t), vb_lerpf(a.b, b.b, t));
}
static VbRGB scale(VbRGB a, float k) { return rgb(a.r * k, a.g * k, a.b * k); }

/* ---- view -------------------------------------------------------------- */

VbView vb_fx_view(int w, int h) {
    VbView v;
    /* the table wants a margin: the void is part of the picture, and the HUD
     * lives in it rather than on top of the glass */
    float sx = (float)w * 0.88f / (2.0f * VB_TABLE_HX);
    float sy = (float)h * 0.72f / (2.0f * VB_TABLE_HY);
    v.sc = sx < sy ? sx : sy;
    v.ox = (float)w * 0.5f;
    v.oy = (float)h * 0.54f;
    return v;
}

Vector2 vb_fx_screen(VbView v, V2 p) {
    Vector2 o;
    o.x = v.ox + p.x * v.sc;
    o.y = v.oy + p.y * v.sc;
    return o;
}

/* ---- lifecycle --------------------------------------------------------- */

static int load_shader(Shader *out, const char *fs330, const char *fs100) {
#if defined(PLATFORM_WEB)
    const char *src = fs100;
    (void)fs330;
#else
    const char *src = fs330;
    (void)fs100;
#endif
    Shader s = LoadShaderFromMemory(NULL, src);
    if (!IsShaderValid(s)) { UnloadShader(s); return 0; }
    *out = s;
    return 1;
}

void vb_fx_init(VbFx *fx, int graybox) {
    memset(fx, 0, sizeof *fx);
    fx->palette = 0;
    fx->graybox = graybox;
    vb_flash_reset(&fx->guard);

    if (graybox) {
        /* DoD 6: the proof that the fun predates the glow. No shaders, no
         * halos, no crowd — rectangles and a dot, exactly as it was tuned. */
        TraceLog(LOG_INFO, "VOLLEYBAR: gray-box mode, shaders disabled");
        return;
    }

    fx->has_table  = load_shader(&fx->sh_table,  VB_TABLE_FS_330,  VB_TABLE_FS_100);
    fx->has_ball   = load_shader(&fx->sh_ball,   VB_BALL_FS_330,   VB_BALL_FS_100);
    fx->has_crowd  = load_shader(&fx->sh_crowd,  VB_CROWD_FS_330,  VB_CROWD_FS_100);
    fx->has_ripple = load_shader(&fx->sh_ripple, VB_RIPPLE_FS_330, VB_RIPPLE_FS_100);
    fx->shaders_ok = fx->has_table || fx->has_ball || fx->has_crowd;
    if (!fx->shaders_ok)
        TraceLog(LOG_WARNING, "VOLLEYBAR: no shader compiled; drawing plain");
}

void vb_fx_shutdown(VbFx *fx) {
    if (fx->has_table)  UnloadShader(fx->sh_table);
    if (fx->has_ball)   UnloadShader(fx->sh_ball);
    if (fx->has_crowd)  UnloadShader(fx->sh_crowd);
    if (fx->has_ripple) UnloadShader(fx->sh_ripple);
    if (fx->tw) UnloadRenderTexture(fx->target);
    memset(fx, 0, sizeof *fx);
}

void vb_fx_settings(VbFx *fx, int palette, int reduce_motion) {
    fx->palette = vb_clampi(palette, 0, VB_NPALETTES - 1);
    fx->reduce_motion = reduce_motion;
    fx->guard.reduce_motion = reduce_motion;
}

/* ---- particles --------------------------------------------------------- */

static void spawn(VbFx *fx, V2 p, V2 v, float life, float size, VbRGB c, int kind) {
    if (fx->npart >= VB_MAXPART) return;
    VbParticle *q = &fx->part[fx->npart++];
    q->p = p; q->v = v;
    q->life = q->max = life;
    q->size = size;
    q->col = c;
    q->kind = kind;
}

static void burst(VbFx *fx, V2 at, int n, float speed, float life, VbRGB c) {
    static unsigned seed = 991u;
    for (int i = 0; i < n; i++) {
        float a = vb_randf(&seed) * VB_TAU;
        float s = speed * (0.35f + 0.65f * vb_randf(&seed));
        spawn(fx, at, v2(cosf(a) * s, sinf(a) * s),
              life * (0.6f + 0.7f * vb_randf(&seed)), 0.006f, c, 0);
    }
}

static void wave(VbFx *fx, V2 at, float dur, float amp, VbRGB c, int big) {
    for (int i = 0; i < VB_MAXWAVE; i++) {
        if (fx->wave[i].t < fx->wave[i].dur) continue;
        fx->wave[i].at = at; fx->wave[i].t = 0.0f;
        fx->wave[i].dur = dur; fx->wave[i].amp = amp;
        fx->wave[i].col = c; fx->wave[i].big = big;
        return;
    }
}

void vb_fx_snapshot(VbFx *fx, const VbSim *s) {
    for (int i = 0; i < VB_MAXBALLS; i++) fx->prev_p[i] = s->balls[i].p;
    for (int i = 0; i < VB_NRODS; i++) fx->prev_off[i] = s->rods[i].off;
    fx->have_prev = 1;
}

void vb_fx_events(VbFx *fx, const VbSim *s) {
    /* Watch the ladder rather than the events: a step is a step whichever
     * verb caused it, and the HUD wants to know the moment it happens. */
    int top = 0;
    for (int i = 0; i < s->nballs; i++)
        if (s->balls[i].alive && s->balls[i].heat > top) top = s->balls[i].heat;
    if (top > fx->heat_seen) fx->heat_pulse = 1.0f;
    fx->heat_seen = top;

    if (fx->graybox) {
        /* gray-box still shakes a little on a goal, and nothing else */
        for (int i = 0; i < s->nev; i++)
            if (s->ev[i].type == VB_EV_GOAL) fx->slowmo = 0.15f;
        return;
    }
    for (int i = 0; i < s->nev; i++) {
        const VbEvent *e = &s->ev[i];
        int heat = (e->b >= 0 && e->b < VB_MAXBALLS) ? s->balls[e->b].heat : 0;
        VbRGB hot = vb_ball_color(heat);
        switch (e->type) {
            case VB_EV_HIT:
                if (e->i == VB_STRIKE_IDLE) {
                    burst(fx, e->at, 4, 0.25f, 0.18f, scale(hot, 0.7f));
                } else {
                    burst(fx, e->at, 10 + heat, 0.55f + 0.06f * (float)heat,
                          0.30f, hot);
                    wave(fx, e->at, 0.26f, 0.35f, hot, 0);
                    fx->shake = vb_maxf(fx->shake, 0.10f + 0.02f * (float)heat);
                    /* a charged flick is the one you leaned on; it lands like it */
                    if (e->i == VB_STRIKE_CHARGED)
                        fx->hitstop = vb_maxf(fx->hitstop, 0.040f);
                }
                break;
            case VB_EV_SNAP:
                burst(fx, e->at, 26, 1.15f, 0.34f, hot);
                wave(fx, e->at, 0.32f, 0.7f, hot, 0);
                fx->shake = vb_maxf(fx->shake, 0.30f);
                /* the snap should feel illegal (§14.4). Two frames of nothing
                 * moving is most of why it does. */
                fx->hitstop = vb_maxf(fx->hitstop, 0.055f);
                break;
            case VB_EV_CHIP:
                burst(fx, e->at, 8, 0.35f, 0.4f, hot);
                break;
            case VB_EV_WALL:
                burst(fx, e->at, 5, 0.30f, 0.20f, scale(hot, 0.85f));
                break;
            case VB_EV_PIN:
                wave(fx, e->at, 0.4f, 0.25f, hot, 0);
                break;
            case VB_EV_SAVE:
                /* the crowd gasps: the bokeh field dips, then comes back */
                fx->gasp = 1.0f;
                burst(fx, e->at, 14, 0.6f, 0.3f, rgb(1, 1, 1));
                break;
            case VB_EV_WHIFF:
                burst(fx, e->at, 6, 0.28f, 0.35f, rgb(0.8f, 0.8f, 0.9f));
                break;
            case VB_EV_MERCY:
                wave(fx, e->at, 0.16f, 0.5f, hot, 0);
                break;
            case VB_EV_GOAL: {
                /* detonation: shockwave through the glass, the scorer's colour
                 * over the void, and 0.4 s of slow motion (§8) */
                VbRGB sc = vb_side_color(fx->palette, e->a);
                int scorcher = vb_heat_scorcher(e->i);
                burst(fx, e->at, scorcher ? 120 : 70, scorcher ? 2.0f : 1.4f,
                      0.9f, mix(sc, rgb(1, 1, 1), 0.4f));
                wave(fx, e->at, scorcher ? 1.0f : 0.75f, scorcher ? 1.6f : 1.0f,
                     sc, 1);
                fx->shake = scorcher ? 1.0f : 0.65f;
                fx->hitstop = scorcher ? 0.11f : 0.075f;
                fx->slowmo = 0.4f;
                fx->flood = 1.0f;
                fx->flood_side = e->a;
                if (scorcher) fx->crack_at = e->at;
                break;
            }
            case VB_EV_STALL:
                wave(fx, v2zero(), 0.5f, 0.4f, rgb(1.0f, 0.85f, 0.3f), 0);
                break;
            default: break;
        }
    }
}

void vb_fx_update(VbFx *fx, float dt) {
    fx->time += dt;
    for (int i = 0; i < fx->npart; ) {
        VbParticle *q = &fx->part[i];
        q->life -= dt;
        if (q->life <= 0.0f) { fx->part[i] = fx->part[--fx->npart]; continue; }
        q->p = v2add(q->p, v2mul(q->v, dt));
        q->v = v2mul(q->v, 1.0f - 3.2f * dt);
        i++;
    }
    for (int i = 0; i < VB_MAXWAVE; i++)
        if (fx->wave[i].t < fx->wave[i].dur) fx->wave[i].t += dt;

    fx->shake = vb_maxf(0.0f, fx->shake - dt * 2.6f);
    fx->slowmo = vb_maxf(0.0f, fx->slowmo - dt);
    /* Hitstop burns down on real time, never on scaled time, or a freeze
     * would hold itself open forever. */
    fx->hitstop = vb_maxf(0.0f, fx->hitstop - dt);
    fx->gasp = vb_maxf(0.0f, fx->gasp - dt * 1.6f);
    fx->flood = vb_maxf(0.0f, fx->flood - dt * 0.9f);
    fx->heat_pulse = vb_maxf(0.0f, fx->heat_pulse - dt * 2.2f);
    fx->shake_t += dt * 43.0f;
}

float vb_fx_timescale(const VbFx *fx) {
    /* reduce-motion kills the slow-mo and the hitstop along with the shake,
     * and keeps every bit of the colour language (§8 Comfort) */
    if (fx->reduce_motion) return 1.0f;
    if (fx->hitstop > 0.0f) return 0.0f;
    return fx->slowmo > 0.0f ? 0.4f : 1.0f;
}

/* ---- primitives -------------------------------------------------------- */

static void trail_push(VbFx *fx, int b, V2 p) {
    int h = fx->trail_head[b];
    fx->trail[b][h] = p;
    fx->trail_head[b] = (h + 1) % VB_TRAIL;
    if (fx->trail_n[b] < VB_TRAIL) fx->trail_n[b]++;
}

static void draw_glow_circle(Vector2 c, float r, VbRGB col, float a, int layers) {
    for (int i = layers; i >= 1; i--) {
        float k = (float)i / (float)layers;
        DrawCircleV(c, r * (0.55f + k * 1.9f), col_of(col, a * 0.20f * (1.0f - k) + 0.02f));
    }
}

/* the bars: translucent slabs of light, brighter for the side that is winning
 * and flaring on contact */
static void draw_rods(VbFx *fx, const VbSim *s, VbView v, float alpha,
                      const int *score, VbRGB under) {
    for (int i = 0; i < VB_NRODS; i++) {
        const VbRod *r = &s->rods[i];
        float off = fx->have_prev ? vb_lerpf(fx->prev_off[i], r->off, alpha) : r->off;
        VbRGB base = vb_side_color(fx->palette, r->side);
        float lit = 0.55f + 0.05f * (float)(score ? score[r->side] : 0);
        if (r->flash > 0) lit += 0.45f * (float)r->flash / (float)VB_TICKS(0.18f);
        if (r->state == VB_ROD_CHARGING)
            lit += 0.30f * vb_clampf((float)r->charge / (float)VB_CHARGE_MAX, 0, 1);
        if (r->state == VB_ROD_WINDUP) lit += 0.25f;
        lit = vb_clampf(lit, 0.0f, 1.6f);

        /* the bar itself, faint: it is the paddles that matter */
        Vector2 a = vb_fx_screen(v, v2(r->x, -VB_TABLE_HY));
        Vector2 b = vb_fx_screen(v, v2(r->x,  VB_TABLE_HY));
        if (!fx->graybox)
            DrawLineEx(a, b, 1.5f, col_of(base, 0.13f));

        int active = vb_rod_is_active(s, i);
        for (int k = 0; k < r->n; k++) {
            float y = off + ((float)k - (float)(r->n - 1) * 0.5f) * r->spacing;
            Vector2 tl = vb_fx_screen(v, v2(r->x - VB_PAD_HALF_X, y - r->half));
            float w = 2.0f * VB_PAD_HALF_X * v.sc;
            float h = 2.0f * r->half * v.sc;
            Rectangle rc = { tl.x, tl.y, w, h };

            if (fx->graybox) {
                DrawRectangleRec(rc, col_of(base, 0.9f));
                DrawRectangleLinesEx(rc, 1.0f, col_of(rgb(1, 1, 1), 0.6f));
                continue;
            }
            /* A soft shadow cast down onto the glass. It costs one rectangle
             * and it is most of what makes the bars read as objects floating
             * a few millimetres above a slab rather than as decals painted on
             * it. Offset away from the table centre, so both halves look lit
             * from the same place. */
            {
                float dir = (r->x < 0.0f) ? -1.0f : 1.0f;
                DrawRectangleRounded((Rectangle){ rc.x + dir * 4.0f, rc.y + 5.0f,
                                                  rc.width, rc.height },
                                     0.55f, 6, (Color){ 0, 0, 0, 90 });
            }

            VbRGB c = scale(base, lit);
            c = vb_spectacle_cap(c, under);
            /* a whiffed swing glows: legible commitment, like a fighting game */
            if (r->whiff > 0) {
                float k2 = (float)r->whiff / (float)VB_TICKS(0.28f);
                DrawRectangleRounded((Rectangle){ rc.x - 6, rc.y - 6, rc.width + 12, rc.height + 12 },
                                     0.5f, 6, col_of(rgb(1.0f, 0.9f, 0.85f), 0.20f * k2));
            }
            DrawRectangleRounded((Rectangle){ rc.x - 3, rc.y - 3, rc.width + 6, rc.height + 6 },
                                 0.6f, 6, col_of(c, 0.18f));
            DrawRectangleRounded(rc, 0.55f, 6, col_of(c, 0.90f));
            /* the bar under the stick carries a bright cap so GRIP players can
             * always find it without hunting */
            if (active)
                DrawRectangleRounded((Rectangle){ rc.x, rc.y, rc.width, 3.0f },
                                     0.5f, 4, col_of(rgb(1, 1, 1), 0.85f));
        }
    }
}

static void draw_table(VbFx *fx, const VbSim *s, VbView v, VbRGB table, VbRGB line,
                       VbRGB ball_col, int cracked) {
    Vector2 tl = vb_fx_screen(v, v2(-VB_TABLE_HX, -VB_TABLE_HY));
    Vector2 br = vb_fx_screen(v, v2( VB_TABLE_HX,  VB_TABLE_HY));
    Rectangle rc = { tl.x, tl.y, br.x - tl.x, br.y - tl.y };

    if (fx->graybox) {
        DrawRectangleRec(rc, col_of(rgb(0.10f, 0.10f, 0.12f), 1.0f));
        DrawRectangleLinesEx(rc, 2.0f, col_of(rgb(0.5f, 0.5f, 0.55f), 1.0f));
        DrawLineEx((Vector2){ (tl.x + br.x) * 0.5f, tl.y },
                   (Vector2){ (tl.x + br.x) * 0.5f, br.y }, 1.0f,
                   col_of(rgb(0.4f, 0.4f, 0.45f), 1.0f));
        return;
    }

    if (fx->has_table) {
        Shader sh = fx->sh_table;
        float res[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
        Vector2 bp = vb_fx_screen(v, s->balls[0].p);
        float ball[2] = { bp.x, res[1] - bp.y };     /* gl_FragCoord is y-up */
        float rect[4] = { rc.x, res[1] - (rc.y + rc.height), rc.width, rc.height };
        float base[3] = { table.r, table.g, table.b };
        float ln[3]   = { line.r, line.g, line.b };
        float bc[3]   = { ball_col.r, ball_col.g, ball_col.b };
        float heat    = vb_heat_temp(s->balls[0].heat);
        float crack   = cracked ? 1.0f : 0.0f;
        Vector2 cs    = vb_fx_screen(v, fx->crack_at);
        float crack_at[2] = { cs.x, res[1] - cs.y };
        SetShaderValue(sh, GetShaderLocation(sh, "uRes"),   res,  SHADER_UNIFORM_VEC2);
        SetShaderValue(sh, GetShaderLocation(sh, "uBall"),  ball, SHADER_UNIFORM_VEC2);
        SetShaderValue(sh, GetShaderLocation(sh, "uRect"),  rect, SHADER_UNIFORM_VEC4);
        SetShaderValue(sh, GetShaderLocation(sh, "uBase"),  base, SHADER_UNIFORM_VEC3);
        SetShaderValue(sh, GetShaderLocation(sh, "uLine"),  ln,   SHADER_UNIFORM_VEC3);
        SetShaderValue(sh, GetShaderLocation(sh, "uBallCol"), bc, SHADER_UNIFORM_VEC3);
        SetShaderValue(sh, GetShaderLocation(sh, "uHeat"),  &heat, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, GetShaderLocation(sh, "uCrack"), &crack, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, GetShaderLocation(sh, "uCrackAt"), crack_at, SHADER_UNIFORM_VEC2);
        BeginShaderMode(sh);
        DrawRectangleRec(rc, WHITE);
        EndShaderMode();
    } else {
        DrawRectangleRec(rc, col_of(table, 1.0f));
        DrawLineEx((Vector2){ (tl.x + br.x) * 0.5f, tl.y },
                   (Vector2){ (tl.x + br.x) * 0.5f, br.y }, 1.5f, col_of(line, 0.5f));
    }
    /* paper-thin fresnel edge */
    DrawRectangleLinesEx(rc, 1.5f, col_of(line, 0.8f));
}

static void draw_goals(VbFx *fx, const VbSim *s, VbView v, VbRGB under) {
    for (int side = 0; side < VB_NSIDES; side++) {
        float x = side ? VB_TABLE_HX : -VB_TABLE_HX;
        VbRGB c = vb_side_color(fx->palette, side);
        if (!fx->graybox) c = vb_spectacle_cap(c, under);
        Vector2 a = vb_fx_screen(v, v2(x, s->goal_y[side] - s->goal_half[side]));
        Vector2 b = vb_fx_screen(v, v2(x, s->goal_y[side] + s->goal_half[side]));
        DrawLineEx(a, b, fx->graybox ? 4.0f : 6.0f, col_of(c, 0.95f));
        if (!fx->graybox) DrawLineEx(a, b, 14.0f, col_of(c, 0.16f));
    }
}

static void draw_ball(VbFx *fx, const VbSim *s, VbView v, int bi, float alpha,
                      VbRGB under) {
    const VbBall *b = &s->balls[bi];
    if (!b->alive) return;
    V2 p = fx->have_prev ? v2lerp(fx->prev_p[bi], b->p, alpha) : b->p;
    VbRGB c = vb_ball_color(b->heat);
    float r = b->radius * v.sc;

    /* a chip in flight throws a shadow and shows both players its landing
     * ring — a through-pass you cannot read is not a skill, it is a lottery */
    if (b->z > 0.0f) {
        Vector2 sh = vb_fx_screen(v, p);
        DrawCircleV(sh, r * 0.85f, col_of(rgb(0, 0, 0), 0.45f));
        float t_fall = b->vz > 0 ? (b->vz / VB_CHIP_GRAV) : 0.0f;
        float t_tot = t_fall + sqrtf(vb_maxf(2.0f * (b->z) / VB_CHIP_GRAV, 0.0f));
        V2 land = v2add(p, v2mul(b->v, t_tot));
        land.y = vb_clampf(land.y, -VB_TABLE_HY, VB_TABLE_HY);
        land.x = vb_clampf(land.x, -VB_TABLE_HX, VB_TABLE_HX);
        Vector2 lp = vb_fx_screen(v, land);
        DrawCircleLinesV(lp, r * 1.9f, col_of(rgb(1.0f, 0.95f, 0.6f), 0.85f));
        DrawCircleLinesV(lp, r * 1.9f + 2.0f, col_of(rgb(1.0f, 0.95f, 0.6f), 0.35f));
    }

    float lift = b->z * v.sc * 3.0f;
    Vector2 sp = vb_fx_screen(v, p);
    sp.y -= lift;

    if (fx->graybox) {
        DrawCircleV(sp, r, col_of(rgb(1, 1, 1), 1.0f));
        return;
    }

    /* the persistent ribbon trail, lengthening with heat */
    int n = fx->trail_n[bi];
    int keep = 6 + (int)(vb_heat_temp(b->heat) * (float)(VB_TRAIL - 8));
    for (int i = 0; i < n && i < keep; i++) {
        int idx = (fx->trail_head[bi] - 1 - i + VB_TRAIL * 2) % VB_TRAIL;
        float k = 1.0f - (float)i / (float)keep;
        Vector2 tp = vb_fx_screen(v, fx->trail[bi][idx]);
        DrawCircleV(tp, r * (0.30f + 0.62f * k), col_of(c, 0.30f * k * k));
    }

    /* A streak along the direction of travel. The trail says where the ball
     * HAS been; this says how fast it is going right now, which is the thing
     * you are actually trying to read when it is coming at you. Reduce-motion
     * keeps it — it is speed information, not movement for its own sake. */
    {
        float sp_len = v2len(b->v);
        float k = vb_clampf((sp_len - VB_V0) / (vb_heat_speed(VB_HEAT_MAX) - VB_V0),
                            0.0f, 1.0f);
        if (k > 0.02f && b->mercy == 0) {
            V2 back = v2mul(v2norm(b->v), -r * (1.1f + 3.4f * k) / v.sc);
            Vector2 tail = vb_fx_screen(v, v2add(p, back));
            tail.y -= lift;
            BeginBlendMode(BLEND_ADDITIVE);
            DrawLineEx(tail, sp, r * 1.5f, col_of(c, 0.16f + 0.20f * k));
            DrawLineEx(tail, sp, r * 0.7f, col_of(c, 0.22f + 0.28f * k));
            EndBlendMode();
        }
    }

    if (fx->has_ball) {
        Shader sh = fx->sh_ball;
        float res[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
        float bp[2] = { sp.x, res[1] - sp.y };
        float cc[3] = { c.r, c.g, c.b };
        float heat = vb_heat_temp(b->heat);
        float pull = fx->reduce_motion ? 0.0f : 1.0f;
        float rad = r;
        SetShaderValue(sh, GetShaderLocation(sh, "uRes"), res, SHADER_UNIFORM_VEC2);
        SetShaderValue(sh, GetShaderLocation(sh, "uBall"), bp, SHADER_UNIFORM_VEC2);
        SetShaderValue(sh, GetShaderLocation(sh, "uRadius"), &rad, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, GetShaderLocation(sh, "uCol"), cc, SHADER_UNIFORM_VEC3);
        SetShaderValue(sh, GetShaderLocation(sh, "uHeat"), &heat, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, GetShaderLocation(sh, "uPull"), &pull, SHADER_UNIFORM_FLOAT);
        BeginBlendMode(BLEND_ADDITIVE);
        BeginShaderMode(sh);
        DrawRectangle((int)(sp.x - r * 9), (int)(sp.y - r * 9),
                      (int)(r * 18), (int)(r * 18), WHITE);
        EndShaderMode();
        EndBlendMode();
    } else {
        BeginBlendMode(BLEND_ADDITIVE);
        draw_glow_circle(sp, r, c, 0.9f, 5);
        EndBlendMode();
        DrawCircleV(sp, r, col_of(c, 1.0f));
    }

    /* Pillar 4, the backstop: whatever ended up under the ball, the ring
     * separates it. This is drawn last and is never dimmed by anything. */
    VbRGB ring = vb_ball_outline(under);
    DrawCircleLinesV(sp, r + 1.0f, col_of(ring, 0.95f));
    DrawCircleV(sp, r * 0.42f, col_of(rgb(1, 1, 1), 0.85f));

    /* the mercy beat is visible: a shimmer while the ball holds its breath */
    if (b->mercy > 0) {
        float k = (float)b->mercy / (float)VB_MERCY_TICKS;
        DrawCircleLinesV(sp, r * (1.6f + 2.2f * (1.0f - k)),
                         col_of(rgb(1, 1, 1), 0.55f * k));
    }

    /* The protected possession after a goal (§5.5) was a rule you could only
     * find out about by losing the ball to it. Now it is a ring that closes. */
    if (s->serve_prot > 0) {
        float k = (float)s->serve_prot / (float)VB_SERVE_PROT;
        VbRGB sc = vb_side_color(fx->palette, s->serve_side);
        DrawCircleLinesV(sp, r * (1.5f + 2.6f * k), col_of(sc, 0.30f + 0.45f * k));
    }
}

/* the aim wedge a pinned ball is shown, to both players */
static void draw_pin_wedge(VbFx *fx, const VbSim *s, VbView v) {
    for (int i = 0; i < VB_NRODS; i++) {
        const VbRod *r = &s->rods[i];
        if (r->pin_ball < 0) continue;
        const VbBall *b = &s->balls[r->pin_ball];
        Vector2 o = vb_fx_screen(v, b->p);
        VbRGB c = vb_side_color(fx->palette, r->side);
        /* drawn the same way rods.c fires it, or the wedge would be a lie */
        float a0 = atan2f(0.0f, r->pin_face) + r->pin_aim * r->pin_face;
        float len = 0.34f * v.sc;
        for (int k = -1; k <= 1; k++) {
            float a = a0 + (float)k * 6.0f * VB_DEG;
            Vector2 e = { o.x + cosf(a) * len, o.y + sinf(a) * len };
            DrawLineEx(o, e, k == 0 ? 2.5f : 1.0f, col_of(c, k == 0 ? 0.8f : 0.35f));
        }
        /* the shot clock, drawn as the wedge draining */
        float t = 1.0f - (float)r->pin_ticks / (float)VB_PIN_CLOCK;
        DrawCircleLinesV(o, 10.0f + 8.0f * t, col_of(c, 0.5f + 0.5f * t));
    }
}

static void draw_particles(VbFx *fx, VbView v) {
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < fx->npart; i++) {
        const VbParticle *q = &fx->part[i];
        float k = q->life / q->max;
        Vector2 p = vb_fx_screen(v, q->p);
        DrawCircleV(p, q->size * v.sc * (0.4f + k), col_of(q->col, k * 0.9f));
    }
    for (int i = 0; i < VB_MAXWAVE; i++) {
        const VbWave *w = &fx->wave[i];
        if (w->t >= w->dur) continue;
        float k = w->t / w->dur;
        Vector2 p = vb_fx_screen(v, w->at);
        float rad = (w->big ? 1.6f : 0.30f) * v.sc * k;
        DrawCircleLinesV(p, rad, col_of(w->col, (1.0f - k) * w->amp));
        DrawCircleLinesV(p, rad * 0.94f, col_of(w->col, (1.0f - k) * w->amp * 0.5f));
    }
    EndBlendMode();
}

static void draw_crowd(VbFx *fx, VbView v, int heat) {
    if (fx->graybox || !fx->has_crowd) return;
    Shader sh = fx->sh_crowd;
    float res[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    Vector2 tl = vb_fx_screen(v, v2(-VB_TABLE_HX, -VB_TABLE_HY));
    Vector2 br = vb_fx_screen(v, v2( VB_TABLE_HX,  VB_TABLE_HY));
    float rect[4] = { tl.x, res[1] - br.y, br.x - tl.x, br.y - tl.y };
    VbRGB c = vb_crowd_color(fx->palette, heat);
    c = vb_spectacle_cap(c, vb_ball_color(heat));
    float cc[3] = { c.r, c.g, c.b };
    float t = fx->time, h = vb_heat_temp(heat), g = fx->gasp;
    SetShaderValue(sh, GetShaderLocation(sh, "uRes"), res, SHADER_UNIFORM_VEC2);
    SetShaderValue(sh, GetShaderLocation(sh, "uRect"), rect, SHADER_UNIFORM_VEC4);
    SetShaderValue(sh, GetShaderLocation(sh, "uCol"), cc, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "uTime"), &t, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, GetShaderLocation(sh, "uHeat"), &h, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, GetShaderLocation(sh, "uGasp"), &g, SHADER_UNIFORM_FLOAT);
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(sh);
    DrawRectangle(0, 0, (int)res[0], (int)res[1], WHITE);
    EndShaderMode();
    EndBlendMode();
}

/* A vignette, drawn last but made of darkness only — it can lower the
 * luminance under the gameplay layer, never raise it, so Pillar 4's guarantee
 * survives it untouched. It does the job the void is supposed to do: pull the
 * eye to the middle of the glass and let the table sit in something. */
static void draw_vignette(VbFx *fx, int heat) {
    if (fx->graybox) return;
    int w = GetScreenWidth(), h = GetScreenHeight();
    /* it tightens as the rally heats up, which is the only way the frame
     * itself gets to join in */
    int band = (int)((float)h * (0.20f + 0.05f * vb_heat_temp(heat)));
    unsigned char a = (unsigned char)(120 + 55.0f * vb_heat_temp(heat));
    Color dark = { 0, 0, 0, a }, clear = { 0, 0, 0, 0 };
    DrawRectangleGradientV(0, 0, w, band, dark, clear);
    DrawRectangleGradientV(0, h - band, w, band, clear, dark);
    int side = (int)((float)w * 0.16f);
    DrawRectangleGradientH(0, 0, side, h, dark, clear);
    DrawRectangleGradientH(w - side, 0, side, h, clear, dark);
}

/* ---- the frame --------------------------------------------------------- */

static void draw_world(VbFx *fx, const VbSim *s, float alpha, const int *score,
                       int cracked, int dim) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    VbView v = vb_fx_view(w, h);

    /* screen shake, and reduce-motion refuses it outright */
    if (!fx->reduce_motion && fx->shake > 0.001f) {
        v.ox += sinf(fx->shake_t * 1.7f) * fx->shake * 9.0f;
        v.oy += cosf(fx->shake_t * 2.3f) * fx->shake * 7.0f;
    }

    int heat = 0;
    for (int i = 0; i < s->nballs; i++)
        if (s->balls[i].alive && s->balls[i].heat > heat) heat = s->balls[i].heat;
    VbRGB ballc = vb_ball_color(heat);

    /* every spectacle colour goes through the law before it is used */
    VbRGB voidc  = vb_void_color(fx->palette, heat);
    VbRGB tablec = vb_spectacle_cap(vb_table_color(fx->palette), ballc);
    VbRGB linec  = vb_spectacle_cap(vb_line_color(fx->palette), ballc);
    if (fx->flood > 0.0f && !fx->graybox) {
        VbRGB f = vb_side_color(fx->palette, fx->flood_side);
        voidc = mix(voidc, scale(f, 0.30f), fx->flood * 0.8f);
    }
    /* and the whole background's brightness is rate-limited for comfort */
    float want = vb_lum(voidc);
    float got = vb_flash_limit(&fx->guard, want, GetFrameTime());
    if (want > 0.001f) voidc = scale(voidc, got / want);
    if (dim) { voidc = scale(voidc, 0.6f); tablec = scale(tablec, 0.7f); }

    ClearBackground(fx->graybox ? col_of(rgb(0.05f, 0.05f, 0.06f), 1)
                                : col_of(voidc, 1));

    draw_crowd(fx, v, heat);
    draw_vignette(fx, heat);
    draw_table(fx, s, v, tablec, linec, ballc, cracked);
    draw_goals(fx, s, v, tablec);
    draw_rods(fx, s, v, alpha, score, tablec);
    draw_pin_wedge(fx, s, v);
    for (int i = 0; i < s->nballs; i++) draw_ball(fx, s, v, i, alpha, tablec);
    draw_particles(fx, v);
}

/* The attract table and the replay behind the stat card: the same world, with
 * no score to brighten the bars and no goal ripple, because neither of those
 * screens is a live match. */
void vb_fx_draw_sim(VbFx *fx, const VbSim *s, float alpha, int dim) {
    for (int i = 0; i < s->nballs; i++)
        if (s->balls[i].alive) trail_push(fx, i, s->balls[i].p);
    draw_world(fx, s, alpha, NULL, 0, dim);
}

void vb_fx_draw(VbFx *fx, const VbMatch *m, float alpha) {
    const VbSim *s = &m->sim;
    int w = GetScreenWidth(), h = GetScreenHeight();

    /* keep the ribbon fed from the interpolated positions */
    for (int i = 0; i < s->nballs; i++)
        if (s->balls[i].alive) trail_push(fx, i, s->balls[i].p);

    int use_target = (fx->has_ripple && !fx->graybox);
    if (use_target && (fx->tw != w || fx->th != h)) {
        if (fx->tw) UnloadRenderTexture(fx->target);
        fx->target = LoadRenderTexture(w, h);
        fx->tw = w; fx->th = h;
    }

    if (use_target) {
        BeginTextureMode(fx->target);
        draw_world(fx, s, alpha, m->score, m->scorcher_crack, 0);
        EndTextureMode();

        Shader sh = fx->sh_ripple;
        float res[2] = { (float)w, (float)h };
        VbView v = vb_fx_view(w, h);
        float wallx = (m->last_goal_side == 0) ? VB_TABLE_HX : -VB_TABLE_HX;
        Vector2 at = vb_fx_screen(v, v2(wallx, s->goal_y[1 - m->last_goal_side]));
        float aat[2] = { at.x, res[1] - at.y };
        float t = 1.0f - fx->flood;
        float amp = fx->reduce_motion ? 0.0f : 1.0f;
        VbRGB f = vb_side_color(fx->palette, fx->flood_side);
        float ff[3] = { f.r, f.g, f.b };
        SetShaderValue(sh, GetShaderLocation(sh, "uRes"), res, SHADER_UNIFORM_VEC2);
        SetShaderValue(sh, GetShaderLocation(sh, "uAt"), aat, SHADER_UNIFORM_VEC2);
        SetShaderValue(sh, GetShaderLocation(sh, "uT"), &t, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, GetShaderLocation(sh, "uAmp"), &amp, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, GetShaderLocation(sh, "uFlood"), ff, SHADER_UNIFORM_VEC3);

        Rectangle src = { 0, 0, (float)w, -(float)h };
        if (fx->flood > 0.0f) BeginShaderMode(sh);
        DrawTextureRec(fx->target.texture, src, (Vector2){ 0, 0 }, WHITE);
        if (fx->flood > 0.0f) EndShaderMode();
    } else {
        draw_world(fx, s, alpha, m->score, m->scorcher_crack, 0);
    }
}
