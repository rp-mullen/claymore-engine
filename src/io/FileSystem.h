#pragma once
#include <string>
#include <vector>
#include <unordered_map>

class FileSystem {
public:
    static FileSystem& Instance() {
        static FileSystem fs; return fs;
    }

    bool MountPak(const std::string& pakPath);
    bool IsPakMounted() const { return m_PakMounted; }

    bool ReadFile(const std::string& path, std::vector<uint8_t>& outData) const;
    bool ReadTextFile(const std::string& path, std::string& outText) const;
    bool Exists(const std::string& path) const;

    // Convert an absolute or project-relative path into a normalized virtual path key
    // For now we use forward slashes and collapse redundant separators.
    static std::string Normalize(const std::string& path);

private:
    FileSystem() = default;
    ~FileSystem() = default;

    bool m_PakMounted = false;
    std::string m_PakPath;
};


