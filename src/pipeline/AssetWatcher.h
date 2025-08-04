#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <mutex>

class AssetPipeline;

class AssetWatcher {
public:
    AssetWatcher(AssetPipeline& pipeline, const std::string& rootPath);
    ~AssetWatcher();

    void Start();
    void Stop();

	void SetRootPath(const std::string& path) { m_RootPath = path; }

private:
    void WatchLoop();
    bool HasFileChanged(const std::string& path, std::filesystem::file_time_type lastWriteTime);

    AssetPipeline& m_Pipeline;
    std::string m_RootPath;
    std::atomic<bool> m_Running;
    std::thread m_Thread;

    std::unordered_map<std::string, std::filesystem::file_time_type> m_FileTimestamps;
    std::mutex m_TimestampMutex;

    static constexpr auto POLL_INTERVAL = std::chrono::seconds(2);
};
