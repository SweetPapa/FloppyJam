/* save.h — §11. One small save file, local only.
 *
 * Settings, Gauntlet progress, cosmetic palettes, Trickshot medals and the
 * lifetime totals. The longest rally you have ever played is the crown jewel
 * and it goes on the title screen.
 *
 * Nothing in here can be unlocked for advantage: every unlock is cosmetic
 * (§15), which is why the struct has no gameplay numbers in it.
 */
#ifndef VB_SAVE_H
#define VB_SAVE_H

#include "core.h"

#define VB_SAVE_MAGIC   0x37424C56u    /* "VLB7"                            */
/* 2: player one moved to the arrow keys. Same layout as 1, so a version-1
 *    file keeps every unlock and setting and only has its keys refreshed. */
#define VB_SAVE_VERSION 2
#define VB_NTRICKSHOTS  20
#define VB_NRUNGS       11             /* ten personalities plus the finale */

typedef struct {
    unsigned magic, version;

    /* settings */
    float master, music, sfx, crowd;
    int   palette;
    int   reduce_motion;
    int   scheme[VB_NPLAYERS];
    float assist[VB_NSIDES];
    int   invert[VB_NPLAYERS];
    int   target;                  /* first to 5 / 7 / 10                   */
    int   binds[VB_NPLAYERS][8];   /* raylib keycodes; see app.h BIND_*     */

    /* progress — cosmetic only */
    int   rung_cleared[3];         /* highest Gauntlet rung per tier        */
    unsigned palettes;             /* bitmask of unlocked palettes          */
    signed char medal[VB_NTRICKSHOTS];  /* -1 none, 0 bronze, 1 silver, 2 gold */

    /* lifetime */
    int   longest_rally;           /* the crown jewel                       */
    int   best_streak;             /* best RALLY keep-up                    */
    int   goals, scorchers, pins, chips, saves;
    int   matches;
} VbSave;

void vb_save_defaults(VbSave *s);
/* Both are best-effort: a save that will not load is a save that gets reset,
 * never a game that will not start. */
void vb_save_load(VbSave *s);
void vb_save_write(const VbSave *s);
/* Folds a finished match into the lifetime totals. */
void vb_save_record(VbSave *s, int longest_rally, int goals, int scorchers,
                    int pins, int chips, int saves);
int  vb_save_palette_unlocked(const VbSave *s, int palette);

#endif /* VB_SAVE_H */
