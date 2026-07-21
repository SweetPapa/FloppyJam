/* Modifier cards (§6). Multipliers stack multiplicatively. */
#ifndef G_CARDS_H
#define G_CARDS_H

#include "common.h"

typedef enum {
    CARD_TURBO = 0, CARD_DEAF, CARD_MIRROR, CARD_TIGHT, CARD_FOG, CARD_DENSE,
    CARD_SPIN, CARD_STROBE, CARD_WILD, CARD_ALLIN,
    CARD_INSURANCE, CARD_METRONOME, CARD_BANK, CARD_BREATHER,
    CARD_COUNT
} CardId;

#define RISK_CARD_COUNT 10   /* CARD_TURBO .. CARD_ALLIN */

typedef struct {
    const char *name;
    const char *line1;   /* <= 16 chars, spectator-readable */
    const char *line2;
    float   mult;
    uint8_t one_shot;
    uint8_t safety;
    uint8_t repeatable;  /* may be drafted more than once */
} CardDef;

extern const CardDef CARDS[CARD_COUNT];

#endif
