/* Reproducible campaign playtest: only uses the public color-input API.
 * No teleporting, immunity overrides, or hazard removal. */
#include "maglava.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#define DT (1.0f/60)

static int choose(const GameSim *g,int careful,int nearest) {
    int best=-1;float value=-1e9f;
    for(int c=0;c<4;c++) {
        int idx=sim_find_magnet(g,g->px,g->py,(MagColor)c,g->attached_idx);
        if(idx<0 || g->lv->mag[idx].y>=g->py-20)continue;
        float gain=g->py-g->lv->mag[idx].y;
        if(careful) {
            GameSim copy=*g;
            sim_update(&copy,DT,c);
            for(int f=0;f<180&&copy.state==PS_SWINGING&&!copy.game_over;f++)sim_update(&copy,DT,-1);
            if(copy.deaths>g->deaths || (!copy.game_over && copy.state!=PS_ATTACHED))continue;
        }
        float rank=nearest?-gain:gain;
        if(rank>value){value=rank;best=c;}
    }
    return best;
}
int main(int argc,char **argv) {
    int strict=0,relaxed=0,deathless=0,fails=0;
    const char *level_key=NULL;
    float lava_rate=1;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--lava-rate") && i+1<argc)lava_rate=sim_valid_lava_rate(strtof(argv[++i],NULL));
        else if(!strcmp(argv[i],"--strict"))strict=1;
        if(!strcmp(argv[i],"--deathless"))deathless=1;
        if(!strcmp(argv[i],"--new-relaxed"))relaxed=1;
        if(!strcmp(argv[i],"--level-key")&&i+1<argc)level_key=argv[++i];
    }
    const int delays[]={0,15,30,45,15,60,90,120,90};
    const char *names[]={"instant","250ms","500ms","750ms","hazard-aware",
                         "1000ms","1500ms","2000ms","1500ms-nearest"};
    puts("level,key,profile,won,seconds,deaths,score");
    for(int mode=relaxed?5:0;mode<(relaxed?9:5);mode++) {
        int wins=0,clean=0,total=0,retries=0;
        for(int id=1;id<=LEVEL_COUNT;id++) {
            if(level_key) { if(strcmp(LEVELS[id-1].key,level_key))continue; }
            else if(relaxed&&LEVELS[id-1].legacy_id)continue;
            total++;
            GameSim g;sim_init(&g,id);sim_set_lava_rate(&g,lava_rate);int wait=0;
            for(int f=0;f<60*180&&!g.won&&g.deaths<12;f++) {
                int input=-1;
                if(g.state==PS_ATTACHED) {
                    if(wait++ >= delays[mode]) input=choose(&g,mode==4,mode==8);
                } else wait=0;
                sim_update(&g,DT,input);
                if(!isfinite(g.px)||!isfinite(g.py)||!isfinite(g.vx)||!isfinite(g.vy))return 2;
            }
            wins+=g.won;clean+=g.won&&g.deaths==0;retries+=g.deaths;
            if((relaxed&&g.deaths>2)||(deathless&&g.deaths))fails++;
            printf("%d,%s,%s,%d,%.3f,%d,%d\n",id,g.lv->key,
                names[mode],
                g.won,g.elapsed,g.deaths,g.score);
        }
        fprintf(stderr,"%s: %d/%d completed; %d without deaths; %d retries\n",
            names[mode],wins,total,clean,retries);
        if(!total||wins!=total)fails++;
    }
    return strict&&fails?1:0;
}
