#include "prefab/PrefabSerializer.h"
#include "prefab/PrefabCache.h"
#include "serialization/Serializer.h"
#include "ecs/Scene.h"
#include <iostream>

// Minimal placeholder migration tool entry point; integrate into editor UI later
namespace EditorMigration {

bool MigrateLegacyPrefabToAuthoring(const std::string& legacyPath, const ClaymoreGUID& newGuid) {
    // Load legacy into a temp scene
    Scene tmp;
    EntityID root = Serializer::LoadPrefabToScene(legacyPath, tmp);
    if (root == (EntityID)-1 || root == 0) { std::cerr << "[PrefabMigrate] Failed to load legacy: " << legacyPath << std::endl; return false; }

    PrefabAsset asset; asset.Guid = newGuid; asset.Name = legacyPath; // use filename later; root guid generated below
    // Traverse and emit nodes
    std::function<void(EntityID, ClaymoreGUID)> dfs = [&](EntityID id, ClaymoreGUID parent) {
        auto* d = tmp.GetEntityData(id); if (!d) return;
        PrefabAssetEntityNode n; n.Guid = d->EntityGuid; n.ParentGuid = parent; n.Name = d->Name;
        // Fill components minimally using existing serializer outputs
        nlohmann::json e = Serializer::SerializeEntity(id, tmp);
        if (e.contains("transform")) n.Components["transform"] = e["transform"];
        if (e.contains("mesh")) n.Components["mesh"] = e["mesh"];
        if (e.contains("skeleton")) n.Components["skeleton"] = e["skeleton"];
        if (e.contains("skinning")) n.Components["skinning"] = e["skinning"];
        for (EntityID c : d->Children) n.Children.push_back(tmp.GetEntityData(c)->EntityGuid);
        asset.Entities.push_back(std::move(n));
        for (EntityID c : d->Children) dfs(c, d->EntityGuid);
    };
    auto* rd = tmp.GetEntityData(root); if (!rd) return false; asset.RootGuid = rd->EntityGuid; dfs(root, ClaymoreGUID{});
    // Save and build cache
    std::string outPath = std::string("assets/prefabs/") + asset.Guid.ToString() + ".prefab.json";
    PrefabIO::SaveAuthoringPrefabJSON(outPath, asset);
    CompiledPrefab compiled; compiled.PrefabGuid = asset.Guid; WriteCompiledPrefab(asset.Guid, compiled);
    return true;
}

}


