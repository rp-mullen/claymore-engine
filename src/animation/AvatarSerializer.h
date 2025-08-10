#pragma once

#include <string>
#include "animation/AvatarDefinition.h"

namespace cm {
namespace animation {

// JSON serialize/deserialize .avatar files
bool SaveAvatar(const AvatarDefinition& avatar, const std::string& path);
bool LoadAvatar(AvatarDefinition& avatar, const std::string& path);

} // namespace animation
} // namespace cm


