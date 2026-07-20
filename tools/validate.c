#include "core.h"
#include "music.h"
#include "sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char**argv){int count=argc>1?atoi(argv[1]):100000,bad=0;uint64_t digest=1469598103934665603ULL;for(int i=0;i<count;i++){char seed[7];sfbs_random_seed(0x51a7ULL+(uint64_t)i,seed);Genome g=sfbs_genome(seed);Phrase p[SFBS_PHRASES];sfbs_phrases(&g,(GameMode)(i%3),p);Song s;sfbs_song_generate(&g,p,&s);if(!sfbs_song_valid(&s))bad++;for(int j=0;j<SFBS_PHRASES;j++){if(sfbs_popcount(p[j].pattern.bits)!=p[j].pattern.pulses||p[j].pattern.steps>16)bad++;if(j>0&&j<12){float before=p[j-1].pattern.pulses/(float)p[j-1].pattern.steps,after=p[j].pattern.pulses/(float)p[j].pattern.steps;if(after>before*1.351f)bad++;}}digest^=g.master+(uint64_t)s.count;digest*=1099511628211ULL;}printf("validated=%d failures=%d digest=%016llx\n",count,bad,(unsigned long long)digest);return bad?1:0;}
