#include "Input.h"

// Static member definitions
GLFWwindow* Input::s_Window = nullptr;

std::unordered_map<int, bool> Input::s_Keys;
std::unordered_map<int, bool> Input::s_MouseButtons;

double Input::s_LastMouseX = 0.0;
double Input::s_LastMouseY = 0.0;

float Input::s_ScrollDelta = 0.0f;
float Input::s_MouseDeltaX = 0.0f;
float Input::s_MouseDeltaY = 0.0f;

void Input::Init(GLFWwindow* window) {
   s_Window = window;
   glfwSetKeyCallback(window, [](GLFWwindow*, int key, int, int action, int) {
      OnKey(key, action);
      });

   glfwSetMouseButtonCallback(window, [](GLFWwindow*, int button, int action, int) {
      OnMouseButton(button, action);
      });

   glfwSetCursorPosCallback(window, [](GLFWwindow*, double xpos, double ypos) {
      OnMouseMove(xpos, ypos);
      });

   glfwSetScrollCallback(window, [](GLFWwindow*, double, double yoffset) {
      OnScroll(yoffset);
      });
   }

void Input::Update() {
   s_ScrollDelta = 0.0f; // Reset scroll after each frame
   s_MouseDeltaX = 0.0f;
   s_MouseDeltaY = 0.0f;
   }

bool Input::IsKeyPressed(int key) {
   return s_Keys[key];
   }

bool Input::IsMouseButtonPressed(int button) {
   return s_MouseButtons[button];
   }

std::pair<float, float> Input::GetMouseDelta() {
   return { s_MouseDeltaX, s_MouseDeltaY };
   }
