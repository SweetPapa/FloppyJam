#include "sim.h"
#include "spawn.h"
#include "ghost.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static PwInput pilot(int t){PwInput i={0};i.pulse=(t%106)==0;i.fire=1;i.drift=(t/900)%3-1;i.roll=(t%601)==0;return i;}
int main(void){
    PwSim a,b;pw_sim_init(&a,12345,1,0,0);pw_sim_init(&b,12345,1,0,0);
    for(int t=0;t<18000;t++){PwInput i=pilot(t);pw_sim_step(&a,i);pw_sim_step(&b,i);}
    assert(!memcmp(&a,&b,sizeof a));
    PwSim c;pw_sim_init(&c,1,1,0,0);c.y=0;c.vy=-15;c.gate[0].z=.05f;c.gate[0].gap_y=12;c.gate[0].gap_h=4;pw_sim_step(&c,(PwInput){0});assert(!c.alive);
    PwSim r;pw_sim_init(&r,7,1,0,0);r.y=16;pw_sim_step(&r,(PwInput){0});assert(!r.alive);int ticks=0;while(r.death_t<.25f){pw_sim_step(&r,(PwInput){0});ticks++;}assert(ticks<240);
    PwSim s;pw_sim_init(&s,9,1,0,0);assert(pw_min_reaction(&s)>=1.75f);
    PwSim d;pw_sim_init(&d,4,0,0,0);d.shot_count=1;d.shot[0]=(PwShot){1,0,0,.01f,0,0,-34,1};pw_sim_step(&d,(PwInput){.roll=1});assert(d.deflects==1&&d.alive);
    PwSim h;pw_sim_init(&h,5,0,0,0);h.shot_count=1;h.shot[0]=(PwShot){1,0,0,.01f,0,0,-34,1};pw_sim_step(&h,(PwInput){0});assert(h.hull==2&&h.alive);
    puts("sim: determinism, sweep, restart, reaction, hull and deflect laws green");return 0;
}
