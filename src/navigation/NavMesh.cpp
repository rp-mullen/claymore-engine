#include "navigation/NavMesh.h"
#include "ecs/Scene.h"
#include "ecs/Components.h"
#include "pipeline/AssetLibrary.h"
#include "navigation/NavQueries.h"
#include "navigation/NavSerialization.h"
#include "navigation/NavJobs.h"
#include <cstring>

using namespace nav;

static uint64_t fnv1a64(const void* data, size_t len, uint64_t seed)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    uint64_t h = 1469598103934665603ULL ^ seed;
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}
static uint64_t HashCombine(uint64_t h, uint64_t k) { h ^= k + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2); return h; }

// Helper: find the owning EntityID for a given NavMeshComponent pointer
static EntityID FindOwnerEntity(Scene& scene, const NavMeshComponent* comp)
{
    for (const auto& e : scene.GetEntities()) {
        auto* d = scene.GetEntityData(e.GetID());
        if (d && d->Navigation && d->Navigation.get() == comp) return e.GetID();
    }
    return INVALID_ENTITY_ID;
}

// Helper: collect all entities starting at root that have a MeshComponent with a valid mesh
static void CollectMeshEntitiesRecursive(Scene& scene, EntityID root, std::vector<EntityID>& out)
{
    auto* d = scene.GetEntityData(root);
    if (!d) return;
    if (d->Mesh && d->Mesh->mesh) out.push_back(root);
    for (EntityID c : d->Children) CollectMeshEntitiesRecursive(scene, c, out);
}

void NavMeshComponent::GetEffectiveSources(Scene& scene, std::vector<EntityID>& out) const
{
    out.clear();
    if (!SourceMeshes.empty()) { out = SourceMeshes; return; }
    EntityID owner = FindOwnerEntity(scene, this);
    if (owner != INVALID_ENTITY_ID) CollectMeshEntitiesRecursive(scene, owner, out);
}

uint64_t NavMeshComponent::ComputeBakeHash(Scene& scene) const
{
    uint64_t h = 0xcbf29ce484222325ULL;
    // Include bake settings
    const uint64_t bakeHash = fnv1a64(&Bake, sizeof(Bake), 0x1234);
    h = HashCombine(h, bakeHash);

    // Determine effective sources: explicit list or (owner + children) if none specified
    std::vector<EntityID> effectiveSources;
    if (!SourceMeshes.empty()) {
        effectiveSources = SourceMeshes;
    } else {
        EntityID owner = FindOwnerEntity(scene, this);
        if (owner != INVALID_ENTITY_ID) {
            CollectMeshEntitiesRecursive(scene, owner, effectiveSources);
        }
    }

    // Include source meshes CPU vertex/index + world transform hash
    for (EntityID id : effectiveSources) {
        auto* d = scene.GetEntityData(id);
        if (!d || !d->Mesh || !d->Mesh->mesh) continue;
        const Mesh& m = *d->Mesh->mesh;
        uint64_t vhash = fnv1a64(m.Vertices.data(), m.Vertices.size() * sizeof(glm::vec3), 0x1111);
        uint64_t ihash = fnv1a64(m.Indices.data(), m.Indices.size() * sizeof(uint32_t), 0x2222);
        uint64_t thash = fnv1a64(&d->Transform.WorldMatrix, sizeof(glm::mat4), 0x3333);
        h = HashCombine(h, vhash);
        h = HashCombine(h, ihash);
        h = HashCombine(h, thash);
    }
    return h;
}

void NavMeshComponent::RequestBake(Scene& scene)
{
    if (Baking.exchange(true)) return; // already baking
    BakingCancel.store(false);
    BakingProgress.store(0.0f);
    // Job dispatched via NavJobs (implemented in NavJobs.cpp)
    namespace jobs = nav::jobs;
    extern void SubmitBake(NavMeshComponent* comp, Scene* scene);
    jobs::SubmitBake(this, &scene);
}

void NavMeshComponent::CancelBake()
{
    BakingCancel.store(true);
}

bool NavMeshComponent::EnsureRuntimeLoaded()
{
    if (Runtime) return true;
    if (BakedAsset.guid == ClaymoreGUID()) return false;
    // Load from asset library path
    if (auto* entry = AssetLibrary::Instance().GetAsset(BakedAsset)) {
        // Use nav serialization loader
        uint64_t fileHash = 0;
        std::shared_ptr<NavMeshRuntime> loaded;
        if (nav::io::LoadNavMeshFromFile(entry->path, loaded, fileHash)) {
            Runtime = loaded;
            BakeHash = fileHash;
            return true;
        }
    }
    return false;
}

NavMeshRuntime::NavMeshRuntime()
{
    m_AreaCost.fill(1.0f);
}

bool NavMeshRuntime::FindPath(const glm::vec3& start, const glm::vec3& end, NavPath& out, const NavAgentParams& params, NavFlags include, NavFlags exclude) const
{
    std::shared_lock lk(m_Lock);
    return queries::FindPath(*this, start, end, params, include, exclude, out);
}

bool NavMeshRuntime::Raycast(const glm::vec3& start, const glm::vec3& end, float& tHit, glm::vec3& hitNormal) const
{
    std::shared_lock lk(m_Lock);
    return queries::RaycastPolyMesh(*this, start, end, tHit, hitNormal);
}

bool NavMeshRuntime::NearestPoint(const glm::vec3& pos, float maxDist, glm::vec3& outOnMesh) const
{
    std::shared_lock lk(m_Lock);
    return queries::NearestPointOnNavmesh(*this, pos, maxDist, outOnMesh);
}

void NavMeshRuntime::RebuildBVH()
{
    std::unique_lock lk(m_Lock);
    // Simple BVH: single node encompassing all triangles; can be improved later
    m_BVH.clear();
    m_BVHIndices.clear();
    Bounds b{}; b.min = glm::vec3(FLT_MAX); b.max = glm::vec3(-FLT_MAX);
    for (const auto& v : m_Vertices) { b.expand(v); }
    m_Bounds = b;
    NavMeshRuntime::BVNode root; root.b = b; root.start = 0; root.count = (uint32_t)m_Polys.size();
    m_BVH.reserve(1);
    m_BVH.push_back(root);
    m_BVHIndices.resize(m_Polys.size());
    for (uint32_t i = 0; i < (uint32_t)m_Polys.size(); ++i) m_BVHIndices[i] = i;
}


