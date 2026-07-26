/* rods.c — Section 3. Two doors, one room.
 *
 * GLIDE is one axis and one button and it is a complete way to play, not a
 * tutorial. GRIP steers one bar at a time and adds foosball's real weapons.
 * Both are legal at the same table in the same match, which is only possible
 * because both funnel into the same five verbs down at the bottom of this
 * file: bunt, flick, charged flick, snap, chip.
 *
 * No spinning. Every strike has windup and recovery frames, so committing to
 * a flick and missing leaves the lane open for real. That is foosball's
 * no-spin law reborn as fighting-game honesty.
 */
#include "rods.h"
#include "heat.h"

static float role_half(int role) {
    return role == VB_GK ? VB_PAD_HALF_GK
         : role == VB_DEF ? VB_PAD_HALF_DEF : VB_PAD_HALF_ATK;
}
static float role_travel(int role) {
    return role == VB_GK ? VB_TRAVEL_GK
         : role == VB_DEF ? VB_TRAVEL_DEF : VB_TRAVEL_ATK;
}
static int role_count(int role) { return role == VB_GK ? 1 : role == VB_DEF ? 2 : 3; }
static float role_spacing(int role) {
    return role == VB_GK ? 0.0f : role == VB_DEF ? 0.580f : 0.365f;
}

static V2 rot(V2 d, float a) {
    float c = cosf(a), s = sinf(a);
    return v2(d.x * c - d.y * s, d.x * s + d.y * c);
}

/* forward: the direction this side attacks in */
static float fwd(int side) { return side == 0 ? 1.0f : -1.0f; }

void vb_rods_init(VbSim *s) {
    for (int i = 0; i < VB_NRODS; i++) {
        VbRod *r = &s->rods[i];
        const VbTilt *t = &s->cfg.tilt[r->side];
        float scale = (t->pad_scale > 0.01f) ? t->pad_scale : 1.0f;

        r->n       = role_count(r->role);
        r->spacing = role_spacing(r->role);
        r->travel  = role_travel(r->role);
        r->half    = role_half(r->role) * scale;

        /* Pillar-2 law, enforced at construction rather than hoped for:
         * a rod is a wall of light, never a wall of "no". */
        if (r->n > 1) {
            float lane_cap = (r->spacing - VB_OPEN_LANE) * 0.5f;
            if (r->half > lane_cap) r->half = lane_cap;
        } else {
            /* the goalie may never seal its own mouth */
            float gs = (s->cfg.tilt[r->side].goal_scale > 0.01f)
                     ? s->cfg.tilt[r->side].goal_scale : 1.0f;
            float mouth_cap = VB_GOAL_HALF * gs - VB_OPEN_LANE * 0.5f;
            if (r->half > mouth_cap) r->half = mouth_cap;
        }
        /* and it must physically fit between the side walls at full stretch */
        float span_cap = VB_TABLE_HY - r->travel - (float)(r->n - 1) * 0.5f * r->spacing;
        if (r->half > span_cap) r->half = span_cap;
        if (r->half < 0.02f) r->half = 0.02f;

        r->off = 0.0f; r->vel = 0.0f;
        r->state = VB_ROD_IDLE; r->timer = 0; r->charge = 0;
        r->kind = VB_STRIKE_NONE; r->pad = 0;
        r->pin_ball = -1; r->pin_ticks = 0; r->pin_aim = 0.0f; r->pin_face = 1.0f;
        r->catch_on = 0; r->bunt_on = 0; r->whiff = 0; r->flash = 0;
    }
    for (int p = 0; p < VB_NPLAYERS; p++) {
        int side = p / 2;
        s->active_rod[p] = vb_rod_of(side, VB_DEF);
        s->handoff_lock[p] = 0;
        s->prev_btn[p] = 0;
        s->hold_flick[p] = 0;
        s->hold_pin[p] = 0;
    }
}

