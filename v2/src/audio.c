#include "audio.h"
#include "voices.h"
#include "raylib.h"

#include <stdatomic.h>

#define MAX_FRAMES 4096
#define MUSIC_VOICES 16
#define SFX_VOICES 10
#define VOX_VOICES 4
#define PAD_VOICES 4
#define BUS_COUNT (STEM_COUNT + 2)   /* + sfx bus, + announcer bus */
#define BUS_SFX (STEM_COUNT)
#define BUS_VOX (STEM_COUNT + 1)

#define CMD_RING 256
#define BAR_RING 32
#define FLAVOR_RING 32

/* ------------------------------------------------------------------ */
/* shared state                                                        */
/* ------------------------------------------------------------------ */
static AudioStream s_stream;
static bool s_ready = false;

static Genome s_genome;

/* command ring: game thread writes head, audio thread reads tail */
static SeqCmd s_cmd[CMD_RING];
static _Atomic unsigned s_cmd_head = 0;
static _Atomic unsigned s_cmd_tail = 0;

/* bar ring: audio thread writes, game thread reads */
static BarInfo s_bars[BAR_RING];
static _Atomic unsigned s_bar_head = 0;
static _Atomic unsigned s_bar_tail = 0;

/* flavors, indexed by bar % FLAVOR_RING, written by audio thread only */
static uint8_t s_flavor[FLAVOR_RING];

/* clock snapshot */
static _Atomic uint64_t s_clock = 0;
static _Atomic int      s_bar_now = 0;
static _Atomic int      s_step_now = 0;
static _Atomic int      s_running = 0;
static _Atomic unsigned s_bpm_bits = 0;
static _Atomic unsigned s_spb_bits = 0;
static _Atomic unsigned s_bass_lvl = 0;
static _Atomic unsigned s_mast_lvl = 0;
static _Atomic uint64_t s_step_start = 0;
static _Atomic uint64_t s_step_end = 0;

static inline unsigned f2b(float f) { unsigned u; memcpy(&u, &f, 4); return u; }
static inline float    b2f(unsigned u) { float f; memcpy(&f, &u, 4); return f; }

/* ------------------------------------------------------------------ */
/* audio-thread only state                                             */
/* ------------------------------------------------------------------ */
static Voice a_music[MUSIC_VOICES];
static Voice a_pad[PAD_VOICES];
static Voice a_sfx[SFX_VOICES];
static Voice a_vox[VOX_VOICES];

static float a_bus_l[BUS_COUNT][MAX_FRAMES];
static float a_bus_r[BUS_COUNT][MAX_FRAMES];

static float a_gain[BUS_COUNT];
static float a_gain_tgt[BUS_COUNT];
static float a_gain_rate[BUS_COUNT];

static double a_pos = 0.0;         /* transport position in samples (fractional) */
static int    a_bar = 0;
static int    a_step = 0;
static int    a_transport = 0;
static float  a_bpm = 120.0f;
static float  a_bpm_pending = 120.0f;
static int    a_bpm_apply_bar = -1;

static BarInfo a_cur, a_next;
static BarPattern a_pat;
static int    a_metronome = 0;
static int    a_drum_mute = 0;
static uint32_t a_vage = 1;

/* master fx */
static float a_lp_cut = 20000.0f, a_lp_cut_tgt = 20000.0f, a_lp_rate = 0.0f;
static float a_lp_zl = 0.0f, a_lp_zr = 0.0f;
static float a_pitch = 1.0f, a_pitch_tgt = 1.0f, a_pitch_rate = 0.0f;
static float a_mgain = 1.0f, a_mgain_tgt = 1.0f, a_mgain_rate = 0.0f;

/* pending bar-boundary gain commands */
typedef struct { int stem; float gain; int bar; int used; } PendingGain;
static PendingGain a_pending[32];

