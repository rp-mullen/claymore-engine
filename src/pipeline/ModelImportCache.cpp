#include "ModelImportCache.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <mutex>
#include <algorithm>

// Minimal stubbed implementation to establish the API and file outputs.
// The full Assimp-based build and binary writers can be incrementally filled in.

namespace {
    using json = nlohmann::json;
    std::mutex g_modelCacheMutex;

    static inline bool EndsWith(const std::string& s, const std::string& suf) {
        if (s.size() < suf.size()) return false;
        return std::equal(suf.rbegin(), suf.rend(), s.rbegin(),
            [](char a, char b){ return ::tolower(a) == ::tolower(b); });
    }

    static std::string ToForwardSlashes(std::string p) {
        std::replace(p.begin(), p.end(), '\\', '/');
        return p;
    }
}

static bool WriteTinyMeta(const std::string& sourceModelPath, const BuiltModelPaths& out) {
    try {
        json j;
        j["version"] = 1;
        j["source"] = ToForwardSlashes(sourceModelPath);
        j["skeleton"] = ToForwardSlashes(out.skelPath);
        // Minimal meshes list with a single submesh entry mapping fileID 0
        j["meshes"] = json::array();
        j["meshes"].push_back({ {"fileID", 0}, {"mesh", ToForwardSlashes(out.meshPath + "#0")} });
        j["animations"] = json::array();

        std::ofstream of(out.metaPath, std::ios::binary | std::ios::trunc);
        if (!of.is_open()) return false;
        of << j.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

bool EnsureModelCache(const std::string& sourceModelPath, BuiltModelPaths& out) {
    namespace fs = std::filesystem;
    fs::path src(sourceModelPath);
    if (!fs::exists(src)) return false;

    fs::path baseDir = src.parent_path();
    fs::path stem = src.stem();

    out.metaPath = (baseDir / (stem.string() + ".meta")).string();
    out.skelPath = (baseDir / (stem.string() + ".skelbin")).string();
    out.meshPath = (baseDir / (stem.string() + ".meshbin")).string();

    // If all files exist and are newer than source, consider up-to-date.
    auto upToDate = [&](const fs::path& p) {
        if (!fs::exists(p)) return false;
        auto tSrc = fs::last_write_time(src);
        auto tP = fs::last_write_time(p);
        return tP >= tSrc;
    };

    bool ok = upToDate(out.metaPath) && upToDate(out.skelPath) && upToDate(out.meshPath);
    if (ok) return true;

    // Otherwise, build now (blocking in caller thread).
    return BuildModelCacheBlocking(sourceModelPath, out);
}

bool BuildModelCacheBlocking(const std::string& sourceModelPath, BuiltModelPaths& out) {
    namespace fs = std::filesystem;
    try {
        std::lock_guard<std::mutex> lk(g_modelCacheMutex);
        fs::path src(sourceModelPath);
        if (!fs::exists(src)) return false;
        fs::path baseDir = src.parent_path();
        fs::path stem = src.stem();

        out.metaPath = (baseDir / (stem.string() + ".meta")).string();
        out.skelPath = (baseDir / (stem.string() + ".skelbin")).string();
        out.meshPath = (baseDir / (stem.string() + ".meshbin")).string();

        // TODO: Integrate Assimp parse via existing ModelLoader here; for now, create placeholder binaries.
        // Ensure directory exists
        {
            std::error_code ec; fs::create_directories(baseDir, ec);
        }
        {
            std::ofstream skel(out.skelPath, std::ios::binary | std::ios::trunc);
            if (!skel.is_open()) { std::cerr << "[ModelImportCache] Cannot open skelbin: " << out.skelPath << "\n"; return false; }
            uint32_t jointCount = 0u;
            skel.write(reinterpret_cast<const char*>(&jointCount), sizeof(jointCount));
        }
        {
            std::ofstream mesh(out.meshPath, std::ios::binary | std::ios::trunc);
            if (!mesh.is_open()) { std::cerr << "[ModelImportCache] Cannot open meshbin: " << out.meshPath << "\n"; return false; }
            uint32_t submeshCount = 0u;
            mesh.write(reinterpret_cast<const char*>(&submeshCount), sizeof(submeshCount));
        }
        if (!WriteTinyMeta(sourceModelPath, out)) return false;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ModelImportCache] Build failed: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "[ModelImportCache] Build failed: unknown error" << std::endl;
        return false;
    }
}


