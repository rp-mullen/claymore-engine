#include "InputInterop.h"
#include "editor/Input.h"
#include "ui/Logger.h"
#include <core/application.h>
#include <imgui.h>
#include <imgui_internal.h>

// --------------------------------------------------------------------------------------
// Function pointer typedefs matching managed delegate signatures.
// --------------------------------------------------------------------------------------
using IsKeyHeld_fn      = int(*)(int key);
using IsKeyDown_fn      = int(*)(int key);
using IsMouseDown_fn    = int(*)(int button);
using GetMouseDelta_fn  = void(*)(float* dx, float* dy);
using DebugLog_fn       = void(*)(const char* msg);
using SetMouseMode_fn   = void(*)(int mode);

// --------------------------------------------------------------------------------------
// Pointer instances passed to managed side (set up in DotNetHost.cpp)
// --------------------------------------------------------------------------------------
IsKeyHeld_fn     IsKeyHeldPtr     = &IsKeyHeld;
IsKeyDown_fn     IsKeyDownPtr     = &IsKeyDown;
IsMouseDown_fn   IsMouseDownPtr   = &IsMouseDown;
GetMouseDelta_fn GetMouseDeltaPtr = &GetMouseDelta;
DebugLog_fn      DebugLogPtr      = &DebugLog;
SetMouseMode_fn  SetMouseModePtr  = &SetMouseMode;

// --------------------------------------------------------------------------------------
// Exported implementation
// --------------------------------------------------------------------------------------
extern "C" {

__declspec(dllexport) int IsKeyHeld(int key)
{
    return Input::IsKeyPressed(key) ? 1 : 0;
}

__declspec(dllexport) int IsKeyDown(int key)
{
    return Input::WasKeyPressedThisFrame(key) ? 1 : 0;
}

__declspec(dllexport) int IsMouseDown(int button)
{
    return Input::IsMouseButtonPressed(button) ? 1 : 0;
}

__declspec(dllexport) void GetMouseDelta(float* deltaX, float* deltaY)
{
    if (!deltaX || !deltaY)
        return;
    auto d = Input::GetMouseDelta();
    *deltaX = d.first;
    *deltaY = d.second;
}

__declspec(dllexport) void DebugLog(const char* msg)
{
    if(msg)
        Logger::Log(msg); 
}

__declspec(dllexport) void SetMouseMode(int mode)
{
    // 0 = free, 1 = captured/relative
    bool capture = (mode == 1);
    // Toggle platform capture and input relative mode
    Application::Get().SetMouseCaptured(capture);
    // In editor, make sure ImGui doesn't capture/hover when captured
    if (capture) {
        ImGuiIO& io = ImGui::GetIO();
        io.WantCaptureMouse = false;
        io.WantCaptureKeyboard = false;
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        ImGui::ClearActiveID();
    }
}
 
}
 