#ifndef SFBS_CONTROL_H
#define SFBS_CONTROL_H
#include "sim.h"
typedef enum { CTRL_TITLE,CTRL_MODE_SELECT,CTRL_PLAYING,CTRL_PAUSED,CTRL_RESULTS } ControlState;
typedef struct { ControlState state;Game game;char seed[7];GameMode mode;uint64_t sample;unsigned transitions,mode_mask; } ControlHarness;
typedef struct { int checks,failures,modes_tested,transitions_tested;uint64_t digest;const char*first_failure; } ControlStatus;
void sfbs_control_init(ControlHarness*,const char*);void sfbs_control_mode(ControlHarness*,GameMode);void sfbs_control_start(ControlHarness*);void sfbs_control_pause(ControlHarness*);void sfbs_control_resume(ControlHarness*);void sfbs_control_step(ControlHarness*,V2,bool,uint32_t);void sfbs_control_restart(ControlHarness*);void sfbs_control_new_seed(ControlHarness*,uint64_t);void sfbs_control_title(ControlHarness*);
ControlStatus sfbs_control_verify(void);void sfbs_control_json(const ControlStatus*);
#endif
