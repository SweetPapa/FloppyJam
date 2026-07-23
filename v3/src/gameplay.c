#include "gameplay.h"

#include "raymath.h"
#include <math.h>

static void add_object(PbGameplay *g, PbObjectType type, Vector3 position)
{
    PbObject *o=&g->objects[g->object_count++];
    *o=(PbObject){position,position,0,(uint8_t)type,true};
}

static void add_enemy(PbGameplay *g, PbEnemyType type, Vector3 position, float phase)
{
    PbEnemy *e=&g->enemies[g->enemy_count++];
    *e=(PbEnemy){position,position,phase,(uint8_t)type,true,false};
}

void pb_gameplay_init(PbGameplay *g, int level_id)
{
    *g=(PbGameplay){0};
    g->health=3;
    if(level_id==PB_LEVEL_CASCADE) {
        add_object(g,PB_OBJECT_BRITTLE,(Vector3){0,11,-7});
        add_object(g,PB_OBJECT_ROTATING_BAR,(Vector3){0,9,-17});
        add_object(g,PB_OBJECT_HEART,(Vector3){0,8,-92});
        add_object(g,PB_OBJECT_BURST_TARGET,(Vector3){-4,10,-98});
        add_object(g,PB_OBJECT_BURST_TARGET,(Vector3){0,12,-104});
        add_object(g,PB_OBJECT_BURST_TARGET,(Vector3){4,14,-110});
        add_enemy(g,PB_ENEMY_ROLLFIN,(Vector3){-5,9,-18},0);
        add_enemy(g,PB_ENEMY_WISP,(Vector3){0,10,-41},1);
        add_enemy(g,PB_ENEMY_WISP,(Vector3){0,11,-107},2);
        add_enemy(g,PB_ENEMY_ROLLFIN,(Vector3){-2,9,-136},3);
        add_enemy(g,PB_ENEMY_ROLLFIN,(Vector3){2,9,-140},4);
        return;
    }
#if POLYBLOOM_INCLUDE_LEVEL3
    if(level_id==PB_LEVEL_FOUNDRY) {
        add_object(g,PB_OBJECT_ROTATING_BAR,(Vector3){0,9,-38});
        add_object(g,PB_OBJECT_BRITTLE,(Vector3){0,11,-58});
        add_object(g,PB_OBJECT_HEART,(Vector3){0,13,-87});
        add_enemy(g,PB_ENEMY_WISP,(Vector3){-4,11,-31},0);
        add_enemy(g,PB_ENEMY_ROLLFIN,(Vector3){3,10,-70},1);
        add_enemy(g,PB_ENEMY_WISP,(Vector3){0,10,-104},2);
        return;
    }
#endif
#if POLYBLOOM_INCLUDE_LEVEL4
    if(level_id==PB_LEVEL_CROWN) {
        add_object(g,PB_OBJECT_BRITTLE,(Vector3){0,10,-30});
        add_object(g,PB_OBJECT_HEART,(Vector3){0,11,-65});
        add_enemy(g,PB_ENEMY_ROLLFIN,(Vector3){-3,13,-45},0);
        add_enemy(g,PB_ENEMY_WISP,(Vector3){3,12,-55},1);
        g->boss_position=(Vector3){0,9.5f,-88}; g->boss_health=5; g->boss_active=true;
        return;
    }
#endif
    add_object(g,PB_OBJECT_BRITTLE,(Vector3){0,3,-15});
    add_object(g,PB_OBJECT_BRITTLE,(Vector3){-1,4,-18});
    add_object(g,PB_OBJECT_BURST_TARGET,(Vector3){1,7,-70});
    add_object(g,PB_OBJECT_ROTATING_BAR,(Vector3){1,3,-45});
    add_object(g,PB_OBJECT_HEART,(Vector3){0,3,-82});
    add_enemy(g,PB_ENEMY_POGO,(Vector3){-1,3,-20},0);
    add_enemy(g,PB_ENEMY_ROLLFIN,(Vector3){1,3,-48},1);
    add_enemy(g,PB_ENEMY_WISP,(Vector3){0,6,-88},2);
}

void pb_gameplay_chase_hit(PbGameplay *g, PbPlayer *p, Vector3 respawn)
{
    g->falls++; g->penalty_time+=3; pb_player_init(p,respawn);
}

static void hurt(PbGameplay *g, PbPlayer *p, Vector3 source, Vector3 respawn)
{
    Vector3 away;
    if (g->invulnerable>0) return;
    g->health--; g->damage_events++; g->invulnerable=1;
    g->effect_type=PB_EFFECT_DAMAGE; g->effect_position=p->position;
    away=Vector3Normalize(Vector3Subtract(p->position,source));
    p->velocity=(Vector3){away.x*7,7,away.z*7}; p->state=PB_PLAYER_HURT;
    if(g->health<=0) { g->health=3; g->penalty_time+=7; pb_player_init(p,respawn); }
}

