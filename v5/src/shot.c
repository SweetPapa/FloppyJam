#include "shot.h"

/* 0 -> 100 in 0.9 s, then oscillate. Punishes greed and hesitation alike. */
#define METER_RISE 0.9f

void bp_shot_reset(BpShot *s, int keep_spin)
{
    float tx = s->tx, ty = s->ty;
    float aim = s->aim;
    s->charging = 0;
    s->meter = 0.0f;
    s->meter_t = 0.0f;
    s->cue_pull = 0.0f;
    s->strike_anim = 0.0f;
    s->charge_lock = 1;      /* require a fresh SPACE press before charging */
    s->aim = aim;
    if (keep_spin) { s->tx = tx; s->ty = ty; }
    else           { s->tx = s->ty = 0.0f; }
}

float bp_shot_power_curve(float p)
{
    return BP_VMAX * powf(bp_clampf(p, BP_POW_MIN, 1.0f), BP_POW_EXP);
}

float bp_shot_charge(BpShot *s, int held, float dt)
{
    /* Swallow a SPACE that was already down when we entered aiming (menu
     * selection, resume), so it cannot start a charge until let go. */
    if (s->charge_lock) {
        if (!held) s->charge_lock = 0;
        s->cue_pull = 0.0f;
        return 0.0f;
    }
    if (held) {
        if (!s->charging) { s->charging = 1; s->meter_t = 0.0f; }
        s->meter_t += dt;
        {
            /* triangle wave: up in METER_RISE, down in METER_RISE, repeat */
            float u = s->meter_t / METER_RISE;
            float f = u - floorf(u * 0.5f) * 2.0f;      /* [0,2) */
            s->meter = (f <= 1.0f) ? f : (2.0f - f);
            if (s->meter < BP_POW_MIN) s->meter = BP_POW_MIN;
        }
        s->cue_pull = s->meter;
        return 0.0f;
    }
    if (s->charging) {
        float p = s->meter;
        s->charging = 0;
        s->last_power = p;
        s->meter = 0.0f;
        s->meter_t = 0.0f;
        return (p < BP_POW_MIN) ? BP_POW_MIN : p;
    }
    s->cue_pull = 0.0f;
    return 0.0f;
}

void bp_shot_update_guide(BpShot *s, const BpWorld *w)
{
    bp_guide(w, 0, s->aim, &s->guide);
}

/* ------------------------------------------------------------------ */
/* trajectory preview                                                  */

#include <string.h>

#define PREVIEW_SEARCH_TICKS 4000   /* enough to rank candidates cheaply */

static int preview_cup(const BpWorld *w)
{
    int i;
    for (i = 0; i < w->npockets; ++i)
        if (w->pockets[i].kind == PK_CUP) return i;
    return -1;
}

/* Distance from the cue ball to whatever the player is actually trying to
 * hit: the cup, or the eight while a rack hole is still sealed. */
static float preview_goal_dist(const BpWorld *w)
{
    int cup = preview_cup(w);
    V3 goal;
    float dx, dz;
    if (w->cup_sealed) {
        int i, found = 0;
        goal = v3zero();
        for (i = 0; i < w->nballs; ++i) {
            if (w->balls[i].kind == BALL_EIGHT && w->balls[i].state != BS_GONE) {
                goal = w->balls[i].p;
                found = 1;
                break;
            }
        }
        if (!found) return 0.0f;          /* eight already potted */
    } else {
        if (cup < 0) return 1e9f;
        goal = v3(w->pockets[cup].x, w->pockets[cup].y, w->pockets[cup].z);
    }
    dx = w->balls[0].p.x - goal.x;
    dz = w->balls[0].p.z - goal.z;
    return sqrtf(dx * dx + dz * dz);
}

/* Halve the resolution in place so a very long shot still shows its whole
 * path rather than being cut off partway. */
static void preview_decimate(BpPreview *p, int *stride)
{
    int i, j = 0;
    for (i = 0; i < p->n; i += 2, ++j) {
        p->pt[j] = p->pt[i];
        /* keep a contact marker if either of the merged samples had one */
        p->hit[j] = (unsigned char)(p->hit[i] |
                    ((i + 1 < p->n) ? p->hit[i + 1] : 0));
    }
    p->n = j;
    *stride *= 2;
}

