#ifndef POLYBLOOM_RENDER_H
#define POLYBLOOM_RENDER_H

#include "meshgen.h"
#include "particles.h"

typedef struct PbRenderer {
    RenderTexture2D target;
    Shader world_shader;
    PbMeshCache meshes;
    int fog_color_location;
    int fog_near_location;
    int fog_far_location;
    int view_location;
} PbRenderer;

void pb_renderer_open(PbRenderer *renderer);
void pb_renderer_close(PbRenderer *renderer);
void pb_renderer_begin(PbRenderer *renderer, Camera3D camera, Color clear,
                       float fog_near, float fog_far);
void pb_renderer_end(PbRenderer *renderer);
void pb_draw_blob_shadow(Vector3 position, float radius, float height);
void pb_draw_world(PbRenderer *renderer, const PbParticles *particles,
                   Vector3 player, float elapsed, int world_style, bool reduced);

#endif
