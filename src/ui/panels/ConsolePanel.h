#pragma once
#include <vector>
#include <string>
#include <unordered_map>

enum class LogLevel {
    Info,
    Warning,
    Error
};

struct ConsoleEntry {
    std::string message;
    LogLevel level;
    int count; // For auto-collapse
};

class ConsolePanel {
public:
    void OnImGuiRender();
    void AddLog(const std::string& message, LogLevel level = LogLevel::Info);
    void Clear();

private:
    std::vector<ConsoleEntry> m_LogEntries;
    std::unordered_map<std::string, int> m_LogIndex; // For collapsing duplicates
    bool m_AutoScroll = true;
    bool m_ShowInfo = true, m_ShowWarning = true, m_ShowError = true;
    char m_SearchBuffer[256] = "";
};
