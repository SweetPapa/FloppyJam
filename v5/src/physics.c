/* physics.c — Section 5 of the spec, implemented literally.
 *
 * Everything here is a pure function of (world state, tick, shot inputs).
 * No wall-clock, no rand(), no float state that survives between holes.
 * Collision is discrete but adaptively substepped so that the ball never
 * advances more than a quarter of the thinnest wall in the level per
 * micro-step; that is what makes the tunnelling test in tests/ pass.
 */
#include "physics.h"
#include <string.h>

/* Section 6.3 friction table. cap != 0 means "sand": hard speed ceiling. */
const BpSurfDef BP_SURF[SURF_COUNT] = {
    /* FELT  */ { 0.30f, 0.090f, 0.050f, 0.0f },
    /* ROUGH */ { 0.62f, 0.330f, 0.160f, 0.0f },
    /* ICE   */ { 0.05f, 0.006f, 0.006f, 0.0f },
    /* SAND  */ { 0.95f, 0.720f, 0.400f, 1.2f },
    /* KICK  */ { 0.30f, 0.090f, 0.050f, 0.0f },
};

#define CONTACT_TOL 0.012f
#define RAIL_H      0.20f
#define RAIL_T      0.075f

/* ------------------------------------------------------------------ */
/* events                                                              */

static void ev_push(BpWorld *w, int kind, int a, int b, float mag, V3 at)
{
    BpEvent *e;
    if (w->nev >= BP_MAX_EVENTS) return;
    e = &w->ev[w->nev++];
    e->kind = (unsigned char)kind;
    e->a = (unsigned char)a;
    e->b = (signed char)b;
    e->mag = mag;
    e->at = at;
}

/* ------------------------------------------------------------------ */
/* movers (6.4): pose is a pure function of ticks since shot start      */

void bp_mover_pose(const BpWorld *w, int mi, float ftick, V3 *off, float *yaw)
{
    const BpMover *m;
    float t, a;
    *off = v3zero();
    *yaw = 0.0f;
    if (mi < 0 || mi >= w->nmovers) return;
    m = &w->movers[mi];
    t = ftick * BP_DT;
    a = m->rate * t + m->phase;
    switch (m->kind) {
    case MOV_ORBIT:
        off->x = m->cx + m->amp * cosf(a);
        off->y = m->cy;
        off->z = m->cz + m->amp * sinf(a);
        *yaw = a;
        break;
    case MOV_SPIN:
        *yaw = a;
        break;
    case MOV_SHUTTLE: {
        /* triangle wave in [-1,1] so the bar reverses smoothly */
        float u = a / BP_PI;
        float f = u - floorf(u * 0.5f) * 2.0f;      /* [0,2) */
        float tri = (f < 1.0f) ? (f * 2.0f - 1.0f) : (3.0f - f * 2.0f);
        off->x = m->dx * m->amp * tri;
        off->z = m->dz * m->amp * tri;
        break;
    }
    case MOV_TILT:
    default:
        break;
    }
}

static V3 mover_vel(const BpWorld *w, int mi, float ftick)
{
    V3 a, b; float ya, yb;
    const float e = 0.5f;
    if (mi < 0) return v3zero();
    bp_mover_pose(w, mi, ftick - e, &a, &ya);
    bp_mover_pose(w, mi, ftick + e, &b, &yb);
    return v3mul(v3sub(b, a), 1.0f / (2.0f * e * BP_DT));
}

/* Tilt movers warp the pad plane instead of translating it. */
static float pad_height(const BpWorld *w, const BpPad *p, float x, float z)
{
    float h = p->y0 + p->sx * (x - p->x0) + p->sz * (z - p->z0);
    if (p->mover) {
        const BpMover *m = &w->movers[p->mover - 1];
        if (m->kind == MOV_TILT) {
            float cx = 0.5f * (p->x0 + p->x1), cz = 0.5f * (p->z0 + p->z1);
            float s = m->amp * sinf(m->rate * (w->ftick * BP_DT) + m->phase);
            h += s * ((x - cx) * m->dx + (z - cz) * m->dz);
        }
    }
    return h;
}

V3 bp_pad_normal(const BpPad *p)
{
    return v3norm(v3(-p->sx, 1.0f, -p->sz));
}

static V3 pad_normal_live(const BpWorld *w, const BpPad *p)
{
    float sx = p->sx, sz = p->sz;
    if (p->mover) {
        const BpMover *m = &w->movers[p->mover - 1];
        if (m->kind == MOV_TILT) {
            float s = m->amp * sinf(m->rate * (w->ftick * BP_DT) + m->phase);
            sx += s * m->dx;
            sz += s * m->dz;
        }
    }
    return v3norm(v3(-sx, 1.0f, -sz));
}

float bp_pad_height(const BpWorld *w, int pad, float x, float z)
{
    if (pad < 0 || pad >= w->npads) return 0.0f;
    return pad_height(w, &w->pads[pad], x, z);
}

V3 bp_pad_normal_at(const BpWorld *w, int pad)
{
    if (pad < 0 || pad >= w->npads) return v3(0.0f, 1.0f, 0.0f);
    return pad_normal_live(w, &w->pads[pad]);
}

