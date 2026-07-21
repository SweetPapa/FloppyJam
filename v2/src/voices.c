#include "voices.h"
#include "sequencer.h"

#define TAU 6.283185307179586f

float note_freq(float midi) { return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f); }

static inline float frand(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x1234567u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return (float)((int32_t)x) * (1.0f / 2147483648.0f);
}

/* band-limited-ish saw via polyBLEP */
static inline float poly_blep(double t, double dt)
{
    if (t < dt) { double x = t / dt; return (float)(x + x - x * x - 1.0); }
    if (t > 1.0 - dt) { double x = (t - 1.0) / dt; return (float)(x * x + x + x + 1.0); }
    return 0.0f;
}
static inline float saw(double ph, double dt)
{
    return (float)(2.0 * ph - 1.0) - poly_blep(ph, dt);
}

void voice_clear(Voice *v) { memset(v, 0, sizeof *v); }

void voice_note(Voice *v, VoiceType type, int bus, float midi, float amp, float pan)
{
    memset(v, 0, sizeof *v);
    v->type = (uint8_t)type;
    v->active = 1;
    v->bus = (int8_t)bus;
    v->f0 = note_freq(midi);
    v->amp = amp;
    v->pan = pan;
    v->gate = 1;
    v->stage = 0;
    v->noise = 0x9E3779B9u ^ (uint32_t)(midi * 7919.0f) ^ (uint32_t)(amp * 104729.0f);
    v->lp_env = 1.0f;
    v->p_env = 1.0f;

    switch (type) {
    case V_PAD:
        v->atk = 0.9f; v->dec = 1.5f; v->sus = 0.75f; v->rel = 1.6f;
        v->f1 = v->f0 * 1.006f;                  /* detune */
        v->lp_cut = v->f0 * 6.0f + 300.0f;
        v->lp_amt = 1.0f; v->lp_env_dec = 0.0f;
        break;
    case V_BASS:
        v->atk = 0.004f; v->dec = 0.35f; v->sus = 0.55f; v->rel = 0.12f;
        v->f1 = v->f0 * 2.0f;
        v->lp_cut = 1400.0f; v->lp_amt = 1.0f;
        break;
    case V_PLUCK:
        v->atk = 0.002f; v->dec = 0.22f; v->sus = 0.0f; v->rel = 0.08f;
        v->lp_cut = v->f0 * 9.0f + 400.0f; v->lp_amt = 1.0f;
        v->lp_env_dec = 14.0f;
        break;
    case V_BELL:
        v->atk = 0.002f; v->dec = 1.1f; v->sus = 0.0f; v->rel = 0.4f;
        v->f1 = v->f0 * 2.76f;                   /* inharmonic partner */
        v->fm_amt = 0.55f;
        break;
    case V_KICK:
        v->atk = 0.0008f; v->dec = 0.26f; v->sus = 0.0f; v->rel = 0.05f;
        v->p_amt = 5.0f; v->p_env_dec = 45.0f;
        break;
    case V_HAT:
        v->atk = 0.0005f; v->dec = 0.055f; v->sus = 0.0f; v->rel = 0.02f;
        v->lp_cut = 9000.0f; v->lp_amt = 0.0f;
        break;
    case V_SNARE:
        v->atk = 0.001f; v->dec = 0.16f; v->sus = 0.0f; v->rel = 0.03f;
        v->lp_cut = 4200.0f; v->lp_amt = 0.5f;
        break;
    case V_DING:
        v->atk = 0.0008f; v->dec = 0.30f; v->sus = 0.0f; v->rel = 0.1f;
        v->f1 = v->f0 * 3.01f; v->fm_amt = 0.25f;
        break;
    case V_NOISE:
        v->atk = 0.001f; v->dec = 0.25f; v->sus = 0.0f; v->rel = 0.05f;
        v->lp_cut = 2500.0f; v->lp_amt = 1.0f; v->lp_env_dec = 6.0f;
        break;
    case V_SWEEP:
        v->atk = 0.002f; v->dec = 0.5f; v->sus = 0.0f; v->rel = 0.1f;
        v->p_amt = -0.85f; v->p_env_dec = 3.0f;
        break;
    default:
        v->atk = 0.01f; v->dec = 0.2f; v->sus = 0.0f; v->rel = 0.05f;
        break;
    }
}

