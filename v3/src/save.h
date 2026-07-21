#ifndef POLYBLOOM_SAVE_H
#define POLYBLOOM_SAVE_H

#include <stdbool.h>
#include <stdint.h>

#define PB_SAVE_MAGIC 0x50424C4Du

typedef struct PbSaveData {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t best_time_ms[2];
    uint8_t best_glints[2];
    uint8_t seed_masks[2];
    uint8_t volume_master;
    uint8_t volume_music;
    uint8_t volume_sfx;
    uint8_t camera_sensitivity;
    uint8_t option_flags;
    uint32_t checksum;
} PbSaveData;

void pb_save_defaults(PbSaveData *save);
bool pb_save_load(PbSaveData *save);
bool pb_save_write(PbSaveData *save);
void pb_save_record(PbSaveData *save, int level, uint32_t time_ms,
                    int glints, uint8_t seed_mask);

#endif