void vb_rods_release_all(VbSim *s) {
    for (int i = 0; i < VB_NRODS; i++) {
        VbRod *r = &s->rods[i];
        r->pin_ball = -1; r->pin_ticks = 0;
        r->state = VB_ROD_IDLE; r->timer = 0; r->charge = 0;
        r->kind = VB_STRIKE_NONE; r->catch_on = 0; r->bunt_on = 0;
        r->vel = 0.0f;
    }
}

int vb_rod_is_active(const VbSim *s, int rod) {
    int slot = vb_rod_owner_slot(s, rod);
    if (s->cfg.scheme[slot] == VB_SCHEME_GLIDE) return 1;   /* all of them */
    return s->active_rod[slot] == rod;
}

/* ---- the assist (§3.1, §5.5) ------------------------------------------ */
/* Bends a shot modestly toward the best open lane, and refuses to hand a
 * GLIDE player their own goal. It never picks the shot for you — the bend is
 * capped at VB_ASSIST_BEND and it aims at gaps that are actually there. */
V2 vb_assist_dir(const VbSim *s, int rod, int ball, V2 dir, float strength) {
    if (strength <= 0.001f) return dir;
    const VbBall *b = &s->balls[ball];
    int side = s->rods[rod].side;
    float f = fwd(side);

    /* no own-goal misery: a rearward flick in your own half gets turned */
    if (dir.x * f < 0.0f && b->p.x * f < 0.0f) {
        dir.x = -dir.x;
        if (vb_absf(dir.y) < 0.15f) dir.y += 0.25f;
        dir = v2norm(dir);
    }

    /* find the next rod the ball has to get past */
    int target = -1;
    float bestdx = 1e9f;
    for (int i = 0; i < VB_NRODS; i++) {
        if (s->rods[i].side == side) continue;
        float dx = (s->rods[i].x - b->p.x) * f;
        if (dx > 0.01f && dx < bestdx) { bestdx = dx; target = i; }
    }
    if (target < 0) return dir;

    /* aim at the widest gap in that rod, closest to where we were already
     * going — the assist helps the shot you were taking, not a different one */
    const VbRod *r = &s->rods[target];
    float straight = b->p.y + dir.y / (vb_absf(dir.x) > 0.05f ? vb_absf(dir.x) : 0.05f) * bestdx;
    float edges[2 * VB_MAXPAD + 2];
    int   ne = 0;
    edges[ne++] = -VB_TABLE_HY;
    for (int k = 0; k < r->n; k++) {
        edges[ne++] = vb_pad_y(r, k) - r->half;
        edges[ne++] = vb_pad_y(r, k) + r->half;
    }
    edges[ne++] = VB_TABLE_HY;

    float aim = straight;
    float score = -1e9f;
    for (int i = 0; i + 1 < ne; i += 2) {
        float lo = edges[i], hi = edges[i + 1];
        float w = hi - lo;
        if (w < VB_OPEN_LANE * 0.8f) continue;
        float c = (lo + hi) * 0.5f;
        float sc = w - vb_absf(c - straight) * 0.85f;
        if (sc > score) { score = sc; aim = c; }
    }

    V2 want = v2norm(v2(f * bestdx, aim - b->p.y));
    float cur = atan2f(dir.y, dir.x);
    float tgt = atan2f(want.y, want.x);
    float d = tgt - cur;
    while (d >  VB_PI) d -= VB_TAU;
    while (d < -VB_PI) d += VB_TAU;
    float cap = VB_ASSIST_BEND * vb_clampf(strength, 0.0f, 1.0f);
    d = vb_clampf(d, -cap, cap);
    return rot(dir, d);
}

/* ---- the five verbs --------------------------------------------------- */

static int next_rod_ahead(const VbSim *s, const VbBall *b, int side) {
    float f = fwd(side);
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < VB_NRODS; i++) {
        float dx = (s->rods[i].x - b->p.x) * f;
        if (dx > 0.02f && dx < bd) { bd = dx; best = i; }
    }
    return best;
}

