#include "levels.h"
#include "raymath.h"

#include <math.h>

#define Q 10.0f
#define PIECE(x,y,z,sx,sy,sz,t,c) {(x)*10,(y)*10,(z)*10,(sx)*10,(sy)*10,(sz)*10,t,c}

enum { LEVEL_SOLID, LEVEL_BOUNCE, LEVEL_MOVING, LEVEL_FALLING };

static const PbLevelPiece garden_pieces[] = {
    PIECE(0,0,2,12,1,12,LEVEL_SOLID,0),
    PIECE(0,.5,-7,5,1,4,LEVEL_SOLID,0), PIECE(2,1,-12,4,1,4,LEVEL_SOLID,0),
    PIECE(0,1.5,-17,4,1,4,LEVEL_SOLID,1), PIECE(-2,2,-22,4,1,4,LEVEL_SOLID,1),
    PIECE(-2,2.5,-27,3,1,3,LEVEL_BOUNCE,2),
    PIECE(-2,1,-36,9,1,5,LEVEL_SOLID,0), PIECE(-1,0,-32,5,1,5,LEVEL_SOLID,3),
    PIECE(1,1,-43,7,1,6,LEVEL_SOLID,0),
    PIECE(-5,2,-49,3,1,4,LEVEL_MOVING,1), PIECE(4,2,-49,3,1,9,LEVEL_SOLID,1),
    PIECE(0,2,-56,9,1,5,LEVEL_SOLID,0),
    PIECE(0,2.5,-62,3,1,3,LEVEL_BOUNCE,2), PIECE(2,5,-67,3,1,3,LEVEL_BOUNCE,2),
    PIECE(-1,7.5,-72,3,1,3,LEVEL_BOUNCE,2), PIECE(-5,4,-65,3,1,8,LEVEL_SOLID,3),
    PIECE(0,7,-78,8,1,5,LEVEL_SOLID,0),
    PIECE(0,6,-85,6,1,6,LEVEL_SOLID,0), PIECE(2,5,-92,6,1,6,LEVEL_SOLID,0),
    PIECE(0,4,-99,7,1,6,LEVEL_SOLID,0), PIECE(-2,3,-106,7,1,6,LEVEL_SOLID,0),
    PIECE(0,2,-113,9,1,7,LEVEL_SOLID,0), PIECE(0,1,-121,12,1,9,LEVEL_SOLID,1)
};

static const PbCameraZone garden_zones[] = {
    {-80,80,-40,80,PB_SECTION_AWAKENING,10}, {-60,60,-160,-40,PB_SECTION_ORCHARD,10},
    {-80,80,-400,-160,PB_SECTION_RINGWATER,12}, {-100,100,-590,-400,PB_SECTION_GROVE,11},
    {-100,100,-810,-590,PB_SECTION_CLIMB,13}, {-120,120,-1300,-810,PB_SECTION_RUNOFF,12}
};

static const PbLevelPiece cascade_pieces[] = {
    PIECE(0,0,3,12,1,14,LEVEL_SOLID,0), PIECE(0,-1,-8,10,1,8,LEVEL_SOLID,0),
    PIECE(-5,-2,-17,3,1,11,LEVEL_SOLID,1), PIECE(0,-2,-17,3,1,11,LEVEL_SOLID,0),
    PIECE(5,-2,-17,3,1,11,LEVEL_SOLID,2), PIECE(0,-3,-27,12,1,6,LEVEL_BOUNCE,3),
    PIECE(-4,-3,-34,3,1,4,LEVEL_FALLING,1), PIECE(0,-3,-39,3,1,4,LEVEL_FALLING,1),
    PIECE(4,-3,-44,3,1,4,LEVEL_FALLING,1), PIECE(0,-5,-38,12,1,18,LEVEL_SOLID,3),
    PIECE(0,-3,-50,10,1,5,LEVEL_SOLID,0),
    PIECE(0,-3,-57,3,1,4,LEVEL_SOLID,1), PIECE(-3,-3,-63,3,1,4,LEVEL_MOVING,1),
    PIECE(3,-3,-69,3,1,4,LEVEL_MOVING,1), PIECE(0,-3,-75,5,1,5,LEVEL_SOLID,0),
    PIECE(0,-4,-82,4,1,7,LEVEL_SOLID,2), PIECE(0,-5,-90,10,1,7,LEVEL_SOLID,0),
    PIECE(-4,-3,-98,3,1,5,LEVEL_SOLID,1), PIECE(0,-1,-104,3,1,5,LEVEL_SOLID,1),
    PIECE(4,1,-110,3,1,5,LEVEL_SOLID,1), PIECE(0,-3,-106,5,1,20,LEVEL_SOLID,3),
    PIECE(0,0,-118,8,1,6,LEVEL_SOLID,0), PIECE(-2,-1,-126,8,1,7,LEVEL_MOVING,1),
    PIECE(2,-2,-135,8,1,8,LEVEL_SOLID,0), PIECE(0,-3,-145,10,1,10,LEVEL_SOLID,2)
};

