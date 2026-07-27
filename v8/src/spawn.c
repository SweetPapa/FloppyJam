#include "spawn.h"
#include <string.h>
void pw_spawn_course(PwSim *s){
    PwRng r={s->seed ^ (uint32_t)(s->stage*7919+s->tier*101)};
    s->gate_count=0;
    float z=82.0f;
    int count=s->mode?PW_MAX_GATES:18+s->stage*2;
    if(count>PW_MAX_GATES)count=PW_MAX_GATES;
    int unlocked=s->mode?10:(s->stage+2);
    if(unlocked>10)unlocked=10;
    for(int i=0;i<count;i++){
        PwGate *g=&s->gate[s->gate_count++];
        memset(g,0,sizeof *g);
        g->type=(i<2)?PW_ARCH:(int)(pw_rand(&r)%(unsigned)unlocked);
        g->z=z;
        g->gap_h=5.7f-s->tier*.35f-pw_unit(&r)*.7f;
        if(g->gap_h<4.1f)g->gap_h=4.1f;
        g->gap_y=-7.0f+pw_unit(&r)*14.0f;
        g->lateral=(pw_unit(&r)-.5f)*9.0f;
        g->phase=pw_unit(&r)*6.283185f;
        g->hp=3+(s->stage/3);
        g->enemy=(i%4)==2;
        z+=72.0f+pw_unit(&r)*25.0f;
    }
    s->stage_length=z+55.0f;
}
float pw_min_reaction(const PwSim *s){
    float m=999;
    for(int i=1;i<s->gate_count;i++){
        float t=(s->gate[i].z-s->gate[i-1].z)/40.0f;
        if(t<m)m=t;
    }
    return m;
}
