#include "player.h"
#include "raymath.h"

#include <math.h>

void pb_player_init(PbPlayer *p, Vector3 position)
{
    *p = (PbPlayer){0};
    p->position = p->previous_position = position;
    p->facing = (Vector3){0,0,-1};
    p->air_burst = true;
}

void pb_player_update(PbPlayer *p, PbCollisionWorld *world, PbInput in, float yaw, float dt)
{
    Vector3 wish = {in.move_x*cosf(yaw)-in.move_y*sinf(yaw),0,
                    -in.move_x*sinf(yaw)-in.move_y*cosf(yaw)};
    float wish_len = Vector3Length(wish);
    float accel;
    int ground;
    p->previous_position = p->position;
    if (wish_len > 1) wish = Vector3Scale(wish, 1/wish_len);
    if (in.jump_pressed) p->jump_buffer = .14f;
    else p->jump_buffer = fmaxf(0,p->jump_buffer-dt);
    p->coyote = p->grounded ? .12f : fmaxf(0,p->coyote-dt);
    p->burst_cooldown = fmaxf(0,p->burst_cooldown-dt);

    if (p->burst_timer > 0) {
        p->burst_timer -= dt;
        p->state = PB_PLAYER_BURST;
    } else {
        if (in.burst_pressed && p->burst_cooldown <= 0 && (p->grounded || p->air_burst)) {
            Vector3 direction = wish_len > .1f ? wish : p->facing;
            p->velocity.x = direction.x*15; p->velocity.z = direction.z*15;
            if (!p->grounded) { p->velocity.y = fmaxf(p->velocity.y,-1); p->air_burst = false; }
            p->burst_timer = .28f; p->burst_cooldown = .45f;
        } else {
            float target_speed = (!p->grounded && in.jump_down && p->velocity.y < 0) ? 10.3f : 8.5f;
            Vector3 horizontal={p->velocity.x,0,p->velocity.z};
            Vector3 target=Vector3Scale(wish,target_speed);
            Vector3 change=Vector3Subtract(target,horizontal);
            float change_length=Vector3Length(change);
            accel = p->grounded ? (wish_len > .05f ? 42 : 55) : 20;
            if(change_length>accel*dt) change=Vector3Scale(change,accel*dt/change_length);
            horizontal=Vector3Add(horizontal,change);
            p->velocity.x=horizontal.x; p->velocity.z=horizontal.z;
        }
        if (wish_len > .1f) {
            Vector3 target_facing=Vector3Normalize(wish);
            p->facing=Vector3Normalize(Vector3Lerp(p->facing,target_facing,fminf(1,14*dt)));
        }
        if (p->jump_buffer > 0 && p->coyote > 0) {
            p->velocity.y = 10.5f; p->grounded = false; p->coyote = 0; p->jump_buffer = 0;
        }
        if (!in.jump_down && p->velocity.y > 4.5f) p->velocity.y = 4.5f;
        if (!p->grounded) {
            if (in.jump_down && p->velocity.y < 0 && p->burst_timer <= 0) {
                p->velocity.y = fmaxf(p->velocity.y-8*dt,-2.7f);
                p->state = PB_PLAYER_GLIDING;
            } else p->velocity.y -= 25*dt;
        } else if (p->burst_timer <= 0) p->state = PB_PLAYER_GROUNDED;
    }
    ground = pb_collision_move(world,&p->position,&p->velocity,.42f,.65f,dt,&p->grounded);
    if (p->grounded) {
        p->air_burst = true;
        if (ground >= 0 && world->items[ground].type == PB_COLLIDER_BOUNCE) {
            p->velocity.y = 13.5f; p->grounded = false; p->state = PB_PLAYER_BOUNCED;
        }
    } else if (p->burst_timer <= 0 && p->state != PB_PLAYER_GLIDING)
        p->state = p->velocity.y > 0 ? PB_PLAYER_RISING : PB_PLAYER_FALLING;
}

Vector3 pb_player_interpolated(const PbPlayer *p, float alpha)
{
    return Vector3Lerp(p->previous_position,p->position,alpha);
}

const char *pb_player_state_name(PbPlayerState state)
{
    static const char *names[] = {"GROUNDED","RISING","FALLING","GLIDING","BURST","BOUNCED","HURT"};
    return names[state];
}