static const PbCameraZone cascade_zones[] = {
    {-100,100,-100,100,PB_SECTION_AWAKENING,12}, {-100,100,-300,-100,PB_SECTION_ORCHARD,13},
    {-100,100,-500,-300,PB_SECTION_RINGWATER,12}, {-100,100,-900,-500,PB_SECTION_GROVE,14},
    {-100,100,-1160,-900,PB_SECTION_CLIMB,12}, {-120,120,-1520,-1160,PB_SECTION_RUNOFF,14}
};

static Color palette(int id)
{
    static const Color colors[] = {{92,190,158,255},{126,211,185,255},{255,195,81,255},{178,151,217,255}};
    return colors[id&3];
}

void pb_level_petalgarden_init(PbLevel *l, PbCollisionWorld *world)
{
    int i;
    *l = (PbLevel){0};
    l->level_id=PB_LEVEL_GARDEN;
    pb_collision_clear(world);
    l->piece_count = (int)(sizeof(garden_pieces)/sizeof(garden_pieces[0]));
    for (i = 0; i < l->piece_count; ++i) {
        PbLevelPiece p = garden_pieces[i];
        Vector3 position = {p.px/Q,p.py/Q,p.pz/Q};
        Vector3 size = {p.sx/Q,p.sy/Q,p.sz/Q};
        PbColliderType type = p.type == LEVEL_BOUNCE ? PB_COLLIDER_BOUNCE :
                              p.type == LEVEL_MOVING ? PB_COLLIDER_MOVING : PB_COLLIDER_SOLID;
        l->pieces[i] = p;
        pb_collision_add_box(world,position,size,type,i*.7f);
    }
    l->camera_zone_count = (int)(sizeof(garden_zones)/sizeof(garden_zones[0]));
    for (i = 0; i < l->camera_zone_count; ++i) l->camera_zones[i] = garden_zones[i];
    for (i = 0; i < PB_LEVEL_GLINTS; ++i) {
        float t = i/29.0f;
        float z = 1.0f-t*123.0f;
        float y;
        if (z > -28) y = 1.2f+(-z/28.0f)*2.0f;
        else if (z > -58) y = 2.2f;
        else if (z > -78) y = 3.2f+(-58.0f-z)*.24f;
        else y = 8.0f-(-78.0f-z)*.15f;
        l->glints[i].position = (Vector3){sinf(t*12.0f)*2.0f,y,z};
    }
    l->seeds[0].position = (Vector3){-2,5,-24};
    l->seeds[1].position = (Vector3){4,4,-50};
    l->seeds[2].position = (Vector3){-5,6,-66};
    l->checkpoint = (Vector3){-2,2.2f,-36};
    l->checkpoints[0]=l->checkpoint; l->checkpoint_count=1;
    l->respawn = (Vector3){0,1.15f,3};
    l->gate = (Vector3){0,2,-124};
}

void pb_level_prismrush_init(PbLevel *l, PbCollisionWorld *world)
{
    int i;
    *l=(PbLevel){0}; l->level_id=PB_LEVEL_CASCADE; l->shatterwake_z=-48;
    pb_collision_clear(world);
    l->piece_count=(int)(sizeof(cascade_pieces)/sizeof(cascade_pieces[0]));
    for(i=0;i<l->piece_count;++i) {
        PbLevelPiece p=cascade_pieces[i];
        PbColliderType type=p.type==LEVEL_BOUNCE?PB_COLLIDER_BOUNCE:p.type==LEVEL_MOVING?PB_COLLIDER_MOVING:PB_COLLIDER_SOLID;
        l->pieces[i]=p;
        pb_collision_add_box(world,(Vector3){p.px/Q,p.py/Q,p.pz/Q},(Vector3){p.sx/Q,p.sy/Q,p.sz/Q},type,i*.55f);
    }
    l->camera_zone_count=(int)(sizeof(cascade_zones)/sizeof(cascade_zones[0]));
    for(i=0;i<l->camera_zone_count;++i) l->camera_zones[i]=cascade_zones[i];
    for(i=0;i<PB_LEVEL_GLINTS;++i) {
        float t=i/29.0f, z=2-t*146;
        l->glints[i].position=(Vector3){sinf(t*18)*3,-1.8f+sinf(t*8)*1.2f,z};
    }
    l->seeds[0].position=(Vector3){5,0,-20};
    l->seeds[1].position=(Vector3){-4,-1,-72};
    l->seeds[2].position=(Vector3){4,3,-110};
    l->checkpoints[0]=(Vector3){0,-1.8f,-49};
    l->checkpoints[1]=(Vector3){0,-2.8f,-90};
    l->checkpoint_count=2; l->checkpoint=l->checkpoints[0];
    l->respawn=(Vector3){0,1.15f,5}; l->gate=(Vector3){0,-1.8f,-147};
}