static void predict_capped(const BpWorld *w, float aim, float power,
                           float tx, float ty, int cap, BpPreview *out)
{
    BpWorld s;
    /* Base stride 3 (a sample every 3 ticks ~= every 12 cm at full speed)
     * keeps the polyline smooth; long shots halve resolution as they fill
     * the buffer, so the whole path is always shown. */
    int t, stride = 3, since = 0, i;
    int track[BP_PREVIEW_OBJ], ntrack = 0;
    unsigned char pending = 0;
    V3 last_rec;

    memset(out, 0, sizeof(*out));
    out->power = power;

    s = *w;                                  /* the copy is the whole trick */
    bp_shot_begin(&s);
    bp_strike(&s, aim, power, tx, ty);

    /* follow the object balls that are closest to the cue ball; those are
     * the ones a combo is most likely to involve */
    for (i = 1; i < s.nballs && ntrack < BP_PREVIEW_OBJ; ++i)
        if (s.balls[i].state != BS_GONE) track[ntrack++] = i;
    out->nobj = ntrack;
    for (i = 0; i < ntrack; ++i) out->obj[i].color = s.balls[track[i]].color;

    out->pt[out->n] = s.balls[0].p;
    out->hit[out->n] = 0;
    out->n++;
    last_rec = s.balls[0].p;

    for (t = 0; t < cap; ++t) {
        int e;
        bp_step(&s);
        for (e = 0; e < s.nev; ++e) {
            if (s.ev[e].a != 0) continue;
            if (s.ev[e].kind == EV_WALL)    pending |= 1;
            if (s.ev[e].kind == EV_BUMPER)  pending |= 2;
            if (s.ev[e].kind == EV_BALLHIT) pending |= 3;
        }
        if (++since >= stride) {
            since = 0;
            if (out->n >= BP_PREVIEW_MAX) preview_decimate(out, &stride);
            if (s.balls[0].state != BS_GONE) {
                /* A jump bigger than one plausible roll step is a teleport (a
                 * warp pocket) or a plunge off the world — flag it so the
                 * renderer draws a ring, not a line streaking across the hole. */
                unsigned char brk =
                    (v3len(v3sub(s.balls[0].p, last_rec)) > BP_PV_JUMP) ? BP_PV_BREAK : 0;
                out->pt[out->n] = s.balls[0].p;
                out->hit[out->n] = (unsigned char)(pending | brk);
                out->n++;
                pending = 0;
                last_rec = s.balls[0].p;
            }
            for (i = 0; i < ntrack; ++i) {
                int b = track[i];
                if (out->obj[i].n < BP_PREVIEW_OBJ_MAX && s.balls[b].state != BS_GONE)
                    out->obj[i].pt[out->obj[i].n++] = s.balls[b].p;
            }
        }
        if (s.holed || bp_settled(&s)) break;
        if (s.balls[0].state == BS_GONE) break;   /* scratched or warped out */
    }

    /* Always finish on the exact final position. Sampling every `stride` ticks
     * otherwise leaves the last point up to a stride short, which on a fast
     * shot that runs the tick cap out is most of a metre — and the end marker
     * is precisely the bit the player is reading. */
    if (s.balls[0].state != BS_GONE) {
        unsigned char brk;
        if (out->n >= BP_PREVIEW_MAX) preview_decimate(out, &stride);
        brk = (v3len(v3sub(s.balls[0].p, last_rec)) > BP_PV_JUMP) ? BP_PV_BREAK : 0;
        out->pt[out->n] = s.balls[0].p;
        out->hit[out->n] = (unsigned char)(pending | brk);
        out->n++;
    }

    out->holed = s.holed;
    out->scratched = s.scratched;
    out->bonus = s.bonus_hits;
    out->rest_dist = s.holed ? 0.0f : preview_goal_dist(&s);

    /* an object ball that never really moved is noise, not information */
    {
        int keep = 0;
        for (i = 0; i < ntrack; ++i) {
            int k = out->obj[i].n;
            if (k > 1 && v3len(v3sub(out->obj[i].pt[k - 1], out->obj[i].pt[0])) > 0.10f) {
                if (keep != i) out->obj[keep] = out->obj[i];
                ++keep;
            }
        }
        out->nobj = keep;
    }
    out->valid = 1;
}

void bp_predict(const BpWorld *w, float aim, float power, float tx, float ty,
                BpPreview *out)
{
    predict_capped(w, aim, power, tx, ty, BP_RIDE_TICKS, out);
}

static float plan_score(const BpPreview *pv)
{
    float sc = pv->holed ? -1000.0f : pv->rest_dist;
    if (pv->scratched) sc += 50.0f;
    sc -= (float)pv->bonus * 5.0f;
    return sc;
}

#define PLAN_COARSE 12
#define PLAN_REFINE 6

void bp_plan_begin(BpPlanner *pl, float aim, float tx, float ty)
{
    pl->aim = aim; pl->tx = tx; pl->ty = ty;
    pl->phase = 0;
    pl->i = 0;
    pl->best_p = 0.5f;
    pl->best_score = 1e18f;
    pl->done = 0;
}

int bp_plan_step(const BpWorld *w, BpPlanner *pl, int budget, BpPreview *out)
{
    BpPreview cur;
    int spent;

    for (spent = 0; spent < budget && !pl->done; ++spent) {
        float p, score;

        if (pl->phase == 0) {
            p = 0.10f + 0.90f * (float)pl->i / (float)(PLAN_COARSE - 1);
        } else {
            /* +-3 steps of 2.2% around the coarse winner */
            p = bp_clampf(pl->best_p + (float)(pl->i - PLAN_REFINE / 2) * 0.022f,
                          BP_POW_MIN, 1.0f);
        }

        predict_capped(w, pl->aim, p, pl->tx, pl->ty, PREVIEW_SEARCH_TICKS, &cur);
        score = plan_score(&cur);
        if (score < pl->best_score) {
            pl->best_score = score;
            pl->best_p = p;
            *out = cur;                  /* show the best so far immediately */
        }

        if (++pl->i >= (pl->phase == 0 ? PLAN_COARSE : PLAN_REFINE)) {
            pl->i = 0;
            if (pl->phase == 0 && pl->best_score > -500.0f) {
                pl->phase = 1;           /* worth refining */
            } else {
                /* The ranking ran on truncated sims for speed. Re-run the
                 * winner at full fidelity so the drawn line is the exact
                 * shot the player will get. */
                bp_predict(w, pl->aim, pl->best_p, pl->tx, pl->ty, out);
                pl->phase = 2;
                pl->done = 1;
            }
        }
    }
    return pl->done;
}
