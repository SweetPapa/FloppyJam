#include "audio.h"
#include <stdio.h>
int main(void){Genome g=sfbs_genome("AUDIO7");Phrase phrases[SFBS_PHRASES];Song song;sfbs_phrases(&g,MODE_FLOW,phrases);sfbs_song_generate(&g,phrases,&song);float peak=0;uint64_t frames=(uint64_t)SFBS_RATE*30;if(!sfbs_audio_offline_check(&song,frames,&peak)){fprintf(stderr,"offline audio validation failed: peak=%f\n",peak);return 1;}printf("offline audio passed: frames=%llu peak=%f events=%d\n",(unsigned long long)frames,peak,song.count);return 0;}