static const float STEM_BASE_GAIN[STEM_COUNT] = {
    0.42f,  /* pad   */
    0.95f,  /* kick  */
    0.34f,  /* hat   */
    0.68f,  /* bass  */
    0.40f,  /* pluck */
    0.34f,  /* bell  */
    0.50f   /* snare */
};

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */
static Voice *alloc_voice(Voice *pool, int n)
{
    Voice *best = NULL;
    for (int i = 0; i < n; i++) {
        if (!pool[i].active) return &pool[i];
        if (!best || pool[i].age < best->age) best = &pool[i];
    }
    return best;
}

static void publish_bar(const BarInfo *b)
{
    unsigned h = atomic_load_explicit(&s_bar_head, memory_order_relaxed);
    unsigned t = atomic_load_explicit(&s_bar_tail, memory_order_acquire);
    if (h - t >= BAR_RING) return;  /* game thread stalled; drop (it will resync) */
    s_bars[h % BAR_RING] = *b;
    atomic_store_explicit(&s_bar_head, h + 1, memory_order_release);
}

static void build_bar_grid(BarInfo *out, int bar_index, uint64_t start, float bpm_from, float bpm_to)
{
    out->bar_index = bar_index;
    out->bpm = bpm_to;
    out->flavor = (BarFlavor)s_flavor[((bar_index % FLAVOR_RING) + FLAVOR_RING) % FLAVOR_RING];
    uint64_t t = start;
    out->step_sample[0] = t;
    for (int s = 0; s < STEPS_PER_BAR; s++) {
        /* BPM ramps over exactly one beat, applied on the downbeat (§7) */
        float k = g_clampf((float)s / (float)STEPS_PER_BEAT, 0.0f, 1.0f);
        float bpm = g_lerpf(bpm_from, bpm_to, k);
        float spb = (float)SAMPLE_RATE * 60.0f / bpm;
        uint64_t dur = (uint64_t)(spb / (float)STEPS_PER_BEAT + 0.5f);
        t += dur;
        out->step_sample[s + 1] = t;
    }
}

static void trigger_step(int step)
{
    const BarPattern *p = &a_pat;

    if (p->chord_change[step]) {
        /* pad follows the chord; slow env, quantised to the bar (§8.1) */
        for (int i = 0; i < PAD_VOICES; i++) voice_release(&a_pad[i]);
        for (int i = 0; i < p->chord_count && i < PAD_VOICES; i++) {
            Voice *v = &a_pad[i];
            float pan = -0.5f + (float)i * (1.0f / 1.6f);
            voice_note(v, V_PAD, STEM_PAD, (float)p->chord_notes[i], 0.55f, g_clampf(pan, -1, 1));
            v->age = a_vage++;
        }
    }
    if (p->is_rest) {
        if (a_metronome && (step % STEPS_PER_BEAT) == 0) {
            Voice *v = alloc_voice(a_sfx, SFX_VOICES);
            voice_note(v, V_HAT, BUS_SFX, 90.0f, step == 0 ? 0.5f : 0.32f, 0.0f);
            v->dec = 0.03f;
            v->age = a_vage++;
        }
        return;
    }

    if (p->kick[step]) {
        Voice *v = alloc_voice(a_music, MUSIC_VOICES);
        voice_note(v, V_KICK, STEM_KICK, 41.0f, 0.95f, 0.0f);
        v->age = a_vage++;
    }
    if (p->hat[step]) {
        Voice *v = alloc_voice(a_music, MUSIC_VOICES);
        voice_note(v, V_HAT, STEM_HAT, 90.0f, p->hat[step] == 2 ? 0.55f : 0.32f, 0.15f);
        v->age = a_vage++;
    }
    if (p->snare[step]) {
        Voice *v = alloc_voice(a_music, MUSIC_VOICES);
        voice_note(v, V_SNARE, STEM_SNARE, 62.0f, 0.6f, -0.1f);
        v->age = a_vage++;
    }
    if (p->bass[step] && p->bass_note[step] >= 0) {
        Voice *v = alloc_voice(a_music, MUSIC_VOICES);
        voice_note(v, V_BASS, STEM_BASS, (float)p->bass_note[step], 0.8f, 0.0f);
        v->age = a_vage++;
    }
    if (p->pluck[step] && p->pluck_note[step] >= 0) {
        Voice *v = alloc_voice(a_music, MUSIC_VOICES);
        voice_note(v, V_PLUCK, STEM_PLUCK, (float)p->pluck_note[step], 0.5f, 0.25f);
        v->age = a_vage++;
    }
    if (p->bell_note[step] >= 0) {
        Voice *v = alloc_voice(a_music, MUSIC_VOICES);
        voice_note(v, V_BELL, STEM_BELL, (float)p->bell_note[step], 0.45f, -0.25f);
        v->age = a_vage++;
    }
    if (a_metronome && (step % STEPS_PER_BEAT) == 0) {
        Voice *v = alloc_voice(a_sfx, SFX_VOICES);
        voice_note(v, V_HAT, BUS_SFX, 96.0f, step == 0 ? 0.55f : 0.35f, 0.0f);
        v->dec = 0.025f;
        v->age = a_vage++;
    }
}

