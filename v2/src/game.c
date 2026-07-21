/* Run engine: scheduling, judging feedback, cards, HP, scoring (§3, §6, §10). */
#include "game.h"
#include "micro/micro_common.h"

Game G;

#define COUNTIN_BARS 2
#define APPROACH_BEATS 1.5f

/* ------------------------------------------------------------------ */
/* playfield geometry                                                  */
/* ------------------------------------------------------------------ */
float pf_top(void)      { return (float)BANNER_H; }
float pf_bottom(void)   { return (float)(SCREEN_H - STATUS_H); }
float pf_center_x(void) { return SCREEN_W * 0.5f; }
float pf_center_y(void) { return (pf_top() + pf_bottom()) * 0.5f; }

float pf_lane_x(int lane, int lanes)
{
    if (lanes < 1) lanes = 1;
    float spacing = (lanes <= 1) ? 0.0f : g_clampf(1000.0f / (float)lanes, 120.0f, 240.0f);
    float off = ((float)lane - (float)(lanes - 1) * 0.5f) * spacing;
    return pf_center_x() + off;
}

float pf_note_progress(const MicroState *m, const Note *n, uint64_t now)
{
    float spb = m->g->clk.samples_per_beat;
    if (spb <= 1.0f) spb = 24000.0f;
    double lead = (double)spb * APPROACH_BEATS;
    double d = (double)n->t - (double)now;
    return (float)(1.0 - d / lead);
}

bool pf_note_visible(const MicroState *m, const Note *n, uint64_t now)
{
    if (!n->resolved) return false;
    if (n->judged && n->kind != NOTE_HOLD) return false;
    float p = pf_note_progress(m, n, now);
    if (p < -0.35f || p > 1.45f) return false;
    /* FOG: nothing shows until one beat before it is due (§6.2) */
    if (m->g->mods.fog && p < 1.0f - (1.0f / APPROACH_BEATS)) return false;
    return true;
}

void pf_draw_note(const MicroState *m, const Note *n, uint64_t now, int lane, int lanes)
{
    const Palette *p = pal();
    float prog = g_clampf(pf_note_progress(m, n, now), -0.4f, 1.5f);
    float x = pf_lane_x(lane, lanes);
    float y = g_lerpf(pf_top() + 10.0f, pf_bottom() - 62.0f, prog);
    float near_hit = g_clampf(1.0f - fabsf(1.0f - prog) * 3.0f, 0.0f, 1.0f);
    Color c = cmix(p->main, p->hot, near_hit);
    float w = 76.0f + 22.0f * near_hit;
    float h = 22.0f + 10.0f * near_hit;

    if (n->kind == NOTE_HOLD) {
        float endp = g_clampf(pf_note_progress(m, &(Note){ .t = n->t_end }, now), -0.4f, 1.5f);
        float y2 = g_lerpf(pf_top() + 10.0f, pf_bottom() - 62.0f, endp);
        Color tail = p->accent; tail.a = 120;
        DrawRectangle((int)(x - 22), (int)y2, 44, (int)(y - y2), tail);
    }
    DrawRectangle((int)(x - w * 0.5f), (int)(y - h * 0.5f), (int)w, (int)h, c);
    if (near_hit > 0.4f) {
        Color g = p->hot; g.a = (unsigned char)(near_hit * 110.0f);
        DrawRectangle((int)(x - w * 0.5f - 6), (int)(y - h * 0.5f - 6), (int)w + 12, (int)h + 12, g);
    }
}

/* ------------------------------------------------------------------ */
/* judgment feedback — audio + visual in the same frame (§4.2)         */
/* ------------------------------------------------------------------ */
int micro_chord_tone(const MicroState *m, int index)
{
    if (index >= 0 && index < m->chart.count && m->chart.n[index].midi >= 0)
        return m->chart.n[index].midi;
    return 72;
}

