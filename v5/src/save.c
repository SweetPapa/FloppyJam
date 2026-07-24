#include "save.h"
#include <stdio.h>
#include <string.h>

void bp_save_defaults(BpSave *s)
{
    memset(s, 0, sizeof(*s));
    s->magic = BP_SAVE_MAGIC;
    s->version = BP_SAVE_VER;
    s->vol_master = 8;
    s->vol_music = 6;
    s->vol_sfx = 9;
}

int bp_save_load(BpSave *s)
{
    FILE *f = fopen(BP_SAVE_FILE, "rb");
    BpSave tmp;
    bp_save_defaults(s);
    if (!f) return 0;
    if (fread(&tmp, sizeof(tmp), 1, f) == 1 &&
        tmp.magic == BP_SAVE_MAGIC && tmp.version == BP_SAVE_VER) {
        *s = tmp;
        /* a corrupt volume byte should not deafen anyone */
        if (s->vol_master > 10) s->vol_master = 8;
        if (s->vol_music  > 10) s->vol_music  = 6;
        if (s->vol_sfx    > 10) s->vol_sfx    = 9;
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

void bp_save_store(const BpSave *s)
{
    FILE *f = fopen(BP_SAVE_FILE, "wb");
    if (!f) return;
    fwrite(s, sizeof(*s), 1, f);
    fclose(f);
}
