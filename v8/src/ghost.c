#include "ghost.h"
void pw_ghost_record(PwGhost*g,PwInput in){if(g->count<PW_MAX_GHOST)g->f[g->count++]=(PwGhostFrame){(int8_t)in.drift,(uint8_t)(in.pulse|(in.fire<<1)|(in.roll<<2))};}
PwInput pw_ghost_input(const PwGhost*g,int t){PwInput in={0};if(t>=0&&t<g->count){in.drift=g->f[t].drift;in.pulse=g->f[t].buttons&1;in.fire=(g->f[t].buttons>>1)&1;in.roll=(g->f[t].buttons>>2)&1;}return in;}
