/* synth — every sound in HUEDUNIT, made at startup (§8.4).
 *
 * There is not one audio file in the repo. SFX are short synthesised waves;
 * the music is a three-voice pattern engine with a mood table per district
 * that gains a voice and a brightness step for every hue the player restores,
 * so the soundtrack recolours along with the town.
 */
#ifndef HD_SYNTH_H
#define HD_SYNTH_H

#include <stdbool.h>

enum {
    SFX_TICK = 0,    /* UI move            */
    SFX_CLICK,       /* UI confirm         */
    SFX_PAGE,        /* journal page turn  */
    SFX_BLIP,        /* dialogue text, pitched to the speaker */
    SFX_CHIME,       /* puzzle piece placed correctly */
    SFX_CLUNK,       /* puzzle piece rejected */
    SFX_SOLVE,       /* puzzle solved      */
    SFX_DEDUCE,      /* board "Deduce!"    */
    SFX_FEATHER,     /* feather collected  */
    SFX_SPARKLE,     /* hint spent         */
    SFX_DOOR,        /* scene change       */
    SFX_STEP,        /* footfall on paper  */
    SFX_MEND,        /* the Prism swell    */
    SFX_COUNT
};

enum {
    MOOD_SILENT = 0, MOOD_TOWN, MOOD_HARBOR, MOOD_MARKET, MOOD_QUARTER,
    MOOD_GARDEN, MOOD_TOWER, MOOD_PUZZLE, MOOD_BOARD, MOOD_SAD, MOOD_FESTIVAL,
    MOOD_COUNT
};

void audio_init(void);
void audio_shutdown(void);
void audio_update(float dt);
void audio_apply_volumes(void);

void sfx_play(int id);
void sfx_blip(int npc_id);          /* ink scritch, pitched to the speaker */

void music_mood(int mood);
int  music_current(void);
void music_duck(bool ducked);       /* auto-ducks under dialogue (§8.4) */

#endif
