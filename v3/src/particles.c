#include "particles.h"

void pb_particles_emit(PbParticles *pool, Vector3 position, Vector3 velocity,
                       Color color, float life, float size)
{
    int limit = pool->reduced ? 400 : PB_MAX_PARTICLES;
    PbParticle *p = &pool->item[pool->cursor++ % limit];
    if (p->life <= 0) pool->active++;
    p->position = position;
    p->velocity = velocity;
    p->color = color;
    p->life = life;
    p->size = size;
}

void pb_particles_update(PbParticles *pool, float dt)
{
    int i;
    for (i = 0; i < PB_MAX_PARTICLES; ++i) {
        PbParticle *p = &pool->item[i];
        if (p->life <= 0) continue;
        p->life -= dt;
        if (p->life <= 0) { p->life = 0; pool->active--; continue; }
        p->position.x += p->velocity.x*dt;
        p->position.y += p->velocity.y*dt;
        p->position.z += p->velocity.z*dt;
        p->velocity.y -= 2.0f*dt;
        p->color.a = (unsigned char)(255.0f*(p->life < 1.0f ? p->life : 1.0f));
    }
}

void pb_particles_draw(const PbParticles *pool)
{
    int i;
    for (i = 0; i < PB_MAX_PARTICLES; ++i) {
        const PbParticle *p = &pool->item[i];
        if (p->life > 0) DrawSphere(p->position, p->size*p->life, p->color);
    }
}