static void apply_pending_gains(int bar)
{
    for (int i = 0; i < 32; i++) {
        if (!a_pending[i].used) continue;
        if (a_pending[i].bar >= 0 && a_pending[i].bar > bar) continue;
        int st = a_pending[i].stem;
        if (st >= 0 && st < STEM_COUNT) {
            a_gain_tgt[st] = a_pending[i].gain * STEM_BASE_GAIN[st];
            a_gain_rate[st] = 6.0f;   /* ~150 ms fade */
        }
        a_pending[i].used = 0;
    }
}

static void advance_bar(void)
{
    a_cur = a_next;
    a_bar = a_cur.bar_index;
    /* refresh flavor: the game thread may have set it after the grid was built */
    a_cur.flavor = (BarFlavor)s_flavor[((a_bar % FLAVOR_RING) + FLAVOR_RING) % FLAVOR_RING];
    seq_bar_pattern(&s_genome, a_bar, a_cur.flavor, &a_pat);
    apply_pending_gains(a_bar);

    float from = a_bpm;
    int nb = a_bar + 1;
    if (a_bpm_apply_bar >= 0 && nb >= a_bpm_apply_bar) {
        a_bpm = a_bpm_pending;
        a_bpm_apply_bar = -1;
    }
    build_bar_grid(&a_next, nb, a_cur.step_sample[STEPS_PER_BAR], from, a_bpm);
    publish_bar(&a_next);
}

static void transport_start(float bpm)
{
    a_bpm = bpm;
    a_bpm_pending = bpm;
    a_bpm_apply_bar = -1;
    a_pos = 0.0;
    a_bar = 0;
    a_step = 0;
    a_transport = 1;
    memset(a_music, 0, sizeof a_music);
    memset(a_pad, 0, sizeof a_pad);
    build_bar_grid(&a_cur, 0, 0, bpm, bpm);
    seq_bar_pattern(&s_genome, 0, a_cur.flavor, &a_pat);
    build_bar_grid(&a_next, 1, a_cur.step_sample[STEPS_PER_BAR], bpm, bpm);
    publish_bar(&a_cur);
    publish_bar(&a_next);
    trigger_step(0);
    atomic_store(&s_running, 1);
}

