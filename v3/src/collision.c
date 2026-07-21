#include "collision.h"

#include <math.h>

void pb_collision_add_box(PbCollisionWorld *w, Vector3 p, Vector3 size, PbColliderType type, float phase)
{
    if (w->count >= PB_MAX_COLLIDERS) return;
    PbCollider *c = &w->items[w->count++];
    c->base = p;
    c->size = size;
    c->phase = phase;
    c->type = (unsigned char)type;
    c->delta = (Vector3){0};
    c->box.min = (Vector3){p.x-size.x*.5f,p.y-size.y*.5f,p.z-size.z*.5f};
    c->box.max = (Vector3){p.x+size.x*.5f,p.y+size.y*.5f,p.z+size.z*.5f};
}

void pb_collision_clear(PbCollisionWorld *w) { *w = (PbCollisionWorld){0}; }

void pb_collision_init_test_course(PbCollisionWorld *w)
{
    int i;
    *w = (PbCollisionWorld){0};
    pb_collision_add_box(w, (Vector3){0,-.5f,0}, (Vector3){34,1,34}, PB_COLLIDER_SOLID, 0);
    for (i = 0; i < 8; ++i)
        pb_collision_add_box(w, (Vector3){-7+i*2.2f,.25f+i*.18f,-5}, (Vector3){1.8f,.5f,2.4f}, PB_COLLIDER_SOLID, 0);
    pb_collision_add_box(w, (Vector3){9,1.0f,-5}, (Vector3){2.2f,.45f,2.2f}, PB_COLLIDER_BOUNCE, 0);
    pb_collision_add_box(w, (Vector3){11.8f,2.3f,-5}, (Vector3){2.2f,.45f,2.2f}, PB_COLLIDER_MOVING, 0);
    pb_collision_add_box(w, (Vector3){14.5f,3.4f,-5}, (Vector3){3.2f,.5f,3.0f}, PB_COLLIDER_SOLID, 0);
}

void pb_collision_update(PbCollisionWorld *w, float elapsed)
{
    int i;
    for (i = 0; i < w->count; ++i) {
        PbCollider *c = &w->items[i];
        if(c->type!=PB_COLLIDER_MOVING) { c->delta=(Vector3){0}; continue; }
        Vector3 old = {(c->box.min.x+c->box.max.x)*.5f,(c->box.min.y+c->box.max.y)*.5f,
                       (c->box.min.z+c->box.max.z)*.5f};
        Vector3 p = c->base;
        if (c->type == PB_COLLIDER_MOVING) p.y += sinf(elapsed*1.6f+c->phase)*.8f;
        c->delta = (Vector3){p.x-old.x,p.y-old.y,p.z-old.z};
        c->box.min = (Vector3){p.x-c->size.x*.5f,p.y-c->size.y*.5f,p.z-c->size.z*.5f};
        c->box.max = (Vector3){p.x+c->size.x*.5f,p.y+c->size.y*.5f,p.z+c->size.z*.5f};
    }
}

static bool vertical_overlap(Vector3 p, float hh, BoundingBox b)
{
    return p.y+hh > b.min.y && p.y-hh < b.max.y;
}

int pb_collision_move(PbCollisionWorld *w, Vector3 *p, Vector3 *v,
                      float radius, float hh, float dt, bool *grounded)
{
    Vector3 old = *p;
    int ground = -1;
    int i;
    *grounded = false;
    p->x += v->x*dt;
    p->z += v->z*dt;
    for (i = 0; i < w->count; ++i) {
        BoundingBox b = w->items[i].box;
        float cx;
        float cz;
        float dx;
        float dz;
        float d2;
        if (!vertical_overlap(*p, hh, b)) continue;
        cx = fmaxf(b.min.x, fminf(p->x, b.max.x));
        cz = fmaxf(b.min.z, fminf(p->z, b.max.z));
        dx = p->x-cx; dz = p->z-cz; d2 = dx*dx+dz*dz;
        if (d2 < radius*radius) {
            float d = sqrtf(d2);
            if (d > .0001f) { p->x = cx+dx/d*radius; p->z = cz+dz/d*radius; }
            else { p->x = old.x; p->z = old.z; }
        }
    }
    old.y = p->y;
    p->y += v->y*dt;
    for (i = 0; i < w->count; ++i) {
        BoundingBox b = w->items[i].box;
        bool xz = p->x+radius>b.min.x && p->x-radius<b.max.x &&
                  p->z+radius>b.min.z && p->z-radius<b.max.z;
        if (!xz) continue;
        /* Ground recovery also accepts a shallow initial overlap. This keeps
           authored respawns and moving-platform seams from dropping a capsule. */
        if (v->y <= 0 && old.y > b.max.y && old.y-hh >= b.max.y-.25f && p->y-hh <= b.max.y) {
            p->y = b.max.y+hh; v->y = 0; *grounded = true; ground = i;
        } else if (v->y > 0 && old.y+hh <= b.min.y && p->y+hh >= b.min.y) {
            p->y = b.min.y-hh; v->y = 0;
        }
    }
    if (ground >= 0 && w->items[ground].type == PB_COLLIDER_MOVING) {
        p->x += w->items[ground].delta.x; p->y += w->items[ground].delta.y; p->z += w->items[ground].delta.z;
    }
    return ground;
}

void pb_collision_draw(const PbCollisionWorld *w)
{
    int i;
    for (i = 0; i < w->count; ++i) {
        const PbCollider *c = &w->items[i];
        Vector3 center = {(c->box.min.x+c->box.max.x)*.5f,(c->box.min.y+c->box.max.y)*.5f,
                          (c->box.min.z+c->box.max.z)*.5f};
        Color color = c->type == PB_COLLIDER_BOUNCE ? (Color){251,188,72,255} :
                      c->type == PB_COLLIDER_MOVING ? (Color){165,133,231,255} : (Color){103,194,171,255};
        DrawCubeV(center, c->size, color);
    }
}
