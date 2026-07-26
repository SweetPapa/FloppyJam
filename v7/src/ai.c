/* ai.c — Section 7. Ten opponents worth learning.
 *
 * Every one of them plays through the same physics and the same five verbs a
 * human has. What differs is temperament: how long they wait, what they reach
 * for under pressure, and what they are too proud to stop doing.
 */
#include "ai.h"
#include "rods.h"
#include "heat.h"

/* Ten personalities with legible styles. Original characters, text only. */
const VbPersona VB_PERSONAS[VB_NPERSONAS] = {
    { "BELLWETHER", "the Wall — nothing gets through, and nothing has to",
      { "I don't have to score. I only have to be there.",
        "Take your swing. I'll be here after it." },
      { 120, 190, 255 }, 0.320f, 0.055f, 0.15f, 0.90f, 0.05f, 0.15f, 0.20f },

    { "HALFPENNY", "the Gambler — chips constantly, and means it every time",
      { "Heads I win. Tails is also quite good for me.",
        "Watch the ball. No — watch where it's going to be." },
      { 255, 196,  70 }, 0.300f, 0.075f, 0.70f, 0.15f, 0.85f, 0.20f, 0.30f },

    { "TOCK", "the Metronome — bunts the heat out of you and wins slow",
      { "There is no hurry. There has never been a hurry.",
        "You'll tire of this before I do." },
      {  90, 220, 190 }, 0.310f, 0.045f, 0.25f, 0.95f, 0.10f, 0.35f, 0.15f },

    { "VEX MARQUEE", "the Showoff — always pins, and always tells you first",
      { "I'm going to catch it. Then I'm going to walk it. Then, well.",
        "Applause is optional but strongly encouraged." },
      { 255, 110, 200 }, 0.290f, 0.070f, 0.65f, 0.30f, 0.25f, 0.95f, 0.25f },

    { "ECHO NINE", "the Mirror — plays your own habits back at you",
      { "Show me something. I'll keep it.",
        "You've done that twice now." },
      { 200, 200, 220 }, 0.300f, 0.060f, 0.50f, 0.50f, 0.40f, 0.40f, 0.40f },

    { "FUSE", "the Firecracker — charges everything, recovers from nothing",
      { "One of us is going to blink and it isn't me.",
        "Fast is a strategy. Ask anyone I've beaten." },
      { 255,  90,  70 }, 0.280f, 0.085f, 0.95f, 0.05f, 0.20f, 0.25f, 0.35f },

    { "QUILL", "the Bank Clerk — never shoots straight, never has to",
      { "The walls are on my side. Ask them.",
        "Straight lines are for people in a hurry." },
      { 160, 130, 255 }, 0.285f, 0.050f, 0.45f, 0.55f, 0.30f, 0.30f, 0.95f },

    { "SABLE DRIFT", "the Curveball — bends every shot around your bars",
      { "It won't go where you think. It never does.",
        "Follow it. Go on." },
      {  70, 210, 255 }, 0.265f, 0.048f, 0.55f, 0.45f, 0.35f, 0.45f, 0.55f },

    { "NINEPIN", "the Counterpuncher — sits deep, snaps from her own goal",
      { "Come to me. Everyone does eventually.",
        "That's a long way back for you, isn't it." },
      { 250, 240, 130 }, 0.250f, 0.042f, 0.40f, 0.70f, 0.25f, 0.80f, 0.30f },

    { "AURORA KADE", "the Finale — every verb, no tell, no mercy but the beat",
      { "You've earned this table. Now hold it.",
        "Nothing personal. It is entirely personal." },
      { 255, 255, 255 }, 0.185f, 0.030f, 0.75f, 0.60f, 0.55f, 0.65f, 0.60f },
};

enum { PLAN_HOLD = 0, PLAN_CLEAR, PLAN_PASS, PLAN_PIN, PLAN_CHIP, PLAN_BANK, PLAN_BUNT };