/* Applies a struck ball's outgoing state. This is where heat is spent. */
static void launch(VbSim *s, int rod, int ball, V2 n, int kind) {
    VbRod  *r = &s->rods[rod];
    VbBall *b = &s->balls[ball];
    int slot = vb_rod_owner_slot(s, rod);
    const VbTilt *tilt = &s->cfg.tilt[r->side];
    int glide = (s->cfg.scheme[slot] == VB_SCHEME_GLIDE);

    /* heat first: every paddle contact raises the ball one step, except the
     * two moves whose entire job is to cool it (§4). */
    int heat;
    if (kind == VB_STRIKE_BUNT)      heat = vb_heat_down(b->heat);
    else if (kind == VB_STRIKE_SNAP) heat = vb_heat_up(vb_heat_up(b->heat, tilt->heat_cap), tilt->heat_cap);
    else                             heat = vb_heat_up(b->heat, tilt->heat_cap);

    float mult = 1.0f;
    switch (kind) {
        case VB_STRIKE_BUNT:    mult = VB_BUNT_SPD;   break;
        case VB_STRIKE_CHARGED: mult = VB_CHARGE_SPD; break;
        case VB_STRIKE_SNAP:    mult = VB_SNAP_SPD;   break;
        case VB_STRIKE_CHIP:    mult = 0.85f;         break;
        default:                mult = 1.0f;          break;
    }
    /* VB_STRIKE_IDLE never arrives here: an untouched bar is a bounce, not a
     * strike, and bounce() handles it a few lines down. */
    float speed = vb_heat_speed(heat) * mult;

    /* Pong DNA: where on the paddle you catch it decides the angle. */
    V2 dir;
    if (vb_absf(n.x) > 0.5f) {
        float t = vb_clampf((b->p.y - vb_pad_y(r, r->pad)) / r->half, -1.0f, 1.0f);
        dir = rot(v2(n.x, 0.0f), t * 55.0f * VB_DEG * (n.x > 0 ? 1.0f : -1.0f));
    } else {
        /* caught on the end of the bar: it skips off sideways */
        dir = v2norm(v2(b->v.x * 0.7f, n.y * vb_maxf(vb_absf(b->v.y), 0.3f)));
        if (v2len2(dir) < 0.01f) dir = v2(fwd(r->side), n.y);
        dir = v2norm(dir);
    }

    if (kind == VB_STRIKE_SNAP) {
        /* The snap is laser-straight and aimed by the wedge you were shown.
         * The aim is measured from the face the ball is held against, so it
         * has to be applied in that face's sense — otherwise "up" on the
         * wedge means up for one player and down for the other. */
        dir = rot(v2(r->pin_face, 0.0f), r->pin_aim * r->pin_face);
    }

    if (glide) dir = vb_assist_dir(s, rod, ball, dir, tilt->assist);

    /* never launch a ball back into the paddle it just left */
    if (v2dot(dir, n) < 0.12f) {
        dir = v2norm(v2add(v2mul(dir, 0.4f), v2mul(n, 0.9f)));
    }

    V2 vel = v2mul(dir, speed);
    /* dragging the bar across the ball throws it, a little, directly */
    vel.y += r->vel * 0.22f;
    /* a struck ball leaves the paddle faster than the paddle is moving */
    {
        float sep = v2dot(vel, n) - r->vel * n.y;
        if (sep < VB_SEP_SPEED) vel = v2add(vel, v2mul(n, VB_SEP_SPEED - sep));
    }

    /* English: lateral rod velocity at contact, applied as decaying curve.
     *
     * The face the ball comes off has to be in here. Spin is induced by the
     * bar's motion ACROSS that face, so a bar sweeping +y induces opposite
     * spin depending on which side of it the ball is on — and the Magnus term
     * is spin x velocity, whose sign flips again with the ball's direction.
     * Leave the face out and the two ends of the table curve the ball
     * opposite ways from identical inputs, which is a side bias in a game
     * whose whole STANDARD ruleset is a promise of a fair table.
     * tests/test_sim.c plays a mirrored match to keep this honest.
     *
     * Spin goes as n x vp, which for a bar moving (0, vel) is n.x * vel. That
     * falls out of the mirror requirement as well as the physics: perp() maps
     * to MINUS its own mirror, so the spin has to flip sign under mirroring
     * for the Magnus term to survive it — and n.x is exactly the quantity
     * that does. It also gets the end-of-bar case right for free: a bar
     * driving into the ball with its short edge rubs nothing, so it imparts
     * no english, and n.x is zero there. */
    float ek = VB_ENGLISH_K * r->vel * n.x;
    if (kind == VB_STRIKE_CHARGED) ek *= VB_CHARGE_CRV;
    if (kind == VB_STRIKE_SNAP)    ek *= 0.15f;   /* its counter is position */
    if (kind == VB_STRIKE_BUNT)    ek *= 0.4f;
    if (vb_absf(ek) > 0.001f) {
        b->curve = v2mul(v2perp(v2norm(vel)), -ek);
        b->curve_ticks = VB_CURVE_TICKS;
    } else {
        b->curve = v2zero();
        b->curve_ticks = 0;
    }

    if (kind == VB_STRIKE_CHIP) {
        /* a lofted strike that hops the ball over exactly one rod plane and
         * comes down live in the lane beyond it — the through-pass, and the
         * goalie's nightmare. Interceptable at the landing ring, which both
         * players are shown. */
        int over = next_rod_ahead(s, b, r->side);
        float flight = 2.0f * sqrtf(2.0f * VB_CHIP_APEX / VB_CHIP_GRAV);
        float range = 0.58f;
        if (over >= 0) {
            float dx = vb_absf(s->rods[over].x - b->p.x);
            range = dx + 0.20f;              /* land in the lane behind it   */
        }
        V2 flat = v2norm(v2(dir.x, dir.y * 0.55f));
        vel = v2mul(flat, range / flight);
        b->vz = sqrtf(2.0f * VB_CHIP_APEX * VB_CHIP_GRAV);
        b->z = 0.0005f;
        b->chip_pass = over;
        b->curve = v2mul(b->curve, 0.35f);
        vb_ev(s, VB_EV_CHIP, rod, ball, over, range, b->p);
    }

    b->heat = heat;
    b->last_rod = rod;
    b->last_side = r->side;
    b->contacts++;

    /* the mercy beat — a breath before the fastest balls in the game fly */
    int mercy = vb_heat_mercy(heat);
    if (mercy > 0 && kind != VB_STRIKE_BUNT) {
        b->mercy = mercy;
        b->mercy_v = vel;
        b->v = v2zero();
        vb_ev(s, VB_EV_MERCY, rod, ball, heat, speed, b->p);
    } else {
        b->v = vel;
    }

    vb_ev(s, kind == VB_STRIKE_SNAP ? VB_EV_SNAP : VB_EV_HIT,
          rod, ball, kind, speed, b->p);
}

