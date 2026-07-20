#ifndef SFBS_RENDER_H
#define SFBS_RENDER_H
#include "sim.h"
#include "raylib.h"
typedef struct { Color bg,bg2,stable,active,player,accent,bad,edge; } Palette;
Palette sfbs_palette(int);void sfbs_render_game(const Game*,Transport,bool,bool,bool,float,float,const char*);void sfbs_render_backdrop(const Game*,float);void sfbs_center_text(const char*,int,int,Color);
#endif
