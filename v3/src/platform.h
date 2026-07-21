#ifndef POLYBLOOM_PLATFORM_H
#define POLYBLOOM_PLATFORM_H

#include <stdbool.h>

typedef struct PbInput {
    float move_x;
    float move_y;
    float camera_x;
    float camera_y;
    bool jump_pressed;
    bool jump_down;
    bool burst_pressed;
    bool quit_pressed;
    bool pause_pressed;
    bool menu_back_pressed;
    bool restart_down;
    bool camera_recenter;
    bool controller_active;
} PbInput;

void pb_platform_open(void);
void pb_platform_close(void);
bool pb_platform_running(void);
float pb_platform_frame_time(void);
PbInput pb_platform_input(void);
#endif
