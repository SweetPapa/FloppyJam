#ifndef POLYBLOOM_PLAYER_H
#define POLYBLOOM_PLAYER_H

#include "collision.h"
#include "platform.h"

typedef enum PbPlayerState {
    PB_PLAYER_GROUNDED, PB_PLAYER_RISING, PB_PLAYER_FALLING,
    PB_PLAYER_GLIDING, PB_PLAYER_BURST, PB_PLAYER_BOUNCED, PB_PLAYER_HURT
} PbPlayerState;

typedef struct PbPlayer {
    Vector3 position;
    Vector3 previous_position;
    Vector3 velocity;
    Vector3 facing;
    PbPlayerState state;
    float coyote;
    float jump_buffer;
    float burst_timer;
    float burst_cooldown;
    bool grounded;
    bool air_burst;
} PbPlayer;

void pb_player_init(PbPlayer *player, Vector3 position);
void pb_player_update(PbPlayer *player, PbCollisionWorld *world, PbInput input,
                      float camera_yaw, float dt);
Vector3 pb_player_interpolated(const PbPlayer *player, float alpha);
const char *pb_player_state_name(PbPlayerState state);

#endif

