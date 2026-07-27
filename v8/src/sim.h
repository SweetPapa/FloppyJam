#ifndef PW_SIM_H
#define PW_SIM_H
#include "core.h"
enum { PW_ARCH, PW_RING, PW_SHARD, PW_ROLLING, PW_BARRICADE, PW_TURBINE, PW_CANYON, PW_SCISSORS, PW_CLOUD, PW_SECRET };
enum { PW_EV_PULSE=1, PW_EV_GATE, PW_EV_PERFECT, PW_EV_CRASH, PW_EV_SHOT, PW_EV_KILL, PW_EV_DEFLECT, PW_EV_WARN, PW_EV_STAGE, PW_EV_HIT, PW_EV_SECRET };
typedef struct { unsigned pulse:1, fire:1, roll:1; int drift; } PwInput;
typedef struct {
    int type, passed, destroyed, hp, enemy;
    float z, gap_y, gap_h, lateral, phase;
} PwGate;
typedef struct { int type; float x,y,z,vx,vy,vz,life; } PwShot;
typedef struct { int type, value; float x,y,z; } PwEvent;
typedef struct {
    uint64_t tick;
    uint32_t seed;
    int mode, stage, tier, alive, dying, won, hull;
    float y, py, vy, x, px, speed, distance, warning, roll_t, roll_cd;
    int score, gates, perfects, kills, combo, best_combo, next_gate, secrets, deflects;
    PwGate gate[PW_MAX_GATES]; int gate_count;
    PwShot shot[PW_MAX_SHOTS]; int shot_count;
    PwEvent ev[16]; int event_count;
    float fire_cd, death_t, stage_length;
} PwSim;
void pw_sim_init(PwSim *s, unsigned seed, int mode, int stage, int tier);
void pw_sim_step(PwSim *s, PwInput in);
float pw_gate_gap_y(const PwGate *g, float time);
int pw_gate_safe(const PwGate *g, float y, float x, float time);
#endif
