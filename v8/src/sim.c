#include "sim.h"
#include "spawn.h"
#include <string.h>
static void ev(PwSim*s,int t,int v,float x,float y,float z){if(s->event_count<16)s->ev[s->event_count++]=(PwEvent){t,v,x,y,z};}
float pw_gate_gap_y(const PwGate*g,float t){
    if(g->type==PW_ROLLING)return g->gap_y+sinf(t*2.1f+g->phase)*3.0f;
    if(g->type==PW_SCISSORS)return g->gap_y+sinf(t*1.65f+g->phase)*4.2f;
    return g->gap_y;
}
int pw_gate_safe(const PwGate*g,float y,float x,float t){
    if(g->destroyed)return 1;
    float gy=pw_gate_gap_y(g,t), h=g->gap_h*.5f;
    if(g->type==PW_BARRICADE && fabsf(x)<3.0f && fabsf(y-gy)<h)return 0;
    if(g->type==PW_CANYON)h*=.78f;
    if(g->type==PW_SECRET)return 1;
    return fabsf(y-gy)<h-.55f;
}
void pw_sim_init(PwSim*s,unsigned seed,int mode,int stage,int tier){
    memset(s,0,sizeof *s);s->seed=seed;s->mode=mode;s->stage=stage;s->tier=tier;
    s->alive=1;s->hull=3;s->speed=22.0f+stage*1.65f+tier*1.5f;s->combo=1;
    if(mode)s->speed=26;s->y=0;s->py=0;pw_spawn_course(s);
}
static void crash(PwSim*s){if(!s->alive)return;s->alive=0;s->dying=1;s->death_t=0;ev(s,PW_EV_CRASH,0,s->x,s->y,0);}
void pw_sim_step(PwSim*s,PwInput in){
    s->event_count=0;s->tick++;
    if(!s->alive){s->death_t+=PW_DT;return;}
    s->py=s->y;s->px=s->x;
    if(in.pulse){s->vy=PW_PULSE;ev(s,PW_EV_PULSE,0,s->x,s->y,0);}
    float gravity=PW_GRAVITY;
    for(int i=s->next_gate;i<s->gate_count&&i<s->next_gate+2;i++){
        PwGate*g=&s->gate[i];float dz=g->z-s->distance;
        if(g->type==PW_TURBINE&&fabsf(dz)<15)gravity*=1.0f+sinf(g->phase)*.4f;
    }
    s->vy-=gravity*PW_DT;if(s->vy<-15)s->vy=-15;s->y+=s->vy*PW_DT;
    float target=pw_clamp((float)in.drift,-1,1)*6.0f;s->x+=pw_clamp(target-s->x,-8*PW_DT,8*PW_DT);
    if(in.roll&&s->roll_cd<=0){s->roll_t=.45f;s->roll_cd=1.5f;}
    if(s->roll_t>0)s->roll_t-=PW_DT;if(s->roll_cd>0)s->roll_cd-=PW_DT;
    s->distance+=s->speed*PW_DT;
    if(s->mode)s->speed=pw_clamp(26.0f+s->distance/400.0f,26,40);
    s->warning=(fabsf(s->y)>13.2f)?pw_clamp((fabsf(s->y)-13.2f)/1.8f,0,1):0;
    if(fabsf(s->y) > 15.0f)crash(s);
    if(s->fire_cd>0)s->fire_cd-=PW_DT;
    if(in.fire&&s->fire_cd<=0){s->fire_cd=.105f;ev(s,PW_EV_SHOT,0,s->x,s->y,0);
        for(int i=s->next_gate;i<s->gate_count&&i<s->next_gate+3;i++){PwGate*g=&s->gate[i];float dz=g->z-s->distance;
            if(dz>0&&dz<95&&fabsf(g->gap_y-s->y)<5){
                if(g->enemy){g->enemy=0;s->kills++;if(s->combo<8)s->combo++;s->score+=175*s->combo;ev(s,PW_EV_KILL,1,g->lateral,g->gap_y,dz);}
                else if(g->type==PW_BARRICADE&&--g->hp<=0){g->destroyed=1;s->kills++;s->score+=50*s->combo;ev(s,PW_EV_KILL,0,g->lateral,g->gap_y,dz);}break;}
        }
    }
    /* Turrets telegraph at 45m, then send one slow, readable bolt. */
    for(int i=s->next_gate;i<s->gate_count&&i<s->next_gate+3;i++){PwGate*g=&s->gate[i];float dz=g->z-s->distance;
        if(g->enemy==1&&dz>32&&dz<45&&s->shot_count<PW_MAX_SHOTS){g->enemy=2;s->shot[s->shot_count++]=(PwShot){1,g->lateral,g->gap_y,dz,0,0,-34,2.0f};ev(s,PW_EV_WARN,0,g->lateral,g->gap_y,dz);}
    }
    for(int i=0;i<s->shot_count;i++){PwShot*q=&s->shot[i];if(q->life<=0)continue;q->z+=q->vz*PW_DT;q->life-=PW_DT;
        if(q->z<=1.0f){q->life=0;if(s->roll_t>0){s->deflects++;s->score+=300*s->combo;if(s->combo<8)s->combo++;ev(s,PW_EV_DEFLECT,s->combo,q->x,q->y,0);}
            else if(fabsf(q->y-s->y)<1.4f&&fabsf(q->x-s->x)<2.2f){s->hull--;s->combo=1;ev(s,PW_EV_HIT,s->hull,s->x,s->y,0);if(s->mode||s->hull<=0)crash(s);}
        }
    }
    while(s->next_gate<s->gate_count&&s->distance>=s->gate[s->next_gate].z){
        PwGate*g=&s->gate[s->next_gate];float tm=s->tick*PW_DT;
        if(!pw_gate_safe(g,s->y,s->x,tm)){crash(s);break;}
        g->passed=1;s->gates++;s->score+=100*s->combo;
        if(g->type==PW_SECRET&&fabsf(s->x-g->lateral)<2.3f){s->secrets++;s->score+=500*s->combo;if(s->combo<8)s->combo++;ev(s,PW_EV_SECRET,s->secrets,s->x,s->y,0);}
        float d=fabsf(s->y-pw_gate_gap_y(g,tm));
        if(d<.85f){s->perfects++;if(s->combo<8)s->combo++;s->score+=150*s->combo;ev(s,PW_EV_PERFECT,s->combo,s->x,s->y,0);}
        else ev(s,PW_EV_GATE,s->combo,s->x,s->y,0);
        if(s->combo>s->best_combo)s->best_combo=s->combo;s->next_gate++;
    }
    if(!s->mode&&s->distance>=s->stage_length){s->won=1;s->alive=0;ev(s,PW_EV_STAGE,s->stage,0,s->y,0);}
}
