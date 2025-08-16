#include "FileSystem.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "pipeline/PakArchive.h"
#include <editor/Project.h>

static PakArchive g_Pak;

static std::string ToForwardSlashes(const std::string& in) {
    std::string s = in;
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

std::string FileSystem::Normalize(const std::string& path) {
    std::string s = ToForwardSlashes(path);
    // Remove "/./" and collapse duplicate '/'
    std::string out; out.reserve(s.size());
    bool lastWasSlash = false;
    for (char c : s) {
        if (c == '/') {
            if (!lastWasSlash) out.push_back('/');
            lastWasSlash = true;
        } else {
            out.push_back(c);
            lastWasSlash = false;
        }
    }
    return out;
}

bool FileSystem::MountPak(const std::string& pakPath) {
    m_PakMounted = g_Pak.Open(pakPath);
    if (m_PakMounted) {
        m_PakPath = pakPath;
        printf("[VFS] Mounted pak: %s\n", pakPath.c_str());
    } else {
        printf("[VFS] Failed to mount pak: %s\n", pakPath.c_str());
    }
    return m_PakMounted;
}

bool FileSystem::ReadFile(const std::string& path, std::vector<uint8_t>& outData) const {
    if (m_PakMounted) {
        std::string key = Normalize(path);
        if (g_Pak.ReadFile(key, outData)) return true;
        // Also try to strip leading ./ or absolute prefix up to "assets/" or "shaders/"
        auto pos = key.find("assets/");
        if (pos != std::string::npos) {
            std::string rel = key.substr(pos);
            if (g_Pak.ReadFile(rel, outData)) return true;
        }
        pos = key.find("shaders/");
        if (pos != std::string::npos) {
            std::string rel = key.substr(pos);
            if (g_Pak.ReadFile(rel, outData)) return true;
            // If key was shaders/<name>.bin but pak used compiled folder, try that
            if (rel.rfind("shaders/", 0) == 0 && rel.find(".bin") != std::string::npos) {
                std::string candidate = std::string("shaders/compiled/windows/") + rel.substr(std::string("shaders/").size());
                if (g_Pak.ReadFile(candidate, outData)) return true;
            }
        }
    }

    // Fallback to disk
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        // Try resolving relative to project root for virtual paths like assets/... or shaders/...
        try {
            std::filesystem::path proj = Project::GetProjectDirectory();
            if (!proj.empty()) {
                std::filesystem::path alt = proj / path;
                in.open(alt.string(), std::ios::binary);
                if (!in.is_open()) {
                    // Also try stripping any leading ./
                    std::string key = Normalize(path);
                    auto pos = key.find("assets/");
                    if (pos != std::string::npos) {
                        std::filesystem::path alt2 = proj / key.substr(pos);
                        in.open(alt2.string(), std::ios::binary);
                    }
                }
            }
        } catch(...) {}
    }
    if (!in.is_open()) return false;
    in.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    outData.resize(size);
    if (size != 0) in.read(reinterpret_cast<char*>(outData.data()), static_cast<std::streamsize>(size));
    return true;
}

bool FileSystem::ReadTextFile(const std::string& path, std::string& outText) const {
    std::vector<uint8_t> data;
    if (!ReadFile(path, data)) return false;
    outText.assign(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

bool FileSystem::Exists(const std::string& path) const {
    if (m_PakMounted) {
        std::string key = Normalize(path);
        std::vector<uint8_t> dummy;
        if (g_Pak.Contains(key)) return true;
        auto pos = key.find("assets/");
        if (pos != std::string::npos && g_Pak.Contains(key.substr(pos))) return true;
        pos = key.find("shaders/");
        if (pos != std::string::npos && g_Pak.Contains(key.substr(pos))) return true;
    }
    return std::filesystem::exists(path);
}


