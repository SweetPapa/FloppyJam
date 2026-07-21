#include "collision.h"
#include "player.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    PbCollisionWorld world;
    PbPlayer player;
    PbInput input={0};
    int i;
    pb_collision_clear(&world);
    pb_collision_add_box(&world,(Vector3){0,0,0},(Vector3){12,1,12},PB_COLLIDER_SOLID,0);
    pb_player_init(&player,(Vector3){0,1.15f,0});
    for(i=0;i<240;++i) {
        pb_collision_update(&world,i/120.0f);
        pb_player_update(&player,&world,input,0,1.0f/120.0f);
    }
    if(!player.grounded||fabsf(player.position.y-1.15f)>.01f) {
        fprintf(stderr,"grounding failed: y=%f grounded=%d\n",player.position.y,player.grounded);
        return 1;
    }
    input.jump_pressed=input.jump_down=true;
    pb_player_update(&player,&world,input,0,1.0f/120.0f);
    input.jump_pressed=false;
    for(i=0;i<300;++i) {
        if(i==30) input.jump_down=false;
        pb_collision_update(&world,(i+240)/120.0f);
        pb_player_update(&player,&world,input,0,1.0f/120.0f);
    }
    if(!player.grounded||fabsf(player.position.y-1.15f)>.01f) {
        fprintf(stderr,"jump landing failed: y=%f grounded=%d\n",player.position.y,player.grounded);
        return 1;
    }
    return 0;
}
