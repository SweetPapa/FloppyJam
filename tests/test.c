#include "core.h"
#include "music.h"
#include "sim.h"
#include "save.h"
#include "i18n.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define T(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);fails++;}}while(0)
int main(void){int fails=0;char s[7];sfbs_seed_normalize("o1-abcz9",s);T(strlen(s)==6);for(int i=0;i<6;i++)T(strchr("23456789ABCDEFGHJKMNPQRSTUVWXYZ",s[i])!=0);for(uint64_t i=0;i<10000;i++){sfbs_random_seed(i,s);T(strlen(s)==6);for(int j=0;j<6;j++)T(strchr("23456789ABCDEFGHJKMNPQRSTUVWXYZ",s[j])!=0);}Genome a=sfbs_genome("ABC234"),b=sfbs_genome("ABC234");T(!memcmp(&a,&b,sizeof a));T(a.music_seed!=a.layout_seed&&a.layout_seed!=a.visual_seed);for(int n=8;n<=16;n+=4)for(int k=2;k<n;k++){Pattern p=sfbs_euclid(k,n,3);T(sfbs_popcount(p.bits)==k);T(p.steps==n);}for(int bpm=84;bpm<=112;bpm+=4)for(uint64_t t=0;t<10000;t+=97){uint64_t sm=sfbs_tick_sample(t,bpm);Transport q=sfbs_transport(sm,bpm);T(q.bar<1000);}
 Phrase ph[SFBS_PHRASES];sfbs_phrases(&a,MODE_FLOW,ph);T(ph[0].start_bar==0&&ph[13].start_bar==52);Song song;sfbs_song_generate(&a,ph,&song);T(sfbs_song_valid(&song));Game g;sfbs_game_init(&g,"ABC234",MODE_FLOW);T(g.integrity==3&&g.total>0);int full_total=g.total;sfbs_game_step(&g,1.0f/60,(V2){1,0},true,0);T(g.player.vel.x>0&&g.total==full_total);PathBuffer pb={0};for(int i=0;i<SFBS_PATH+20;i++)sfbs_path_push(&pb,(V2){i,0},i);V2 got;T(pb.count==SFBS_PATH&&sfbs_path_at(&pb,SFBS_PATH+19,&got)&&got.x==SFBS_PATH+19);T(fabsf(sfbs_segment_distance((V2){5,3},(V2){0,0},(V2){10,0})-3)<.001f);T(sfbs_recovery(&g)>=0&&sfbs_recovery(&g)<=100);for(int l=0;l<LANG_COUNT;l++){T(strlen(sfbs_language_name((Language)l))>0);for(int id=0;id<TXT_COUNT;id++)T(strlen(sfbs_text((Language)l,(TextId)id))>0);}SaveData d;sfbs_save_defaults(&d);T(sfbs_save_valid(&d)&&d.first_person==0);d.language=LANG_DE;d.checksum=0;T(!sfbs_save_valid(&d));sfbs_save_defaults(&d);d.first_person=2;T(!sfbs_save_valid(&d));sfbs_save_defaults(&d);d.volume=101;T(!sfbs_save_valid(&d));printf("%s (%d music events)\n",fails?"TESTS FAILED":"all core tests passed",song.count);return fails?1:0;}
