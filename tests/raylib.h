#ifndef TEST_RAYLIB_H
#define TEST_RAYLIB_H
#include <stdbool.h>
typedef struct Vector2{float x,y;}Vector2;typedef struct Rectangle{float x,y,width,height;}Rectangle;typedef struct Color{unsigned char r,g,b,a;}Color;typedef struct Texture2D{unsigned int id;int width,height,mipmaps,format;}Texture2D;typedef struct RenderTexture2D{unsigned int id;Texture2D texture,depth;}RenderTexture2D;typedef struct AudioStream{void*buffer;void*processor;unsigned int sampleRate,sampleSize,channels;}AudioStream;
#define BLACK (Color){0,0,0,255}
#define WHITE (Color){255,255,255,255}
#define FLAG_WINDOW_RESIZABLE 4
#define FLAG_VSYNC_HINT 64
#define TEXTURE_FILTER_POINT 0
#define KEY_ENTER 257
#define KEY_SPACE 32
#define KEY_ESCAPE 256
#define KEY_BACKSPACE 259
#define KEY_RIGHT 262
#define KEY_LEFT 263
#define KEY_DOWN 264
#define KEY_UP 265
#define KEY_A 65
#define KEY_C 67
#define KEY_D 68
#define KEY_M 77
#define KEY_N 78
#define KEY_P 80
#define KEY_R 82
#define KEY_S 83
#define KEY_W 87
#define KEY_Z 90
#define KEY_ONE 49
#define KEY_F1 290
#define KEY_F2 291
#define KEY_F3 292
#define KEY_F4 293
#define KEY_F5 294
#define KEY_F6 295
#define KEY_F7 296
#define KEY_F8 297
#define KEY_F11 300
#define KEY_EQUAL 61
#define KEY_MINUS 45
#define KEY_Q 81
#define GAMEPAD_BUTTON_RIGHT_FACE_DOWN 7
#define GAMEPAD_BUTTON_LEFT_FACE_LEFT 13
#define GAMEPAD_BUTTON_LEFT_FACE_RIGHT 14
#define GAMEPAD_BUTTON_LEFT_FACE_UP 11
#define GAMEPAD_BUTTON_LEFT_FACE_DOWN 12
#define GAMEPAD_AXIS_LEFT_X 0
#define GAMEPAD_AXIS_LEFT_Y 1
#define GAMEPAD_AXIS_RIGHT_TRIGGER 5
void InitWindow(int,int,const char*);void CloseWindow(void);bool WindowShouldClose(void);void SetConfigFlags(unsigned int);void SetTargetFPS(int);int GetScreenWidth(void);int GetScreenHeight(void);float GetFrameTime(void);double GetTime(void);bool IsKeyPressed(int);bool IsKeyDown(int);int GetCharPressed(void);bool IsGamepadAvailable(int);bool IsGamepadButtonPressed(int,int);float GetGamepadAxisMovement(int,int);void ToggleFullscreen(void);bool IsWindowFullscreen(void);void SetMasterVolume(float);
RenderTexture2D LoadRenderTexture(int,int);void UnloadRenderTexture(RenderTexture2D);void SetTextureFilter(Texture2D,int);void BeginTextureMode(RenderTexture2D);void EndTextureMode(void);void BeginDrawing(void);void EndDrawing(void);void ClearBackground(Color);void DrawTexturePro(Texture2D,Rectangle,Rectangle,Vector2,float,Color);void DrawText(const char*,int,int,int,Color);int MeasureText(const char*,int);void DrawRectangle(int,int,int,int,Color);void DrawCircleLines(int,int,float,Color);void DrawLineEx(Vector2,Vector2,float,Color);void DrawCircleV(Vector2,float,Color);void DrawPoly(Vector2,int,float,float,Color);void DrawLine(int,int,int,int,Color);void DrawRing(Vector2,float,float,float,float,int,Color);Color ColorAlpha(Color,float);
void InitAudioDevice(void);void CloseAudioDevice(void);void SetAudioStreamBufferSizeDefault(int);AudioStream LoadAudioStream(unsigned int,unsigned int,unsigned int);void UnloadAudioStream(AudioStream);void SetAudioStreamCallback(AudioStream,void(*)(void*,unsigned int));void PlayAudioStream(AudioStream);void StopAudioStream(AudioStream);void PauseAudioStream(AudioStream);void ResumeAudioStream(AudioStream);
#endif
