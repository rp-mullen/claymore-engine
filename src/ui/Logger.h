// Logger.h
#pragma once
#include <string>
#include <functional>
#include "panels/ConsolePanel.h"

class Logger {
public:
    static void Log(const std::string& message);
    static void LogWarning(const std::string& message);
    static void LogError(const std::string& message);

    static void SetCallback(std::function<void(const std::string&, LogLevel)> cb) { s_Callback = cb; }

private:
    static std::function<void(const std::string&, LogLevel)> s_Callback;
};