/* ---- announcer word table (§8.4) ------------------------------------- */
typedef struct {
    uint8_t n;
    float   pitch[VOX_MAX_SYL];   /* midi */
    float   dur[VOX_MAX_SYL];     /* seconds */
    float   bright[VOX_MAX_SYL];  /* formant openness 0..1 */
} VoxWord;

/* 7 core words + one per microgame (15) */
static const VoxWord VOX_WORDS[] = {
    /* READY  low-high */ { 2, { 50, 57 },        { 0.15f, 0.20f },               { 0.35f, 0.75f } },
    /* GO     high     */ { 1, { 62 },            { 0.22f },                      { 0.9f } },
    /* NICE   mid warm */ { 1, { 57 },            { 0.18f },                      { 0.6f } },
    /* WIPED  desc m3  */ { 2, { 55, 52 },        { 0.16f, 0.34f },               { 0.7f, 0.2f } },
    /* OOF    groan    */ { 1, { 45 },            { 0.30f },                      { 0.15f } },
    /* HYPE            */ { 3, { 60, 64, 67 },    { 0.10f, 0.10f, 0.24f },        { 0.7f, 0.8f, 0.95f } },
    /* LAUGH           */ { 4, { 64, 60, 57, 55 },{ 0.09f, 0.09f, 0.09f, 0.14f }, { 0.8f, 0.7f, 0.6f, 0.5f } },
    /* --- microgame names, 1-2 stabs each --- */
    /* TAP       */ { 1, { 59 }, { 0.14f }, { 0.7f } },
    /* UPBEAT    */ { 2, { 55, 62 }, { 0.12f, 0.16f }, { 0.5f, 0.85f } },
    /* ALTERNATE */ { 3, { 57, 60, 55 }, { 0.10f, 0.10f, 0.16f }, { 0.6f, 0.75f, 0.5f } },
    /* HOLD      */ { 1, { 53 }, { 0.24f }, { 0.4f } },
    /* REST      */ { 1, { 50 }, { 0.28f }, { 0.25f } },
    /* SNIPE     */ { 1, { 64 }, { 0.16f }, { 0.9f } },
    /* DOUBLE    */ { 2, { 57, 57 }, { 0.11f, 0.15f }, { 0.6f, 0.6f } },
    /* STAIRS    */ { 2, { 55, 64 }, { 0.12f, 0.16f }, { 0.5f, 0.9f } },
    /* RAPID     */ { 2, { 62, 62 }, { 0.08f, 0.10f }, { 0.85f, 0.85f } },
    /* ECHO      */ { 2, { 60, 55 }, { 0.13f, 0.17f }, { 0.8f, 0.4f } },
    /* BLIND     */ { 1, { 48 }, { 0.26f }, { 0.2f } },
    /* SWAP      */ { 2, { 64, 57 }, { 0.09f, 0.13f }, { 0.9f, 0.5f } },
    /* MEASURE   */ { 2, { 55, 59 }, { 0.14f, 0.18f }, { 0.55f, 0.7f } },
    /* CALLBACK  */ { 2, { 59, 52 }, { 0.12f, 0.20f }, { 0.75f, 0.35f } },
    /* FINALE    */ { 3, { 52, 59, 67 }, { 0.14f, 0.14f, 0.32f }, { 0.4f, 0.7f, 1.0f } }
};
#define VOX_WORD_COUNT ((int)(sizeof(VOX_WORDS)/sizeof(VOX_WORDS[0])))

void voice_vox(Voice *v, int word_id)
{
    if (word_id < 0) word_id = 0;
    if (word_id >= VOX_WORD_COUNT) word_id = VOX_WORD_COUNT - 1;
    const VoxWord *w = &VOX_WORDS[word_id];
    memset(v, 0, sizeof *v);
    v->type = V_VOX;
    v->active = 1;
    v->bus = -2;
    v->amp = 0.55f;
    v->pan = 0.0f;
    v->noise = 0x2545F491u + (uint32_t)word_id * 2654435761u;
    v->syl_count = w->n;
    v->syl_idx = 0;
    v->syl_t = 0.0f;
    for (int i = 0; i < w->n; i++) {
        v->syl_pitch[i] = w->pitch[i];
        v->syl_dur[i] = w->dur[i];
        v->syl_bright[i] = w->bright[i];
    }
    v->f0 = note_freq(v->syl_pitch[0]);
}