void micro_flash_lane(MicroState *m, int lane)
{
    if (lane >= 0 && lane < LANE_COUNT) m->lane_flash[lane] = 1.0f;
}

void micro_feedback(MicroState *m, Judgment j, int note_index, float x, float y)
{
    const Palette *p = pal();
    switch (j) {
    case J_PERFECT: {
        int tone = micro_chord_tone(m, note_index) + 12;
        audio_sfx(SFX_PERFECT, (float)tone);          /* streaks play melodies */
        pop_text("PERFECT", x, y - 40.0f, 4, p->hot, 0.55f);
        particles_burst(x, y, 14, p->hot, 300.0f, 0.45f);
        G.flow++;
        if (G.flow > G.best_flow) G.best_flow = G.flow;
        if (G.flow >= 4) hitstop_add(0.03f);           /* hit-stop on PERFECT chains */
        break;
    }
    case J_GOOD:
        audio_sfx(SFX_GOOD, 0);
        pop_text("GOOD", x, y - 30.0f, 3, p->accent, 0.45f);
        particles_burst(x, y, 5, p->accent, 180.0f, 0.3f);
        G.flow = 0;
        break;
    case J_MISS:
        audio_sfx(SFX_MISS, 0);
        pop_text("X", x, y - 30.0f, 5, p->hot, 0.5f);
        particles_burst(x, y, 10, p->dim, 260.0f, 0.5f);
        shake_add(0.35f);
        G.flow = 0;
        break;
    default: break;
    }
}

float run_window_scale(void)
{
    float w = heat_window_scale(G.tier);
    if (G.mods.tight) w *= 0.7f;
    if (G.micro.active && G.micro.fx[3] > 0.01f) w *= G.micro.fx[3];
    w *= (G.tuning_window_scale > 0.01f) ? G.tuning_window_scale : 1.0f;
    return g_clampf(w, 0.25f, 3.0f);
}

/* ------------------------------------------------------------------ */
/* cards                                                               */
/* ------------------------------------------------------------------ */
float cards_mult(void)
{
    float m = 1.0f;
    for (int i = 0; i < CARD_COUNT; i++)
        for (int k = 0; k < G.mods.owned[i]; k++)
            m *= CARDS[i].mult;
    if (m < 0.05f) m = 0.05f;
    return m;
}

static void apply_card_effect(int id)
{
    switch (id) {
    case CARD_TURBO:
        G.mods.bpm_bonus += 12.0f;
        audio_set_bpm(heat_bpm(G.tier, G.genome.bpm_base) + G.mods.bpm_bonus
                      + G.tuning_bpm_offset, G.clk.bar + 2);
        break;
    case CARD_DEAF:   G.mods.deaf = 1; audio_drum_mute(1); break;
    case CARD_MIRROR: G.mods.mirror = 1; break;
    case CARD_TIGHT:  G.mods.tight = 1; break;
    case CARD_FOG:    G.mods.fog = 1; break;
    case CARD_DENSE:  G.mods.dense = 1; break;
    case CARD_SPIN:   G.mods.spin = 1; break;
    case CARD_STROBE: G.mods.strobe = 1; break;
    case CARD_ALLIN:  G.mods.allin = 1; break;
    case CARD_INSURANCE:
        if (G.hp < 3) G.hp++;
        audio_sfx(SFX_HEAL, 0);
        break;
    case CARD_METRONOME: G.mods.metronome = 1; audio_metronome(1); break;
    case CARD_BANK: {
        int64_t add = (int64_t)((double)G.score * 0.25);
        G.score += add;
        char buf[32];
        snprintf(buf, sizeof buf, "+%lld BANKED", (long long)add);
        pop_text(buf, pf_center_x(), pf_center_y(), 4, pal()->accent, 1.6f);
        break;
    }
    case CARD_BREATHER: G.mods.breather = 1; break;
    case CARD_WILD: {
        int hidden = rng_range(&G.rng_cards, 0, RISK_CARD_COUNT - 1);
        G.mods.wild_hidden = hidden;
        G.mods.wild_reveal_phrase = G.phrase_index + 1;
        apply_card_effect(hidden);
        audio_announce(VOX_LAUGH);
        break;
    }
    default: break;
    }
}

