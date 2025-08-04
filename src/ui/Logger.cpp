#include "Logger.h"

std::function<void(const std::string&, LogLevel)> Logger::s_Callback;

void Logger::Log(const std::string& message) {
    if (s_Callback) s_Callback(message, LogLevel::Info);
}
void Logger::LogWarning(const std::string& message) {
    if (s_Callback) s_Callback(message, LogLevel::Warning);
}
void Logger::LogError(const std::string& message) {
    if (s_Callback) s_Callback(message, LogLevel::Error);
}
