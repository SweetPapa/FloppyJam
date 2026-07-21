#include "save.h"

#include <stdio.h>
#include <string.h>

static uint32_t checksum(const PbSaveData *save)
{
    const unsigned char *bytes = (const unsigned char *)save;
    uint32_t hash = 2166136261u;
    size_t i;
    for (i = 0; i < sizeof(*save)-sizeof(save->checksum); ++i) hash = (hash^bytes[i])*16777619u;
    return hash;
}

void pb_save_defaults(PbSaveData *save)
{
    *save = (PbSaveData){0};
    save->magic = PB_SAVE_MAGIC;
    save->version = 1;
    save->volume_master = save->volume_music = save->volume_sfx = 80;
    save->camera_sensitivity = 50;
    save->checksum = checksum(save);
}

bool pb_save_load(PbSaveData *save)
{
    FILE *file = fopen("polybloom.sav", "rb");
    PbSaveData loaded;
    if (!file) { pb_save_defaults(save); return false; }
    if (fread(&loaded,sizeof(loaded),1,file)!=1) { fclose(file); pb_save_defaults(save); return false; }
    fclose(file);
    if (loaded.magic!=PB_SAVE_MAGIC || loaded.version!=1 || loaded.checksum!=checksum(&loaded)) {
        pb_save_defaults(save); return false;
    }
    *save=loaded;
    return true;
}

bool pb_save_write(PbSaveData *save)
{
    FILE *file;
    save->checksum=checksum(save);
    file=fopen("polybloom.sav","wb");
    if (!file) return false;
    if (fwrite(save,sizeof(*save),1,file)!=1) { fclose(file); return false; }
    return fclose(file)==0;
}

void pb_save_record(PbSaveData *save, int level, uint32_t time_ms, int glints, uint8_t seeds)
{
    if (level<0 || level>1) return;
    save->flags |= (uint16_t)(1u<<level);
    if (!save->best_time_ms[level] || time_ms<save->best_time_ms[level]) save->best_time_ms[level]=time_ms;
    if (glints>save->best_glints[level]) save->best_glints[level]=(uint8_t)glints;
    save->seed_masks[level]|=seeds;
}
