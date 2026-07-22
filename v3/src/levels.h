#ifndef POLYBLOOM_LEVELS_H
#define POLYBLOOM_LEVELS_H

#include "collision.h"
#include "render.h"
#include <stdint.h>

#define PB_LEVEL_GLINTS 30
#define PB_LEVEL_SEEDS 3
#define PB_MAX_LEVEL_PIECES 96
#define PB_MAX_CAMERA_ZONES 32

typedef enum PbSectionId {
    PB_SECTION_AWAKENING,
    PB_SECTION_ORCHARD,
    PB_SECTION_RINGWATER,
    PB_SECTION_GROVE,
    PB_SECTION_CLIMB,
    PB_SECTION_RUNOFF
} PbSectionId;

typedef enum PbLevelId {
    PB_LEVEL_GARDEN, PB_LEVEL_CASCADE, PB_LEVEL_FOUNDRY, PB_LEVEL_CROWN
} PbLevelId;

typedef struct PbLevelPiece {
    int16_t px, py, pz;
    int16_t sx, sy, sz;
    uint8_t type;
    uint8_t color;
} PbLevelPiece;

typedef struct PbCameraZone {
    int16_t min_x, max_x, min_z, max_z;
    uint8_t section;
    uint8_t distance;
} PbCameraZone;

typedef struct PbCollectible {
    Vector3 position;
    bool collected;
} PbCollectible;

typedef struct PbLevel {
    PbLevelPiece pieces[PB_MAX_LEVEL_PIECES];
    int piece_count;
    PbCameraZone camera_zones[PB_MAX_CAMERA_ZONES];
    int camera_zone_count;
    PbCollectible glints[PB_LEVEL_GLINTS];
    PbCollectible seeds[PB_LEVEL_SEEDS];
    Vector3 checkpoint;
    Vector3 respawn;
    Vector3 gate;
    int glint_count;
    int seed_count;
    int section;
    bool checkpoint_active;
    bool complete;
    int level_id;
    Vector3 checkpoints[2];
    int checkpoint_count;
    float falling_timer[PB_MAX_LEVEL_PIECES];
    float shatterwake_z;
    bool chase_active;
    bool chase_escaped;
    bool chase_hit;
    float completion_time;
    bool gate_locked;
} PbLevel;

void pb_level_petalgarden_init(PbLevel *level, PbCollisionWorld *collision);
void pb_level_prismrush_init(PbLevel *level, PbCollisionWorld *collision);
void pb_level_foundry_init(PbLevel *level, PbCollisionWorld *collision);
void pb_level_crown_init(PbLevel *level, PbCollisionWorld *collision);
void pb_level_update(PbLevel *level, PbCollisionWorld *collision, Vector3 player, float dt);
void pb_level_draw(PbLevel *level, PbRenderer *renderer, float elapsed, bool reduced);
float pb_level_camera_distance(const PbLevel *level, Vector3 player);
const char *pb_level_section_name(const PbLevel *level);

#endif
