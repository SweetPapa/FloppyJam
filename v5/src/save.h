/* save.h — Section 8 persistence. One tiny binary file, < 1 KB. */
#ifndef BP_SAVE_H
#define BP_SAVE_H

#include "course.h"

#define BP_SAVE_FILE  "breakpar.sav"
#define BP_SAVE_MAGIC 0x52415042u   /* 'BPAR' little-endian */
#define BP_SAVE_VER   1

typedef struct {
    unsigned int   magic;
    unsigned short version;
    unsigned char  best_hole[BP_NHOLES];  /* 0 = never finished          */
    unsigned short best_front, best_back, best_full;
    unsigned short aces;
    unsigned char  vol_master, vol_music, vol_sfx;  /* 0..10             */
    unsigned char  fullscreen;
    unsigned char  tips_used;             /* bitmask, max 5 tips (S.15)  */
    unsigned char  pad[3];
} BpSave;

void bp_save_defaults(BpSave *s);
int  bp_save_load(BpSave *s);   /* 0 = fresh install                    */
void bp_save_store(const BpSave *s);

#endif
