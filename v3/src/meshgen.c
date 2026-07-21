#include "meshgen.h"
#include "raymath.h"

#include <string.h>

static Mesh make_wedge(void)
{
    static const float vertices[] = {
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
         0.5f,-0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f,-0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
    };
    Mesh mesh = {0};
    int i;
    mesh.vertexCount = 24;
    mesh.triangleCount = 8;
    mesh.vertices = MemAlloc(sizeof(vertices));
    mesh.normals = MemAlloc(sizeof(vertices));
    memcpy(mesh.vertices, vertices, sizeof(vertices));
    for (i = 0; i < mesh.vertexCount; ++i) {
        Vector3 n = Vector3Normalize((Vector3){vertices[i*3], 0.7f, vertices[i*3 + 2]});
        mesh.normals[i*3] = n.x;
        mesh.normals[i*3 + 1] = n.y;
        mesh.normals[i*3 + 2] = n.z;
    }
    UploadMesh(&mesh, false);
    return mesh;
}

void pb_meshes_load(PbMeshCache *cache)
{
    cache->model[PB_MESH_CUBE] = LoadModelFromMesh(GenMeshCube(1, 1, 1));
    cache->model[PB_MESH_SPHERE] = LoadModelFromMesh(GenMeshSphere(0.5f, 8, 12));
    cache->model[PB_MESH_CYLINDER] = LoadModelFromMesh(GenMeshCylinder(0.5f, 1, 6));
    cache->model[PB_MESH_CONE] = LoadModelFromMesh(GenMeshCone(0.5f, 1, 7));
    cache->model[PB_MESH_RING] = LoadModelFromMesh(GenMeshTorus(0.12f, 0.5f, 6, 16));
    cache->model[PB_MESH_WEDGE] = LoadModelFromMesh(make_wedge());
    cache->model[PB_MESH_PETAL] = LoadModelFromMesh(GenMeshCube(0.38f, 0.12f, 0.9f));
}

void pb_meshes_unload(PbMeshCache *cache)
{
    int i;
    for (i = 0; i < PB_MESH_COUNT; ++i) UnloadModel(cache->model[i]);
}

void pb_meshes_set_shader(PbMeshCache *cache, Shader shader)
{
    int i;
    for (i = 0; i < PB_MESH_COUNT; ++i) cache->model[i].materials[0].shader = shader;
}

void pb_draw_mesh(PbMeshCache *cache, PbMeshId id, Vector3 position,
                  Vector3 axis, float angle, Vector3 scale, Color color)
{
    DrawModelEx(cache->model[id], position, axis, angle, scale, color);
}
