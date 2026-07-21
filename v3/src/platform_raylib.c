#include "platform.h"

#include "raylib.h"
#include <math.h>

void pb_platform_open(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "POLYBLOOM - platform spike");
    SetTargetFPS(60);
}

void pb_platform_close(void)
{
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
}

bool pb_platform_running(void) { return !WindowShouldClose(); }
float pb_platform_frame_time(void) { return GetFrameTime(); }

PbInput pb_platform_input(void)
{
    PbInput input = {0};
    float keyboard_x = (float)(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT)) -
                       (float)(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT));
    float keyboard_y = (float)(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP)) -
                       (float)(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN));
    Vector2 mouse_delta = GetMouseDelta();
    bool pad = IsGamepadAvailable(0);
    input.move_x = keyboard_x;
    input.move_y = keyboard_y;
    input.camera_x = mouse_delta.x;
    input.camera_y = mouse_delta.y;
    input.camera_pointer = fabsf(mouse_delta.x)>.01f||fabsf(mouse_delta.y)>.01f;
    input.jump_pressed = IsKeyPressed(KEY_SPACE);
    input.jump_down = IsKeyDown(KEY_SPACE);
    input.burst_pressed = IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_J);
    input.quit_pressed = IsKeyPressed(KEY_ESCAPE);
    input.pause_pressed = input.quit_pressed;
    input.menu_back_pressed = input.quit_pressed;
    input.restart_down = IsKeyDown(KEY_BACKSPACE);
    input.camera_recenter = IsKeyPressed(KEY_R);
    if (pad) {
        float x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float y = -GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        float magnitude=sqrtf(x*x+y*y);
        if (magnitude > 0.18f) {
            float scaled=fminf(1,(magnitude-.18f)/.82f)/magnitude;
            input.move_x = x*scaled;
            input.move_y = y*scaled;
            input.controller_active = true;
        }
        if (IsGamepadButtonDown(0,GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) input.move_x=1;
        if (IsGamepadButtonDown(0,GAMEPAD_BUTTON_LEFT_FACE_LEFT)) input.move_x=-1;
        if (IsGamepadButtonDown(0,GAMEPAD_BUTTON_LEFT_FACE_UP)) input.move_y=1;
        if (IsGamepadButtonDown(0,GAMEPAD_BUTTON_LEFT_FACE_DOWN)) input.move_y=-1;
        if(!input.camera_pointer) {
            input.camera_x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
            input.camera_y = -GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y);
            if(fabsf(input.camera_x)<.12f) input.camera_x=0;
            if(fabsf(input.camera_y)<.12f) input.camera_y=0;
        }
        input.jump_pressed |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        input.jump_down |= IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        input.burst_pressed |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        input.pause_pressed |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT);
        input.menu_back_pressed |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        input.restart_down |= IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1) &&
                              IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        input.camera_recenter |= IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
    }
    return input;
}
