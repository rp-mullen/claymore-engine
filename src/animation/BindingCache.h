#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>

struct SkeletonComponent;
class Scene;

namespace cm { namespace animation { struct HumanoidAvatar; } }

namespace cm {
namespace animation {

// Resolves property paths and skeleton bone names to stable runtime ids/indices.
class BindingCache {
public:
    BindingCache() = default;

    void SetScene(Scene* scene) { m_Scene = scene; }
    void SetSkeleton(const SkeletonComponent* skeleton);

    // Property binding: path -> resolved id (opaque)
    std::uint64_t ResolveProperty(const std::string& path) const;
    int ResolveBoneByName(const std::string& name) const; // -1 if not found

    void Clear();

private:
    Scene* m_Scene = nullptr;
    const SkeletonComponent* m_Skeleton = nullptr;
    std::unordered_map<std::string, std::uint64_t> m_PropertyPathToId;
};

} // namespace animation
} // namespace cm



