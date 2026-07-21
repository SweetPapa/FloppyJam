/* The roster, in spec order (§5.2). Index 0 (TAP) is the forced first phrase. */
#include "micro_common.h"

const Microgame *MICROGAMES[MICROGAME_COUNT] = {
    &MG_TAP, &MG_UPBEAT, &MG_ALTERNATE, &MG_HOLD, &MG_REST,
    &MG_SNIPE, &MG_DOUBLE, &MG_STAIRS, &MG_RAPID, &MG_ECHO,
    &MG_BLIND, &MG_SWAP, &MG_MEASURE, &MG_CALLBACK, &MG_FINALE
};