float bp_ground_h(const BpWorld *w, float x, float z, float bally, int *pad_out)
{
    float ref = bally - BP_R + 0.06f;   /* tolerate a 6 cm step up */
    float best = -1e9f, lowest = 1e9f;
    int bi = -1, li = -1, i;
    for (i = 0; i < w->npads; ++i) {
        const BpPad *p = &w->pads[i];
        float h;
        if (x < p->x0 - 1e-4f || x > p->x1 + 1e-4f) continue;
        if (z < p->z0 - 1e-4f || z > p->z1 + 1e-4f) continue;
        h = pad_height(w, p, x, z);
        if (h < lowest) { lowest = h; li = i; }
        if (h <= ref && h > best) { best = h; bi = i; }
    }
    if (bi < 0) {
        /* Ball somehow below every pad here: recover onto the lowest one
         * rather than dropping it through the world. */
        if (li >= 0) { if (pad_out) *pad_out = li; return lowest; }
        if (pad_out) *pad_out = -1;
        return -1e9f;
    }
    if (pad_out) *pad_out = bi;
    return best;
}

/* ------------------------------------------------------------------ */
/* pockets                                                             */

static float pocket_radius(const BpPocket *pk)
{
    return (pk->kind == PK_CUP) ? BP_CUP_R : BP_POCKET_R;
}

