#pragma once
#include "prefab/PrefabAsset.h"
#include "prefab/PrefabOverrides.h"
#include "ecs/Scene.h"

namespace PrefabIO {
    // Authoring JSON
    bool LoadAuthoringPrefabJSON(const std::string& path, PrefabAsset& out);
    bool SaveAuthoringPrefabJSON(const std::string& path, const PrefabAsset& in);

    bool LoadVariantJSON(const std::string& path, ClaymoreGUID& outGuid, ClaymoreGUID& outBaseGuid, PrefabOverrides& out);
    bool SaveVariantJSON(const std::string& path, const ClaymoreGUID& guid, const ClaymoreGUID& baseGuid, const PrefabOverrides& ov);
}


