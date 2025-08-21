#pragma once
#include "pipeline/AssetReference.h"
#include "prefab/PrefabAsset.h"
#include "prefab/PrefabOverrides.h"
#include "prefab/PrefabCache.h"
#include "ecs/Scene.h"

// Loading / Instantiation
EntityID InstantiatePrefab(const ClaymoreGUID& prefabGuid, Scene& dst, const PrefabOverrides* instanceOverridesOpt = nullptr);
EntityID InstantiatePrefabFromAuthoringPath(const std::string& authoringPath, Scene& dst, const PrefabOverrides* instanceOverridesOpt = nullptr);
bool SavePrefab(const ClaymoreGUID& prefabGuid, const PrefabAsset& src); // writes .prefab.json (base or variant)

// Diffs
PrefabOverrides ComputeOverrides(const PrefabAsset& base, const Scene& editedScene, EntityID editedRoot);
bool ApplyOverrides(EntityID root, const PrefabOverrides& ov, Scene& scene);

// Cache
bool LoadCompiledPrefab(const ClaymoreGUID& prefabGuid, CompiledPrefab& out);
bool WriteCompiledPrefab(const ClaymoreGUID& prefabGuid, const CompiledPrefab& in);

// Validation
struct Diagnostics { std::vector<std::string> Errors; std::vector<std::string> Warnings; };
Diagnostics ValidatePrefab(const ClaymoreGUID& prefabGuid);

// Editor helpers
bool BuildPrefabAssetFromScene(Scene& scene, EntityID root, PrefabAsset& out);


