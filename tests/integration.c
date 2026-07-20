#include "sim.h"
#include "save.h"
#include "i18n.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"integration failure line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 Game g;sfbs_game_init(&g,"TRACK7",MODE_BLOOM);CHECK(sfbs_song_valid(&g.song));
 int seen[SFBS_PHRASES]={0};uint64_t end=sfbs_tick_sample((uint64_t)SFBS_BARS*4*SFBS_PPQN,g.genome.bpm);
 for(uint64_t sample=0;sample<=end+SFBS_RATE;sample+=SFBS_RATE/16){
  bool pulse=((sample/(SFBS_RATE/2))&3)==0;
  sfbs_game_step(&g,1.0f/60.0f,(V2){.65f,.35f},pulse,sample);
  if(g.phrase_index>=0&&g.phrase_index<SFBS_PHRASES)seen[g.phrase_index]=1;
 }
 CHECK(g.ended&&g.won);CHECK(g.total>80);CHECK(g.node_count<=SFBS_MAX_NODES);
 for(int i=0;i<SFBS_PHRASES;i++)CHECK(seen[i]);

 const char *path="/tmp/sfbs-integration-save.bin";SaveData out,in;
 sfbs_save_defaults(&out);memcpy(out.seed,"TRACK7",7);out.mode=MODE_PULSE;out.volume=37;out.fullscreen=1;out.tutorial=1;out.language=LANG_FR;
 CHECK(sfbs_save_write(path,&out));CHECK(sfbs_save_load(path,&in));CHECK(sfbs_save_valid(&in));
 CHECK(!memcmp(out.seed,in.seed,7)&&in.mode==MODE_PULSE&&in.volume==37&&in.fullscreen&&in.tutorial&&in.language==LANG_FR);
 CHECK(remove(path)==0);
 printf("integration passed: phrases=%d total_weight=%d recovery=%d%%\n",SFBS_PHRASES,g.total,sfbs_recovery(&g));return 0;
}
