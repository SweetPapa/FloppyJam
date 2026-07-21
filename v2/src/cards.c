#include "cards.h"

const CardDef CARDS[CARD_COUNT] = {
    /* ---- risk (§6.2) ---- */
    [CARD_TURBO]     = { "TURBO",     "+12 BPM",       "RIGHT NOW",    1.50f, 0, 0, 1 },
    [CARD_DEAF]      = { "DEAF",      "DRUMS MUTED",   "FOREVER",      2.00f, 0, 0, 0 },
    [CARD_MIRROR]    = { "MIRROR",    "F-J AND G-H",   "SWAPPED",      1.50f, 0, 0, 0 },
    [CARD_TIGHT]     = { "TIGHT",     "WINDOWS X0.7",  "GOOD LUCK",    2.00f, 0, 0, 0 },
    [CARD_FOG]       = { "FOG",       "NOTES HIDDEN",  "TIL 1 BEAT",   1.75f, 0, 0, 0 },
    [CARD_DENSE]     = { "DENSE",     "DOUBLE THE",    "NOTES",        1.75f, 0, 0, 0 },
    [CARD_SPIN]      = { "SPIN",      "THE FLOOR",     "ROTATES",      1.25f, 0, 0, 0 },
    [CARD_STROBE]    = { "STROBE",    "FLASHING",      "LIGHTS",       1.25f, 0, 0, 0 },
    [CARD_WILD]      = { "WILD",      "SECRET RISK",   "REVEALED L8R", 2.50f, 0, 0, 1 },
    [CARD_ALLIN]     = { "ALL-IN",    "NEXT PHRASE",   "MISS = DEATH", 3.00f, 1, 0, 1 },
    /* ---- safety / economy (§6.3) ---- */
    [CARD_INSURANCE] = { "INSURANCE", "HEAL 1 HP",     "",             0.75f, 0, 1, 1 },
    [CARD_METRONOME] = { "METRONOME", "CLICK ON",      "EVERY BEAT",   0.80f, 0, 1, 0 },
    [CARD_BANK]      = { "BANK",      "CASH 25%",      "OF SCORE",     1.00f, 1, 1, 1 },
    [CARD_BREATHER]  = { "BREATHER",  "SKIP NEXT",     "CARD PICK",    0.90f, 0, 1, 1 }
};