void pb_level_update(PbLevel *l, PbCollisionWorld *world, Vector3 p, float dt)
{
    int i;
    for (i = 0; i < PB_LEVEL_GLINTS; ++i)
        if (!l->glints[i].collected && Vector3Distance(p,l->glints[i].position)<1.0f) {
            l->glints[i].collected=true; l->glint_count++;
        }
    for (i = 0; i < PB_LEVEL_SEEDS; ++i)
        if (!l->seeds[i].collected && Vector3Distance(p,l->seeds[i].position)<1.2f) {
            l->seeds[i].collected=true; l->seed_count++;
        }
    for(i=0;i<l->checkpoint_count;++i) if(Vector3Distance(p,l->checkpoints[i])<2.5f) {
        l->checkpoint_active=true; l->checkpoint=l->checkpoints[i]; l->respawn=l->checkpoints[i];
    }
    if (Vector3Distance(p,l->gate)<2.3f) l->complete=true;
    for (i = 0; i < l->camera_zone_count; ++i) {
        PbCameraZone z=l->camera_zones[i];
        if (p.x>=z.min_x/Q && p.x<=z.max_x/Q && p.z>=z.min_z/Q && p.z<=z.max_z/Q) l->section=z.section;
    }
    if(l->level_id==PB_LEVEL_CASCADE) {
        for(i=0;i<l->piece_count;++i) if(l->pieces[i].type==LEVEL_FALLING) {
            Vector3 center={(l->pieces[i].px/Q),l->pieces[i].py/Q,l->pieces[i].pz/Q};
            if(l->falling_timer[i]<=0&&Vector3Distance(p,center)<2.5f) l->falling_timer[i]=.001f;
            if(l->falling_timer[i]>0) {
                l->falling_timer[i]+=dt;
                if(l->falling_timer[i]>.8f&&l->falling_timer[i]<3.2f) {
                    float drop=fminf(12,(l->falling_timer[i]-.8f)*18);
                    world->items[i].box.min.y=center.y-l->pieces[i].sy/Q*.5f-drop;
                    world->items[i].box.max.y=center.y+l->pieces[i].sy/Q*.5f-drop;
                } else if(l->falling_timer[i]>=3.2f) {
                    l->falling_timer[i]=0;
                    world->items[i].box.min.y=center.y-l->pieces[i].sy/Q*.5f;
                    world->items[i].box.max.y=center.y+l->pieces[i].sy/Q*.5f;
                }
            }
        }
        if(!l->chase_escaped&&!l->chase_active&&p.z<-50) { l->chase_active=true; l->shatterwake_z=p.z+10; }
        if(l->chase_active) {
            l->shatterwake_z-=6.5f*dt;
            if(p.z<-89) { l->chase_active=false; l->chase_escaped=true; }
            else if(l->shatterwake_z<=p.z+1.2f) { l->chase_hit=true; l->chase_active=false; l->shatterwake_z=-48; }
        }
    }
}

float pb_level_camera_distance(const PbLevel *l, Vector3 p)
{
    int i;
    for (i=0;i<l->camera_zone_count;++i) {
        PbCameraZone z=l->camera_zones[i];
        if(p.x>=z.min_x/Q&&p.x<=z.max_x/Q&&p.z>=z.min_z/Q&&p.z<=z.max_z/Q) return z.distance;
    }
    return 10;
}