void cards_apply(int id, int announce)
{
    if (id < 0 || id >= CARD_COUNT) return;
    G.mods.owned[id]++;
    if (G.mods.chip_count < 8) G.mods.chip[G.mods.chip_count++] = id;
    G.mods.mult = cards_mult();
    apply_card_effect(id);
    if (announce) {
        audio_sfx(SFX_CARD, 79.0f);
        pop_text(CARDS[id].name, pf_center_x(), pf_center_y() - 120.0f, 7, pal()->hot, 1.4f);
        particles_burst(pf_center_x(), pf_center_y(), 30, pal()->accent, 300.0f, 0.9f);
    }
}

static int card_available(int id)
{
    if (CARDS[id].repeatable) return 1;
    return G.mods.owned[id] == 0;
}

void cards_begin_pick(int bar)
{
    G.card_bar = bar;
    G.card_picked = -1;
    G.card_prepared = 1;
    G.card_t = 0.0f;
    audio_bar_flavor(bar, FLAVOR_BREAKDOWN);

    int pool[CARD_COUNT], n = 0;
    for (int i = 0; i < CARD_COUNT; i++) if (card_available(i)) pool[n++] = i;
    rng_shuffle(&G.rng_cards, pool, n);

    int a = n > 0 ? pool[0] : CARD_TURBO;
    int b = n > 1 ? pool[1] : CARD_BANK;

    /* mercy rule: at 1 HP one option is always a safety card (§6.1) */
    if (G.hp <= 1 && !CARDS[a].safety && !CARDS[b].safety) {
        for (int i = 0; i < n; i++)
            if (CARDS[pool[i]].safety) { b = pool[i]; break; }
    }
    G.card_choice[0] = a;
    G.card_choice[1] = b;
}

void cards_update(void)
{
    if (!G.card_active) return;
    G.card_t += G.dt;

    for (int i = 0; i < G.in.count; i++) {
        const InputEvent *e = &G.in.e[i];
        if (!e->down || G.card_picked >= 0) continue;
        if (e->raw == LANE_F) G.card_picked = 0;
        else if (e->raw == LANE_J) G.card_picked = 1;
        if (G.card_picked >= 0) {
            cards_apply(G.card_choice[G.card_picked], 1);
            G.card_active = 0;
        }
    }

    /* no pick by the bar's end: the LEFT card auto-applies. Hesitation costs. */
    if (G.card_active && G.clk.bar > G.card_bar) {
        cards_apply(G.card_choice[0], 1);
        G.card_active = 0;
    }
}

void cards_draw(void)
{
    if (!G.card_active) return;
    const Palette *p = pal();
    float t = g_ease_out(G.card_t / 0.25f);
    float cw = 420.0f, ch = 300.0f;
    float cy = pf_center_y() - ch * 0.5f;

    Color veil = p->bg; veil.a = 190;
    DrawRectangle(0, (int)pf_top(), SCREEN_W, (int)(pf_bottom() - pf_top()), veil);

    for (int i = 0; i < 2; i++) {
        int id = G.card_choice[i];
        float target_x = (i == 0) ? (pf_center_x() - cw - 30.0f) : (pf_center_x() + 30.0f);
        float from_x = (i == 0) ? -cw - 40.0f : (float)SCREEN_W + 40.0f;
        float x = g_lerpf(from_x, target_x, t);
        Color edge = CARDS[id].safety ? p->accent : p->hot;
        DrawRectangle((int)x, (int)cy, (int)cw, (int)ch, p->bg);
        DrawRectangleLinesEx((Rectangle){x, cy, cw, ch}, 5.0f, edge);
        gtext_center(CARDS[id].name, (int)(x + cw * 0.5f), (int)cy + 30, 6, edge);
        gtext_center(CARDS[id].line1, (int)(x + cw * 0.5f), (int)cy + 120, 3, p->main);
        gtext_center(CARDS[id].line2, (int)(x + cw * 0.5f), (int)cy + 152, 3, p->main);
        char mult[16];
        snprintf(mult, sizeof mult, "X%.2f", (double)CARDS[id].mult);
        gtext_center(mult, (int)(x + cw * 0.5f), (int)cy + 210, 6, p->accent);
        gkeycap(i == 0 ? "F" : "J", (int)(x + cw * 0.5f) - 20, (int)cy + ch - 56, 4, edge, false);
    }
    gtext_center("PICK ONE", (int)pf_center_x(), (int)pf_top() + 20, 5, p->main);
    gtext_center("NO PICK = LEFT CARD", (int)pf_center_x(), (int)pf_bottom() - 46, 3, p->dim);
}

