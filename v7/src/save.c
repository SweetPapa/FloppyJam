/* save.c — §11. One file, next to the executable, and nothing else. */
#include "save.h"

#include <stdio.h>
#include <string.h>

#define SAVE_PATH "volleybar.sav"

void vb_save_defaults(VbSave *s) {
    memset(s, 0, sizeof *s);
    s->magic = VB_SAVE_MAGIC;
    s->version = VB_SAVE_VERSION;
    s->master = 0.85f; s->music = 0.55f; s->sfx = 0.90f; s->crowd = 0.60f;
    s->palette = 0;
    s->reduce_motion = 0;
    s->target = 5;
    for (int i = 0; i < VB_NPLAYERS; i++) {
        /* GLIDE is the door everyone walks in through, and it is a complete
         * way to play — not a tutorial setting (§3.1). */
        s->scheme[i] = VB_SCHEME_GLIDE;
        s->invert[i] = 0;
    }
    for (int i = 0; i < VB_NSIDES; i++) s->assist[i] = 1.0f;
    for (int i = 0; i < 3; i++) s->rung_cleared[i] = -1;
    for (int i = 0; i < VB_NTRICKSHOTS; i++) s->medal[i] = -1;
    s->palettes = 1u;                       /* ARC LIGHT is always there    */
}

void vb_save_load(VbSave *s) {
    vb_save_defaults(s);
    FILE *f = fopen(SAVE_PATH, "rb");
    if (!f) return;
    VbSave in;
    size_t n = fread(&in, 1, sizeof in, f);
    fclose(f);
    if (n != sizeof in) return;
    if (in.magic != VB_SAVE_MAGIC) return;
    if (in.version != VB_SAVE_VERSION && in.version != 1) return;
    *s = in;

    /* A version-1 file has the same layout but the old key bindings, where
     * player one was on W and S. Keep everything they earned and blank the
     * keys only — app.c reads a zeroed first bind as "give me the defaults",
     * so they come back on the arrows without losing a single medal. */
    if (in.version == 1) {
        for (int p = 0; p < VB_NPLAYERS; p++)
            for (int k = 0; k < 8; k++) s->binds[p][k] = 0;
        s->version = VB_SAVE_VERSION;
    }
    /* a save file is not a licence to hand the game bad numbers */
    s->palette = vb_clampi(s->palette, 0, VB_NPALETTES_MAX - 1);
    s->target = (s->target == 7 || s->target == 10) ? s->target : 5;
    s->master = vb_clampf(s->master, 0, 1);
    s->music  = vb_clampf(s->music, 0, 1);
    s->sfx    = vb_clampf(s->sfx, 0, 1);
    s->crowd  = vb_clampf(s->crowd, 0, 1);
    for (int i = 0; i < VB_NSIDES; i++) s->assist[i] = vb_clampf(s->assist[i], 0, 1);
    for (int i = 0; i < VB_NPLAYERS; i++)
        s->scheme[i] = s->scheme[i] ? VB_SCHEME_GRIP : VB_SCHEME_GLIDE;
    s->palettes |= 1u;
}

void vb_save_write(const VbSave *s) {
    FILE *f = fopen(SAVE_PATH, "wb");
    if (!f) return;
    fwrite(s, 1, sizeof *s, f);
    fclose(f);
}

void vb_save_record(VbSave *s, int longest_rally, int goals, int scorchers,
                    int pins, int chips, int saves) {
    if (longest_rally > s->longest_rally) s->longest_rally = longest_rally;
    s->goals += goals;
    s->scorchers += scorchers;
    s->pins += pins;
    s->chips += chips;
    s->saves += saves;
    s->matches++;
}

int vb_save_palette_unlocked(const VbSave *s, int palette) {
    if (palette <= 0) return 1;
    return (s->palettes & (1u << palette)) != 0;
}