/* A touch nobody asked for: the ball bounces off a bar that happens to be
 * there. Restitution on the normal, almost everything kept on the tangent —
 * which is why a graze off the end of a paddle carries on down the lane
 * instead of being served back. Inactive bars block; they do not defend.
 *
 * [W0] §4 reads "every paddle contact raises the ball's heat one step". A
 * deflection off a bar nobody was steering does not, here, and it bleeds
 * energy instead of being re-launched at the heat's nominal speed. Both halves
 * of that matter and they are the same decision: with twelve paddles on the
 * table, counting incidental deflections meant every rally pinned itself at
 * heat 12 within seconds and then ping-ponged between the middle bars forever,
 * with nothing able to slow down enough to be threaded past anything. Nobody
 * scored. Heat here counts blows STRUCK — which is what makes the meter read
 * as the drama of the exchange (Pillar 2) rather than as a contact tally, and
 * what lets a deadened ball be picked up and worked. §0 resolves conflicts in
 * favour of §5 and then the Pillars; this is that resolution. */
static void bounce(VbSim *s, int rod, int ball, V2 n) {
    VbRod  *r = &s->rods[rod];
    VbBall *b = &s->balls[ball];

    /* Solved in the BAR's frame, because a rod is a moving wall. Doing it in
     * the table's frame lets a bar travelling faster than the ball's escape
     * speed drag it along its own edge for as long as it likes — the ball
     * ends up welded to a sliding paddle, buzzing. */
    V2 vp = v2(0.0f, r->vel);
    V2 rel = v2sub(b->v, vp);
    float speed_in = v2len(rel);

    float vn = v2dot(rel, n);
    V2 tang = v2sub(rel, v2mul(n, vn));
    V2 out = v2add(v2mul(tang, 0.92f), v2mul(n, vb_absf(vn) * VB_REST_IDLE));

    /* Idle paddle restitution, §5.2 exactly: 0.65 of what arrived. A ball
     * that comes off a bar twice is walking, and walking balls are what a pin
     * or a chip is for. */
    float want = vb_maxf(speed_in * VB_REST_IDLE, VB_SEP_SPEED);
    float cur = v2len(out);
    out = (cur > 1e-6f) ? v2mul(out, want / cur) : v2mul(n, want);

    /* and it always leaves, faster than the bar can follow it */
    float sep = v2dot(out, n);
    if (sep < VB_SEP_SPEED) out = v2add(out, v2mul(n, VB_SEP_SPEED - sep));

    b->v = v2add(out, vp);
    b->last_rod = rod;
    b->last_side = r->side;
    b->contacts++;
    vb_ev(s, VB_EV_HIT, rod, ball, VB_STRIKE_IDLE, v2len(out), b->p);
}

