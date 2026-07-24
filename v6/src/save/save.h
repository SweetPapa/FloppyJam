/* save — versioned blob of the flag store + settings (API.md §7.3, §5.4).
 *
 * The save format is "every flag that is non-zero, by name". Content agents
 * can add clues, characters and chapters forever without breaking anyone's
 * save file, because the save never encodes a layout — only names and ints.
 */
#ifndef HD_SAVE_H
#define HD_SAVE_H

#include <stdbool.h>

#define SAVE_VERSION 1
#define SAVE_SLOTS   3          /* 3 manual slots + slot 0 = autosave */

typedef struct {
    int  text_size;             /* 0 normal, 1 large, 2 largest */
    int  highlight;             /* interactable sparkle: default ON */
    int  music_vol;             /* 0..10 */
    int  sfx_vol;               /* 0..10 */
    int  fullscreen;
    int  reduce_motion;
    char detective[24];         /* player-chosen name */
} Settings;

Settings *settings(void);
float     text_scale(void);     /* 1.00 / 1.25 / 1.50 */

void settings_defaults(void);
void settings_load(void);
void settings_store(void);

/* slot 0 is the autosave */
bool save_write(int slot, const char *scene_id, const char *recap);
bool save_read(int slot);
bool save_peek(int slot, char *chapter_out, int cap, char *recap_out, int rcap);
bool save_exists(int slot);
void save_autosave(const char *scene_id);

/* what save_read() restored, for the app to resume into */
const char *save_last_scene(void);

/* "Previously, in Prismbrook..." — assembled from the flag store, so it is
 * always true even for a save written by an older build (§1.2.1). */
void save_build_recap(char *buf, int cap);

#endif
