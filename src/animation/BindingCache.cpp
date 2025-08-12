#include "animation/BindingCache.h"
#include "ecs/AnimationComponents.h"
#include "ecs/Scene.h"

#include <functional>

namespace cm {
namespace animation {

void BindingCache::SetSkeleton(const SkeletonComponent* skeleton) { m_Skeleton = skeleton; }

std::uint64_t BindingCache::ResolveProperty(const std::string& path) const
{
    // Very simple hash-based id for now; editor will hold the full path
    // and runtime writeback will route via systems using the id.
    auto it = m_PropertyPathToId.find(path);
    if (it != m_PropertyPathToId.end()) return it->second;
    std::hash<std::string> h; // non-stable across runs, but acceptable for MVP editor preview
    std::uint64_t id = static_cast<std::uint64_t>(h(path));
    const_cast<BindingCache*>(this)->m_PropertyPathToId.emplace(path, id);
    return id;
}

int BindingCache::ResolveBoneByName(const std::string& name) const
{
    if (!m_Skeleton) return -1;
    return m_Skeleton->GetBoneIndex(name);
}

void BindingCache::Clear()
{
    m_PropertyPathToId.clear();
}

} // namespace animation
} // namespace cm