static void handle_cmd(const SeqCmd *c)
{
    switch (c->type) {
    case CMD_START: transport_start(c->fa > 0 ? c->fa : (float)s_genome.bpm_base); break;
    case CMD_STOP:
        a_transport = 0;
        atomic_store(&s_running, 0);
        for (int i = 0; i < MUSIC_VOICES; i++) voice_release(&a_music[i]);
        for (int i = 0; i < PAD_VOICES; i++) voice_release(&a_pad[i]);
        break;
    case CMD_SET_BPM:
        a_bpm_pending = c->fa;
        a_bpm_apply_bar = c->ia;
        break;
    case CMD_STEM_GAIN: {
        if (c->ib < 0) {
            if (c->ia >= 0 && c->ia < STEM_COUNT) {
                a_gain_tgt[c->ia] = c->fa * STEM_BASE_GAIN[c->ia];
                a_gain_rate[c->ia] = (c->fb > 0.0f) ? (3.0f / c->fb) : 6.0f;
            }
        } else {
            for (int i = 0; i < 32; i++) {
                if (!a_pending[i].used) {
                    a_pending[i].used = 1;
                    a_pending[i].stem = c->ia;
                    a_pending[i].gain = c->fa;
                    a_pending[i].bar = c->ib;
                    break;
                }
            }
        }
    } break;
    case CMD_BAR_FLAVOR:
        s_flavor[((c->ia % FLAVOR_RING) + FLAVOR_RING) % FLAVOR_RING] = (uint8_t)c->ib;
        if (c->ia == a_bar) {
            a_cur.flavor = (BarFlavor)c->ib;
            seq_bar_pattern(&s_genome, a_bar, a_cur.flavor, &a_pat);
        }
        break;
    case CMD_MASTER_LP:
        a_lp_cut_tgt = c->fa;
        a_lp_rate = (c->fb > 0.0f) ? (3.0f / c->fb) : 40.0f;
        break;
    case CMD_MASTER_PITCH:
        a_pitch_tgt = c->fa;
        a_pitch_rate = (c->fb > 0.0f) ? (3.0f / c->fb) : 40.0f;
        break;
    case CMD_MASTER_GAIN:
        a_mgain_tgt = c->fa;
        a_mgain_rate = (c->fb > 0.0f) ? (3.0f / c->fb) : 40.0f;
        break;
    case CMD_METRONOME: a_metronome = c->ia; break;
    case CMD_DRUM_MUTE: a_drum_mute = c->ia; break;
    case CMD_SFX: {
        Voice *v = alloc_voice(a_sfx, SFX_VOICES);
        switch (c->ia) {
        case SFX_PERFECT:
            voice_note(v, V_DING, BUS_SFX, c->fa > 0 ? c->fa : 84.0f, 0.40f, 0.0f);
            break;
        case SFX_GOOD:
            voice_note(v, V_HAT, BUS_SFX, 80.0f, 0.22f, 0.0f);
            v->dec = 0.04f;
            break;
        case SFX_MISS:
            voice_note(v, V_NOISE, BUS_SFX, 40.0f, 0.35f, 0.0f);
            v->lp_cut = 900.0f; v->dec = 0.12f;
            break;
        case SFX_HONK:
            voice_note(v, V_BASS, BUS_SFX, 41.0f, 0.75f, 0.0f);
            v->dec = 0.45f; v->sus = 0.4f; v->rel = 0.2f;
            break;
        case SFX_SCRATCH:
            voice_note(v, V_SWEEP, BUS_SFX, 76.0f, 0.5f, 0.0f);
            break;
        case SFX_STAMP:
            voice_note(v, V_NOISE, BUS_SFX, 40.0f, 0.7f, 0.0f);
            v->lp_cut = 3000.0f; v->dec = 0.2f;
            break;
        case SFX_CARD:
            voice_note(v, V_DING, BUS_SFX, c->fa > 0 ? c->fa : 79.0f, 0.30f, 0.0f);
            v->dec = 0.18f;
            break;
        case SFX_UI:
            voice_note(v, V_PLUCK, BUS_SFX, 72.0f, 0.3f, 0.0f);
            break;
        case SFX_CLICK:
            voice_note(v, V_HAT, BUS_SFX, 96.0f, 0.4f, 0.0f);
            v->dec = 0.02f;
            break;
        case SFX_HEAL:
            voice_note(v, V_BELL, BUS_SFX, 76.0f, 0.4f, 0.0f);
            break;
        default:
            voice_note(v, V_HAT, BUS_SFX, 84.0f, 0.25f, 0.0f);
            break;
        }
        v->age = a_vage++;
    } break;
    case CMD_ANNOUNCE: {
        Voice *v = alloc_voice(a_vox, VOX_VOICES);
        voice_vox(v, c->ia);
        v->age = a_vage++;
    } break;
    default: break;
    }
}

