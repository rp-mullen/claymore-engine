#pragma once
#include "core/Application.h"
inline JobSystem& Jobs() { return Application::Get().Jobs(); }