/* ------------------------------------------------------------------ */
/* stems / tier                                                        */
/* ------------------------------------------------------------------ */
static void apply_tier_audio(int tier, int bar)
{
    for (int s = 0; s < STEM_COUNT; s++)
        audio_stem_gain(s, heat_stem_active(tier, s) ? 1.0f : 0.0f, bar);
    audio_set_bpm(heat_bpm(tier, G.genome.bpm_base) + G.mods.bpm_bonus + G.tuning_bpm_offset,
                  bar + 1);
}

/* ------------------------------------------------------------------ */
/* scheduling                                                          */
/* ------------------------------------------------------------------ */
static int pick_microgame(int phrase_index, int tier)
{
    if (G.force_game >= 0 && G.force_game < MICROGAME_COUNT) return G.force_game;
    if (phrase_index == 0) return 0;   /* TAP is always the first phrase */
    /* FINALE opens every tier from 5 up: the wall arrives on a schedule the
     * room can count down to, instead of hiding in a 14-way random draw. */
    if (tier >= 5 && (phrase_index % HEAT_PHRASES_PER_TIER) == 0
        && G.last_game_id != MICROGAME_COUNT - 1)
        return MICROGAME_COUNT - 1;

    int pool[MICROGAME_COUNT], n = 0;
    for (int i = 0; i < MICROGAME_COUNT; i++) {
        if (MICROGAMES[i]->min_tier > tier) continue;
        if (i == G.last_game_id) continue;                       /* no repeats */
        if (i == 14 && tier < 5) continue;                       /* FINALE gate */
        /* REST and BLIND never back-to-back */
        if ((i == 4 || i == 10) && (G.last_game_id == 4 || G.last_game_id == 10)) continue;
        pool[n++] = i;
    }
    if (n == 0) return 0;
    return pool[rng_range(&G.rng_sched, 0, n - 1)];
}

static void prepare_phrase(int bar)
{
    MicroState *m = &G.pending;
    memset(m, 0, sizeof *m);

    /* the tier this phrase will actually run at — it is prepared one bar
     * before it is promoted, so G.tier has not caught up yet */
    int tier = G.phrase_index / HEAT_PHRASES_PER_TIER;
    int id = pick_microgame(G.phrase_index, tier);
    G.last_game_id = id;          /* recorded here so "no immediate repeats" holds */
    m->def = MICROGAMES[id];
    m->game_id = id;
    m->g = &G;
    m->ctx.base_bar = bar;
    m->ctx.phrase_index = G.phrase_index;
    m->ctx.tier = tier;
    m->ctx.dense = G.mods.dense;
    m->ctx.seed_code = G.seed_code;
    m->fx[3] = 1.0f;
    for (int i = 0; i < LANE_COUNT; i++) m->key_map[i] = (int8_t)i;

    chart_reset(&m->chart, bar, PHRASE_BARS);
    /* clear any flavor left over from an earlier lap of the ring */
    for (int b = 0; b < PHRASE_BARS; b++) audio_bar_flavor(bar + b, FLAVOR_NONE);
    m->def->build_chart(&m->chart, &G.genome, &m->ctx);

    G.pending_valid = 1;
    G.banner_t = 0.0f;
    audio_announce(VOX_GAME_BASE + id);
}

