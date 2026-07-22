#include "collision.h"
#include "player.h"
#include "camera.h"
#include "levels.h"
#include "gameplay.h"

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
    input=(PbInput){0}; input.move_y=1;
    for(i=0;i<60;++i) pb_player_update(&player,&world,input,0,1.0f/120.0f);
    if(player.position.z>-.8f||fabsf(player.position.x)>.05f) {
        fprintf(stderr,"forward directional input failed: x=%f z=%f\n",player.position.x,player.position.z);
        return 1;
    }
    pb_player_init(&player,(Vector3){0,1.15f,0});
    input=(PbInput){0}; input.move_x=1; input.move_y=1;
    for(i=0;i<30;++i) pb_player_update(&player,&world,input,0,1.0f/120.0f);
    if(player.velocity.x<2||player.velocity.z>-2||
       fabsf(fabsf(player.velocity.x)-fabsf(player.velocity.z))>.05f||
       sqrtf(player.velocity.x*player.velocity.x+player.velocity.z*player.velocity.z)>8.51f) {
        fprintf(stderr,"diagonal movement failed: vx=%f vz=%f\n",player.velocity.x,player.velocity.z);
        return 1;
    }
    {
        PbFollowCamera camera;
        pb_camera_init(&camera,player.position); camera.yaw=1.5f;
        for(i=0;i<720;++i) pb_camera_update(&camera,player.position,(Vector3){0,0,-8},&world,(PbInput){0},1.0f/120.0f);
        if(fabsf(camera.yaw)>.08f) {
            fprintf(stderr,"automatic camera follow failed: yaw=%f\n",camera.yaw);
            return 1;
        }
        {
            PbFollowCamera slow_frame,fast_frame;
            PbInput pointer={0}; pointer.camera_x=100; pointer.camera_pointer=true;
            pb_camera_init(&slow_frame,player.position); pb_camera_init(&fast_frame,player.position);
            pb_camera_update(&slow_frame,player.position,(Vector3){0},&world,pointer,1.0f/30.0f);
            pb_camera_update(&fast_frame,player.position,(Vector3){0},&world,pointer,1.0f/240.0f);
            if(fabsf(slow_frame.yaw-fast_frame.yaw)>.0001f) {
                fprintf(stderr,"mouse look depends on frame rate: %f vs %f\n",slow_frame.yaw,fast_frame.yaw);
                return 1;
            }
        }
    }
    {
        PbLevel level;
        pb_level_foundry_init(&level,&world);
        pb_player_init(&player,level.respawn); input=(PbInput){0};
        for(i=0;i<180;++i) { pb_collision_update(&world,i/120.0f); pb_player_update(&player,&world,input,0,1.0f/120.0f); }
        if(!player.grounded) { fprintf(stderr,"Foundry spawn is not grounded\n"); return 1; }
        pb_level_crown_init(&level,&world);
        pb_player_init(&player,level.respawn);
        for(i=0;i<180;++i) { pb_collision_update(&world,i/120.0f); pb_player_update(&player,&world,input,0,1.0f/120.0f); }
        if(!player.grounded||!level.gate_locked) { fprintf(stderr,"Crown spawn/gate setup failed\n"); return 1; }
        {
            PbGameplay gameplay;
            pb_gameplay_init(&gameplay,PB_LEVEL_CROWN);
            for(i=0;i<5;++i) {
                player.position=gameplay.boss_position; player.state=PB_PLAYER_BURST;
                gameplay.boss_hit_cooldown=0;
                pb_gameplay_update(&gameplay,&player,level.respawn,0,0);
            }
            if(gameplay.boss_active||gameplay.boss_health!=0) {
                fprintf(stderr,"boss defeat sequence failed: health=%d\n",gameplay.boss_health);
                return 1;
            }
        }
    }
    return 0;
}
