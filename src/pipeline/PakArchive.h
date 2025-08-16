#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Simple uncompressed .pak archive format:
// [magic: "CLYP" 4 bytes]
// [version: uint32 = 1]
// [fileCount: uint32]
// repeated fileCount times:
//   [pathLen: uint32]
//   [path bytes UTF-8]
//   [offset: uint64]
//   [size: uint64]
// [blob data...]

class PakArchive {
public:
    struct Entry {
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    // Writer API
    void AddFile(const std::string& virtualPath, const std::vector<uint8_t>& data);
    bool SaveToFile(const std::string& pakPath) const;

    // Reader API
    bool Open(const std::string& pakPath);
    bool Contains(const std::string& virtualPath) const;
    bool ReadFile(const std::string& virtualPath, std::vector<uint8_t>& outData) const;

private:
    struct FileData {
        std::string path;
        std::vector<uint8_t> data;
    };

    // For writer
    std::vector<FileData> m_Files;

    // For reader
    std::string m_PakPath;
    std::unordered_map<std::string, Entry> m_Index;
};