static void begin_segment(int bar)
{
    if (G.next_seg_kind == 0) {
        prepare_phrase(bar);
        G.next_seg_bar = bar + PHRASE_BARS;
        G.phrases_since_card++;
        if (G.phrases_since_card >= 2) {
            G.phrases_since_card = 0;
            if (G.mods.breather) {
                G.mods.breather = 0;    /* BREATHER: skip this pick entirely */
                G.next_seg_kind = 0;
            } else {
                G.next_seg_kind = 1;
            }
        } else {
            G.next_seg_kind = 0;
        }
    } else {
        cards_begin_pick(bar);
        G.next_seg_bar = bar + 1;
        G.next_seg_kind = 0;
    }
}

void run_on_bar(const BarInfo *bi)
{
    if (!G.run_active) return;
    G.bars_seen = bi->bar_index;
    if (bi->bar_index == G.next_seg_bar) begin_segment(bi->bar_index);
    if (G.micro.active) chart_resolve_bar(&G.micro.chart, bi);
    if (G.pending_valid) chart_resolve_bar(&G.pending.chart, bi);
}

/* ------------------------------------------------------------------ */
/* HP / wipe                                                           */
/* ------------------------------------------------------------------ */
static void wipe_now(void)
{
    if (G.state == ST_WIPE) return;
    G.run_active = 0;
    G.micro.active = 0;
    G.pending_valid = 0;
    G.card_active = 0;
    audio_sfx(SFX_SCRATCH, 0);
    audio_master_pitch(0.35f, 0.4f);     /* record scratch: exponential drop */
    audio_master_lp(320.0f, 0.5f);       /* stems collapse to the lone pad */
    for (int s = 0; s < STEM_COUNT; s++)
        if (s != STEM_PAD) audio_stem_gain(s, 0.0f, -1);
    audio_announce(VOX_WIPED);
    shake_add(1.0f);
    particles_burst(pf_center_x(), pf_center_y(), 90, pal()->hot, 520.0f, 1.2f);
    screens_enter(ST_WIPE);
}

static void lose_hp(void)
{
    if (G.tuning_infinite_hp) return;
    G.hp--;
    audio_announce(VOX_OOF);
    shake_add(0.8f);
    audio_master_lp(2200.0f - 500.0f * (float)(3 - G.hp), 0.3f);
    /* one stem drops out for the next phrase */
    int drop[3] = { STEM_PLUCK, STEM_BELL, STEM_HAT };
    int idx = g_clampi(2 - G.hp, 0, 2);
    audio_stem_gain(drop[idx], 0.0f, -1);
    if (G.hp <= 0) wipe_now();
}

static void finalize_phrase(void)
{
    MicroState *m = &G.micro;
    if (!m->active) return;
    m->active = 0;

    bool ok = m->def->judge_phrase(m);
    int base = stats_score(&m->stats);
    float tier_bonus = 1.0f + 0.1f * (float)m->ctx.tier;
    float mult = cards_mult();
    if (G.mods.allin_active) mult *= 3.0f;
    int64_t add = (int64_t)((double)base * (double)tier_bonus * (double)mult);
    G.score += add;

    if (add > 0) {
        char buf[32];
        snprintf(buf, sizeof buf, "+%lld", (long long)add);
        pop_text(buf, pf_center_x(), pf_top() + 160.0f, 6,
                 ok ? pal()->accent : pal()->dim, 1.0f);
    }
    if (ok) {
        if (m->stats.miss == 0 && m->stats.judged > 0) audio_announce(VOX_NICE);
        particles_burst(pf_center_x(), pf_center_y(), 24, pal()->accent, 240.0f, 0.7f);
    } else {
        lose_hp();
    }

    G.mods.allin_active = 0;
    if (G.mods.wild_hidden >= 0 && G.phrase_index >= G.mods.wild_reveal_phrase) {
        pop_text(CARDS[G.mods.wild_hidden].name, pf_center_x(), pf_center_y() - 60.0f,
                 6, pal()->hot, 1.6f);
        G.mods.wild_hidden = -1;
    }
}

