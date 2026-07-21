#ifndef POLYBLOOM_PARTICLES_H
#define POLYBLOOM_PARTICLES_H

#include "raylib.h"

#define PB_MAX_PARTICLES 1024

typedef struct PbParticle {
    Vector3 position;
    Vector3 velocity;
    Color color;
    float life;
    float size;
} PbParticle;

typedef struct PbParticles {
    PbParticle item[PB_MAX_PARTICLES];
    int cursor;
    int active;
    bool reduced;
} PbParticles;

void pb_particles_emit(PbParticles *pool, Vector3 position, Vector3 velocity,
                       Color color, float life, float size);
void pb_particles_update(PbParticles *pool, float dt);
void pb_particles_draw(const PbParticles *pool);

#endif

