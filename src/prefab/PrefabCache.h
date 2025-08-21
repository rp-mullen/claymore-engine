#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "pipeline/AssetReference.h"

struct CompiledPrefab_SkinnedInfo {
    std::vector<uint16_t> Remap;
    std::vector<uint16_t> UsedJointList;
    uint32_t PaletteSize = 0; // number of joints in palette
};

struct CompiledPrefabEntityRecord {
    ClaymoreGUID EntityGuid;
    std::string Name;
    // Component data in runtime-friendly binary/json; simplified here
    nlohmann::json Components;
    CompiledPrefab_SkinnedInfo Skinned;
};

struct CompiledPrefab {
    ClaymoreGUID PrefabGuid;
    std::string EngineVersion;
    uint64_t PrefabHash = 0; // hash(base + overrides)
    // Quick validity: include import hashes of referenced assets
    std::vector<std::pair<ClaymoreGUID, std::string>> ReferencedAssetImportHashes;

    std::vector<CompiledPrefabEntityRecord> Entities;
};

bool LoadCompiledPrefab(const ClaymoreGUID& prefabGuid, CompiledPrefab& out);
bool WriteCompiledPrefab(const ClaymoreGUID& prefabGuid, const CompiledPrefab& in);


