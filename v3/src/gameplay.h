#ifndef POLYBLOOM_GAMEPLAY_H
#define POLYBLOOM_GAMEPLAY_H

#include "player.h"
#include "render.h"
#include "levels.h"

#define PB_MAX_OBJECTS 256
#define PB_MAX_ENEMIES 32

typedef enum PbObjectType {
    PB_OBJECT_BRITTLE,
    PB_OBJECT_BURST_TARGET,
    PB_OBJECT_ROTATING_BAR,
    PB_OBJECT_HEART
} PbObjectType;

typedef enum PbEnemyType {
    PB_ENEMY_POGO,
    PB_ENEMY_ROLLFIN,
    PB_ENEMY_WISP
} PbEnemyType;

typedef struct PbObject {
    Vector3 position;
    Vector3 base;
    float phase;
    uint8_t type;
    bool active;
} PbObject;

typedef struct PbEnemy {
    Vector3 position;
    Vector3 base;
    float phase;
    uint8_t type;
    bool active;
    bool vulnerable;
} PbEnemy;

typedef struct PbGameplay {
    PbObject objects[PB_MAX_OBJECTS];
    PbEnemy enemies[PB_MAX_ENEMIES];
    int object_count;
    int enemy_count;
    int health;
    int falls;
    int damage_events;
    float invulnerable;
    float run_time;
    float penalty_time;
    bool result_recorded;
    Vector3 effect_position;
    uint8_t effect_type;
    Vector3 boss_position;
    int boss_health;
    float boss_hit_cooldown;
    float boss_phase;
    bool boss_active;
} PbGameplay;

enum { PB_EFFECT_NONE, PB_EFFECT_BREAK, PB_EFFECT_DEFEAT, PB_EFFECT_DAMAGE };

void pb_gameplay_init(PbGameplay *gameplay, int level_id);
void pb_gameplay_update(PbGameplay *gameplay, PbPlayer *player, Vector3 respawn,
                        float dt, float elapsed);
void pb_gameplay_fall(PbGameplay *gameplay, PbPlayer *player, Vector3 respawn);
void pb_gameplay_chase_hit(PbGameplay *gameplay, PbPlayer *player, Vector3 respawn);
void pb_gameplay_draw(const PbGameplay *gameplay, PbRenderer *renderer, float elapsed);
uint32_t pb_gameplay_result_ms(const PbGameplay *gameplay);
bool pb_gameplay_flow_run(const PbGameplay *gameplay);

#endif
