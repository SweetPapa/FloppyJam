#ifndef POLYBLOOM_MESHGEN_H
#define POLYBLOOM_MESHGEN_H

#include "raylib.h"

typedef enum PbMeshId {
    PB_MESH_CUBE,
    PB_MESH_SPHERE,
    PB_MESH_CYLINDER,
    PB_MESH_CONE,
    PB_MESH_RING,
    PB_MESH_WEDGE,
    PB_MESH_PETAL,
    PB_MESH_COUNT
} PbMeshId;

typedef struct PbMeshCache {
    Model model[PB_MESH_COUNT];
} PbMeshCache;

void pb_meshes_load(PbMeshCache *cache);
void pb_meshes_unload(PbMeshCache *cache);
void pb_meshes_set_shader(PbMeshCache *cache, Shader shader);
void pb_draw_mesh(PbMeshCache *cache, PbMeshId id, Vector3 position,
                  Vector3 axis, float angle, Vector3 scale, Color color);

#endif

