#pragma once
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <utility>

class Input {
public:
   static void Init(GLFWwindow* window);
   static void Update();

   static bool IsKeyPressed(int key);
   static bool WasKeyPressedThisFrame(int key);
   static bool IsMouseButtonPressed(int button);
   static std::pair<float, float> GetMouseDelta();

   static void OnKey(int key, int action) {
      if (action == GLFW_PRESS) s_Keys[key] = true;
      else if (action == GLFW_RELEASE) s_Keys[key] = false;
      }

   static void OnMouseButton(int button, int action) {
      if (action == GLFW_PRESS) s_MouseButtons[button] = true;
      else if (action == GLFW_RELEASE) s_MouseButtons[button] = false;
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
   static GLFWwindow* s_Window;

   // Previous GLFW callbacks so we can forward events (to ImGui, etc.)
   static GLFWkeyfun          s_PrevKeyCallback;
   static GLFWmousebuttonfun  s_PrevMouseButtonCallback;
   static GLFWcursorposfun    s_PrevCursorPosCallback;
   static GLFWscrollfun       s_PrevScrollCallback;

   static std::unordered_map<int, bool> s_Keys;
   static std::unordered_map<int, bool> s_KeyDownEdge;
   static std::unordered_map<int, bool> s_MouseButtons;

   static double s_LastMouseX;
   static double s_LastMouseY;

   static float s_ScrollDelta;
   static float s_MouseDeltaX;
   static float s_MouseDeltaY;
   };
