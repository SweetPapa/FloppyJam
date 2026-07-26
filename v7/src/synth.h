/* synth.h — §9. The second half of stunning.
 *
 * Every sound in this game is generated at boot or streamed at runtime. No
 * audio file ships, and none is ever read from disk (§0).
 *
 * Impacts are pitched by ball speed and filtered by strike type, the crowd is
 * a shaped noise bed that tracks the heat and gasps on saves, and the music is
 * a generative synthwave engine whose four layers enter at heat 3, 6, 9 and 12
 * — with the key lifting a half-step at match point, for anyone with ears.
 */
#ifndef VB_SYNTH_H
#define VB_SYNTH_H

enum {
    VB_SFX_BUNT = 0,   /* felt thud                                        */
    VB_SFX_FLICK,      /* struck glass                                     */
    VB_SFX_CHARGED,    /* struck glass, leaned on                          */
    VB_SFX_SNAP,       /* whipcrack — the fastest shot in the game         */
    VB_SFX_CHIP,       /* hollow pop                                       */
    VB_SFX_LAND,       /* a chip touching back down                        */
    VB_SFX_WALL,       /* the bank                                         */
    VB_SFX_PIN,        /* the catch: everything stops                      */
    VB_SFX_WHIFF,      /* air, and the lane you just opened                */
    VB_SFX_GOAL,       /* subsonic detonation + a choir-like shimmer       */
    VB_SFX_SCORCHER,   /* the same, but it cracks the glass                */
    VB_SFX_SAVE,
    VB_SFX_MERCY,      /* the breath before a step-12 ball flies           */
    VB_SFX_REF,        /* the anti-stall pulse                             */
    VB_SFX_UI,
    VB_SFX_UI_BIG,
    VB_SFX_COUNT
};

/* Synthesises every buffer. Safe to call when there is no audio device: the
 * whole module turns into a no-op rather than failing the game. */
void vb_synth_init(void);
void vb_synth_shutdown(void);
int  vb_synth_ready(void);

/* The web build may not open a device until the player has touched something;
 * app.c calls this on the first input gesture (§0). Harmless on desktop. */
void vb_synth_gesture(void);

void vb_synth_volumes(float master, float music, float sfx, float crowd);

/* pitch is a multiplier (ball speed drives it), gain is 0..1 */
void vb_sfx(int id, float pitch, float gain);
/* The same, placed across the stereo field: pan is -1 (left end of the table)
 * to +1 (right end). The table is two units wide and the players sit at either
 * end of it, so a hit that sounds like it came from where it happened is most
 * of what makes the thing feel like a physical object in a room. */
void vb_sfx_at(int id, float pitch, float gain, float pan);

/* heat01 drives the bed's weight, gasp 0..1 dips it on a save */
void vb_synth_crowd(float heat01, float gasp);
/* layers 0..4 (§9), key lifts a half-step at match point, mode 1 = the
 * beatless ambient set RALLY gets instead */
void vb_synth_music(int layers, int match_point, int ambient);
/* Pumps the stream. Call once per frame. */
void vb_synth_update(void);

#endif /* VB_SYNTH_H */
