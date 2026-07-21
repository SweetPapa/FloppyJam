#ifndef POLYBLOOM_CAMERA_H
#define POLYBLOOM_CAMERA_H

#include "platform.h"
#include "collision.h"
#include "raylib.h"

typedef struct PbFollowCamera {
    Camera3D view;
    Vector3 target;
    float yaw;
    float pitch;
    float distance;
} PbFollowCamera;

void pb_camera_init(PbFollowCamera *camera, Vector3 target);
void pb_camera_update(PbFollowCamera *camera, Vector3 player, Vector3 velocity,
                      const PbCollisionWorld *world, PbInput input, float dt);

#endif
