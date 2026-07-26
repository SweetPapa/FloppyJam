/* sim.c — Section 5, exactly.
 *
 * Fixed 1/240 s steps, semi-implicit Euler, swept collision. The ball at heat
 * 12 crosses several of its own radii per tick, so discrete overlap tests
 * would let it teleport through a goalie; every contact in here is solved as a
 * time of impact instead. tests/test_sim.c fires 20,000 balls at a rod wall at
 * maximum heat and asserts that not one of them gets through.
 *
 * Nothing in this file reads a clock, allocates, or knows what a pixel is.
 */
#include "sim.h"
#include "rods.h"
#include "heat.h"

/* The interleave (§2), left goal to right goal:
 *   P1-GK · P1-DEF · P2-ATK · P1-ATK · P2-DEF · P2-GK
 * Side 0 defends -x and attacks +x. Your attack rod living behind their
 * defence is the entire strategic engine of the game. */
static const struct { int side, role; float x; } LAYOUT[VB_NRODS] = {
    { 0, VB_GK,  -VB_ROD_GK_X  },
    { 0, VB_DEF, -VB_ROD_DEF_X },
    { 1, VB_ATK, -VB_ROD_ATK_X },
    { 0, VB_ATK,  VB_ROD_ATK_X },
    { 1, VB_DEF,  VB_ROD_DEF_X },
    { 1, VB_GK,   VB_ROD_GK_X  },
};

int vb_rod_of(int side, int role) {
    for (int i = 0; i < VB_NRODS; i++)
        if (LAYOUT[i].side == side && LAYOUT[i].role == role) return i;
    return -1;
}

float vb_pad_y(const VbRod *r, int i) {
    return r->off + ((float)i - (float)(r->n - 1) * 0.5f) * r->spacing;
}

void vb_pad_span(const VbRod *r, int i, float ball_r, float *lo, float *hi) {
    float c = vb_pad_y(r, i);
    float l = c - r->half, h = c + r->half;
    float w = 2.0f * ball_r;
    if (i == 0 && (l - (-VB_TABLE_HY)) < w)      l = -VB_TABLE_HY - 0.02f;
    if (i == r->n - 1 && (VB_TABLE_HY - h) < w)  h =  VB_TABLE_HY + 0.02f;
    *lo = l; *hi = h;
}

float vb_rod_open_lane(const VbRod *r) {
    /* Paddles are ordered bottom to top, so the gaps are just the spaces
     * between consecutive covers plus the two wall margins. */
    float best = vb_pad_y(r, 0) - r->half - (-VB_TABLE_HY);
    for (int i = 1; i < r->n; i++) {
        float g = (vb_pad_y(r, i) - r->half) - (vb_pad_y(r, i - 1) + r->half);
        if (g > best) best = g;
    }
    float top = VB_TABLE_HY - (vb_pad_y(r, r->n - 1) + r->half);
    if (top > best) best = top;
    return best;
}

int vb_rod_owner_slot(const VbSim *s, int rod) {
    int side = s->rods[rod].side;
    if (!s->cfg.doubles) return side * 2;
    /* Foosball doubles: one teammate owns GK+DEF, the other owns ATK — and
     * they can trade between points, which is the argument every pair of
     * doubles partners has ever had, made legal (§3.3). */
    int atk = (s->rods[rod].role == VB_ATK);
    if (s->cfg.swap[side]) atk = !atk;
    return side * 2 + (atk ? 1 : 0);
}