void vb_ai_init(VbAI *a, int persona, int tier, int slot, unsigned seed) {
    for (size_t i = 0; i < sizeof(*a); i++) ((char *)a)[i] = 0;
    a->persona = vb_clampi(persona, 0, VB_NPERSONAS - 1);
    a->tier = vb_clampi(tier, 0, 2);
    a->slot = slot;
    a->rng = seed ? seed : 7u;
    /* Difficulty tunes latency, aim noise and how steadily the hand tracks.
     * That is the entire lever — no tier touches the physics, and no tier
     * gets an input a human could not produce. */
    static const float TIER_NOISE[3] = { 2.40f, 1.50f, 1.00f };
    static const float TIER_GAIN[3]  = { 5.00f, 6.50f, 8.50f };
    /* The TIER sets the reaction time and the persona only flavours it. Doing
     * it the other way round — subtracting a fixed amount from each persona's
     * own base — collapsed every quick personality onto the 180 ms floor at
     * every difficulty, so the whole ladder played at one speed. §7 wants
     * 180–320 ms across the tiers, and it wants Aurora to feel faster than
     * Bellwether at the same tier, which is what the 0.6 weight buys. */
    static const float TIER_REACT[3] = { 0.320f, 0.250f, 0.190f };
    const float NOMINAL = 0.2885f;      /* mean of the personas' bases      */
    float flavour = (VB_PERSONAS[a->persona].react_base - NOMINAL) * 0.6f;
    float react = vb_clampf(TIER_REACT[a->tier] + flavour, 0.180f, 0.320f);
    a->lag = vb_clampi((int)(react * (float)VB_TICK_HZ), 1, VB_AI_LAG - 1);
    a->noise_mul = TIER_NOISE[a->tier];
    a->gain = TIER_GAIN[a->tier];
    a->plan = PLAN_HOLD;
}

void vb_ai_observe(VbAI *a, const VbInput *human) {
    if (human->btn & VB_BTN_FLICK) a->mirror_flicks++;
    if (human->btn & VB_BTN_PIN)   a->mirror_pins++;
}

/* fold a y coordinate back inside the side walls — a bar that ignores banks
 * is not a player */
static float fold_y(float y) {
    for (int i = 0; i < 4; i++) {
        if (y >  VB_TABLE_HY) y =  2.0f * VB_TABLE_HY - y;
        else if (y < -VB_TABLE_HY) y = -2.0f * VB_TABLE_HY - y;
        else break;
    }
    return y;
}

/* What the AI is allowed to see: the table as it was `lag` ticks ago, then
 * carried forward by its own dead reckoning. This is the whole honesty model.
 * A human at 250 ms of latency does not play 250 ms behind the ball — they
 * predict, and they are wrong exactly when the ball changes direction inside
 * their reaction window. So is this. */
static void perceive(VbAI *a, const VbSim *s, V2 *p, V2 *v) {
    int bi = -1;
    for (int i = 0; i < s->nballs; i++) if (s->balls[i].alive) { bi = i; break; }
    V2 now_p = bi >= 0 ? s->balls[bi].p : v2zero();
    V2 now_v = bi >= 0 ? s->balls[bi].v : v2zero();

    int w = (int)(s->tick % VB_AI_LAG);
    a->hist_p[w] = now_p;
    a->hist_v[w] = now_v;
    if (a->hist_n < VB_AI_LAG) a->hist_n++;

    int r = w - a->lag;
    while (r < 0) r += VB_AI_LAG;
    V2 sp, sv;
    if (a->hist_n <= a->lag) { sp = now_p; sv = now_v; }
    else { sp = a->hist_p[r]; sv = a->hist_v[r]; }

    /* dead reckoning across the reaction window */
    float dt = (float)a->lag * VB_DT;
    sp = v2add(sp, v2mul(sv, dt));
    sp.y = fold_y(sp.y);
    sp.x = vb_clampf(sp.x, -VB_TABLE_HX, VB_TABLE_HX);
    *p = sp;
    *v = sv;
}

/* where the ball will cross this rod's plane, from the delayed picture */
static float intercept_y(V2 p, V2 v, float plane_x) {
    if (vb_absf(v.x) < 1e-4f) return p.y;
    float t = (plane_x - p.x) / v.x;
    if (t < 0.0f) return p.y;
    return fold_y(p.y + v.y * t);
}

