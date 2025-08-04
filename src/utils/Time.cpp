// Time.cpp
#include "Time.h"

std::chrono::steady_clock::time_point Time::s_StartTime;
std::chrono::steady_clock::time_point Time::s_LastFrameTime;
float Time::s_DeltaTime = 0.0f;

void Time::Init() {
   s_StartTime = std::chrono::steady_clock::now();
   s_LastFrameTime = s_StartTime;
   s_DeltaTime = 0.0f;
   }

void Time::Tick() {
   auto now = std::chrono::steady_clock::now();
   s_DeltaTime = std::chrono::duration<float>(now - s_LastFrameTime).count();
   s_LastFrameTime = now;
   }

float Time::GetDeltaTime() {
   return s_DeltaTime;
   }

float Time::GetTotalTime() {
   return std::chrono::duration<float>(std::chrono::steady_clock::now() - s_StartTime).count();
   }
