#include "Input.h"

// Static member definitions
// No GLFW state in Win32 backend path

std::unordered_map<int, bool> Input::s_Keys;
std::unordered_map<int, bool> Input::s_MouseButtons;
std::unordered_map<int, bool> Input::s_KeyDownEdge;
 
double Input::s_LastMouseX = 0.0;
double Input::s_LastMouseY = 0.0;

float Input::s_ScrollDelta = 0.0f;
float Input::s_MouseDeltaX = 0.0f;
float Input::s_MouseDeltaY = 0.0f;
bool  Input::s_RelativeMode = false;
float Input::s_LockedCenterX = 0.0f;
float Input::s_LockedCenterY = 0.0f;

void Input::Init() {
   // Nothing to hook; Win32 window forwards events directly
}

void Input::Update() {
   // Clear edge presses at frame start
   s_KeyDownEdge.clear();

   s_ScrollDelta = 0.0f; // Reset scroll after each frame
   s_MouseDeltaX = 0.0f;
   s_MouseDeltaY = 0.0f;
   }

bool Input::IsKeyPressed(int key) {
   return s_Keys[key];
}

bool Input::WasKeyPressedThisFrame(int key) {
   return s_KeyDownEdge[key];
}

bool Input::IsMouseButtonPressed(int button) {
   return s_MouseButtons[button];
   }

std::pair<float, float> Input::GetMouseDelta() {
   return { s_MouseDeltaX, s_MouseDeltaY };
   }
