/* replay.c — input recording and the best-rally playback (§5.1, §10). */
#include "replay.h"

static void copy(void *dst, const void *src, size_t n) {
    char *d = (char *)dst; const char *s = (const char *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

void vb_replay_reset(VbReplay *r) {
    for (size_t i = 0; i < sizeof(*r); i++) ((char *)r)[i] = 0;
    r->cur = 0;
}

void vb_replay_begin(VbReplay *r, const VbSim *s) {
    /* Round-robin the slots and let vb_replay_end decide whether what we just
     * captured was worth keeping. Recording is always on (§5.1). */
    VbRally *sl = &r->slot[r->cur];
    copy(&sl->start, s, sizeof(VbSim));
    sl->len = 0;
    sl->contacts = sl->top_heat = sl->scorcher = 0;
    sl->scored_by = -1;
    sl->used = 0;
    r->recording = 1;
}

void vb_replay_record(VbReplay *r, const VbInput in[VB_NPLAYERS]) {
    if (!r->recording) return;
    VbRally *sl = &r->slot[r->cur];
    if (sl->len >= VB_RALLY_TICKS) { r->recording = 0; return; }
    for (int p = 0; p < VB_NPLAYERS; p++) {
        float a = vb_clampf(in[p].axis, -1.0f, 1.0f);
        sl->in[sl->len][p].axis = (signed char)(a * 127.0f);
        sl->in[sl->len][p].btn  = (unsigned char)(in[p].btn & 0xFFu);
    }
    sl->len++;
}

static int rank(const VbRally *s) {
    if (!s->used) return -1;
    return s->contacts * 100 + s->top_heat * 6 + (s->scorcher ? 40 : 0);
}

void vb_replay_end(VbReplay *r, int contacts, int top_heat, int scorcher,
                   int scored_by) {
    if (!r->recording) return;
    r->recording = 0;
    VbRally *sl = &r->slot[r->cur];
    sl->contacts = contacts;
    sl->top_heat = top_heat;
    sl->scorcher = scorcher;
    sl->scored_by = scored_by;
    sl->used = sl->len > 0;

    /* keep the best three and always keep the most recent one: "run it back"
     * wants the last rally, the victory screen wants the best one */
    int worst = -1, worst_rank = 1 << 30;
    for (int i = 0; i < VB_REPLAY_SLOTS; i++) {
        if (i == r->cur) continue;
        int k = rank(&r->slot[i]);
        if (k < worst_rank) { worst_rank = k; worst = i; }
    }
    r->cur = (worst >= 0) ? worst : (r->cur + 1) % VB_REPLAY_SLOTS;
}

int vb_replay_best(const VbReplay *r) {
    int best = -1, best_rank = 0;
    for (int i = 0; i < VB_REPLAY_SLOTS; i++) {
        int k = rank(&r->slot[i]);
        if (k > best_rank) { best_rank = k; best = i; }
    }
    return best;
}

void vb_replay_play(VbReplay *r, int slot) {
    if (slot < 0 || slot >= VB_REPLAY_SLOTS || !r->slot[slot].used) {
        r->playing = 0;
        return;
    }
    copy(&r->play_sim, &r->slot[slot].start, sizeof(VbSim));
    r->play_slot = slot;
    r->play_t = 0;
    r->playing = 1;
}

int vb_replay_step(VbReplay *r) {
    if (!r->playing) return 0;
    VbRally *sl = &r->slot[r->play_slot];
    if (r->play_t >= sl->len) { r->playing = 0; return 0; }
    VbInput in[VB_NPLAYERS];
    for (int p = 0; p < VB_NPLAYERS; p++) {
        in[p].axis = (float)sl->in[r->play_t][p].axis / 127.0f;
        in[p].btn  = sl->in[r->play_t][p].btn;
    }
    vb_sim_step(&r->play_sim, in);
    r->play_t++;
    return 1;
}
