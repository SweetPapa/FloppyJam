#ifndef POLYBLOOM_UI_H
#define POLYBLOOM_UI_H

#include "save.h"
#include "gameplay.h"
#include "levels.h"

typedef enum PbGameMode {
    PB_MODE_TITLE, PB_MODE_LEVEL_SELECT, PB_MODE_LEVEL_INTRO,
    PB_MODE_PLAYING, PB_MODE_LEVEL_COMPLETE, PB_MODE_RESULTS,
    PB_MODE_PAUSED, PB_MODE_OPTIONS
} PbGameMode;

void pb_ui_title(int selection, bool controller);
void pb_ui_level_select(int selection, const PbSaveData *save, bool controller);
void pb_ui_intro(const PbLevel *level, float timer);
void pb_ui_pause(int selection, bool controller);
void pb_ui_options(int selection, const PbSaveData *save, bool controller);
void pb_ui_results(const PbLevel *level, const PbGameplay *gameplay,
                   int selection, bool controller);
void pb_ui_hud(const PbLevel *level, const PbGameplay *gameplay, bool controller);

#endif
