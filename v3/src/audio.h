#ifndef POLYBLOOM_AUDIO_H
#define POLYBLOOM_AUDIO_H

#include <stdbool.h>

typedef enum PbSfx {
    PB_SFX_JUMP, PB_SFX_GLIDE_OPEN, PB_SFX_GLIDE_RING, PB_SFX_BURST,
    PB_SFX_BOUNCE, PB_SFX_LANDING, PB_SFX_GLINT, PB_SFX_SEED,
    PB_SFX_CHECKPOINT, PB_SFX_DAMAGE, PB_SFX_FALL, PB_SFX_ENEMY_DEFEAT,
    PB_SFX_PRISM_BREAK, PB_SFX_TILE_WARNING, PB_SFX_GATE,
    PB_SFX_MENU_MOVE, PB_SFX_MENU_CONFIRM, PB_SFX_LEVEL_COMPLETE,
    PB_SFX_COUNT
} PbSfx;

void pb_audio_open(void);
void pb_audio_close(void);
void pb_audio_set_level(int level);
void pb_audio_set_chase(bool active);
void pb_audio_set_paused(bool paused);
void pb_audio_set_volume(int master, int music, int sfx);
void pb_audio_sfx(PbSfx effect);
void pb_audio_glint(int chain);

#endif
