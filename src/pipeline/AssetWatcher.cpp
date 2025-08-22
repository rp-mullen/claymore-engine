#include "AssetWatcher.h"
#include "AssetPipeline.h"
#include "AssetLibrary.h"
#include "AssetRegistry.h"
#include "AssetMetadata.h"
#include <editor/Project.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>

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

                // Opportunistically refresh GUID→path registration using sidecar .meta (handles renames/moves)
                try {
                    // Sidecar .meta next to asset
                    fs::path metaPath = entry.path(); metaPath += ".meta";
                    if (fs::exists(metaPath)) {
                        // Parse meta
                        std::ifstream mi(metaPath.string());
                        nlohmann::json mj; mi >> mj; mi.close();
                        AssetMetadata meta = mj.get<AssetMetadata>();
                        if (meta.guid.high != 0 || meta.guid.low != 0) {
                            // Build virtual path relative to project root, normalize to forward slashes
                            std::error_code ec;
                            fs::path rel = fs::relative(entry.path(), Project::GetProjectDirectory(), ec);
                            std::string vpath = (ec ? entry.path().string() : rel.string());
                            std::replace(vpath.begin(), vpath.end(), '\\', '/');
                            // Ensure it starts with assets/
                            size_t pos = vpath.find("assets/");
                            if (pos != std::string::npos) vpath = vpath.substr(pos);
                            // Infer AssetType from extension
                            std::string lowerExt = ext; std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
                            AssetType at = AssetType::Mesh;
                            if (lowerExt == ".fbx" || lowerExt == ".gltf" || lowerExt == ".glb" || lowerExt == ".obj") at = AssetType::Mesh;
                            else if (lowerExt == ".png" || lowerExt == ".jpg" || lowerExt == ".jpeg" || lowerExt == ".tga") at = AssetType::Texture;
                            else if (lowerExt == ".prefab") at = AssetType::Prefab;
                            else if (lowerExt == ".ttf" || lowerExt == ".otf") at = AssetType::Font;
                            // Register mapping and alias (RegisterAsset now dedupes silently)
                            AssetLibrary::Instance().RegisterAsset(AssetReference(meta.guid, 0, static_cast<int32_t>(at)), at, vpath, entry.path().filename().string());
                            AssetLibrary::Instance().RegisterPathAlias(meta.guid, filePath);
                        }
                    }
                } catch(...) { /* silent; watcher continues */ }
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