int vb_rod_contact(VbSim *s, int rod, int pad, int ball, V2 n) {
    VbRod  *r = &s->rods[rod];
    VbBall *b = &s->balls[ball];
    r->pad = pad;
    r->flash = VB_TICKS(0.18f);

    /* PIN — foosball's signature control move, and the fantasy centrepiece */
    if (r->catch_on && r->state != VB_ROD_RECOVER && r->pin_ball < 0
        && vb_absf(n.x) > 0.5f && b->z <= VB_CHIP_CLEAR) {
        r->state = VB_ROD_PINNED;
        r->pin_ball = ball;
        r->pin_ticks = 0;
        r->pin_aim = 0.0f;
        r->pin_face = n.x > 0 ? 1.0f : -1.0f;
        r->timer = 0;
        b->v = v2zero();
        b->curve = v2zero(); b->curve_ticks = 0;
        b->mercy = 0;
        b->heat = vb_heat_down(b->heat);
        b->last_rod = rod;
        b->last_side = r->side;
        b->contacts++;
        vb_ev(s, VB_EV_PIN, rod, ball, b->heat, 0.0f, b->p);
        return 1;
    }

    /* A committed swing beats a cushioned touch: if you tapped the catch and
     * then flicked, the flick is the shot you asked for. */
    if (r->state == VB_ROD_STRIKE) {
        int kind = r->kind;
        r->state = VB_ROD_RECOVER;
        r->timer = (kind == VB_STRIKE_CHARGED) ? VB_RC_CHARGED : VB_RC_FLICK;
        r->kind = VB_STRIKE_NONE;
        r->bunt_on = 0;
        launch(s, rod, ball, n, kind);
        return 0;
    }

    if (r->bunt_on) { r->bunt_on = 0; launch(s, rod, ball, n, VB_STRIKE_BUNT); return 0; }

    /* no button, or the rod is still paying for the last swing: a soft touch.
     * In GLIDE this is the automatic gentle bunt of §3.1. */
    bounce(s, rod, ball, n);
    return 0;
}

/* ---- per-tick ---------------------------------------------------------- */

static void start_strike(VbSim *s, int rod, int kind) {
    VbRod *r = &s->rods[rod];
    if (r->state != VB_ROD_IDLE && r->state != VB_ROD_CHARGING) return;
    r->kind = kind;
    r->state = VB_ROD_WINDUP;
    r->timer = (kind == VB_STRIKE_CHARGED) ? VB_WU_CHARGED : VB_WU_FLICK;
    r->charge = 0;
}

