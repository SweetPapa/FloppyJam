#include "render.h"

#include <math.h>

static const char *world_vs =
    "#version 330\n"
    "in vec3 vertexPosition; in vec3 vertexNormal; in vec4 vertexColor;\n"
    "uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;\n"
    "out vec3 n; out vec3 wp; out vec4 vc;\n"
    "void main(){ vec4 w=matModel*vec4(vertexPosition,1); wp=w.xyz;"
    "n=normalize((matNormal*vec4(vertexNormal,0)).xyz); vc=vertexColor; gl_Position=mvp*vec4(vertexPosition,1); }";

static const char *world_fs =
    "#version 330\n"
    "in vec3 n; in vec3 wp; in vec4 vc; out vec4 color;\n"
    "uniform vec3 viewPos; uniform vec4 colDiffuse;\n"
    "void main(){ vec3 l=normalize(vec3(-.4,.85,-.3)); float d=max(dot(n,l),0);"
    "float hemi=.48+.20*n.y; float rim=pow(1-max(dot(normalize(viewPos-wp),n),0),3)*.22;"
    "vec3 c=vc.rgb*colDiffuse.rgb*(hemi+d*.55)+rim*vec3(1,.8,1);"
    "color=vec4(c,vc.a*colDiffuse.a); }";

void pb_renderer_open(PbRenderer *renderer)
{
    renderer->target = LoadRenderTexture(640, 360);
    SetTextureFilter(renderer->target.texture, TEXTURE_FILTER_BILINEAR);
    renderer->world_shader = LoadShaderFromMemory(world_vs, world_fs);
    renderer->view_location = GetShaderLocation(renderer->world_shader, "viewPos");
    renderer->world_shader.locs[SHADER_LOC_VECTOR_VIEW] = renderer->view_location;
    pb_meshes_load(&renderer->meshes);
    pb_meshes_set_shader(&renderer->meshes, renderer->world_shader);
}

void pb_renderer_close(PbRenderer *renderer)
{
    pb_meshes_unload(&renderer->meshes);
    UnloadShader(renderer->world_shader);
    UnloadRenderTexture(renderer->target);
}

void pb_renderer_begin(PbRenderer *renderer, Camera3D camera, Color clear)
{
    float view[3] = {camera.position.x, camera.position.y, camera.position.z};
    SetShaderValue(renderer->world_shader, renderer->view_location, view, SHADER_UNIFORM_VEC3);
    BeginTextureMode(renderer->target);
    ClearBackground(clear);
    BeginMode3D(camera);
}

void pb_renderer_end(PbRenderer *renderer)
{
    float sx;
    float sy;
    float scale;
    Rectangle source = {0, 0, 640, -360};
    Rectangle dest;
    EndMode3D();
    EndTextureMode();
    BeginDrawing();
    ClearBackground(BLACK);
    sx = GetScreenWidth()/640.0f;
    sy = GetScreenHeight()/360.0f;
    scale = sx < sy ? sx : sy;
    dest = (Rectangle){(GetScreenWidth() - 640*scale)*0.5f,
                       (GetScreenHeight() - 360*scale)*0.5f,
                       640*scale, 360*scale};
    DrawTexturePro(renderer->target.texture, source, dest, (Vector2){0, 0}, 0, WHITE);
}

void pb_draw_blob_shadow(Vector3 position, float radius, float height)
{
    float fade = 1.0f - fminf(height/7.0f, 0.8f);
    DrawCylinder((Vector3){position.x, 0.012f, position.z}, radius*fade, radius*fade,
                 0.018f, 16, (Color){46, 48, 75, (unsigned char)(85*fade)});
}

void pb_draw_world(PbRenderer *r, const PbParticles *particles, Vector3 player,
                   float elapsed, int style, bool reduced)
{
    int i;
    int decor_count=reduced?8:18;
    Color mint = style?(Color){20,24,62,255}:(Color){73,168,211,255};
    pb_draw_mesh(&r->meshes,PB_MESH_CUBE,(Vector3){0,-1.35f,-38},(Vector3){0,1,0},0,
                 (Vector3){90,.12f,180},mint);
    if(!style) for(i=0;i<(reduced?7:16);++i) {
        float z=-4-i*7.0f+fmodf(elapsed*(.3f+(i%3)*.08f),7.0f);
        float x=((i*11)%31)-15.0f;
        pb_draw_mesh(&r->meshes,PB_MESH_CUBE,(Vector3){x,-1.25f,z},(Vector3){0,1,0},0,
                     (Vector3){5.5f,.025f,.12f},(Color){151,224,238,175});
    }
    for (i = 0; i < decor_count; ++i) {
        float x = (float)((i*7)%17) - 8.0f;
        float z = (float)((i*11)%19) - 9.0f;
        float sway = sinf(elapsed + i)*0.08f;
        if(style) {
            pb_draw_mesh(&r->meshes,PB_MESH_WEDGE,(Vector3){x,1.2f+sway,z},(Vector3){0,1,0},i*37+elapsed*12,
                         (Vector3){.8f,2.8f,.8f},i&1?(Color){45,186,221,255}:(Color){179,70,190,255});
        } else {
            int p;
            pb_draw_mesh(&r->meshes, PB_MESH_CYLINDER, (Vector3){x,0,z}, (Vector3){0,1,0}, 0,
                         (Vector3){0.4f,1.2f,0.4f}, (Color){79,112,91,255});
            pb_draw_mesh(&r->meshes, PB_MESH_SPHERE, (Vector3){x+sway,1.65f,z}, (Vector3){0,1,0}, 0,
                         (Vector3){1.45f,1.45f,1.45f}, (Color){116,219,170,255});
            for(p=0;p<5;++p) {
                float a=p*1.2566f;
                pb_draw_mesh(&r->meshes,PB_MESH_PETAL,(Vector3){x+cosf(a)*.55f,.2f,z+sinf(a)*.55f},
                             (Vector3){0,1,0},a*57.3f,(Vector3){.65f,.65f,.65f},(Color){255,154,139,255});
            }
        }
    }
    for(i=0;i<(reduced?3:7);++i) {
        Vector3 cloud={(float)((i*13)%23)-11,10+(i%3)*1.7f,-12-i*14.0f};
        pb_draw_mesh(&r->meshes,PB_MESH_SPHERE,cloud,(Vector3){0,1,0},0,
                     (Vector3){4,1.4f,1.8f},style?(Color){91,75,151,180}:(Color){255,249,225,190});
    }
    if(style) for(i=0;i<(reduced?32:88);++i) {
        float x=player.x+(float)((i*47)%101)/4.0f-12.5f;
        float z=player.z+(float)((i*71)%109)/4.0f-17.0f;
        float fall=fmodf((float)((i*31)%97)/7.0f-elapsed*(1.8f+(i%5)*.12f),15.0f);
        float y=player.y+(fall<0?fall+15:fall)-3.0f;
        float drift=sinf(elapsed*.7f+i)*.35f;
        pb_draw_mesh(&r->meshes,PB_MESH_SPHERE,(Vector3){x+drift,y,z},(Vector3){0,1,0},0,
                     (Vector3){.07f+(i%3)*.025f,.07f+(i%3)*.025f,.07f+(i%3)*.025f},
                     i&1?(Color){225,244,255,220}:(Color){176,220,255,210});
    }
    pb_draw_blob_shadow(player, .65f, player.y);
    pb_particles_draw(particles);
}