int vb_nearest_ball(const VbSim *s, int rod) {
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < s->nballs; i++) {
        if (!s->balls[i].alive) continue;
        float d = vb_absf(s->balls[i].p.x - s->rods[rod].x);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

void vb_ev(VbSim *s, int type, int a, int b, int i, float f, V2 at) {
    if (s->nev >= VB_MAXEV) return;
    VbEvent *e = &s->ev[s->nev++];
    e->type = type; e->a = a; e->b = b; e->i = i; e->f = f; e->at = at;
}

/* ---- setup ------------------------------------------------------------ */

static void ball_reset(VbBall *b) {
    b->p = v2zero(); b->v = v2zero();
    b->z = 0; b->vz = 0;
    b->curve = v2zero(); b->curve_ticks = 0;
    b->radius = VB_BALL_R;
    b->heat = 0; b->alive = 0;
    b->mercy = 0; b->mercy_v = v2zero();
    b->last_rod = -1; b->last_side = -1;
    b->chip_pass = -1; b->contacts = 0;
}

void vb_sim_init(VbSim *s, const VbSimCfg *cfg, unsigned seed) {
    for (size_t i = 0; i < sizeof(*s); i++) ((char *)s)[i] = 0;
    s->cfg = *cfg;
    s->rng = seed ? seed : 1u;
    s->tick = 0;
    s->nballs = 1;
    for (int i = 0; i < VB_MAXBALLS; i++) ball_reset(&s->balls[i]);

    for (int i = 0; i < VB_NRODS; i++) {
        VbRod *r = &s->rods[i];
        r->side = LAYOUT[i].side;
        r->role = LAYOUT[i].role;
        r->x    = LAYOUT[i].x;
        r->pin_ball = -1;
        r->state = VB_ROD_IDLE;
    }
    vb_rods_init(s);

    for (int t = 0; t < VB_NSIDES; t++) {
        float gs = s->cfg.tilt[t].goal_scale > 0 ? s->cfg.tilt[t].goal_scale : 1.0f;
        s->goal_half[t] = VB_GOAL_HALF * gs;
        s->goal_y[t] = 0.0f;
    }
    s->ctrl_side = -1;
    s->goal_side = -1;
    s->serve_side = 0;

    if (s->cfg.mutators & VB_MUT_BIG_BALL)
        for (int i = 0; i < VB_MAXBALLS; i++) s->balls[i].radius = VB_BALL_R * 1.75f;
    if (s->cfg.mutators & VB_MUT_PEA_BALL)
        for (int i = 0; i < VB_MAXBALLS; i++) s->balls[i].radius = VB_BALL_R * 0.55f;
}

void vb_sim_serve(VbSim *s, int side) {
    vb_rods_release_all(s);
    for (int i = 0; i < VB_MAXBALLS; i++) { ball_reset(&s->balls[i]); }
    if (s->cfg.mutators & VB_MUT_BIG_BALL) s->balls[0].radius = VB_BALL_R * 1.75f;
    if (s->cfg.mutators & VB_MUT_PEA_BALL) s->balls[0].radius = VB_BALL_R * 0.55f;

    int gk = vb_rod_of(side, VB_GK);
    float dir = (side == 0) ? 1.0f : -1.0f;
    VbBall *b = &s->balls[0];
    b->alive = 1;
    b->p = v2(s->rods[gk].x + dir * (VB_PAD_HALF_X + b->radius + 0.012f),
              vb_pad_y(&s->rods[gk], 0));
    b->v = v2(dir * VB_V0 * 0.45f, 0.0f);
    /* Sudden Heat starts the rally already halfway up the ladder. */
    b->heat = (s->cfg.mutators & VB_MUT_SUDDEN_HEAT) ? 6 : 0;
    b->last_side = side;
    s->nballs = 1;

    if (s->cfg.mutators & VB_MUT_MULTIBALL) {
        vb_sim_spawn(s, v2(0.0f,  0.24f), v2(-dir * VB_V0 * 0.5f,  0.10f));
        vb_sim_spawn(s, v2(0.0f, -0.24f), v2(-dir * VB_V0 * 0.5f, -0.10f));
    }

    s->serve_prot = VB_SERVE_PROT;
    s->serve_side = side;
    s->ctrl_side = side;
    s->ctrl_ticks = 0;
    s->goal_side = -1;
}

int vb_sim_spawn(VbSim *s, V2 p, V2 v) {
    for (int i = 0; i < VB_MAXBALLS; i++) {
        if (s->balls[i].alive) continue;
        ball_reset(&s->balls[i]);
        s->balls[i].alive = 1;
        s->balls[i].p = p;
        s->balls[i].v = v;
        if (s->cfg.mutators & VB_MUT_BIG_BALL) s->balls[i].radius = VB_BALL_R * 1.75f;
        if (s->cfg.mutators & VB_MUT_PEA_BALL) s->balls[i].radius = VB_BALL_R * 0.55f;
        if (i + 1 > s->nballs) s->nballs = i + 1;
        return i;
    }
    return -1;
}

void vb_sim_pop_center(VbSim *s) {
    for (int i = 0; i < s->nballs; i++) {
        VbBall *b = &s->balls[i];
        if (!b->alive) continue;
        b->heat = 0;
        b->curve = v2zero(); b->curve_ticks = 0;
        b->mercy = 0;
        b->z = 0; b->vz = 0; b->chip_pass = -1;
        float away = (b->p.x > 0) ? -1.0f : 1.0f;
        b->v = v2(away * VB_V0 * 0.8f, (b->p.y > 0 ? -1.0f : 1.0f) * VB_V0 * 0.25f);
    }
    vb_rods_release_all(s);
    s->ctrl_side = -1;
    s->ctrl_ticks = 0;
}

/* ---- swept collision -------------------------------------------------- */

/* Point vs static AABB over [0,tmax], in the box's frame. The box has already
 * been Minkowski-expanded by the ball radius, so the ball is a point. */
static int sweep_box(V2 p, V2 v, V2 c, V2 h, float tmax, float *tout, V2 *nout) {
    float d[2] = { p.x - c.x, p.y - c.y };
    float vv[2] = { v.x, v.y };
    float hh[2] = { h.x, h.y };

    if (vb_absf(d[0]) <= hh[0] && vb_absf(d[1]) <= hh[1]) {
        /* Already interpenetrating (a rod slid into the ball). Push out along
         * the shallowest axis — the rod wins, exactly like a real bar. */
        float px = hh[0] - vb_absf(d[0]), py = hh[1] - vb_absf(d[1]);
        *tout = 0.0f;
        *nout = (px < py) ? v2(d[0] < 0 ? -1.0f : 1.0f, 0.0f)
                          : v2(0.0f, d[1] < 0 ? -1.0f : 1.0f);
        return 1;
    }

    float t0 = 0.0f, t1 = tmax;
    int axis = -1;
    float sgn = 1.0f;
    for (int a = 0; a < 2; a++) {
        if (vb_absf(vv[a]) < 1e-7f) {
            if (d[a] < -hh[a] || d[a] > hh[a]) return 0;
            continue;
        }
        float inv = 1.0f / vv[a];
        float ta = (-hh[a] - d[a]) * inv;
        float tb = ( hh[a] - d[a]) * inv;
        float lo = vb_minf(ta, tb), hi = vb_maxf(ta, tb);
        if (lo > t0) { t0 = lo; axis = a; sgn = (vv[a] > 0) ? -1.0f : 1.0f; }
        if (hi < t1) t1 = hi;
        if (t0 > t1) return 0;
    }
    if (axis < 0 || t0 < 0.0f || t0 > tmax) return 0;
    *tout = t0;
    *nout = (axis == 0) ? v2(sgn, 0.0f) : v2(0.0f, sgn);
    return 1;
}

/* Was this ball on its way into that side's mouth? Used to call saves. */
static int goal_bound(const VbSim *s, const VbBall *b, int side) {
    float wall = (side == 0) ? -VB_TABLE_HX : VB_TABLE_HX;
    float dx = wall - b->p.x;
    if (dx * b->v.x <= 0.0f) return 0;
    if (vb_absf(b->v.x) < 1e-5f) return 0;
    float t = dx / b->v.x;
    float y = b->p.y + b->v.y * t;
    return vb_absf(y - s->goal_y[side]) < s->goal_half[side] + b->radius;
}

/* ---- per-ball integration --------------------------------------------- */

static void ball_step(VbSim *s, int bi) {
    VbBall *b = &s->balls[bi];
    if (!b->alive) return;

    /* The mercy beat (§4): the ball is struck, then holds its breath for
     * 120 ms of shimmer before it flies. A step-12 exchange stays reactable. */
    if (b->mercy > 0) {
        b->mercy--;
        if (b->mercy == 0) b->v = b->mercy_v;
        return;
    }

    /* english: Magnus-style lateral acceleration, decaying over 0.5 s */
    if (b->curve_ticks > 0) {
        float k = (float)b->curve_ticks / (float)VB_CURVE_TICKS;
        b->v = v2add(b->v, v2mul(b->curve, k * VB_DT));
        b->curve_ticks--;
        if (b->curve_ticks == 0) b->curve = v2zero();
    }

    /* the chip hop */
    if (b->z > 0.0f || b->vz != 0.0f) {
        b->vz -= VB_CHIP_GRAV * VB_DT;
        b->z  += b->vz * VB_DT;
        if (b->z <= 0.0f) {
            b->z = 0.0f; b->vz = 0.0f; b->chip_pass = -1;
            vb_ev(s, VB_EV_LAND, -1, bi, 0, 0.0f, b->p);
        }
    }

    /* §5.2: the ball never fully stops. No stalemates, no shoving matches
     * with a dead ball. The floor rides the heat, because a ball that has been
     * struck to step 10 and then clipped twice off idle bars should not be
     * dribbling — the meter says the rally is on fire, and the ball has to
     * agree with it. */
    float sp = v2len(b->v);
    float floor_sp = vb_maxf(VB_MIN_SPEED, VB_FLOOR_K * vb_heat_speed(b->heat));
    if (sp < floor_sp) {
        if (sp < 0.02f) {
            /* genuinely dead: drift it back toward the centre line */
            float toward = (b->p.x > 0.0f) ? -1.0f : 1.0f;
            b->v = v2(toward * 0.05f, b->p.y > 0.0f ? -0.02f : 0.02f);
            sp = v2len(b->v);
        }
        float want = vb_minf(floor_sp, sp + VB_NUDGE * VB_DT);
        b->v = v2mul(v2mul(b->v, 1.0f / sp), want);
    }

    /* Magnet Paddles (PARTY): a slight attract radius, nothing more. */
    if (s->cfg.mutators & VB_MUT_MAGNET) {
        for (int i = 0; i < VB_NRODS; i++) {
            const VbRod *r = &s->rods[i];
            if (vb_absf(r->x - b->p.x) > 0.10f) continue;
            for (int k = 0; k < r->n; k++) {
                V2 c = v2(r->x, vb_pad_y(r, k));
                V2 d = v2sub(c, b->p);
                float l = v2len(d);
                if (l > 0.001f && l < 0.11f)
                    b->v = v2add(b->v, v2mul(v2mul(d, 1.0f / l), 1.4f * VB_DT));
            }
        }
    }

    float rem = VB_DT;
    for (int iter = 0; iter < 6 && rem > 1e-7f; iter++) {
        float best = rem;
        int   kind = 0;            /* 0 none, 1 wall, 2 paddle, 3 goal      */
        int   hrod = -1, hpad = -1, gside = -1;
        V2    n = v2zero();
        float r = b->radius;

        /* Side walls — live, and banks are core play. A negative time of
         * impact means the ball is already past the plane (a rod can shove it
         * there): clamp to now rather than letting it leave the building. */
        if (b->v.y > 1e-7f) {
            float t = (VB_TABLE_HY - r - b->p.y) / b->v.y;
            if (t < 0.0f) t = 0.0f;
            if (t < best) { best = t; kind = 1; n = v2(0, -1); }
        } else if (b->v.y < -1e-7f) {
            float t = (-VB_TABLE_HY + r - b->p.y) / b->v.y;
            if (t < 0.0f) t = 0.0f;
            if (t < best) { best = t; kind = 1; n = v2(0, 1); }
        }

        /* end walls — a mouth in the middle of each */
        for (int e = 0; e < 2; e++) {
            float wall = e ? VB_TABLE_HX : -VB_TABLE_HX;
            float nx = e ? -1.0f : 1.0f;
            float vx = b->v.x;
            if ((e && vx <= 1e-7f) || (!e && vx >= -1e-7f)) continue;
            /* nx points INTO the table, so the ball's centre meets this end
             * at wall + nx*r — inside the wall, not beyond it. */
            float t = (wall + nx * r - b->p.x) / vx;
            if (t < 0.0f) t = 0.0f;
            if (t >= best) continue;
            float y = b->p.y + b->v.y * t;
            /* e == 1 is the right mouth, defended by side 1 */
            int def = e;
            if (vb_absf(y - s->goal_y[def]) <= s->goal_half[def] - r * 0.6f) {
                best = t; kind = 3; gside = 1 - def; n = v2(nx, 0);
            } else {
                best = t; kind = 1; n = v2(nx, 0);
            }
        }

        /* paddles — skipped entirely while the ball is airborne on a chip */
        if (b->z <= VB_CHIP_CLEAR) {
            for (int i = 0; i < VB_NRODS; i++) {
                const VbRod *rod = &s->rods[i];
                if (rod->pin_ball == bi) continue;         /* already held  */
                for (int k = 0; k < rod->n; k++) {
                    float lo, hi;
                    vb_pad_span(rod, k, r, &lo, &hi);
                    V2 c = v2(rod->x, (lo + hi) * 0.5f);
                    V2 h = v2(VB_PAD_HALF_X + r, (hi - lo) * 0.5f + r);
                    /* solved in the rod's frame: the bar is moving too */
                    V2 rel = v2(b->v.x, b->v.y - rod->vel);
                    float t; V2 nn;
                    if (!sweep_box(b->p, rel, c, h, best, &t, &nn)) continue;
                    if (t < best || (t <= best && kind == 0)) {
                        best = t; kind = 2; hrod = i; hpad = k; n = nn;
                    }
                }
            }
        }

        b->p = v2add(b->p, v2mul(b->v, best));
        rem -= best;

        if (kind == 0) break;

        if (kind == 3) {
            b->alive = 0;
            s->goal_side = gside;
            s->goal_heat = b->heat;
            vb_ev(s, VB_EV_GOAL, gside, bi, b->heat, v2len(b->v), b->p);
            return;
        }

        if (kind == 1) {
            if (vb_absf(n.x) > 0.5f) {
                b->v.x = -b->v.x * VB_REST_WALL;
                b->p.x += n.x * 0.0008f;
            } else {
                b->v.y = -b->v.y * VB_REST_WALL;
                b->p.y += n.y * 0.0008f;
                /* english sharpens the bank exit (§3.2) — the reason a curved
                 * ball off the side wall is a different shot to a straight one */
                if (b->curve_ticks > 0)
                    b->v.x += vb_signf(b->v.x) * v2len(b->curve) * 0.06f;
            }
            vb_ev(s, VB_EV_WALL, -1, bi, 0, v2len(b->v), b->p);
            continue;
        }

        /* kind == 2: a paddle. Note the save before the strike rewrites it. */
        {
            const VbRod *rod = &s->rods[hrod];
            int def = rod->side;
            if (goal_bound(s, b, def) && b->heat >= 3)
                vb_ev(s, VB_EV_SAVE, hrod, bi, b->heat, v2len(b->v), b->p);

            /* Put the ball exactly on the surface rather than nudging it by an
             * epsilon. A ball wedged between a bar and a wall would otherwise
             * re-collide every tick forever, which is not a rally, it is a
             * buzzing noise. */
            float lo, hi;
            vb_pad_span(rod, hpad, r, &lo, &hi);
            V2 c = v2(rod->x, (lo + hi) * 0.5f);
            V2 h = v2(VB_PAD_HALF_X + r, (hi - lo) * 0.5f + r);
            if (vb_absf(n.x) > 0.5f) b->p.x = c.x + n.x * (h.x + 0.0008f);
            else                     b->p.y = c.y + n.y * (h.y + 0.0008f);

            if (vb_rod_contact(s, hrod, hpad, bi, n)) return;  /* pinned */
            if (b->mercy > 0) return;                          /* shimmering */
        }
    }

    /* The backstop. Six iterations of swept resolution is generous, but a ball
     * squeezed between a moving bar and a wall can still end a tick a hair
     * outside. Nothing in this game is ever allowed to leave the table except
     * through a mouth, and that path returns above. */
    float lim = VB_TABLE_HY - b->radius;
    if (b->p.y >  lim) { b->p.y =  lim; if (b->v.y > 0) b->v.y = -b->v.y * VB_REST_WALL; }
    if (b->p.y < -lim) { b->p.y = -lim; if (b->v.y < 0) b->v.y = -b->v.y * VB_REST_WALL; }
    float limx = VB_TABLE_HX - b->radius;
    if (b->p.x >  limx) { b->p.x =  limx; if (b->v.x > 0) b->v.x = -b->v.x * VB_REST_WALL; }
    if (b->p.x < -limx) { b->p.x = -limx; if (b->v.x < 0) b->v.x = -b->v.x * VB_REST_WALL; }
}

/* ---- the tick --------------------------------------------------------- */

/* A triangle wave on the tick counter. Deliberately not sinf: libm is not
 * bit-identical between platforms and §5.1 does not allow the ball's fate to
 * depend on which one you linked. */
static float tri_wave(unsigned tick, unsigned period) {
    unsigned t = tick % period;
    float u = (float)t / (float)period;          /* 0..1 */
    float w = (u < 0.5f) ? (u * 4.0f - 1.0f) : (3.0f - u * 4.0f);
    return w;                                     /* -1..1 */
}

void vb_sim_step(VbSim *s, const VbInput in[VB_NPLAYERS]) {
    s->nev = 0;
    s->goal_side = -1;

    /* Moving Goals (PARTY): the mouths oscillate, slowly and legibly. */
    if (s->cfg.mutators & VB_MUT_MOVING_GOAL) {
        float w = tri_wave(s->tick, 1500);
        s->goal_y[0] =  w * 0.20f;
        s->goal_y[1] = -w * 0.20f;
    }

    if (s->serve_prot > 0) s->serve_prot--;

    vb_rods_update(s, in);

    int any_alive = 0;
    for (int i = 0; i < s->nballs; i++) {
        ball_step(s, i);
        if (s->balls[i].alive) any_alive = 1;
        if (s->goal_side >= 0) break;
    }
    (void)any_alive;

    /* anti-stall referee (§5.5): possession that never advances gets a pulse
     * and then a heat-0 pop. rules.c owns the pop; the sim just tells it. */
    if (s->goal_side < 0) {
        int holder = -1;
        for (int i = 0; i < VB_NRODS; i++)
            if (s->rods[i].pin_ball >= 0) holder = s->rods[i].side;
        if (holder < 0) {
            for (int i = 0; i < s->nballs; i++) {
                const VbBall *b = &s->balls[i];
                if (!b->alive || b->last_side < 0) continue;
                float own = (b->last_side == 0) ? -1.0f : 1.0f;
                /* still loitering in the toucher's own half */
                if (b->p.x * own > 0.0f) holder = b->last_side;
            }
        }
        if (holder >= 0 && holder == s->ctrl_side) {
            s->ctrl_ticks++;
            if (s->ctrl_ticks == VB_STALL_TICKS)
                vb_ev(s, VB_EV_STALL, holder, -1, 0, 0.0f, v2zero());
        } else {
            s->ctrl_side = holder;
            s->ctrl_ticks = 0;
        }
    }

    s->tick++;
}