void pb_level_draw(PbLevel *l, PbRenderer *r, float elapsed, bool reduced)
{
    int i;
    for(i=0;i<l->piece_count;++i) {
        PbLevelPiece p=l->pieces[i];
        Vector3 pos={p.px/Q,p.py/Q,p.pz/Q};
        Vector3 size={p.sx/Q,p.sy/Q,p.sz/Q};
        Color piece_color=l->level_id==PB_LEVEL_CASCADE?(Color[]){(Color){53,157,196,255},(Color){91,202,226,255},(Color){242,117,181,255},(Color){153,225,184,255}}[p.color&3]:palette(p.color);
        if(p.type==LEVEL_MOVING) pos.y+=sinf(elapsed*1.6f+i*.7f)*.8f;
        if(p.type==LEVEL_FALLING&&l->falling_timer[i]>.8f) pos.y-=fminf(12,(l->falling_timer[i]-.8f)*18);
        if(p.type==LEVEL_FALLING&&l->falling_timer[i]>0&&l->falling_timer[i]<.8f&&
           fmodf(l->falling_timer[i],.2f)<.1f) piece_color=(Color){255,95,170,255};
        pb_draw_mesh(&r->meshes,PB_MESH_CUBE,pos,(Vector3){0,1,0},0,size,piece_color);
    }
    for(i=0;i<PB_LEVEL_GLINTS;++i) if(!l->glints[i].collected) {
        Vector3 p=l->glints[i].position; p.y+=sinf(elapsed*4+i)*.12f;
        pb_draw_mesh(&r->meshes,PB_MESH_SPHERE,p,(Vector3){0,1,0},0,(Vector3){.28f,.28f,.28f},(Color){220,247,255,255});
    }
    for(i=0;i<PB_LEVEL_SEEDS;++i) if(!l->seeds[i].collected)
        pb_draw_mesh(&r->meshes,PB_MESH_PETAL,l->seeds[i].position,(Vector3){0,1,0},elapsed*70,
                     (Vector3){1.5f,1.5f,1.5f},(Color){255,206,70,255});
    for(i=0;i<l->checkpoint_count;++i) DrawCylinder(l->checkpoints[i],.8f,.8f,3,8,
        Vector3Distance(l->respawn,l->checkpoints[i])<.1f?(Color){255,220,90,180}:(Color){160,220,255,150});
    {
        float bloom=1+fminf(l->completion_time,1.5f)*.35f;
        pb_draw_mesh(&r->meshes,PB_MESH_RING,l->gate,(Vector3){1,0,0},90+elapsed*12,
                     (Vector3){4.4f*bloom,4.4f*bloom,4.4f*bloom},(Color){255,154,123,255});
        for(i=0;i<l->seed_count;++i)
            pb_draw_mesh(&r->meshes,PB_MESH_RING,l->gate,(Vector3){1,0,0},90+elapsed*(18+i*7),
                         (Vector3){5.2f+i*.8f,5.2f+i*.8f,5.2f+i*.8f},(Color){255,211,84,210});
        if(l->completion_time>0) for(i=0;i<(reduced?5:12);++i) {
            float a=i*6.28318f/(reduced?5:12)+elapsed;
            Vector3 q={l->gate.x+cosf(a)*(2+l->completion_time*2),l->gate.y+sinf(a)*(2+l->completion_time),l->gate.z};
            pb_draw_mesh(&r->meshes,PB_MESH_PETAL,q,(Vector3){0,0,1},a*57.3f,
                         (Vector3){1.4f,1.4f,1.4f},(Color){255,184,105,220});
        }
    }
    if(l->chase_active) for(i=0;i<(reduced?10:24);++i) {
        Vector3 q={(float)((i*7)%11)-5,(float)((i*5)%7)-4,l->shatterwake_z+(i%3)*.4f};
        pb_draw_mesh(&r->meshes,PB_MESH_WEDGE,q,(Vector3){0,1,0},elapsed*90+i*17,
                     (Vector3){1.2f,1.2f,1.2f},(Color){73,37,104,230});
    }
}

const char *pb_level_section_name(const PbLevel *l)
{
    static const char *garden[]={"AWAKENING TERRACE","PRISM ORCHARD","RINGWATER GAP","TURNING GROVE","SUNPETAL CLIMB","GARDEN RUNOFF"};
    static const char *cascade[]={"CASCADE ENTRY","SPLIT FALLS","FALLING STEPS","SHATTERWAKE CHASE","CORKSCREW CANOPY","FINAL RUSH"};
    return l->level_id==PB_LEVEL_CASCADE?cascade[l->section]:garden[l->section];
}
