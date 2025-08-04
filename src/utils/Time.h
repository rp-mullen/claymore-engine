// Time.h
#pragma once
#include <chrono>

class Time {
public:
   static void Init();
   static void Tick(); // call this once per frame

   static float GetDeltaTime(); // seconds
   static float GetTotalTime(); // seconds

private:
   static std::chrono::steady_clock::time_point s_StartTime;
   static std::chrono::steady_clock::time_point s_LastFrameTime;
   static float s_DeltaTime;
   };
