#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------
// Native <-> Managed Input Interop exports
// ----------------------------------------
// Returns 1 if the given key is currently held, 0 otherwise.
__declspec(dllexport) int IsKeyHeld(int key);
// Returns 1 only on the frame the key transitioned to pressed.
__declspec(dllexport) int IsKeyDown(int key);
// Returns 1 if the given mouse button is currently pressed, 0 otherwise.
__declspec(dllexport) int IsMouseDown(int button);
// Returns current mouse delta since last frame.
__declspec(dllexport) void GetMouseDelta(float* deltaX, float* deltaY);
// Writes a message to the engine logger.
__declspec(dllexport) void DebugLog(const char* msg);

#ifdef __cplusplus
}
#endif

// --------------------------------------------------------------------------------------
// Function pointer typedefs & extern declarations
// --------------------------------------------------------------------------------------
using IsKeyHeld_fn = int(*)(int key);
using IsKeyDown_fn     = int(*)(int key);
using IsMouseDown_fn   = int(*)(int button);
using GetMouseDelta_fn = void(*)(float* dx, float* dy);

extern IsKeyHeld_fn      IsKeyHeldPtr;
extern IsKeyDown_fn      IsKeyDownPtr;
extern IsMouseDown_fn    IsMouseDownPtr;
extern GetMouseDelta_fn  GetMouseDeltaPtr;

// Logger
using DebugLog_fn = void(*)(const char* msg);
extern DebugLog_fn DebugLogPtr;