static void promote_pending(void)
{
    finalize_phrase();
    if (G.state == ST_WIPE) { G.pending_valid = 0; return; }

    G.micro = G.pending;
    G.micro.g = &G;
    G.micro.active = 1;
    G.pending_valid = 0;
    G.phrase_index++;

    if (G.mods.allin) { G.mods.allin = 0; G.mods.allin_active = 1; }

    int new_tier = G.phrase_index / HEAT_PHRASES_PER_TIER;
    if (new_tier != G.tier) {
        G.tier = new_tier;
        apply_tier_audio(G.tier, G.micro.ctx.base_bar);
        if (G.tier >= 5) audio_announce(VOX_HYPE);
    }
    /* honour lost stems: a damaged run stays damaged */
    for (int k = 0; k < 3 - G.hp && k < 3; k++) {
        int drop[3] = { STEM_PLUCK, STEM_BELL, STEM_HAT };
        audio_stem_gain(drop[k], 0.0f, G.micro.ctx.base_bar);
    }
}

/* ------------------------------------------------------------------ */
/* run lifecycle                                                       */
/* ------------------------------------------------------------------ */
void game_init(uint32_t seed_code)
{
    G.seed_code = seed_code;
    genome_init(&G.genome, seed_code);
    draw_set_palette(G.genome.palette);
    audio_set_genome(&G.genome);
}

void run_start(void)
{
    G.score = 0;
    G.display_score = 0;
    G.hp = 3;
    G.tier = 0;
    G.phrase_index = 0;
    G.flow = 0;
    G.best_flow = 0;
    G.flow_meter = 0.0f;
    G.bars_seen = 0;
    G.next_seg_bar = COUNTIN_BARS;
    G.next_seg_kind = 0;
    G.phrases_since_card = 0;
    G.last_game_id = -1;
    G.pending_valid = 0;
    G.card_active = 0;
    if (!G.replay_playing) G.replay_count = 0;
    G.replay_cursor = 0;
    G.desat = 0.0f;
    G.spin_angle = 0.0f;
    memset(&G.micro, 0, sizeof G.micro);
    memset(&G.pending, 0, sizeof G.pending);
    memset(&G.mods, 0, sizeof G.mods);
    G.mods.mult = 1.0f;
    G.mods.wild_hidden = -1;

    G.rng_sched = rng_stream(G.seed_code, RNG_STREAM_SCHEDULER);
    G.rng_cards = rng_stream(G.seed_code, RNG_STREAM_CARDS);
    G.rng_cos   = rng_stream(G.seed_code, RNG_STREAM_COSMETIC);

    audio_drum_mute(0);
    audio_metronome(0);
    audio_master_pitch(1.0f, 0.05f);
    audio_master_lp(20000.0f, 0.2f);
    audio_master_gain(1.0f, 0.2f);
    audio_stop();
    audio_set_genome(&G.genome);
    for (int s = 0; s < STEM_COUNT; s++)
        audio_stem_gain(s, heat_stem_active(0, s) ? 1.0f : 0.0f, -1);
    audio_start(heat_bpm(0, G.genome.bpm_base) + G.tuning_bpm_offset);
    clock_reset();
    particles_clear();
    pops_clear();

    G.run_active = 1;
    audio_announce(VOX_READY);
}

void run_abort(void)
{
    G.hp = 0;
    wipe_now();
}