VbInput vb_ai_think(VbAI *a, const VbSim *s) {
    VbInput out;
    out.axis = 0.0f;
    out.btn = 0;

    const VbPersona *pp = &VB_PERSONAS[a->persona];
    int side = a->slot / 2;
    V2 bp, bv;
    perceive(a, s, &bp, &bv);

    int bi = -1;
    for (int i = 0; i < s->nballs; i++) if (s->balls[i].alive) { bi = i; break; }
    if (bi < 0) return out;

    int grip = (s->cfg.scheme[a->slot] == VB_SCHEME_GRIP);
    int rod = grip ? s->active_rod[a->slot] : -1;

    /* Pick the bar to care about: the one the ball is arriving at. In GLIDE
     * the axis moves them all anyway, so this is only an aiming choice. */
    if (rod < 0 || vb_rod_owner_slot(s, rod) != a->slot) {
        float best = 1e9f; int pick = -1;
        for (int i = 0; i < VB_NRODS; i++) {
            if (s->rods[i].side != side) continue;
            if (vb_rod_owner_slot(s, i) != a->slot) continue;
            float d = vb_absf(s->rods[i].x - bp.x);
            if (bv.x * (s->rods[i].x - bp.x) > 0) d *= 0.55f;   /* incoming  */
            if (d < best) { best = d; pick = i; }
        }
        rod = pick;
    }
    if (rod < 0) return out;
    const VbRod *r = &s->rods[rod];

    /* re-plan on a human-ish cadence, not every tick */
    if (--a->plan_ticks <= 0) {
        a->plan_ticks = VB_TICKS(0.14f) + (int)(vb_randf(&a->rng) * VB_TICKS(0.10f));
        a->aim_bias = vb_rands(&a->rng) * pp->noise * a->noise_mul;

        float f = (side == 0) ? 1.0f : -1.0f;
        int in_our_half = (bp.x * f < 0.0f);
        float roll = vb_randf(&a->rng);
        int mirror = (a->persona == 4);
        float pin_love = pp->pin_love, chip_love = pp->chip_love;
        if (mirror) {
            /* Echo Nine builds its taste out of yours */
            int tot = a->mirror_flicks + a->mirror_pins + 1;
            pin_love = 0.20f + 0.75f * (float)a->mirror_pins / (float)tot;
            chip_love = 0.20f + 0.55f * (float)a->mirror_flicks / (float)tot;
        }

        if (in_our_half && roll < pin_love * 0.55f)        a->plan = PLAN_PIN;
        else if (roll < chip_love * 0.45f)                 a->plan = PLAN_CHIP;
        else if (s->balls[bi].heat >= 8 && roll < pp->patience * 0.5f)
                                                           a->plan = PLAN_BUNT;
        else if (roll < pp->bank_love * 0.40f)             a->plan = PLAN_BANK;
        else if (roll < 0.30f + pp->aggression * 0.5f)     a->plan = PLAN_CLEAR;
        else                                               a->plan = PLAN_PASS;
    }

    /* --- steering ------------------------------------------------------- */
    float want_y = intercept_y(bp, bv, r->x) + a->aim_bias;

    /* aim off-centre on the paddle to send it where the plan wants it */
    float f = (side == 0) ? 1.0f : -1.0f;
    float aim_off = 0.0f;
    switch (a->plan) {
        case PLAN_BANK:  aim_off = (want_y > 0 ? -0.55f : 0.55f) * r->half; break;
        case PLAN_PASS:  aim_off = (want_y > 0 ?  0.30f : -0.30f) * r->half; break;
        case PLAN_CLEAR: aim_off = 0.0f; break;
        default:         aim_off = 0.0f; break;
    }
    want_y -= aim_off;

    /* the goalie stays home; the rest chase */
    if (r->role == VB_GK) want_y = vb_clampf(want_y, -VB_TRAVEL_GK, VB_TRAVEL_GK);

    /* which paddle on the bar should meet it — the one nearest already */
    int pad = 0;
    float bestd = 1e9f;
    for (int k = 0; k < r->n; k++) {
        float d = vb_absf(vb_pad_y(r, k) - want_y);
        if (d < bestd) { bestd = d; pad = k; }
    }
    float pad_rel = ((float)pad - (float)(r->n - 1) * 0.5f) * r->spacing;
    float want_off = vb_clampf(want_y - pad_rel, -r->travel, r->travel);

    float err = want_off - r->off;
    out.axis = vb_clampf(err * a->gain, -1.0f, 1.0f);
    /* Bellwether never lunges — it arrives early and stops. */
    if (a->persona == 0) out.axis *= 0.85f;

    /* --- the trigger ----------------------------------------------------
     * A hand commits: it presses once and holds through the swing. Strobing
     * a trigger every tick is exactly the inhuman input the honesty audit
     * exists to forbid, and it whiffs constantly anyway. */
    float closing = vb_absf(bv.x);
    float ttc = closing > 1e-4f ? vb_absf(r->x - bp.x) / closing : 9.9f;
    int incoming = ((r->x - bp.x) * bv.x > 0.0f);
    int lined_up = (vb_absf(vb_pad_y(r, pad) - want_y) < r->half * 1.15f);
    int idle = (r->state == VB_ROD_IDLE || r->state == VB_ROD_CHARGING);

    /* holding a pinned ball: walk the line, then let it go as a snap */
    if (grip && r->pin_ball >= 0) {
        out.axis = ((a->rng >> 6) & 1) ? 0.6f : -0.6f;
        int hold_for = VB_TICKS(0.45f) + (int)(pp->pin_love * (float)VB_TICKS(1.0f));
        if (r->pin_ticks < hold_for) out.btn |= VB_BTN_PIN;
        /* releasing PIN *is* the snap — the rod machine does the rest */
        return out;
    }

    if (grip && a->catch_hold > 0) {
        a->catch_hold--;
        out.btn |= VB_BTN_PIN;
        return out;
    }
    if (grip && a->tap_hold > 0) {
        /* a short tap of the catch, released before contact: the bunt */
        a->tap_hold--;
        out.btn |= VB_BTN_PIN;
        return out;
    }

    if (a->swing > 0) {
        a->swing--;
        out.btn |= VB_BTN_FLICK;
        if (a->swing_chip) out.btn |= VB_BTN_CHIP;
        return out;
    }

    /* GRIP's heaviest tool: jab early, keep leaning on the bar while the ball
     * comes, and let go so the charged strike lands as it arrives. It costs a
     * long recovery if the read was wrong, which is the trade. */
    if (grip && a->charging) {
        out.btn |= VB_BTN_FLICK;
        /* release early enough that the charged windup finishes on the ball */
        if (!incoming || ttc < 0.20f) a->charging = 0;
        return out;
    }
    if (grip && idle && incoming && ttc > 0.42f && ttc < 1.30f && lined_up
        && vb_randf(&a->rng) < 0.18f + pp->aggression * 0.45f) {
        a->charging = 1;
        out.btn |= VB_BTN_FLICK;
        return out;
    }

    if (grip && a->plan == PLAN_PIN && incoming && lined_up && idle
        && ttc < 0.14f && ttc > 0.0f) {
        a->catch_hold = VB_TICKS(0.30f);
        out.btn |= VB_BTN_PIN;
        return out;
    }
    if (grip && a->plan == PLAN_BUNT && incoming && lined_up && idle
        && ttc < 0.20f && ttc > 0.10f) {
        a->tap_hold = VB_TICKS(0.045f);   /* a tap: under VB_TAP_TICKS       */
        out.btn |= VB_BTN_PIN;
        return out;
    }

    /* GLIDE is one axis and one button, and the button is cheap: a light
     * windup, a short recovery and an assist on the way out. A GLIDE player
     * swings at nearly everything, so a GLIDE opponent that waited for the
     * perfect ball would play nothing like one — and would never heat the
     * ball up, because only struck balls carry heat. */
    float window = grip ? 0.13f : 0.15f;
    float eager  = grip ? (0.62f + pp->aggression * 0.35f)
                        : (0.80f + pp->aggression * 0.2f);
    if (incoming && lined_up && idle && ttc < window && ttc > 0.0f
        && vb_randf(&a->rng) < eager) {
        a->swing = VB_TICKS(0.11f);
        a->swing_chip = (grip && a->plan == PLAN_CHIP);
        out.btn |= VB_BTN_FLICK;
        if (a->swing_chip) out.btn |= VB_BTN_CHIP;
    }

    (void)f;
    return out;
}
