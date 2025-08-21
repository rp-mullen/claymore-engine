#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "ecs/Scene.h"

struct ResolvedTarget {
    EntityID Entity = (EntityID)-1;
    std::string ComponentType; // e.g., "Transform"
    std::vector<std::string> FieldChain; // e.g., {"S"} or {"Position"}
    int OrdinalIndex = -1; // for ComponentType#i
    std::optional<int> ArrayIndex; // when FieldSel includes [index]
};

// Very minimal resolver: supports only "@root/Transform" and optional .Position/.Rotation/.Scale/.S
bool ResolvePath(const std::string& path, EntityID root, Scene& scene, ResolvedTarget& out);

// Apply set for supported targets (Transform vec3 fields)
bool ApplySet(Scene& scene, const ResolvedTarget& tgt, const nlohmann::json& value);


