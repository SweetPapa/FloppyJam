/* synth.h — Section 11. Every sound in the game is generated here at
 * startup or streamed at runtime. No audio files ship. */
#ifndef BP_SYNTH_H
#define BP_SYNTH_H

typedef enum {
    SFX_CRACK = 0,   /* the strike — brightness scales with power        */
    SFX_CLACK,       /* ball on ball                                     */
    SFX_THUMP,       /* cushion                                          */
    SFX_RIM,         /* rattling the lip                                 */
    SFX_SWALLOW,     /* the cup takes it                                 */
    SFX_SPLASH,      /* void / water                                     */
    SFX_SCRATCH,     /* SCRATCH!                                         */
    SFX_SAD,         /* the rim-out slide down                           */
    SFX_BANNER,      /* score banner slam                                */
    SFX_KICK,        /* kicker pad                                       */
    SFX_WARP,        /* pocket to pocket                                 */
    SFX_UNCAP,       /* rack hole opens                                  */
    SFX_UI,          /* menu tick                                        */
    SFX_LAND,        /* ball landing after flight                        */
    SFX_BOING0, SFX_BOING1, SFX_BOING2, SFX_BOING3, SFX_BOING4,
    SFX_COUNT
} BpSfx;

void bp_synth_init(void);
void bp_synth_shutdown(void);
void bp_synth_volumes(float master, float music, float sfx);

void bp_sfx(int id, float pitch, float gain);

/* Rolling audio: cutoff and gain track ball speed, timbre tracks surface. */
void bp_roll(float speed, int surf);

void bp_music_mood(int mood);          /* 0 = silence, 1 = front, 2 = back */
void bp_music_duck(float amount);      /* 0 = normal, 1 = fully ducked     */
void bp_music_update(void);

#endif
