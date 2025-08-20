#pragma once
#include <unordered_map>
#include <utility>

// Minimal compatibility defines for existing key/button constants
#ifndef GLFW_KEY_DELETE
#define GLFW_KEY_DELETE 261
#endif
#ifndef GLFW_KEY_S
#define GLFW_KEY_S 83
#endif
#ifndef GLFW_MOUSE_BUTTON_LEFT
#define GLFW_MOUSE_BUTTON_LEFT 0
#endif

class Input {
public:
   static void Init();
   static void Update();

   static bool IsKeyPressed(int key);
   static bool WasKeyPressedThisFrame(int key);
   static bool IsMouseButtonPressed(int button);
   static std::pair<float, float> GetMouseDelta();
   static std::pair<float, float> GetMousePosition() { return { static_cast<float>(s_LastMouseX), static_cast<float>(s_LastMouseY) }; }

   static void OnKey(int key, int action) {
      bool isPress = (action != 0);
      if (isPress && !s_Keys[key]) s_KeyDownEdge[key] = true;
      s_Keys[key] = isPress;            // zero = release
      }

   static void OnMouseButton(int button, int action) {
      if (action != 0) s_MouseButtons[button] = true; // non-zero = press
      else s_MouseButtons[button] = false;            // zero = release
      }

   static void OnMouseMove(double xpos, double ypos) {
      s_MouseDeltaX = static_cast<float>(xpos - s_LastMouseX);
      s_MouseDeltaY = static_cast<float>(ypos - s_LastMouseY);
      s_LastMouseX = xpos;
      s_LastMouseY = ypos;
      }

   static void OnScroll(double yoffset) {
      s_ScrollDelta = static_cast<float>(yoffset);
      }

private:

   static std::unordered_map<int, bool> s_Keys;
   static std::unordered_map<int, bool> s_KeyDownEdge;
   static std::unordered_map<int, bool> s_MouseButtons;

   static double s_LastMouseX;
   static double s_LastMouseY;

   static float s_ScrollDelta;
   static float s_MouseDeltaX;
   static float s_MouseDeltaY;
   };
