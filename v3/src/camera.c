#include "camera.h"
#include "raymath.h"

#include <math.h>

void pb_camera_init(PbFollowCamera *c, Vector3 target)
{
    *c = (PbFollowCamera){0};
    c->target = target; c->yaw = .65f; c->pitch = .48f; c->distance = 10;
    c->view.up = (Vector3){0,1,0}; c->view.fovy = 55; c->view.projection = CAMERA_PERSPECTIVE;
}

void pb_camera_update(PbFollowCamera *c, Vector3 player, Vector3 velocity,
                      const PbCollisionWorld *world, PbInput in, float dt)
{
    Vector3 look = {player.x+velocity.x*.18f,player.y+.45f,player.z+velocity.z*.18f};
    Vector3 desired;
    Vector3 ray_direction;
    float allowed;
    float response = 1-expf(-8*dt);
    int i;
    c->yaw += in.camera_x*1.8f*dt;
    c->pitch = Clamp(c->pitch-in.camera_y*1.2f*dt,.18f,.85f);
    c->target = Vector3Lerp(c->target,look,response);
    c->view.target = c->target;
    desired = (Vector3){c->target.x+sinf(c->yaw)*cosf(c->pitch)*c->distance,
                        c->target.y+sinf(c->pitch)*c->distance,
                        c->target.z+cosf(c->yaw)*cosf(c->pitch)*c->distance};
    ray_direction = Vector3Normalize(Vector3Subtract(desired,c->target));
    allowed = c->distance;
    for (i = 0; i < world->count; ++i) {
        RayCollision hit = GetRayCollisionBox((Ray){c->target,ray_direction},world->items[i].box);
        if (hit.hit && hit.distance < allowed) allowed = fmaxf(1.2f,hit.distance-.3f);
    }
    c->view.position = Vector3Add(c->target,Vector3Scale(ray_direction,allowed));
}
