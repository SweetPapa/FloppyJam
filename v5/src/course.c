/* course.c — turn a BpHole table into a playable BpWorld. */
#include "course.h"
#include <string.h>

#include "../data/holes.h"

#define WALL_T 0.075f
#define WALL_H 0.24f

static void add_wall(BpWorld *w, const BpWallDef *d)
{
    float dx = d->x1 - d->x0, dz = d->z1 - d->z0;
    float len = sqrtf(dx * dx + dz * dz);
    float h = (d->h > 0.0f) ? d->h : WALL_H;
    BpBox *b;
    if (w->nboxes >= BP_MAX_BOXES || len < 1e-4f) return;
    b = &w->boxes[w->nboxes++];
    memset(b, 0, sizeof(*b));
    b->cx = 0.5f * (d->x0 + d->x1);
    b->cy = d->y + 0.5f * h;
    b->cz = 0.5f * (d->z0 + d->z1);
    b->hx = 0.5f * len + 0.5f * WALL_T;   /* overlap at corners */
    b->hy = 0.5f * h;
    b->hz = 0.5f * WALL_T;
    b->yaw = atan2f(dz, dx);
    b->kind = d->kind;
    b->gate = d->gate;
    b->mover = d->mover;
}

void bp_course_build(BpWorld *w, int index)
{
    const BpHole *H;
    int i;

    if (index < 0) index = 0;
    if (index >= BP_NHOLES) index = BP_NHOLES - 1;
    H = &BP_HOLES[index];

    bp_world_reset(w);
    w->kill_y = H->kill_y;
    w->cup_sealed = H->sealed;

    for (i = 0; i < H->npads && w->npads < BP_MAX_PADS; ++i)
        w->pads[w->npads++] = H->pads[i];
    for (i = 0; i < H->nposts && w->nposts < BP_MAX_POSTS; ++i)
        w->posts[w->nposts++] = H->posts[i];
    for (i = 0; i < H->npockets && w->npockets < BP_MAX_POCKETS; ++i)
        w->pockets[w->npockets++] = H->pockets[i];
    for (i = 0; i < H->nmovers && w->nmovers < BP_MAX_MOVERS; ++i)
        w->movers[w->nmovers++] = H->movers[i];
    for (i = 0; i < H->nwalls; ++i)
        add_wall(w, &H->walls[i]);

    /* A sealed cup gets a flush cap disc; it is removed when the eight drops. */
    if (H->sealed) {
        for (i = 0; i < w->npockets; ++i) {
            if (w->pockets[i].kind == PK_CUP && w->nboxes < BP_MAX_BOXES) {
                BpBox *b = &w->boxes[w->nboxes++];
                memset(b, 0, sizeof(*b));
                b->cx = w->pockets[i].x;
                b->cy = w->pockets[i].y - 0.012f;
                b->cz = w->pockets[i].z;
                b->hx = b->hz = BP_CUP_R * 1.15f;
                b->hy = 0.012f;
                b->kind = BOX_CAP;
            }
        }
    }

    /* cue ball first, always index 0 */
    w->nballs = 1;
    memset(&w->balls[0], 0, sizeof(BpBall));
    w->balls[0].kind = BALL_CUE;
    w->balls[0].state = BS_REST;
    w->balls[0].axis = v3(1.0f, 0.0f, 0.0f);

    for (i = 0; i < H->nballs && w->nballs < BP_MAX_BALLS; ++i) {
        const BpBallDef *d = &H->balls[i];
        BpBall *b = &w->balls[w->nballs++];
        memset(b, 0, sizeof(*b));
        b->kind = d->kind;
        b->color = d->color;
        b->num = d->num;
        b->state = BS_REST;
        b->axis = v3(1.0f, 0.0f, 0.0f);
        b->p = v3(d->x, 0.0f, d->z);
    }

    bp_world_finalize(w);

    /* drop everything onto the floor now that the geometry exists */
    w->balls[0].p = v3(H->tee_x, 0.0f, H->tee_z);
    for (i = 0; i < w->nballs; ++i) {
        BpBall *b = &w->balls[i];
        float g = bp_ground_h(w, b->p.x, b->p.z, 50.0f, NULL);
        if (g < -1e8f) g = 0.0f;
        b->p.y = g + BP_R;
        b->home = b->p;
        b->prev = b->p;
    }
}

int bp_course_cup(const BpWorld *w)
{
    int i;
    for (i = 0; i < w->npockets; ++i)
        if (w->pockets[i].kind == PK_CUP) return i;
    return -1;
}

void bp_course_bounds(const BpWorld *w, V3 *lo, V3 *hi)
{
    int i;
    *lo = v3( 1e9f,  1e9f,  1e9f);
    *hi = v3(-1e9f, -1e9f, -1e9f);
    for (i = 0; i < w->npads; ++i) {
        const BpPad *p = &w->pads[i];
        float h0 = p->y0, h1 = p->y0 + p->sx * (p->x1 - p->x0) + p->sz * (p->z1 - p->z0);
        if (p->x0 < lo->x) lo->x = p->x0;
        if (p->z0 < lo->z) lo->z = p->z0;
        if (p->x1 > hi->x) hi->x = p->x1;
        if (p->z1 > hi->z) hi->z = p->z1;
        if (h0 < lo->y) lo->y = h0;
        if (h1 < lo->y) lo->y = h1;
        if (h0 > hi->y) hi->y = h0;
        if (h1 > hi->y) hi->y = h1;
    }
    if (w->npads == 0) { *lo = v3(-1, 0, -1); *hi = v3(1, 0, 1); }
}