static void release_snap(VbSim *s, int rod) {
    VbRod *r = &s->rods[rod];
    int ball = r->pin_ball;
    if (ball < 0) return;
    r->pin_ball = -1;
    r->state = VB_ROD_RECOVER;
    r->timer = VB_RC_SNAP;
    V2 n = v2(r->pin_face, 0.0f);
    launch(s, rod, ball, n, VB_STRIKE_SNAP);
    r->pin_ticks = 0;
}

/* GRIP auto-handoff: the rod best positioned for the ball. It never picks a
 * rod that is mid-swing or holding a pin — you keep what you committed to. */
static int best_rod_for(const VbSim *s, int slot) {
    int side = slot / 2;
    int bi = -1;
    float bx = 0.0f;
    for (int i = 0; i < s->nballs; i++)
        if (s->balls[i].alive) { bi = i; bx = s->balls[i].p.x; break; }
    if (bi < 0) return s->active_rod[slot];

    int best = -1;
    float score = -1e9f;
    for (int i = 0; i < VB_NRODS; i++) {
        if (s->rods[i].side != side) continue;
        if (vb_rod_owner_slot(s, i) != slot) continue;
        float dx = s->rods[i].x - bx;
        float sc = -vb_absf(dx);
        /* prefer the bar the ball is actually coming toward */
        if (dx * s->balls[bi].v.x < 0.0f) sc += 0.22f;
        if (sc > score) { score = sc; best = i; }
    }
    return best < 0 ? s->active_rod[slot] : best;
}

static void owned_rods(const VbSim *s, int slot, int *out, int *n) {
    *n = 0;
    for (int i = 0; i < VB_NRODS; i++)
        if (vb_rod_owner_slot(s, i) == slot && s->rods[i].side == slot / 2)
            out[(*n)++] = i;
}

/* the paddle nearest the ball, across a set of rods — GLIDE's whole aiming
 * model: you move, and the game picks the bar that is actually there */
static int nearest_rod_to_ball(const VbSim *s, const int *rods, int n) {
    int bi = -1;
    for (int i = 0; i < s->nballs; i++) if (s->balls[i].alive) { bi = i; break; }
    if (bi < 0) return rods[0];
    const VbBall *b = &s->balls[bi];
    int best = rods[0];
    float bd = 1e9f;
    for (int i = 0; i < n; i++) {
        const VbRod *r = &s->rods[rods[i]];
        for (int k = 0; k < r->n; k++) {
            float dx = r->x - b->p.x, dy = vb_pad_y(r, k) - b->p.y;
            float d = dx * dx * 1.6f + dy * dy;
            if (d < bd) { bd = d; best = rods[i]; }
        }
    }
    return best;
}

static void move_rod(VbSim *s, int rod, float axis, float speed_mul, int pinned) {
    VbRod *r = &s->rods[rod];
    float top = (pinned ? VB_PIN_DRAG : VB_ROD_SPEED) * speed_mul;
    float want = vb_clampf(axis, -1.0f, 1.0f) * top;
    float dv = VB_ROD_ACCEL * VB_DT * (pinned ? 0.7f : 1.0f);
    if (r->vel < want) r->vel = vb_minf(want, r->vel + dv);
    else               r->vel = vb_maxf(want, r->vel - dv);
    r->off += r->vel * VB_DT;
    if (r->off >  r->travel) { r->off =  r->travel; if (r->vel > 0) r->vel = 0; }
    if (r->off < -r->travel) { r->off = -r->travel; if (r->vel < 0) r->vel = 0; }
}

