#include "camera.h"
#include "raymath.h"

#include <math.h>

static float angle_delta(float from, float to)
{
    float d=fmodf(to-from+PI,PI*2)-PI;
    return d<-PI?d+PI*2:d;
}

void pb_camera_init(PbFollowCamera *c, Vector3 target)
{
    *c = (PbFollowCamera){0};
    c->target = target; c->yaw = 0; c->pitch = .48f; c->distance = 10;
    c->obstruction_distance=c->distance;
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
    if(fabsf(in.camera_x)>.02f||fabsf(in.camera_y)>.02f) {
        float look_scale=in.camera_pointer?.0038f:2.35f*dt;
        c->yaw += in.camera_x*look_scale;
        c->pitch = Clamp(c->pitch-in.camera_y*look_scale*.72f,.20f,.78f);
        c->manual_timer=1.8f;
    } else {
        c->manual_timer=fmaxf(0,c->manual_timer-dt);
        c->pitch+=(.46f-c->pitch)*(1-expf(-2.2f*dt));
        /* Both authored courses advance toward -Z. Returning to their forward
           framing avoids camera/input feedback loops while retaining orbit. */
        if(c->manual_timer<=0)
            c->yaw+=angle_delta(c->yaw,0)*(1-expf(-.72f*dt));
    }
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
    c->obstruction_distance+=(allowed-c->obstruction_distance)*
        (1-expf(-(allowed<c->obstruction_distance?18.0f:4.0f)*dt));
    c->view.position = Vector3Add(c->target,Vector3Scale(ray_direction,c->obstruction_distance));
}
