#include "Input.h"

// Static member definitions
GLFWwindow* Input::s_Window = nullptr;

GLFWkeyfun         Input::s_PrevKeyCallback         = nullptr;
GLFWmousebuttonfun Input::s_PrevMouseButtonCallback = nullptr;
GLFWcursorposfun   Input::s_PrevCursorPosCallback   = nullptr;
GLFWscrollfun      Input::s_PrevScrollCallback      = nullptr;

std::unordered_map<int, bool> Input::s_Keys;
std::unordered_map<int, bool> Input::s_MouseButtons;
std::unordered_map<int, bool> Input::s_KeyDownEdge;

double Input::s_LastMouseX = 0.0;
double Input::s_LastMouseY = 0.0;

float Input::s_ScrollDelta = 0.0f;
float Input::s_MouseDeltaX = 0.0f;
float Input::s_MouseDeltaY = 0.0f;

void Input::Init(GLFWwindow* window) {
   s_Window = window;

   // Chain existing callbacks (e.g., ImGui) so both systems receive events
   s_PrevKeyCallback         = glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int, int action, int mods) {
       // Detect edge press
       if(action == GLFW_PRESS && !s_Keys[key])
           s_KeyDownEdge[key] = true;

       OnKey(key, action);
       if (s_PrevKeyCallback)
           s_PrevKeyCallback(win, key, 0, action, mods);
   });

   s_PrevMouseButtonCallback = glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
       OnMouseButton(button, action);
       if (s_PrevMouseButtonCallback)
           s_PrevMouseButtonCallback(win, button, action, mods);
   });

   s_PrevCursorPosCallback   = glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
       OnMouseMove(xpos, ypos);
       if (s_PrevCursorPosCallback)
           s_PrevCursorPosCallback(win, xpos, ypos);
   });

   s_PrevScrollCallback      = glfwSetScrollCallback(window, [](GLFWwindow* win, double, double yoffset) {
       OnScroll(yoffset);
       if (s_PrevScrollCallback)
           s_PrevScrollCallback(win, 0.0, yoffset);
   });
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