void pb_gameplay_update(PbGameplay *g, PbPlayer *p, Vector3 respawn, float dt, float elapsed)
{
    int i;
    g->run_time+=dt;
    g->invulnerable=fmaxf(0,g->invulnerable-dt);
    for(i=0;i<g->object_count;++i) {
        PbObject *o=&g->objects[i];
        if(!o->active) continue;
        if(o->type==PB_OBJECT_ROTATING_BAR) {
            o->position.x=o->base.x+sinf(elapsed*1.3f)*2.5f;
            if(Vector3Distance(p->position,o->position)<1.0f) hurt(g,p,o->position,respawn);
        } else if(o->type==PB_OBJECT_HEART && Vector3Distance(p->position,o->position)<1) {
            if(g->health<3) g->health++;
            o->active=false;
            g->effect_type=PB_EFFECT_BREAK; g->effect_position=o->position;
        } else if((o->type==PB_OBJECT_BRITTLE || o->type==PB_OBJECT_BURST_TARGET) &&
                  Vector3Distance(p->position,o->position)<1.2f && p->state==PB_PLAYER_BURST) {
            o->active=false;
            if(o->type==PB_OBJECT_BURST_TARGET) { p->velocity.y=12; p->air_burst=true; p->state=PB_PLAYER_BOUNCED; }
        }
    }
    for(i=0;i<g->enemy_count;++i) {
        PbEnemy *e=&g->enemies[i];
        if(!e->active) continue;
        if(e->type==PB_ENEMY_POGO) e->position.y=e->base.y+fabsf(sinf(elapsed*2+e->phase))*1.2f;
        else if(e->type==PB_ENEMY_ROLLFIN) e->position.x=e->base.x+sinf(elapsed*1.4f+e->phase)*2;
        else {
            float cycle=fmodf(elapsed+e->phase,2.4f);
            e->position.y=e->base.y+sinf(elapsed+e->phase)*.5f;
            e->vulnerable=cycle>.7f;
            if(cycle>.7f&&cycle<1.05f&&fabsf(p->position.z-e->position.z)<.65f&&
               fabsf(p->position.x-e->position.x)<4.5f) hurt(g,p,e->position,respawn);
        }
        if(Vector3Distance(p->position,e->position)<1.1f) {
            bool stomp=p->velocity.y<0 && p->position.y>e->position.y+.3f;
            bool defeat=p->state==PB_PLAYER_BURST && (e->type!=PB_ENEMY_WISP || e->vulnerable);
            if(stomp||defeat) { e->active=false; g->effect_type=PB_EFFECT_DEFEAT; g->effect_position=e->position;
                p->velocity.y=11; p->air_burst=true; p->state=PB_PLAYER_BOUNCED; }
            else hurt(g,p,e->position,respawn);
        }
    }
#if POLYBLOOM_INCLUDE_LEVEL4
    if(g->boss_active) {
        float cycle;
        float distance;
        g->boss_phase+=dt; g->boss_hit_cooldown=fmaxf(0,g->boss_hit_cooldown-dt);
        g->boss_position.x=sinf(g->boss_phase*.8f)*5.2f;
        g->boss_position.y=9.5f+sinf(g->boss_phase*1.7f)*.8f;
        distance=Vector3Distance(p->position,g->boss_position);
        if(p->state==PB_PLAYER_BURST&&distance<2.5f&&g->boss_hit_cooldown<=0) {
            g->boss_health--; g->boss_hit_cooldown=.7f; p->velocity.y=12; p->air_burst=true;
            g->effect_type=PB_EFFECT_DEFEAT; g->effect_position=g->boss_position;
            if(g->boss_health<=0) g->boss_active=false;
        } else if(distance<1.8f) hurt(g,p,g->boss_position,respawn);
        cycle=fmodf(g->boss_phase,3.2f);
        if(cycle>.8f&&cycle<1.25f) {
            float radius=(cycle-.8f)*20;
            float planar=sqrtf((p->position.x-g->boss_position.x)*(p->position.x-g->boss_position.x)+
                               (p->position.z-g->boss_position.z)*(p->position.z-g->boss_position.z));
            if(fabsf(planar-radius)<.45f) hurt(g,p,g->boss_position,respawn);
        }
    }
#endif
}

void pb_gameplay_fall(PbGameplay *g, PbPlayer *p, Vector3 respawn)
{
    g->falls++; g->health--; g->penalty_time+=3;
    if(g->health<=0) { g->health=3; g->penalty_time+=7; }
    pb_player_init(p,respawn);
}