static void drain_cmds(void)
{
    unsigned t = atomic_load_explicit(&s_cmd_tail, memory_order_relaxed);
    unsigned h = atomic_load_explicit(&s_cmd_head, memory_order_acquire);
    while (t != h) {
        SeqCmd c = s_cmd[t % CMD_RING];
        handle_cmd(&c);
        t++;
    }
    atomic_store_explicit(&s_cmd_tail, t, memory_order_release);
}

/* ------------------------------------------------------------------ */
/* the callback                                                        */
/* ------------------------------------------------------------------ */
static void render_block(float *out, int frames)
{
    for (int b = 0; b < BUS_COUNT; b++) {
        memset(a_bus_l[b], 0, sizeof(float) * (size_t)frames);
        memset(a_bus_r[b], 0, sizeof(float) * (size_t)frames);
    }

    /* --- transport: chunk to step boundaries ---------------------- */
    int done = 0;
    while (done < frames) {
        int chunk = frames - done;
        if (a_transport) {
            uint64_t next_edge = a_cur.step_sample[a_step + 1];
            double remain = (double)next_edge - a_pos;
            if (remain <= 0.0) remain = 0.0;
            double rate = (double)a_pitch;
            if (rate < 0.02) rate = 0.02;
            int to_edge = (int)(remain / rate);
            if (to_edge < chunk) chunk = (to_edge > 0) ? to_edge : 1;
        }
        if (chunk > frames - done) chunk = frames - done;
        if (chunk <= 0) chunk = 1;

        /* render every voice into its bus */
        for (int i = 0; i < PAD_VOICES; i++) {
            if (!a_pad[i].active) continue;
            int bus = a_pad[i].bus;
            voice_render(&a_pad[i], a_bus_l[bus] + done, a_bus_r[bus] + done, chunk, a_pitch);
        }
        for (int i = 0; i < MUSIC_VOICES; i++) {
            if (!a_music[i].active) continue;
            int bus = a_music[i].bus;
            voice_render(&a_music[i], a_bus_l[bus] + done, a_bus_r[bus] + done, chunk, a_pitch);
        }
        for (int i = 0; i < SFX_VOICES; i++) {
            if (!a_sfx[i].active) continue;
            voice_render(&a_sfx[i], a_bus_l[BUS_SFX] + done, a_bus_r[BUS_SFX] + done, chunk, 1.0f);
        }
        for (int i = 0; i < VOX_VOICES; i++) {
            if (!a_vox[i].active) continue;
            voice_render(&a_vox[i], a_bus_l[BUS_VOX] + done, a_bus_r[BUS_VOX] + done, chunk, 1.0f);
        }

        done += chunk;
        a_pos += (double)chunk * (double)a_pitch;

        if (a_transport) {
            while (a_pos >= (double)a_cur.step_sample[a_step + 1]) {
                a_step++;
                if (a_step >= STEPS_PER_BAR) {
                    a_step = 0;
                    advance_bar();
                }
                trigger_step(a_step);
                atomic_store_explicit(&s_step_now, a_step, memory_order_relaxed);
                atomic_store_explicit(&s_bar_now, a_bar, memory_order_relaxed);
                atomic_store_explicit(&s_step_start, a_cur.step_sample[a_step], memory_order_relaxed);
                atomic_store_explicit(&s_step_end, a_cur.step_sample[a_step + 1], memory_order_relaxed);
            }
        }
    }

    /* --- mix buses through master ---------------------------------- */
    float bass_peak = 0.0f, master_peak = 0.0f;
    const float inv_sr = 1.0f / (float)SAMPLE_RATE;
    for (int i = 0; i < frames; i++) {
        float l = 0.0f, r = 0.0f;
        for (int b = 0; b < BUS_COUNT; b++) {
            float target = (b < STEM_COUNT) ? a_gain_tgt[b] : 1.0f;
            if (b < STEM_COUNT) {
                float rate = a_gain_rate[b] > 0.0f ? a_gain_rate[b] : 6.0f;
                a_gain[b] += (target - a_gain[b]) * (rate * inv_sr);
                float g = a_gain[b];
                if (a_drum_mute && (b == STEM_KICK || b == STEM_HAT || b == STEM_SNARE)) g = 0.0f;
                l += a_bus_l[b][i] * g;
                r += a_bus_r[b][i] * g;
                if (b == STEM_BASS) {
                    float m = fabsf(a_bus_l[b][i] * g);
                    if (m > bass_peak) bass_peak = m;
                }
            } else {
                l += a_bus_l[b][i];
                r += a_bus_r[b][i];
            }
        }

        /* master lowpass (WIPE collapse / damage states) */
        a_lp_cut += (a_lp_cut_tgt - a_lp_cut) * (a_lp_rate * inv_sr);
        float a = 1.0f - expf(-6.2831853f * g_clampf(a_lp_cut, 60.0f, 20000.0f) * inv_sr);
        a_lp_zl += a * (l - a_lp_zl);
        a_lp_zr += a * (r - a_lp_zr);
        l = a_lp_zl; r = a_lp_zr;

        a_pitch += (a_pitch_tgt - a_pitch) * (a_pitch_rate * inv_sr);
        a_mgain += (a_mgain_tgt - a_mgain) * (a_mgain_rate * inv_sr);

        l *= a_mgain * 0.9f;
        r *= a_mgain * 0.9f;

        /* soft-clip limiter */
        l = tanhf(l * 1.15f);
        r = tanhf(r * 1.15f);

        out[i * 2 + 0] = l;
        out[i * 2 + 1] = r;
        float m = fabsf(l);
        if (m > master_peak) master_peak = m;
    }

    atomic_store_explicit(&s_clock, (uint64_t)a_pos, memory_order_relaxed);
    atomic_store_explicit(&s_bpm_bits, f2b(a_bpm), memory_order_relaxed);
    atomic_store_explicit(&s_spb_bits, f2b((float)SAMPLE_RATE * 60.0f / a_bpm), memory_order_relaxed);
    atomic_store_explicit(&s_bass_lvl, f2b(bass_peak), memory_order_relaxed);
    atomic_store_explicit(&s_mast_lvl, f2b(master_peak), memory_order_relaxed);
}

