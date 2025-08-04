#include "AssetWatcher.h"
#include "AssetPipeline.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

AssetWatcher::AssetWatcher(AssetPipeline& pipeline, const std::string& rootPath)
    : m_Pipeline(pipeline), m_RootPath(rootPath), m_Running(false) {
}

AssetWatcher::~AssetWatcher() {
    Stop();
}

void AssetWatcher::Start() {
    if (m_Running) return;
    m_Running = true;
    m_Thread = std::thread(&AssetWatcher::WatchLoop, this);
    std::cout << "[AssetWatcher] Started for: " << m_RootPath << std::endl;
}

void AssetWatcher::Stop() {
    m_Running = false;
    if (m_Thread.joinable()) {
        m_Thread.join();
        std::cout << "[AssetWatcher] Stopped." << std::endl;
    }
}

void AssetWatcher::WatchLoop() {
    std::cout << "[AssetWatcher] Watching: " << m_RootPath << std::endl;

    while (m_Running) {
        try {
            for (auto& entry : fs::recursive_directory_iterator(m_RootPath)) {
                if (!entry.is_regular_file()) continue;

                std::string filePath = entry.path().string();
                std::string ext = entry.path().extension().string();

                if (!m_Pipeline.IsSupportedAsset(ext)) continue;

                auto lastWriteTime = fs::last_write_time(entry);

                if (HasFileChanged(filePath, lastWriteTime)) {
                    m_Pipeline.EnqueueAssetImport(filePath);
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[AssetWatcher] Error: " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(POLL_INTERVAL);
    }
}

bool AssetWatcher::HasFileChanged(const std::string& path, fs::file_time_type lastWriteTime) {
    std::lock_guard<std::mutex> lock(m_TimestampMutex);

    auto it = m_FileTimestamps.find(path);
    if (it == m_FileTimestamps.end()) {
        // New file detected
        m_FileTimestamps[path] = lastWriteTime;
        return true;
    }

    if (it->second != lastWriteTime) {
        // Modified file detected
        it->second = lastWriteTime;
        return true;
    }

    return false;
}
