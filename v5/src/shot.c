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