void voice_release(Voice *v)
{
    if (!v->active) return;
    v->gate = 0;
    v->stage = 2;
}

/* one-pole lowpass */
static inline float lp1(float in, float *z, float cut)
{
    float a = 1.0f - expf(-TAU * cut / (float)SAMPLE_RATE);
    if (a > 1.0f) a = 1.0f;
    *z += a * (in - *z);
    return *z;
}

/* state-variable bandpass, used for the announcer formants */
static inline float svf_bp(float in, float *lo, float *bp, float cut, float q)
{
    float f = 2.0f * sinf(3.14159265f * g_clampf(cut, 40.0f, 9000.0f) / (float)SAMPLE_RATE);
    float hi = in - *lo - q * (*bp);
    *bp += f * hi;
    *lo += f * (*bp);
    return *bp;
}

void voice_render(Voice *v, float *outL, float *outR, int frames, float pitch_mul)
{
    if (!v->active) return;
    const float sr = (float)SAMPLE_RATE;
    const float inv_sr = 1.0f / sr;

    for (int i = 0; i < frames; i++) {
        /* ---- amplitude envelope ---- */
        if (v->stage == 0) {
            v->env += (v->atk > 0.0f) ? inv_sr / v->atk : 1.0f;
            if (v->env >= 1.0f) { v->env = 1.0f; v->stage = 1; }
        } else if (v->stage == 1) {
            float k = (v->dec > 0.0f) ? expf(-inv_sr / v->dec * 3.0f) : 0.0f;
            v->env = v->sus + (v->env - v->sus) * k;
            if (v->sus <= 0.0001f && v->env < 0.0006f) { v->active = 0; return; }
        } else {
            float k = (v->rel > 0.0f) ? expf(-inv_sr / v->rel * 4.0f) : 0.0f;
            v->env *= k;
            if (v->env < 0.0005f) { v->active = 0; return; }
        }

        if (v->lp_env_dec > 0.0f) v->lp_env += (0.0f - v->lp_env) * (v->lp_env_dec * inv_sr);
        if (v->p_env_dec > 0.0f)  v->p_env  += (0.0f - v->p_env)  * (v->p_env_dec * inv_sr);

        float f = v->f0 * pitch_mul;
        float s = 0.0f;

        switch (v->type) {
        case V_PAD: {
            double dt1 = (double)(f * inv_sr);
            double dt2 = (double)(v->f1 * pitch_mul * inv_sr);
            v->ph += dt1;  if (v->ph >= 1.0) v->ph -= 1.0;
            v->ph2 += dt2; if (v->ph2 >= 1.0) v->ph2 -= 1.0;
            s = 0.5f * (saw(v->ph, dt1) + saw(v->ph2, dt2));
            s = lp1(s, &v->lp_z1, v->lp_cut * pitch_mul);
            s = lp1(s, &v->lp_z2, v->lp_cut * pitch_mul);
            s *= 0.5f;
        } break;
        case V_BASS: {
            v->ph += (double)(f * inv_sr);  if (v->ph >= 1.0) v->ph -= 1.0;
            v->ph2 += (double)(f * 2.0f * inv_sr); if (v->ph2 >= 1.0) v->ph2 -= 1.0;
            float tri = 4.0f * fabsf((float)v->ph2 - 0.5f) - 1.0f;
            s = sinf(TAU * (float)v->ph) + 0.12f * tri;
            s = lp1(s, &v->lp_z1, v->lp_cut * pitch_mul);
        } break;
        case V_PLUCK: {
            v->ph += (double)(f * inv_sr); if (v->ph >= 1.0) v->ph -= 1.0;
            float tri = 4.0f * fabsf((float)v->ph - 0.5f) - 1.0f;
            float cut = (200.0f + v->lp_cut * v->lp_env) * pitch_mul;
            s = lp1(tri, &v->lp_z1, cut);
        } break;
        case V_BELL: {
            v->ph2 += (double)(v->f1 * pitch_mul * inv_sr); if (v->ph2 >= 1.0) v->ph2 -= 1.0;
            float mod = sinf(TAU * (float)v->ph2) * v->fm_amt;
            v->ph += (double)(f * inv_sr); if (v->ph >= 1.0) v->ph -= 1.0;
            s = sinf(TAU * (float)v->ph + mod * 3.0f) * 0.8f;
        } break;
        case V_DING: {
            v->ph2 += (double)(v->f1 * pitch_mul * inv_sr); if (v->ph2 >= 1.0) v->ph2 -= 1.0;
            v->ph += (double)(f * inv_sr); if (v->ph >= 1.0) v->ph -= 1.0;
            s = sinf(TAU * (float)v->ph) * 0.85f
              + sinf(TAU * (float)v->ph2) * v->fm_amt;
        } break;
        case V_KICK: {
            float pf = f * (1.0f + v->p_amt * v->p_env);
            v->ph += (double)(pf * inv_sr); if (v->ph >= 1.0) v->ph -= 1.0;
            s = sinf(TAU * (float)v->ph);
            if (v->t < 90) s += frand(&v->noise) * 0.5f * (1.0f - v->t / 90.0f);
        } break;
        case V_HAT: {
            float n = frand(&v->noise);
            float hp = n - lp1(n, &v->lp_z1, 3000.0f);
            s = hp * 0.7f;
        } break;
        case V_SNARE: {
            float n = frand(&v->noise);
            v->ph += (double)(f * inv_sr); if (v->ph >= 1.0) v->ph -= 1.0;
            float tone = sinf(TAU * (float)v->ph) * 0.35f;
            s = lp1(n, &v->lp_z1, v->lp_cut) * 0.8f + tone;
        } break;
        case V_NOISE: {
            float n = frand(&v->noise);
            s = lp1(n, &v->lp_z1, 300.0f + v->lp_cut * v->lp_env);
        } break;
        case V_SWEEP: {
            float pf = f * (1.0f + v->p_amt * (1.0f - v->p_env));
            if (pf < 20.0f) pf = 20.0f;
            v->ph += (double)(pf * inv_sr); if (v->ph >= 1.0) v->ph -= 1.0;
            s = sinf(TAU * (float)v->ph);
        } break;
        case V_VOX: {
            /* syllable stabs: pulse + noise through two formant bandpasses */
            if (v->syl_idx >= v->syl_count) { v->active = 0; return; }
            v->syl_t += inv_sr;
            float dur = v->syl_dur[v->syl_idx];
            float u = v->syl_t / (dur > 0.0f ? dur : 0.1f);
            if (u >= 1.0f) {
                v->syl_idx++;
                v->syl_t = 0.0f;
                if (v->syl_idx >= v->syl_count) { v->active = 0; return; }
                u = 0.0f;
            }
            float pitch = v->syl_pitch[v->syl_idx];
            float bright = v->syl_bright[v->syl_idx];
            /* a little intra-syllable pitch glide gives it that toy-robot swagger */
            float fq = note_freq(pitch + (bright - 0.5f) * 2.0f * u) * pitch_mul;
            v->ph += (double)(fq * inv_sr); if (v->ph >= 1.0) v->ph -= 1.0;
            float pulse = (v->ph < 0.42) ? 1.0f : -1.0f;
            float src = pulse * 0.7f + frand(&v->noise) * 0.25f;
            float f1c = 350.0f + bright * 500.0f;
            float f2c = 900.0f + bright * 1600.0f;
            float a = svf_bp(src, &v->svf_l[0], &v->svf_b[0], f1c, 0.35f);
            float b = svf_bp(src, &v->svf_l[1], &v->svf_b[1], f2c, 0.45f);
            /* envelope per syllable: quick attack, gentle fall, hard tail */
            float e = (u < 0.12f) ? (u / 0.12f) : (1.0f - (u - 0.12f) / 0.88f);
            e = g_clampf(e, 0.0f, 1.0f);
            s = (a * 1.2f + b * 0.9f) * e * e;
            v->env = 1.0f;
        } break;
        default: break;
        }

        float amp = s * v->env * v->amp;
        float pl = 0.5f * (1.0f - v->pan);
        float pr = 0.5f * (1.0f + v->pan);
        outL[i] += amp * (pl + 0.5f);
        outR[i] += amp * (pr + 0.5f);
        v->t++;
    }
    v->age++;
}