int bp_pocket_at(const BpWorld *w, float x, float z, float y)
{
    int i;
    for (i = 0; i < w->npockets; ++i) {
        const BpPocket *pk = &w->pockets[i];
        float dx, dz, pr;
        if (pk->kind == PK_CUP && w->cup_sealed) continue;
        if (y > -1e8f && fabsf(pk->y - y) > 0.09f) continue;
        dx = x - pk->x; dz = z - pk->z;
        pr = pocket_radius(pk);
        if (dx * dx + dz * dz < pr * pr) return i;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* impulses                                                            */

/* Normal restitution plus a Coulomb-capped tangential impulse at the
 * contact point. This one routine is the whole english story: running
 * english slips with the cushion and widens the rebound, check english
 * slips against it and narrows/kills it (5.4). */
static void contact_impulse(BpBall *b, V3 n, float e, float mu, float boost)
{
    float vn = v3dot(b->v, n);
    float jn, us;
    V3 r, u, ut, uh, J;
    if (vn > 0.0f) vn = 0.0f;
    jn = -(1.0f + e) * vn + boost;
    r = v3mul(n, -BP_R);
    u = v3add(b->v, v3cross(b->w, r));
    ut = v3tan(u, n);
    b->v = v3add(b->v, v3mul(n, jn));
    us = v3len(ut);
    if (us > 1e-5f && jn > 0.0f) {
        float jt_max = mu * jn;
        float jt_stop = (2.0f / 7.0f) * us;
        float jt = jt_max < jt_stop ? jt_max : jt_stop;
        uh = v3mul(ut, 1.0f / us);
        J = v3mul(uh, -jt);
        b->v = v3add(b->v, J);
        b->w = v3add(b->w, v3mul(v3cross(r, J), 5.0f / (2.0f * BP_R * BP_R)));
    }
}

/* 5.4: hard smashes come off the cushion dead. */
static float wall_e(float speed)
{
    if (speed <= 4.0f) return BP_E_WALL;
    if (speed >= BP_VMAX) return 0.75f;
    return bp_lerpf(BP_E_WALL, 0.75f, (speed - 4.0f) / (BP_VMAX - 4.0f));
}

/* ------------------------------------------------------------------ */
/* surface regimes (5.2)                                               */

static void surface_step(BpBall *b, V3 n, const BpSurfDef *s, float h)
{
    V3 gv = v3(0.0f, -BP_G, 0.0f);
    float gn = v3dot(gv, n);
    V3 gtan = v3sub(gv, v3mul(n, gn));
    float N = -gn;
    V3 r, u, ut;
    float us, wn, dwn;

    if (N < 0.0f) N = 0.0f;

    /* slope acceleration */
    b->v = v3add(b->v, v3mul(gtan, h));

    r = v3mul(n, -BP_R);
    u = v3add(b->v, v3cross(b->w, r));
    ut = v3tan(u, n);
    us = v3len(ut);

    if (us > BP_SLIP_EPS) {
        /* SLIDING: friction opposes contact-point slip and torques the ball
         * toward rolling. Draw / follow / stun all live here. */
        float mu = s->mu_slide;
        float tstop = us / (3.5f * mu * N + 1e-9f);
        float dt = h < tstop ? h : tstop;
        V3 uh = v3mul(ut, 1.0f / us);
        b->v = v3sub(b->v, v3mul(uh, mu * N * dt));
        b->w = v3add(b->w, v3mul(v3cross(n, uh),
                                 (5.0f * mu * N) / (2.0f * BP_R) * dt));
    } else {
        /* ROLLING: only rolling resistance; regear omega to pure roll but
         * keep the vertical-axis english, which decays on its own below. */
        V3 vt = v3tan(b->v, n);
        float vs = v3len(vt);
        if (vs > 1e-6f) {
            float dv = s->mu_roll * N * h;
            float k = (dv >= vs) ? 0.0f : (vs - dv) / vs;
            b->v = v3add(v3mul(n, v3dot(b->v, n)), v3mul(vt, k));
        }
        wn = v3dot(b->w, n);
        b->w = v3add(v3mul(v3cross(n, b->v), 1.0f / BP_R), v3mul(n, wn));
    }

    /* vertical-axis spin bleeds off independently (mu_spin) */
    wn = v3dot(b->w, n);
    dwn = (5.0f * s->mu_spin * N) / (2.0f * BP_R) * h;
    if (fabsf(wn) <= dwn) b->w = v3sub(b->w, v3mul(n, wn));
    else                  b->w = v3sub(b->w, v3mul(n, (wn > 0 ? dwn : -dwn)));
}

/* ------------------------------------------------------------------ */
/* box / post collision                                                */

static void box_pose(const BpWorld *w, const BpBox *bx, V3 *c, float *yaw, V3 *vel)
{
    *c = v3(bx->cx, bx->cy, bx->cz);
    *yaw = bx->yaw;
    *vel = v3zero();
    if (bx->mover) {
        V3 off; float dy;
        bp_mover_pose(w, bx->mover - 1, w->ftick, &off, &dy);
        *c = v3add(*c, off);
        *yaw += dy;
        *vel = mover_vel(w, bx->mover - 1, w->ftick);
    }
}

static int box_active(const BpWorld *w, const BpBox *bx)
{
    if (bx->gate && w->gate_open[bx->gate - 1]) return 0;
    if (bx->kind == BOX_CAP && !w->cup_sealed) return 0;
    return 1;
}

/* closest point on a yaw-oriented box, world space; returns penetration */
static int box_contact(V3 p, V3 c, float yaw, float hx, float hy, float hz,
                       V3 *n_out, float *pen_out)
{
    float cs = cosf(yaw), sn = sinf(yaw);
    V3 d = v3sub(p, c);
    float lx =  d.x * cs + d.z * sn;
    float lz = -d.x * sn + d.z * cs;
    float ly =  d.y;
    float qx = bp_clampf(lx, -hx, hx);
    float qy = bp_clampf(ly, -hy, hy);
    float qz = bp_clampf(lz, -hz, hz);
    float ex = lx - qx, ey = ly - qy, ez = lz - qz;
    float d2 = ex * ex + ey * ey + ez * ez;
    V3 nl;
    if (d2 > 1e-12f) {
        float dist = sqrtf(d2);
        if (dist >= BP_R) return 0;
        nl = v3(ex / dist, ey / dist, ez / dist);
        *pen_out = BP_R - dist;
    } else {
        float px = hx - fabsf(lx), py = hy - fabsf(ly), pz = hz - fabsf(lz);
        if (px <= py && px <= pz)      { nl = v3(lx < 0 ? -1.f : 1.f, 0, 0); *pen_out = px + BP_R; }
        else if (py <= pz)             { nl = v3(0, ly < 0 ? -1.f : 1.f, 0); *pen_out = py + BP_R; }
        else                           { nl = v3(0, 0, lz < 0 ? -1.f : 1.f); *pen_out = pz + BP_R; }
    }
    n_out->x = nl.x * cs - nl.z * sn;
    n_out->y = nl.y;
    n_out->z = nl.x * sn + nl.z * cs;
    return 1;
}

void bp_box_pose(const BpWorld *w, int i, V3 *c, float *yaw)
{
    V3 vel;
    box_pose(w, &w->boxes[i], c, yaw, &vel);
}

int bp_box_visible(const BpWorld *w, int i)
{
    return box_active(w, &w->boxes[i]);
}

void bp_post_pose(const BpWorld *w, int i, V3 *c)
{
    const BpPost *po = &w->posts[i];
    *c = v3(po->cx, po->cy, po->cz);
    if (po->mover) {
        V3 off; float dy;
        bp_mover_pose(w, po->mover - 1, w->ftick, &off, &dy);
        *c = v3add(*c, off);
    }
}

static void resolve_boxes(BpWorld *w, int bi)
{
    BpBall *b = &w->balls[bi];
    int i;
    for (i = 0; i < w->nboxes; ++i) {
        const BpBox *bx = &w->boxes[i];
        V3 c, vel, n; float yaw, pen, vn, e, speed;
        if (!box_active(w, bx)) continue;
        box_pose(w, bx, &c, &yaw, &vel);
        if (!box_contact(b->p, c, yaw, bx->hx, bx->hy, bx->hz, &n, &pen)) continue;
        b->p = v3add(b->p, v3mul(n, pen + 1e-4f));
        b->v = v3sub(b->v, vel);                 /* work in the mover frame */
        vn = v3dot(b->v, n);
        if (vn < 0.0f) {
            speed = v3len(b->v);
            if (bx->kind == BOX_BUMPER_WALL) {
                contact_impulse(b, n, BP_E_BUMPER, BP_MU_WALL, BP_BUMPER_KICK);
                ev_push(w, EV_BUMPER, bi, -1, speed, b->p);
            } else if (fabsf(n.y) > 0.7f) {
                contact_impulse(b, n, BP_E_FLOOR, BP_MU_SLIDE, 0.0f);
            } else {
                e = wall_e(speed);
                contact_impulse(b, n, e, BP_MU_WALL, 0.0f);
                if (speed > 0.18f) {
                    ev_push(w, EV_WALL, bi, -1, speed, b->p);
                    if (bi == 0) w->wall_hits++;
                }
            }
        }
        b->v = v3add(b->v, vel);
    }
}

static void resolve_posts(BpWorld *w, int bi)
{
    BpBall *b = &w->balls[bi];
    int i;
    for (i = 0; i < w->nposts; ++i) {
        BpPost *po = &w->posts[i];
        V3 c = v3(po->cx, po->cy, po->cz), vel = v3zero();
        float dx, dz, dxz, qy, ny, nx, nz, dist, pen, vn, speed;
        V3 n;
        if (po->mover) {
            V3 off; float dy;
            bp_mover_pose(w, po->mover - 1, w->ftick, &off, &dy);
            c = v3add(c, off);
            vel = mover_vel(w, po->mover - 1, w->ftick);
        }
        dx = b->p.x - c.x; dz = b->p.z - c.z;
        dxz = sqrtf(dx * dx + dz * dz);
        qy = bp_clampf(b->p.y, c.y, c.y + po->h);
        ny = b->p.y - qy;
        if (dxz > po->r) { nx = dx * (1.0f - po->r / dxz); nz = dz * (1.0f - po->r / dxz); }
        else             { nx = 0.0f; nz = 0.0f; }
        dist = sqrtf(nx * nx + ny * ny + nz * nz);
        if (dist >= BP_R) continue;
        if (dist > 1e-6f) { n = v3(nx / dist, ny / dist, nz / dist); pen = BP_R - dist; }
        else {
            float ld = (dxz > 1e-6f) ? dxz : 1e-6f;
            n = v3(dx / ld, 0.0f, dz / ld);
            pen = BP_R + (po->r - dxz);
        }
        b->p = v3add(b->p, v3mul(n, pen + 1e-4f));
        b->v = v3sub(b->v, vel);
        vn = v3dot(b->v, n);
        if (vn < 0.0f) {
            speed = v3len(b->v);
            if (po->kind == POST_BUMPER) {
                contact_impulse(b, n, BP_E_BUMPER, BP_MU_WALL, BP_BUMPER_KICK);
                ev_push(w, EV_BUMPER, bi, (signed char)i, speed, b->p);
            } else {
                contact_impulse(b, n, wall_e(speed), BP_MU_WALL, 0.0f);
                if (speed > 0.18f) {
                    ev_push(w, EV_WALL, bi, -1, speed, b->p);
                    if (bi == 0) w->wall_hits++;
                }
                if (po->kind == POST_TARGET && po->target && !po->lit) {
                    po->lit = 1;
                    w->gate_open[po->target - 1] = 1;
                    ev_push(w, EV_TARGET, bi, (signed char)i, speed, b->p);
                }
            }
        }
        b->v = v3add(b->v, vel);
    }
}

/* Rim lip (a circle in space) and the inner cylinder wall. Together these
 * produce honest rim-outs: no magnetism, no capture assist (5.7). */
static void resolve_pocket_geom(BpWorld *w, int bi)
{
    BpBall *b = &w->balls[bi];
    int i;
    for (i = 0; i < w->npockets; ++i) {
        const BpPocket *pk = &w->pockets[i];
        float pr = pocket_radius(pk);
        float dx, dz, dxz, ux, uz, dist, pen, vn;
        V3 rimp, diff, n;
        if (pk->kind == PK_CUP && w->cup_sealed) continue;
        dx = b->p.x - pk->x; dz = b->p.z - pk->z;
        dxz = sqrtf(dx * dx + dz * dz);
        if (dxz > pr + 2.0f * BP_R) continue;
        if (b->p.y > pk->y + 2.0f * BP_R) continue;
        if (b->p.y < pk->y - BP_CUP_DEPTH - BP_R) continue;

        if (dxz > 1e-6f) { ux = dx / dxz; uz = dz / dxz; }
        else             { ux = 1.0f; uz = 0.0f; }

        /* Only the golf cup has a rim lip all the way round — that lip is
         * what makes rim-outs possible. Pool pockets have open jaws, so a
         * ball that gets over the opening simply drops. */
        if (pk->kind != PK_CUP) goto cylinder;

        /* lip: closest point on the rim circle */
        rimp = v3(pk->x + ux * pr, pk->y, pk->z + uz * pr);
        diff = v3sub(b->p, rimp);
        dist = v3len(diff);
        if (dist < BP_R && dist > 1e-6f) {
            n = v3mul(diff, 1.0f / dist);
            pen = BP_R - dist;
            b->p = v3add(b->p, v3mul(n, pen + 1e-4f));
            vn = v3dot(b->v, n);
            if (vn < -0.05f) {
                contact_impulse(b, n, BP_E_RIM, BP_MU_WALL, 0.0f);
                ev_push(w, EV_RIM, bi, (signed char)i, -vn, b->p);
            } else if (vn < 0.0f) {
                b->v = v3sub(b->v, v3mul(n, vn));
            }
            continue;
        }

cylinder:
        /* inner cylinder: rattling around inside the cup */
        if (b->p.y < pk->y - 0.2f * BP_R && dxz + BP_R > pr) {
            n = v3(-ux, 0.0f, -uz);
            pen = dxz + BP_R - pr;
            b->p = v3add(b->p, v3mul(n, pen + 1e-4f));
            vn = v3dot(b->v, n);
            if (vn < 0.0f) {
                contact_impulse(b, n, 0.40f, BP_MU_WALL, 0.0f);
                if (-vn > 0.25f) ev_push(w, EV_RIM, bi, (signed char)i, -vn, b->p);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* pocket capture + consequences (7.2, 7.3, 7.4, 5.8)                  */

static void pocket_capture(BpWorld *w, int bi, int pi)
{
    BpBall *b = &w->balls[bi];
    const BpPocket *pk = &w->pockets[pi];
    ev_push(w, EV_POCKET, bi, (signed char)pi, v3len(b->v), b->p);

    if (pk->kind == PK_WARP && pk->link >= 0 && pk->link < w->npockets) {
        const BpPocket *ex = &w->pockets[pk->link];
        V3 dir = v3norm(v3(b->v.x, 0.0f, b->v.z));
        float sp = sqrtf(b->v.x * b->v.x + b->v.z * b->v.z);
        if (sp < 0.6f) sp = 0.6f;
        if (v3len2(dir) < 0.5f) dir = v3(0.0f, 0.0f, 1.0f);
        b->p = v3(ex->x + dir.x * (BP_POCKET_R + BP_R * 2.5f), ex->y + BP_R * 1.02f,
                  ex->z + dir.z * (BP_POCKET_R + BP_R * 2.5f));
        b->v = v3mul(dir, sp);
        b->w = v3mul(v3cross(v3(0.0f, 1.0f, 0.0f), b->v), 1.0f / BP_R);
        b->state = BS_ROLL;
        b->cool = 48;          /* don't re-enter the partner on the way out */
        return;
    }

    if (b->kind == BALL_CUE) {
        if (pk->kind == PK_CUP) { w->holed = 1; b->state = BS_GONE; }
        else                    { w->scratched = 1; b->state = BS_GONE; }
        return;
    }

    /* object / eight ball */
    if (b->kind == BALL_EIGHT && w->cup_sealed) {
        w->cup_sealed = 0;
        w->eight_potted = 1;
    } else if (b->kind == BALL_EIGHT) {
        w->eight_potted = 1;
    }
    if (pk->kind == PK_BONUS) w->bonus_hits++;
    b->state = BS_GONE;
    b->respawn = 0;
}

/* ------------------------------------------------------------------ */
/* ball-ball (5.5)                                                     */

static void resolve_balls(BpWorld *w)
{
    int i, j;
    for (i = 0; i < w->nballs; ++i) {
        if (w->balls[i].state == BS_GONE) continue;
        for (j = i + 1; j < w->nballs; ++j) {
            BpBall *a = &w->balls[i], *b = &w->balls[j];
            V3 d, n; float dist, pen, rel, jimp;
            if (b->state == BS_GONE) continue;
            d = v3sub(b->p, a->p);
            dist = v3len(d);
            if (dist >= 2.0f * BP_R || dist < 1e-6f) continue;
            n = v3mul(d, 1.0f / dist);
            pen = 2.0f * BP_R - dist;
            a->p = v3sub(a->p, v3mul(n, pen * 0.5f + 1e-5f));
            b->p = v3add(b->p, v3mul(n, pen * 0.5f + 1e-5f));
            rel = v3dot(v3sub(b->v, a->v), n);
            if (rel < 0.0f) {
                jimp = -(1.0f + BP_E_BALL) * rel * 0.5f;
                a->v = v3sub(a->v, v3mul(n, jimp));
                b->v = v3add(b->v, v3mul(n, jimp));
                if (a->state == BS_REST) a->state = BS_ROLL;
                if (b->state == BS_REST) b->state = BS_ROLL;
                ev_push(w, EV_BALLHIT, i, (signed char)j, -rel, v3lerp(a->p, b->p, 0.5f));
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* per-ball integration                                                */

static void ball_step(BpWorld *w, int bi, float h)
{
    BpBall *b = &w->balls[bi];
    int padi = -1, pi;
    float gh, support;
    int grounded, in_pocket = 0;
    V3 n = v3(0.0f, 1.0f, 0.0f), oldp;
    const BpSurfDef *s;
    unsigned char surf = SURF_FELT;

    if (b->state == BS_GONE) return;
    oldp = b->p;

    gh = bp_ground_h(w, b->p.x, b->p.z, b->p.y, &padi);
    support = gh;
    pi = bp_pocket_at(w, b->p.x, b->p.z, gh);
    if (pi >= 0) {
        support = w->pockets[pi].y - BP_CUP_DEPTH;
        in_pocket = 1;
        n = v3(0.0f, 1.0f, 0.0f);
    } else if (padi >= 0) {
        n = pad_normal_live(w, &w->pads[padi]);
        surf = w->pads[padi].surf;
    }
    if (padi < 0 && !in_pocket) support = -1e9f;

    grounded = (support > -1e8f) && (b->p.y - BP_R <= support + CONTACT_TOL);
    b->grounded = (unsigned char)grounded;
    if (b->surf != surf && grounded && !in_pocket) {
        ev_push(w, EV_SURFACE, bi, (signed char)surf, v3len(b->v), b->p);
    }
    b->surf = surf;
    s = &BP_SURF[surf];

    if (grounded) {
        float vn = v3dot(b->v, n);
        if (b->p.y < support + BP_R) b->p.y = support + BP_R;
        if (vn < -0.30f) {
            contact_impulse(b, n, BP_E_FLOOR, s->mu_slide, 0.0f);
            ev_push(w, EV_LAND, bi, (signed char)surf, -vn, b->p);
        } else if (vn < 0.0f) {
            b->v = v3sub(b->v, v3mul(n, vn));
        }
        if (b->state == BS_AIR) b->state = BS_ROLL;

        surface_step(b, n, s, h);

        /* kicker pads: hard speed reset along the arrow (6.2 #3) */
        if (surf == SURF_KICK && !in_pocket && padi >= 0) {
            const BpPad *p = &w->pads[padi];
            float sp = sqrtf(b->v.x * b->v.x + b->v.z * b->v.z);
            V3 dir = v3(sinf(p->ka), 0.0f, cosf(p->ka));
            if (sp < p->ks * 0.98f || v3dot(v3norm(v3(b->v.x, 0, b->v.z)), dir) < 0.9f) {
                if (v3len(b->v) > 0.02f || 1)
                    ev_push(w, EV_KICK, bi, (signed char)padi, p->ks, b->p);
            }
            b->v = v3(dir.x * p->ks, b->v.y, dir.z * p->ks);
            b->w = v3mul(v3cross(n, b->v), 1.0f / BP_R);
        }
        /* sand: hard speed ceiling (6.2 #8) */
        if (s->cap > 0.0f) {
            float sp = sqrtf(b->v.x * b->v.x + b->v.z * b->v.z);
            if (sp > s->cap) {
                float k = s->cap / sp;
                b->v.x *= k; b->v.z *= k;
            }
        }
    } else {
        b->v.y -= BP_G * h;
        if (b->state != BS_AIR && !in_pocket) b->state = BS_AIR;
    }

    b->p = v3add(b->p, v3mul(b->v, h));

    resolve_boxes(w, bi);
    resolve_posts(w, bi);
    resolve_pocket_geom(w, bi);

    /* cosmetic roll bookkeeping */
    {
        float ws = v3len(b->w);
        if (ws > 1e-4f) { b->axis = v3mul(b->w, 1.0f / ws); b->spinvis += ws * h; }
    }

    if (bi == 0) w->dist_travelled += v3len(v3sub(b->p, oldp));

    /* capture */
    pi = (b->cool > 0) ? -1 : bp_pocket_at(w, b->p.x, b->p.z, -1e9f);
    if (pi >= 0) {
        const BpPocket *pk = &w->pockets[pi];
        float sink = (pk->kind == PK_CUP) ? 0.9f * BP_R : 0.05f * BP_R;
        if (b->p.y < pk->y - sink && fabsf(pk->y - gh) < 0.35f) {
            pocket_capture(w, bi, pi);
            return;
        }
    }

    /* void / water (5.8) */
    if (b->p.y < w->kill_y) {
        ev_push(w, EV_VOID, bi, -1, 0.0f, b->p);
        b->state = BS_GONE;
        if (b->kind == BALL_CUE) w->scratched = 1;
        else b->respawn = 1;
        return;
    }

    /* rest test */
    if (grounded && b->state != BS_REST) {
        float sp = v3len(b->v);
        float wr = v3len(v3tan(b->w, n)) * BP_R;
        if (sp < BP_REST_V && wr < 0.09f) {
            V3 gv = v3(0.0f, -BP_G, 0.0f);
            V3 gtan = v3sub(gv, v3mul(n, v3dot(gv, n)));
            float slope = v3len(gtan);
            if (slope < s->mu_roll * BP_G * 0.95f) {
                b->v = v3zero();
                b->w = v3zero();
                b->state = BS_REST;
                ev_push(w, EV_REST, bi, -1, 0.0f, b->p);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* public                                                              */

void bp_world_reset(BpWorld *w)
{
    memset(w, 0, sizeof(*w));
    w->kill_y = -6.0f;
    w->min_thick = 0.05f;
}

static void add_rail_box(BpWorld *w, float cx, float cy, float cz,
                         float hx, float hy, float hz)
{
    BpBox *b;
    if (w->nboxes >= BP_MAX_BOXES) return;
    b = &w->boxes[w->nboxes++];
    memset(b, 0, sizeof(*b));
    b->cx = cx; b->cy = cy; b->cz = cz;
    b->hx = hx; b->hy = hy; b->hz = hz;
    b->kind = BOX_RAIL;
}

void bp_world_finalize(BpWorld *w)
{
    int i;
    float t = RAIL_T * 0.5f, hh = RAIL_H * 0.5f;

    for (i = 0; i < w->npads; ++i) {
        const BpPad *p = &w->pads[i];
        float xm = 0.5f * (p->x0 + p->x1), zm = 0.5f * (p->z0 + p->z1);
        float hxr = 0.5f * (p->x1 - p->x0), hzr = 0.5f * (p->z1 - p->z0);
        float dx = p->sx * (p->x1 - p->x0), dz = p->sz * (p->z1 - p->z0);
        float lo, hi;
        /* A rail on a sloped pad must span the whole edge, from the low end
         * up to RAIL_H above the high end, or the ball rolls under/over it. */
        if (p->rails & PAD_W) {
            lo = p->y0 + (dz < 0 ? dz : 0); hi = p->y0 + (dz > 0 ? dz : 0) + RAIL_H;
            add_rail_box(w, p->x0 - t, 0.5f * (lo + hi), zm, t, 0.5f * (hi - lo), hzr + 2 * t);
        }
        if (p->rails & PAD_E) {
            lo = p->y0 + dx + (dz < 0 ? dz : 0); hi = p->y0 + dx + (dz > 0 ? dz : 0) + RAIL_H;
            add_rail_box(w, p->x1 + t, 0.5f * (lo + hi), zm, t, 0.5f * (hi - lo), hzr + 2 * t);
        }
        if (p->rails & PAD_N) {
            lo = p->y0 + (dx < 0 ? dx : 0); hi = p->y0 + (dx > 0 ? dx : 0) + RAIL_H;
            add_rail_box(w, xm, 0.5f * (lo + hi), p->z0 - t, hxr + 2 * t, 0.5f * (hi - lo), t);
        }
        if (p->rails & PAD_S) {
            lo = p->y0 + dz + (dx < 0 ? dx : 0); hi = p->y0 + dz + (dx > 0 ? dx : 0) + RAIL_H;
            add_rail_box(w, xm, 0.5f * (lo + hi), p->z1 + t, hxr + 2 * t, 0.5f * (hi - lo), t);
        }
    }
    (void)hh;

    w->min_thick = 1e9f;
    for (i = 0; i < w->nboxes; ++i) {
        float a = 2.0f * w->boxes[i].hx, b = 2.0f * w->boxes[i].hz;
        float m = a < b ? a : b;
        if (m < w->min_thick) w->min_thick = m;
    }
    for (i = 0; i < w->nposts; ++i) {
        float m = 2.0f * w->posts[i].r;
        if (m < w->min_thick) w->min_thick = m;
    }
    if (w->min_thick > 0.30f) w->min_thick = 0.30f;
    if (w->min_thick < 0.03f) w->min_thick = 0.03f;
}

void bp_shot_begin(BpWorld *w)
{
    int i;
    for (i = 0; i < w->nballs; ++i) {
        BpBall *b = &w->balls[i];
        if (b->respawn) { b->p = b->home; b->state = BS_REST; b->respawn = 0; }
        b->prev = b->p;
        b->cool = 0;
        b->v = v3zero();
        b->w = v3zero();
        if (b->state != BS_GONE) b->state = BS_REST;
    }
    w->tick = 0;
    w->ftick = 0.0f;
    w->nev = 0;
    w->holed = 0;
    w->scratched = 0;
    w->bonus_hits = 0;
    w->eight_potted = 0;
    w->wall_hits = 0;
    w->dist_travelled = 0.0f;
}

void bp_strike(BpWorld *w, float aim, float power, float tx, float ty)
{
    BpBall *b = &w->balls[0];
    V3 dir, up = v3(0.0f, 1.0f, 0.0f), right, q;
    float speed, mag;
    power = bp_clampf(power, BP_POW_MIN, 1.0f);
    /* clamp the tip inside the legal contact disc */
    mag = sqrtf(tx * tx + ty * ty);
    if (mag > BP_TIP_MAX) { tx *= BP_TIP_MAX / mag; ty *= BP_TIP_MAX / mag; }
    speed = BP_VMAX * powf(power, BP_POW_EXP);
    dir = v3(sinf(aim), 0.0f, cosf(aim));
    right = v3cross(dir, up);                    /* screen-right of the aim */
    b->v = v3mul(dir, speed);
    q = v3add(v3mul(right, tx * BP_R), v3mul(up, ty * BP_R));
    b->w = v3mul(v3cross(q, b->v), BP_K_SPIN / (BP_R * BP_R));
    b->state = BS_ROLL;
}

void bp_step(BpWorld *w)
{
    int i, sub, nsub;
    float vmax = 0.0f, h;

    w->nev = 0;
    for (i = 0; i < w->nballs; ++i) {
        float sp;
        if (w->balls[i].state == BS_GONE) continue;
        sp = v3len(w->balls[i].v);
        if (sp > vmax) vmax = sp;
    }
    /* never advance more than a quarter of the thinnest wall per micro-step */
    nsub = (int)(vmax * BP_DT / (0.25f * w->min_thick)) + 1;
    nsub = bp_clampi(nsub, 1, 32);
    h = BP_DT / (float)nsub;

    for (sub = 0; sub < nsub; ++sub) {
        w->ftick = (float)w->tick + (float)sub / (float)nsub;
        for (i = 0; i < w->nballs; ++i) ball_step(w, i, h);
        resolve_balls(w);
    }
    for (i = 0; i < w->nballs; ++i) if (w->balls[i].cool > 0) w->balls[i].cool--;
    w->tick++;
    w->ftick = (float)w->tick;
}

void bp_respawn_cue(BpWorld *w)
{
    BpBall *b = &w->balls[0];
    b->p = b->prev;
    b->v = v3zero();
    b->w = v3zero();
    b->state = BS_REST;
    b->cool = 0;
}

int bp_settled(const BpWorld *w)
{
    int i;
    for (i = 0; i < w->nballs; ++i) {
        const BpBall *b = &w->balls[i];
        if (b->state == BS_GONE || b->state == BS_REST) continue;
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* aim guide (4.3) — geometry only, deliberately spin-blind             */

void bp_guide(const BpWorld *w, int bi, float aim, BpGuideHit *out)
{
    const BpBall *cb = &w->balls[bi];
    V3 dir = v3(sinf(aim), 0.0f, cosf(aim));
    V3 p = cb->p;
    float step = 0.02f, travelled = 0.0f;
    float base;
    int guard, padi;

    memset(out, 0, sizeof(*out));
    out->kind = 0;
    out->dist = 30.0f;
    out->point = v3add(p, v3mul(dir, 30.0f));
    out->rebound = dir;
    out->ball = -1;

    base = bp_ground_h(w, p.x, p.z, p.y, &padi);

    for (guard = 0; guard < 1600; ++guard) {
        int i;
        float gh;
        travelled += step;
        p = v3add(cb->p, v3mul(dir, travelled));
        p.y = cb->p.y;

        /* object balls */
        for (i = 0; i < w->nballs; ++i) {
            const BpBall *ob = &w->balls[i];
            float dx, dz, d2;
            if (i == bi || ob->state == BS_GONE) continue;
            dx = ob->p.x - p.x; dz = ob->p.z - p.z;
            d2 = dx * dx + dz * dz;
            if (d2 < (2.0f * BP_R) * (2.0f * BP_R)) {
                V3 od = v3norm(v3(ob->p.x - p.x, 0.0f, ob->p.z - p.z));
                out->kind = 2;
                out->ball = i;
                out->point = v3(p.x, cb->p.y, p.z);
                out->objdir = od;
                out->normal = od;
                out->rebound = v3norm(v3tan(dir, od));
                out->dist = travelled;
                return;
            }
        }

        /* pockets */
        for (i = 0; i < w->npockets; ++i) {
            const BpPocket *pk = &w->pockets[i];
            float dx, dz, pr;
            if (pk->kind == PK_CUP && w->cup_sealed) continue;
            if (fabsf(pk->y - cb->p.y + BP_R) > 0.25f) continue;
            dx = p.x - pk->x; dz = p.z - pk->z;
            pr = pocket_radius(pk) * 0.75f;
            if (dx * dx + dz * dz < pr * pr) {
                out->kind = 3;
                out->point = v3(pk->x, pk->y, pk->z);
                out->dist = travelled;
                out->ball = i;
                return;
            }
        }

        /* boxes */
        for (i = 0; i < w->nboxes; ++i) {
            const BpBox *bx = &w->boxes[i];
            V3 c, vel, n; float yaw, pen;
            if (!box_active(w, bx)) continue;
            box_pose(w, bx, &c, &yaw, &vel);
            if (!box_contact(p, c, yaw, bx->hx, bx->hy, bx->hz, &n, &pen)) continue;
            if (fabsf(n.y) > 0.8f) continue;
            n.y = 0.0f; n = v3norm(n);
            out->kind = 1;
            out->point = p;
            out->normal = n;
            out->rebound = v3norm(v3sub(dir, v3mul(n, 2.0f * v3dot(dir, n))));
            out->dist = travelled;
            return;
        }

        /* posts */
        for (i = 0; i < w->nposts; ++i) {
            const BpPost *po = &w->posts[i];
            V3 c = v3(po->cx, po->cy, po->cz), n;
            float dx, dz, dxz, rr;
            if (po->mover) {
                V3 off; float dy;
                bp_mover_pose(w, po->mover - 1, w->ftick, &off, &dy);
                c = v3add(c, off);
            }
            if (p.y < c.y - BP_R || p.y > c.y + po->h + BP_R) continue;
            dx = p.x - c.x; dz = p.z - c.z;
            dxz = sqrtf(dx * dx + dz * dz);
            rr = po->r + BP_R;
            if (dxz < rr) {
                n = v3norm(v3(dx, 0.0f, dz));
                out->kind = 1;
                out->point = p;
                out->normal = n;
                out->rebound = v3norm(v3sub(dir, v3mul(n, 2.0f * v3dot(dir, n))));
                out->dist = travelled;
                return;
            }
        }

        /* ledge / void */
        gh = bp_ground_h(w, p.x, p.z, cb->p.y, &padi);
        if (padi < 0 || gh < base - 0.14f) {
            out->kind = 4;
            out->point = p;
            out->dist = travelled;
            out->rebound = dir;
            return;
        }
        base = gh;
        if (travelled > 30.0f) break;
    }
}
