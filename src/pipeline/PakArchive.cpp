#include "PakArchive.h"
#include <fstream>
#include <filesystem>
#include <cstring>

namespace {
    constexpr uint32_t kPakVersion = 1;
    constexpr char kMagic[4] = {'C','L','Y','P'};
}

void PakArchive::AddFile(const std::string& virtualPath, const std::vector<uint8_t>& data) {
    m_Files.push_back({ virtualPath, data });
}

bool PakArchive::SaveToFile(const std::string& pakPath) const {
    std::ofstream out(pakPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    // Write header
    out.write(kMagic, 4);
    uint32_t version = kPakVersion;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));

    uint32_t fileCount = static_cast<uint32_t>(m_Files.size());
    out.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));

    // Reserve space for table; compute offsets as we go
    // We'll first compute the starting offset after the table
    // Table size = sum over files of (4 + pathLen + 8 + 8)
    uint64_t tableSize = 0;
    for (const auto& f : m_Files) {
        tableSize += 4 + static_cast<uint32_t>(f.path.size()) + 8 + 8;
    }
    const uint64_t headerSize = 4 + 4 + 4 + tableSize;

    uint64_t runningOffset = headerSize;
    // Write table
    for (const auto& f : m_Files) {
        uint32_t pathLen = static_cast<uint32_t>(f.path.size());
        out.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        out.write(f.path.data(), pathLen);
        out.write(reinterpret_cast<const char*>(&runningOffset), sizeof(runningOffset));
        uint64_t size = static_cast<uint64_t>(f.data.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        runningOffset += size;
    }

    // Write blobs
    for (const auto& f : m_Files) {
        if (!f.data.empty())
            out.write(reinterpret_cast<const char*>(f.data.data()), static_cast<std::streamsize>(f.data.size()));
    }

    return true;
}

bool PakArchive::Open(const std::string& pakPath) {
    m_Index.clear();
    m_PakPath = pakPath;

    std::ifstream in(pakPath, std::ios::binary);
    if (!in.is_open()) return false;

    char magic[4] = {};
    in.read(magic, 4);
    if (std::memcmp(magic, kMagic, 4) != 0) return false;

    uint32_t version = 0;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != kPakVersion) return false;

    uint32_t fileCount = 0;
    in.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));

    for (uint32_t i = 0; i < fileCount; ++i) {
        uint32_t pathLen = 0;
        in.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
        std::string path;
        path.resize(pathLen);
        if (pathLen) in.read(path.data(), pathLen);
        Entry e{};
        in.read(reinterpret_cast<char*>(&e.offset), sizeof(e.offset));
        in.read(reinterpret_cast<char*>(&e.size), sizeof(e.size));
        m_Index.emplace(std::move(path), e);
    }
    return true;
}

bool PakArchive::Contains(const std::string& virtualPath) const {
    return m_Index.find(virtualPath) != m_Index.end();
}

bool PakArchive::ReadFile(const std::string& virtualPath, std::vector<uint8_t>& outData) const {
    auto it = m_Index.find(virtualPath);
    if (it == m_Index.end()) return false;
    const Entry& e = it->second;

    std::ifstream in(m_PakPath, std::ios::binary);
    if (!in.is_open()) return false;
    in.seekg(static_cast<std::streamoff>(e.offset), std::ios::beg);
    outData.resize(static_cast<size_t>(e.size));
    if (e.size != 0) in.read(reinterpret_cast<char*>(outData.data()), static_cast<std::streamsize>(e.size));
    return true;
}


