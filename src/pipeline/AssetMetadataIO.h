#pragma once
#include "AssetMetadata.h"
#include <string>

class AssetMetadataIO {
public:
    static bool Load(const std::string& metaPath, AssetMetadata& outMeta);
    static void Save(const std::string& metaPath, const AssetMetadata& meta);
};
