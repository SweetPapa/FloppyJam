#ifndef SFBS_SAVE_H
#define SFBS_SAVE_H
#include "core.h"
typedef struct { char magic[4],seed[7]; uint8_t mode,volume,fullscreen,tutorial,language; uint32_t checksum; } SaveData;
void sfbs_save_defaults(SaveData*); bool sfbs_save_valid(const SaveData*); bool sfbs_save_load(const char*,SaveData*); bool sfbs_save_write(const char*,SaveData*);
#endif