void pb_gameplay_draw(const PbGameplay *g, PbRenderer *r, float elapsed)
{
    int i;
    for(i=0;i<g->object_count;++i) {
        const PbObject *o=&g->objects[i];
        if(!o->active) continue;
        if(o->type==PB_OBJECT_BRITTLE)
            pb_draw_mesh(&r->meshes,PB_MESH_WEDGE,o->position,(Vector3){0,1,0},elapsed*30,(Vector3){1,2,1},(Color){255,151,177,255});
        else if(o->type==PB_OBJECT_BURST_TARGET)
            pb_draw_mesh(&r->meshes,PB_MESH_RING,o->position,(Vector3){1,0,0},90,(Vector3){1.5f,1.5f,1.5f},(Color){255,220,75,255});
        else if(o->type==PB_OBJECT_ROTATING_BAR)
            pb_draw_mesh(&r->meshes,PB_MESH_CUBE,o->position,(Vector3){0,1,0},elapsed*90,(Vector3){4,.25f,.25f},(Color){245,87,133,255});
        else pb_draw_mesh(&r->meshes,PB_MESH_PETAL,o->position,(Vector3){0,1,0},elapsed*50,(Vector3){1.4f,1.4f,1.4f},(Color){255,105,145,255});
    }
    for(i=0;i<g->enemy_count;++i) {
        const PbEnemy *e=&g->enemies[i];
        Color c=e->type==PB_ENEMY_WISP?(e->vulnerable?(Color){125,242,225,255}:(Color){181,116,232,255}):(Color){242,113,126,255};
        if(!e->active) continue;
        if(e->type==PB_ENEMY_POGO) {
            int leg;
            pb_draw_mesh(&r->meshes,PB_MESH_SPHERE,e->position,(Vector3){0,1,0},0,
                         (Vector3){1.15f,.8f,1.15f},c);
            for(leg=0;leg<3;++leg) { float a=leg*2.094f;
                pb_draw_mesh(&r->meshes,PB_MESH_CONE,(Vector3){e->position.x+cosf(a)*.45f,e->position.y-.55f,e->position.z+sinf(a)*.45f},
                             (Vector3){1,0,0},180,(Vector3){.35f,.65f,.35f},(Color){108,76,145,255}); }
        } else if(e->type==PB_ENEMY_ROLLFIN) {
            pb_draw_mesh(&r->meshes,PB_MESH_SPHERE,e->position,(Vector3){0,1,0},elapsed*120,
                         (Vector3){1.4f,1.4f,1.4f},c);
            pb_draw_mesh(&r->meshes,PB_MESH_WEDGE,(Vector3){e->position.x,e->position.y+.8f,e->position.z},
                         (Vector3){0,1,0},elapsed*120,(Vector3){.35f,.8f,.8f},(Color){255,207,90,255});
        } else {
            int shard;
            for(shard=0;shard<4;++shard) { float a=elapsed*2+shard*1.5708f;
                pb_draw_mesh(&r->meshes,PB_MESH_WEDGE,(Vector3){e->position.x+cosf(a)*.55f,e->position.y+sinf(a*2)*.2f,e->position.z+sinf(a)*.55f},
                             (Vector3){0,1,0},a*57.3f,(Vector3){.45f,1,.45f},c); }
        }
        if(e->type==PB_ENEMY_WISP) {
            float cycle=fmodf(elapsed+e->phase,2.4f);
            if(cycle>.7f&&cycle<1.7f) {
                float width=(cycle-.7f)*7;
                pb_draw_mesh(&r->meshes,PB_MESH_RING,(Vector3){e->position.x,e->base.y-.45f,e->position.z},
                             (Vector3){1,0,0},90,(Vector3){width,width,width},(Color){232,105,220,180});
            }
        }
    }
#if POLYBLOOM_INCLUDE_LEVEL4
    if(g->boss_active) {
        int shard;
        float cycle=fmodf(g->boss_phase,3.2f);
        pb_draw_mesh(&r->meshes,PB_MESH_SPHERE,g->boss_position,(Vector3){0,1,0},0,
                     (Vector3){3.2f,2.5f,3.2f},(Color){102,56,160,255});
        for(shard=0;shard<8;++shard) { float a=elapsed*1.4f+shard*.7854f;
            Vector3 q={g->boss_position.x+cosf(a)*2.2f,g->boss_position.y+sinf(a*2)*.6f,g->boss_position.z+sinf(a)*2.2f};
            pb_draw_mesh(&r->meshes,PB_MESH_WEDGE,q,(Vector3){0,1,0},a*57.3f,
                         (Vector3){.65f,1.5f,.65f},(Color){245,112,192,255}); }
        if(cycle>.8f&&cycle<1.25f) {
            float radius=(cycle-.8f)*20;
            pb_draw_mesh(&r->meshes,PB_MESH_RING,(Vector3){g->boss_position.x,8.05f,g->boss_position.z},
                         (Vector3){1,0,0},90,(Vector3){radius,radius,radius},(Color){255,106,182,210});
        }
    }
#endif
}

uint32_t pb_gameplay_result_ms(const PbGameplay *g) { return (uint32_t)((g->run_time+g->penalty_time)*1000); }
bool pb_gameplay_flow_run(const PbGameplay *g) { return g->falls==0 && g->damage_events==0; }
