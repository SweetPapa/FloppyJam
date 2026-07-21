#ifndef SFBS_MUSIC_H
#define SFBS_MUSIC_H
#include "core.h"
typedef struct { MusicEvent events[SFBS_EVENTS]; int count; } Song;
void sfbs_song_generate(const Genome*,const Phrase[SFBS_PHRASES],Song*);
bool sfbs_song_valid(const Song*);
#endif