static void audio_callback(void *buffer, unsigned int frames)
{
    float *out = (float *)buffer;
    drain_cmds();
    unsigned int done = 0;
    while (done < frames) {
        unsigned int n = frames - done;
        if (n > MAX_FRAMES) n = MAX_FRAMES;
        render_block(out + done * 2, (int)n);
        done += n;
    }
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */
bool audio_init(void)
{
    SetAudioStreamBufferSizeDefault(1024);
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return false;
    s_stream = LoadAudioStream(SAMPLE_RATE, 32, 2);
    SetAudioStreamCallback(s_stream, audio_callback);
    for (int i = 0; i < STEM_COUNT; i++) { a_gain[i] = 0.0f; a_gain_tgt[i] = 0.0f; a_gain_rate[i] = 6.0f; }
    memset(s_flavor, 0, sizeof s_flavor);
    PlayAudioStream(s_stream);
    s_ready = true;
    return true;
}

void audio_shutdown(void)
{
    if (!s_ready) return;
    StopAudioStream(s_stream);
    UnloadAudioStream(s_stream);
    CloseAudioDevice();
    s_ready = false;
}

void audio_set_genome(const Genome *g) { s_genome = *g; }

void audio_push(SeqCmd c)
{
    unsigned h = atomic_load_explicit(&s_cmd_head, memory_order_relaxed);
    unsigned t = atomic_load_explicit(&s_cmd_tail, memory_order_acquire);
    if (h - t >= CMD_RING) return;
    s_cmd[h % CMD_RING] = c;
    atomic_store_explicit(&s_cmd_head, h + 1, memory_order_release);
}

bool audio_pop_bar(BarInfo *out)
{
    unsigned t = atomic_load_explicit(&s_bar_tail, memory_order_relaxed);
    unsigned h = atomic_load_explicit(&s_bar_head, memory_order_acquire);
    if (t == h) return false;
    *out = s_bars[t % BAR_RING];
    atomic_store_explicit(&s_bar_tail, t + 1, memory_order_release);
    return true;
}

void audio_snapshot(ClockSnap *out)
{
    out->sample_clock = atomic_load_explicit(&s_clock, memory_order_relaxed);
    out->bpm = b2f(atomic_load_explicit(&s_bpm_bits, memory_order_relaxed));
    out->samples_per_beat = b2f(atomic_load_explicit(&s_spb_bits, memory_order_relaxed));
    out->bar = atomic_load_explicit(&s_bar_now, memory_order_relaxed);
    out->step = atomic_load_explicit(&s_step_now, memory_order_relaxed);
    out->running = atomic_load_explicit(&s_running, memory_order_relaxed);
    uint64_t a = atomic_load_explicit(&s_step_start, memory_order_relaxed);
    uint64_t b = atomic_load_explicit(&s_step_end, memory_order_relaxed);
    float sp = (b > a) ? (float)((double)(out->sample_clock - a) / (double)(b - a)) : 0.0f;
    sp = g_clampf(sp, 0.0f, 1.0f);
    int stp = out->step;
    out->beat_phase = ((stp % STEPS_PER_BEAT) + sp) / (float)STEPS_PER_BEAT;
    out->bar_phase = (stp + sp) / (float)STEPS_PER_BAR;
    if (out->samples_per_beat <= 0.0f) out->samples_per_beat = 24000.0f;
    if (out->bpm <= 0.0f) out->bpm = 120.0f;
}

void audio_start(float bpm) { SeqCmd c = { CMD_START, 0, 0, bpm, 0 }; audio_push(c); }
void audio_stop(void) { SeqCmd c = { CMD_STOP, 0, 0, 0, 0 }; audio_push(c); }
void audio_stem_gain(int stem, float gain, int apply_bar)
{ SeqCmd c = { CMD_STEM_GAIN, stem, apply_bar, gain, 0.15f }; audio_push(c); }
void audio_bar_flavor(int bar, BarFlavor f)
{ SeqCmd c = { CMD_BAR_FLAVOR, bar, (int)f, 0, 0 }; audio_push(c); }
void audio_set_bpm(float bpm, int apply_bar)
{ SeqCmd c = { CMD_SET_BPM, apply_bar, 0, bpm, 0 }; audio_push(c); }
void audio_sfx(int id, float param)
{ SeqCmd c = { CMD_SFX, id, 0, param, 0 }; audio_push(c); }
void audio_announce(int word)
{ SeqCmd c = { CMD_ANNOUNCE, word, 0, 0, 0 }; audio_push(c); }
void audio_master_lp(float cut, float sec)
{ SeqCmd c = { CMD_MASTER_LP, 0, 0, cut, sec }; audio_push(c); }
void audio_master_pitch(float mul, float sec)
{ SeqCmd c = { CMD_MASTER_PITCH, 0, 0, mul, sec }; audio_push(c); }
void audio_master_gain(float g, float sec)
{ SeqCmd c = { CMD_MASTER_GAIN, 0, 0, g, sec }; audio_push(c); }
void audio_metronome(int on) { SeqCmd c = { CMD_METRONOME, on, 0, 0, 0 }; audio_push(c); }
void audio_drum_mute(int on) { SeqCmd c = { CMD_DRUM_MUTE, on, 0, 0, 0 }; audio_push(c); }

float audio_bass_level(void) { return b2f(atomic_load_explicit(&s_bass_lvl, memory_order_relaxed)); }
float audio_master_level(void) { return b2f(atomic_load_explicit(&s_mast_lvl, memory_order_relaxed)); }
