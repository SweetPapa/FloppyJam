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
    float keyboard_x = (float)IsKeyDown(KEY_D) - (float)IsKeyDown(KEY_A);
    float keyboard_y = (float)IsKeyDown(KEY_W) - (float)IsKeyDown(KEY_S);
    bool pad = IsGamepadAvailable(0);
    input.move_x = keyboard_x;
    input.move_y = keyboard_y;
    input.camera_x = (float)IsKeyDown(KEY_RIGHT) - (float)IsKeyDown(KEY_LEFT);
    input.camera_y = (float)IsKeyDown(KEY_UP) - (float)IsKeyDown(KEY_DOWN);
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
        if (fabsf(x) > 0.15f || fabsf(y) > 0.15f) {
            input.move_x = x;
            input.move_y = y;
            input.controller_active = true;
        }
        input.camera_x += GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
        input.camera_y -= GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y);
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
