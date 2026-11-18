// Nintendo 3DS services of the platform layer (libctru): the screens before
// the renderer starts, the log on the bottom screen and the console's
// buttons, which press the keyboard keys the game reads.
#ifndef OPENCMR2_CTR_H
#define OPENCMR2_CTR_H

#include <stdint.h>
#include <stdio.h>

#include <functional>

// Speeds up the CPU on a New 3DS, starts the screens and the bottom-screen log.
bool Ctr_Init(void);
void Ctr_Shutdown(void);
// The CIA's own files (romfs:/) are mounted: a standalone CIA may hold the game data.
bool Ctr_HasRomfs(void);
// X was pressed on a stage since the last call: the gearbox's quick reverse.
bool Ctr_TakeQuickReverse(void);
// The Circle Pad's steering on a stage, -63 (full left) to 63: false when it
// does not steer now (menus, the D-Pad held, [3ds] analog_steering = 0).
bool Ctr_GetAnalogSteering(int *amount);
bool Ctr_HasAnalogSteering(void);
// Logs the console and the memory the game got.
void Ctr_LogSystem(void);
// This port needs a New 3DS in its 124 MB mode: false (after saying why on
// the bottom screen) on an Old 3DS or with less memory. [3ds]
// allow_low_memory = 1 lets it start anyway.
bool Ctr_CheckSystem(void);

// A line on the bottom-screen log.
void Ctr_Print(const char *line);
// Shows a message on the bottom screen until A or START is pressed (or the
// system asks the game to close).
void Ctr_ShowMessage(const char *title, const char *text);

// Reads the buttons and the system state; false when the game should close.
bool Ctr_Update(void);
// Key presses since the last call, as DirectInput scan codes (with repeats
// in menus).
bool Ctr_TakeKeyEvent(int *dik, bool *repeat);
bool Ctr_IsKeyDown(int dik);
// Text typed on the system keyboard (X in a menu), one Windows-1252
// character at a time.
bool Ctr_TakeText(int *character);
// Driving on a stage, or in a menu: the buttons press different keys.
void Ctr_SetDrivingContext(bool driving);
bool Ctr_IsDriving(void);

// The bottom screen's log pauses while the renderer shows the stage map
// and the HUD there (and is cleared when it comes back).
void Ctr_PauseConsole(bool paused);
// The stage map (platform/ctr_map.cpp): whether it is wanted now ([3ds] map
// and driving), its image (320x240 0xAARRGGBB landscape rows, NULL before a
// route; changed: since the last call) and the frame rate it shows.
bool Ctr_MapWanted(void);
const uint32_t *Ctr_MapImage(bool *changed);
void Ctr_MapSetFps(float fps);
// The player's car on the stage, whole world units (false before a stage).
bool Ctr_MapPlayerPosition(int *x, int *z);

// Writing to the SD card on a thread of its own: a write can stall for
// seconds, and the game's thread must not wait for it. The log's lines go
// to the file given to Ctr_StartWriter; Ctr_WriteLater runs a job there
// (a frame log). After Ctr_StopWriter, which writes what is left, both run
// directly.
void Ctr_StartWriter(FILE *log);
void Ctr_StopWriter(void);
void Ctr_LogLine(const char *line);
void Ctr_WriteLater(std::function<void()> job);

// The watchdog: when no frame has been shown for a few seconds, the log
// gets what the game was last doing (the breadcrumb), every few seconds.
void Ctr_StartWatchdog(void);
void Ctr_StopWatchdog(void);
void Ctr_FrameShown(void);
void Ctr_Breadcrumb(const char *format, ...);

// render/gfx_c3d.cpp: 3D draws with the minimal vertex shader (the GPU test).
void Gfx_CtrSetMinimalShader(bool on);

#endif
