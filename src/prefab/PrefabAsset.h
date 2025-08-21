#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "pipeline/AssetReference.h"
#include "ecs/Entity.h"

struct PrefabAssetEntityNode {
    ClaymoreGUID Guid; // stable entity guid within prefab
    std::string Name;
    ClaymoreGUID ParentGuid; // zero for root
    nlohmann::json Components; // authoring component data by type
    std::vector<ClaymoreGUID> Children; // child entity guids
};

struct PrefabAsset {
    ClaymoreGUID Guid;
    std::string Name;
    ClaymoreGUID RootGuid;
    std::vector<PrefabAssetEntityNode> Entities;
};


