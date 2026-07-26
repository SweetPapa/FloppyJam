/* ai.h — Section 7. The Gauntlet ladder and the practice partner.
 *
 * The honesty audit is the point of this module's shape: the AI receives a
 * DELAYED copy of the table and emits a VbInput, the same struct a gamepad
 * emits. It cannot read the human's buttons, it cannot move a rod it does not
 * own, it cannot exceed a rod's speed, and difficulty tunes only latency and
 * aim noise — never physics. tests/test_sim.c checks the first four of those
 * by construction; the fifth is enforced by there being no other lever here.
 *
 * Raylib-free.
 */
#ifndef VB_AI_H
#define VB_AI_H

#include "sim.h"

typedef struct {
    const char *name;
    const char *style;        /* the gimmick, nameable by a stranger        */
    const char *banter[2];
    unsigned char col[3];     /* light signature                            */
    float react_base;         /* seconds of reaction latency at tier 0      */
    float noise;              /* aim noise, table units                     */
    float aggression;         /* how eagerly it commits to a strike         */
    float patience;           /* how willing it is to bunt heat back down   */
    float chip_love;
    float pin_love;
    float bank_love;
} VbPersona;

#define VB_NPERSONAS 10
extern const VbPersona VB_PERSONAS[VB_NPERSONAS];

#define VB_AI_LAG 96          /* 400 ms of memory at 240 Hz                 */

typedef struct {
    int      persona, tier, slot;
    unsigned rng;
    int      lag;                       /* ticks of reaction latency        */
    float    noise_mul, gain;           /* the other two difficulty levers  */
    V2       hist_p[VB_AI_LAG];
    V2       hist_v[VB_AI_LAG];
    int      hist_n;
    int      plan;                      /* the verb it is committed to      */
    int      plan_ticks;
    float    aim_bias;                  /* re-rolled per decision           */
    int      swing, swing_chip;         /* a committed press, held like a
                                         * hand does — not strobed          */
    int      catch_hold, tap_hold;
    int      charging;                  /* leaning on the bar for pace      */
    int      mirror_flicks;             /* the Mirror, learning your habits */
    int      mirror_pins;
} VbAI;

/* tier: 0 easy, 1 middling, 2 hard. Latency 320/250/180 ms (§7). */
void    vb_ai_init(VbAI *a, int persona, int tier, int slot, unsigned seed);
/* One tick of thought. Feed the result straight into vb_match_step. */
VbInput vb_ai_think(VbAI *a, const VbSim *s);
/* The Mirror echoes your habits back at you: tell it what you just did. */
void    vb_ai_observe(VbAI *a, const VbInput *human);

#endif /* VB_AI_H */