void vb_rods_update(VbSim *s, const VbInput in[VB_NPLAYERS]) {
    int moved[VB_NRODS];
    for (int i = 0; i < VB_NRODS; i++) moved[i] = 0;
    for (int i = 0; i < VB_NRODS; i++) { s->rods[i].catch_on = 0; }

    int nslots = s->cfg.doubles ? 4 : 2;
    for (int q = 0; q < nslots; q++) {
        int slot = s->cfg.doubles ? q : q * 2;
        const VbInput *inp = &in[slot];
        int side = slot / 2;
        const VbTilt *tilt = &s->cfg.tilt[side];
        float smul = (tilt->rod_speed > 0.01f) ? tilt->rod_speed : 1.0f;
        float axis = vb_clampf(inp->axis, -1.0f, 1.0f);
        if (s->cfg.mutators & VB_MUT_MIRROR) axis = -axis;

        unsigned btn = inp->btn, prev = s->prev_btn[slot];
        unsigned pressed = btn & ~prev, released = prev & ~btn;

        int rods[VB_NRODS], nr;
        owned_rods(s, slot, rods, &nr);
        if (nr == 0) continue;

        int glide = (s->cfg.scheme[slot] == VB_SCHEME_GLIDE);

        /* --- which bar is under the stick --------------------------------- */
        if (s->handoff_lock[slot] > 0) s->handoff_lock[slot]--;
        int held_pin_rod = -1;
        for (int i = 0; i < nr; i++)
            if (s->rods[rods[i]].pin_ball >= 0) held_pin_rod = rods[i];

        if (!glide) {
            int a = s->active_rod[slot];
            int ok = 0;
            for (int i = 0; i < nr; i++) if (rods[i] == a) ok = 1;
            if (!ok) { a = rods[0]; s->active_rod[slot] = a; }

            if (pressed & (VB_BTN_PREV | VB_BTN_NEXT)) {
                int idx = 0;
                for (int i = 0; i < nr; i++) if (rods[i] == a) idx = i;
                idx += (pressed & VB_BTN_NEXT) ? 1 : -1;
                idx = vb_clampi(idx, 0, nr - 1);
                s->active_rod[slot] = rods[idx];
                s->handoff_lock[slot] = VB_TICKS(1.2f);
            } else if (held_pin_rod >= 0) {
                s->active_rod[slot] = held_pin_rod;   /* you hold what you caught */
            } else if (s->handoff_lock[slot] == 0) {
                const VbRod *ar = &s->rods[s->active_rod[slot]];
                if (ar->state == VB_ROD_IDLE || ar->state == VB_ROD_CHARGING)
                    s->active_rod[slot] = best_rod_for(s, slot);
            }
        }

        /* --- motion ------------------------------------------------------- */
        if (glide) {
            /* one axis slides ALL of this player's bars together */
            for (int i = 0; i < nr; i++) {
                move_rod(s, rods[i], axis, smul, s->rods[rods[i]].pin_ball >= 0);
                moved[rods[i]] = 1;
            }
        } else {
            int a = s->active_rod[slot];
            move_rod(s, a, axis, smul, s->rods[a].pin_ball >= 0);
            moved[a] = 1;
        }

        /* --- PIN / BUNT (GRIP only; GLIDE has one button and it is FLICK) -- */
        if (!glide) {
            if (pressed & VB_BTN_PIN)   s->hold_pin[slot] = 1;
            else if (btn & VB_BTN_PIN)  s->hold_pin[slot]++;

            if (released & VB_BTN_PIN) {
                if (s->hold_pin[slot] <= VB_TAP_TICKS && held_pin_rod < 0) {
                    /* a tapped catch is a cushioned touch: −1 heat, drops it
                     * short. Tempo control, and the way a trap gets set up.
                     * It arms the bar under the stick, not the whole side. */
                    s->rods[s->active_rod[slot]].bunt_on = VB_BUNT_WINDOW;
                }
                s->hold_pin[slot] = 0;
                if (held_pin_rod >= 0) release_snap(s, held_pin_rod);
            }
            if (btn & VB_BTN_PIN) {
                if (held_pin_rod >= 0) s->rods[held_pin_rod].catch_on = 1;
                else s->rods[s->active_rod[slot]].catch_on = 1;
            }
        }

        /* while pinned: the shot clock, the wedge, and walking the line */
        if (held_pin_rod >= 0) {
            VbRod *pr = &s->rods[held_pin_rod];
            pr->pin_ticks++;
            pr->pin_aim = vb_clampf(pr->pin_aim + axis * 1.4f * VB_DT,
                                    -50.0f * VB_DEG, 50.0f * VB_DEG);
            if (pr->pin_ticks >= VB_PIN_CLOCK) release_snap(s, held_pin_rod);
        }

        /* --- FLICK -------------------------------------------------------- */
        if (btn & VB_BTN_FLICK) s->hold_flick[slot]++;
        else s->hold_flick[slot] = 0;

        if (pressed & VB_BTN_FLICK) {
            int rod = glide ? nearest_rod_to_ball(s, rods, nr) : s->active_rod[slot];
            if (s->rods[rod].pin_ball >= 0) {
                release_snap(s, rod);
            } else {
                int kind = (!glide && (btn & VB_BTN_CHIP))
                         ? VB_STRIKE_CHIP : VB_STRIKE_FLICK;
                start_strike(s, rod, kind);
            }
        }

        /* Holding after the jab winds the bar up: more pace, more curve, and
         * a longer recovery if you are wrong about it (GRIP only). */
        if (!glide) {
            int rod = s->active_rod[slot];
            VbRod *r = &s->rods[rod];
            if ((btn & VB_BTN_FLICK) && r->state == VB_ROD_IDLE
                && r->pin_ball < 0 && s->hold_flick[slot] > VB_CHARGE_MIN) {
                r->state = VB_ROD_CHARGING;
                r->charge = 0;
            }
            if (r->state == VB_ROD_CHARGING) {
                if (btn & VB_BTN_FLICK) {
                    if (r->charge < VB_CHARGE_MAX) r->charge++;
                } else {
                    if (r->charge >= VB_CHARGE_MIN)
                        start_strike(s, rod, (btn & VB_BTN_CHIP)
                                              ? VB_STRIKE_CHIP : VB_STRIKE_CHARGED);
                    else { r->state = VB_ROD_IDLE; r->charge = 0; }
                }
            }
        }

        s->prev_btn[slot] = btn;
    }

    /* --- rods nobody is steering hold position, and every state machine
     *     advances whether or not it is being watched ---------------------- */
    for (int i = 0; i < VB_NRODS; i++) {
        VbRod *r = &s->rods[i];
        if (!moved[i]) {
            /* inactive bars coast to a stop — they still block passively, and
             * where you left them is part of the craft */
            float dv = VB_ROD_ACCEL * VB_DT;
            if (r->vel > 0) r->vel = vb_maxf(0.0f, r->vel - dv);
            else            r->vel = vb_minf(0.0f, r->vel + dv);
            r->off = vb_clampf(r->off + r->vel * VB_DT, -r->travel, r->travel);
        }
        if (r->bunt_on > 0) r->bunt_on--;
        if (r->whiff > 0) r->whiff--;
        if (r->flash > 0) r->flash--;

        switch (r->state) {
            case VB_ROD_WINDUP:
                if (--r->timer <= 0) {
                    r->state = VB_ROD_STRIKE;
                    r->timer = VB_STRIKE_WINDOW;
                }
                break;
            case VB_ROD_STRIKE:
                if (--r->timer <= 0) {
                    /* committed and missed. The lane behind is open for real,
                     * and it glows so both players can read it. */
                    r->state = VB_ROD_RECOVER;
                    r->timer = (r->kind == VB_STRIKE_CHARGED) ? VB_RC_CHARGED : VB_RC_FLICK;
                    r->whiff = VB_TICKS(0.28f);
                    r->kind = VB_STRIKE_NONE;
                    vb_ev(s, VB_EV_WHIFF, i, -1, 0, 0.0f, v2(r->x, vb_pad_y(r, r->pad)));
                }
                break;
            case VB_ROD_RECOVER:
                if (--r->timer <= 0) { r->state = VB_ROD_IDLE; r->charge = 0; }
                break;
            default: break;
        }

        /* a pinned ball rides the bar: this is what "walk the line" means */
        if (r->pin_ball >= 0) {
            VbBall *b = &s->balls[r->pin_ball];
            b->p = v2(r->x + r->pin_face * (VB_PAD_HALF_X + b->radius + 0.0015f),
                      vb_pad_y(r, r->pad));
            b->v = v2zero();
            b->z = 0.0f; b->vz = 0.0f;
        }
    }
}
