#ifndef PW_GHOST_H
#define PW_GHOST_H
#include "sim.h"
typedef struct { int8_t drift; uint8_t buttons; } PwGhostFrame;
typedef struct { PwGhostFrame f[PW_MAX_GHOST]; int count; unsigned seed; float best; } PwGhost;
void pw_ghost_record(PwGhost*g,PwInput in);
PwInput pw_ghost_input(const PwGhost*g,int tick);
#endif
