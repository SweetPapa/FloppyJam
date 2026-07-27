#ifndef PW_APP_H
#define PW_APP_H
#include "raylib.h"
#include "sim.h"
#include "ghost.h"
enum { PW_TITLE, PW_MODE, PW_PLAY, PW_MAP, PW_SETTINGS };
typedef struct { Vector3 p,v; Color c; float life,size; } PwParticle;
typedef struct {
    int running,state,selection,mode,stage,tier,graybox,reduce_motion,autofire;
    int medals[9], unlocked, deaths;
    float acc,time,shake,flash,title_idle,callout_t,laser_t;
    char callout[48];
    PwSim sim,ghost_sim;
    PwGhost run,best;
    PwParticle particles[512]; int particle_at;
    Camera3D camera;
    Sound pulse,gate,perfect,crash,laser,deflect,warning;
    int audio_ready;
} PwApp;
void app_init(PwApp*a,int graybox);
void app_frame(PwApp*a);
void app_shutdown(PwApp*a);
#endif
