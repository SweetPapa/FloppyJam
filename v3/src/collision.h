#ifndef POLYBLOOM_COLLISION_H
#define POLYBLOOM_COLLISION_H

#include "raylib.h"

#define PB_MAX_COLLIDERS 512

typedef enum PbColliderType {
    PB_COLLIDER_SOLID,
    PB_COLLIDER_BOUNCE,
    PB_COLLIDER_MOVING
} PbColliderType;

typedef struct PbCollider {
    BoundingBox box;
    Vector3 base;
    Vector3 delta;
    Vector3 size;
    float phase;
    unsigned char type;
} PbCollider;

typedef struct PbCollisionWorld {
    PbCollider items[PB_MAX_COLLIDERS];
    int count;
} PbCollisionWorld;

void pb_collision_init_test_course(PbCollisionWorld *world);
void pb_collision_clear(PbCollisionWorld *world);
void pb_collision_add_box(PbCollisionWorld *world, Vector3 position, Vector3 size,
                          PbColliderType type, float phase);
void pb_collision_update(PbCollisionWorld *world, float elapsed);
int pb_collision_move(PbCollisionWorld *world, Vector3 *position, Vector3 *velocity,
                      float radius, float half_height, float dt, bool *grounded);
void pb_collision_draw(const PbCollisionWorld *world);

#endif
