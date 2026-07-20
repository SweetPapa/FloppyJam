#ifndef SFBS_SIM_H
#define SFBS_SIM_H
#include "core.h"
#include "music.h"
typedef struct { V2 pos,vel; float pulse_cd,invuln,glow; } Player;
typedef struct {
 char seed[7]; Genome genome; Phrase phrases[SFBS_PHRASES]; Song song; GameMode mode;
 Player player; Node nodes[SFBS_MAX_NODES]; int node_count,phrase_index,integrity,recovered,total;
 PathBuffer path; uint64_t last_path_tick,last_beat; float echo_width,echo_delay; bool echo_active,freeze_echo,ended,won;
} Game;
void sfbs_game_init(Game*,const char*,GameMode); void sfbs_game_step(Game*,float,V2,bool,uint64_t);
void sfbs_path_push(PathBuffer*,V2,uint64_t); bool sfbs_path_at(const PathBuffer*,uint64_t,V2*);
float sfbs_segment_distance(V2,V2,V2);
int sfbs_recovery(const Game*); void sfbs_force_damage(Game*);
#endif
