#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "navigation/NavTypes.h"

namespace nav { class NavMeshRuntime; }

namespace nav::io
{
    static constexpr uint32_t NAVBIN_MAGIC = 'B' | ('V'<<8) | ('A'<<16) | ('N'<<24); // 'NAVB' little-endian
    static constexpr uint32_t NAVBIN_VERSION = 1;

    bool WriteNavbin(const NavMeshRuntime& rt, uint64_t bakeHash, const std::string& filePath);
    bool ReadNavbin(const std::string& filePath, std::shared_ptr<NavMeshRuntime>& out, uint64_t& outHash);

    // Helper called by component EnsureRuntimeLoaded()
    bool LoadNavMeshFromFile(const std::string& path, std::shared_ptr<NavMeshRuntime>& out, uint64_t& outHash);
}