void run_update(void)
{
    BarInfo bi;
    while (audio_pop_bar(&bi)) run_on_bar(&bi);

    if (!G.run_active) return;

    /* count-in "GO" on the downbeat of the run */
    if (!G.first_phrase_done && G.clk.bar >= COUNTIN_BARS) {
        G.first_phrase_done = 1;
        audio_announce(VOX_GO);
    }

    if (G.pending_valid && G.clk.bar >= G.pending.ctx.base_bar) {
        promote_pending();
        if (!G.run_active) return;
    }

    /* card pick occupies exactly one bar; the groove never stops */
    if (G.card_prepared && G.clk.bar >= G.card_bar) {
        G.card_prepared = 0;
        G.card_active = 1;
        G.card_t = 0.0f;
        audio_sfx(SFX_UI, 0);
    }
    cards_update();

    if (G.micro.active) {
        for (int i = 0; i < LANE_COUNT; i++)
            G.micro.lane_flash[i] = g_approach(G.micro.lane_flash[i], 0.0f, 12.0f, G.dt);
        G.micro.def->update(&G.micro, &G.in, &G.clk);

        /* ALL-IN: any MISS ends the run right there */
        if (G.mods.allin_active && (G.micro.stats.miss > 0 || G.micro.hard_fail)) {
            pop_text("ALL-IN BUST", pf_center_x(), pf_center_y(), 8, pal()->hot, 1.5f);
            G.hp = 0;
            wipe_now();
            return;
        }
        /* a phrase that has run past its last bar finalises on the boundary */
        if (G.clk.bar >= G.micro.ctx.base_bar + PHRASE_BARS && !G.pending_valid)
            finalize_phrase();
    }

    /* cosmetics driven by the mods */
    float bass = audio_bass_level();
    float target_spin = G.mods.spin ? (sinf((float)G.clk.bar * 0.7f
                        + G.clk.bar_phase * 6.2831853f) * 10.0f * (0.3f + bass * 2.0f)) : 0.0f;
    G.spin_angle = g_approach(G.spin_angle, target_spin, 6.0f, G.dt);
    G.strobe_v = G.mods.strobe
               ? g_clampf(1.0f - G.clk.beat_phase * 3.0f, 0.0f, 1.0f) : 0.0f;
    G.flow_meter = g_approach(G.flow_meter, g_clampf((float)G.flow / 12.0f, 0.0f, 1.0f),
                              4.0f, G.dt);
    G.desat = g_clampf((float)(3 - G.hp) * 0.14f, 0.0f, 0.5f);
    G.display_score += (int64_t)(((double)G.score - (double)G.display_score) * 0.18);
    if (G.score - G.display_score < 3) G.display_score = G.score;
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */
/* ------------------------------------------------------------------ */
static void draw_beat_rings(void)
{
    const Palette *p = pal();
    /* center pulse ring on every beat — every microgame inherits it (§9.1) */
    float u = G.clk.beat_phase;
    float intensity = heat_intensity(G.tier);
    int rings = 1 + (G.tier >= 1 ? 1 : 0) + (G.tier >= 4 ? 1 : 0);
    for (int i = 0; i < rings; i++) {
        float uu = u + (float)i * 0.33f;
        if (uu > 1.0f) uu -= 1.0f;
        float r = 60.0f + g_ease_out(uu) * (260.0f + 120.0f * intensity);
        Color c = cmix(p->dim, p->main, 1.0f - uu);
        c.a = (unsigned char)((1.0f - uu) * 130.0f);
        ring(pf_center_x(), pf_center_y(), r, 3.0f + 4.0f * (1.0f - uu), c);
    }
}

void run_draw_playfield(void)
{
    const Palette *p = pal();
    if (G.mods.strobe && G.strobe_v > 0.05f) {
        Color f = cmix(p->bg, p->main, G.strobe_v * 0.55f);
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, f);
    }
    draw_beat_rings();
    if (G.micro.active && G.micro.def->draw) G.micro.def->draw(&G.micro);
    particles_draw();
}

static void draw_banner(void)
{
    const Palette *p = pal();
    DrawRectangle(0, 0, SCREEN_W, BANNER_H, cmix(p->bg, p->dim, 0.5f));
    DrawRectangle(0, BANNER_H - 4, SCREEN_W, 4, p->dim);

    const MicroState *show = G.pending_valid ? &G.pending
                           : (G.micro.active ? &G.micro : NULL);
    if (!show || !show->def) return;
    const Microgame *d = show->def;
    int upcoming = G.pending_valid;

    gicon(d->icon_id, 24, 22, 5, upcoming ? p->hot : p->main);
    gtext(d->name, 130, 20, 6, upcoming ? p->hot : p->main);
    gtext(d->prompt, 130, 74, 3, p->accent);

    /* literal keycaps, big enough for spectators */
    int x = SCREEN_W - 40;
    for (int i = LANE_COUNT - 1; i >= 0; i--) {
        if (!(d->keys & (1u << i))) continue;
        int w = (i == LANE_SPACE) ? 74 : 44;
        x -= w + 10;
        gkeycap(LANE_LABEL[i], x, 30, 4, p->main, (G.in.down_mask & (1u << i)) != 0);
    }
    if (upcoming) gtext("NEXT", SCREEN_W / 2 - 60, 20, 4, p->accent);
}

void run_draw_hud(void)
{
    const Palette *p = pal();
    draw_banner();

    int y = SCREEN_H - STATUS_H;
    DrawRectangle(0, y, SCREEN_W, STATUS_H, cmix(p->bg, p->dim, 0.5f));
    DrawRectangle(0, y, SCREEN_W, 4, p->dim);

    char buf[64];
    snprintf(buf, sizeof buf, "%lld", (long long)G.display_score);
    gtext(buf, 20, y + 16, 4, p->main);

    float mult = cards_mult();
    snprintf(buf, sizeof buf, "X%.1f", (double)mult);
    gtext(buf, 20 + gtext_width("0000000", 4), y + 16, 4,
          mult >= 2.0f ? p->hot : p->accent);

    /* modifier chips */
    int cx = 420;
    for (int i = 0; i < G.mods.chip_count && cx < SCREEN_W - 380; i++) {
        int id = G.mods.chip[i];
        if (G.mods.wild_hidden >= 0 && id == G.mods.wild_hidden) continue;
        Color c = CARDS[id].safety ? p->accent : p->hot;
        int w = gtext_width(CARDS[id].name, 2) + 12;
        DrawRectangleLinesEx((Rectangle){(float)cx, (float)y + 22, (float)w, 24.0f}, 2.0f, c);
        gtext(CARDS[id].name, cx + 6, y + 30, 2, c);
        cx += w + 8;
    }

    /* HP hearts */
    int hx = SCREEN_W - 320;
    for (int i = 0; i < 3; i++) {
        Color c = (i < G.hp) ? p->hot : p->dim;
        DrawRectangle(hx + i * 44, y + 20, 30, 26, c);
        DrawRectangle(hx + i * 44 + 6, y + 14, 18, 8, c);
    }
    snprintf(buf, sizeof buf, "T%d %s", G.tier, heat_visual_name(G.tier));
    gtext(buf, SCREEN_W - 170, y + 22, 3, p->main);

    /* FLOW meter — cosmetic only, by design (§10) */
    draw_bar_meter(20, y + 50, 360, 8, G.flow_meter, p->hot, p->dim);

    draw_cracks(3 - G.hp, 0.75f, p->hot);
    cards_draw();
    pops_draw();

#ifdef TUNING
    if (G.tuning_overlay) {
        snprintf(buf, sizeof buf, "WIN %.2f  BPM %.0f  OFF %+.1fMS  CAL %+.1f",
                 (double)run_window_scale(), (double)G.clk.bpm,
                 (double)G.last_offset_ms, (double)clock_calibration_ms());
        gtext(buf, 10, BANNER_H + 8, 2, p->accent);
    }
#endif
}
