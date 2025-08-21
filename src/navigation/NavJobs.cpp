#include "navigation/NavJobs.h"
#include "navigation/NavMesh.h"
#include "navigation/NavSerialization.h"
#include "ecs/Scene.h"
#include "ecs/Components.h"
#include "pipeline/AssetLibrary.h"
#include "ui/Logger.h"
#include <filesystem>
#include <thread>

using namespace nav;
using namespace nav::jobs;

// Very simplified mesh gather and triangulation copy: take all triangle indices from meshes as nav triangles
static void GatherSourceTriangles(Scene& scene, const NavMeshComponent& comp, std::vector<glm::vec3>& outVerts, std::vector<uint32_t>& outTris, Bounds& outBounds)
{
    outVerts.clear(); outTris.clear();
    outBounds.min = glm::vec3(FLT_MAX); outBounds.max = glm::vec3(-FLT_MAX);
    for (EntityID id : comp.SourceMeshes) {
        auto* d = scene.GetEntityData(id);
        if (!d || !d->Mesh || !d->Mesh->mesh) continue;
        const Mesh& m = *d->Mesh->mesh;
        const glm::mat4& M = d->Transform.WorldMatrix;
        uint32_t base = (uint32_t)outVerts.size();
        outVerts.reserve(outVerts.size() + m.Vertices.size());
        for (const auto& v : m.Vertices) {
            glm::vec3 w = glm::vec3(M * glm::vec4(v, 1.0f));
            outVerts.push_back(w);
            outBounds.expand(w);
        }
        // indices are uint16_t in Mesh; widen to uint32_t with base
        for (size_t i = 0; i + 2 < m.Indices.size(); i += 3) {
            outTris.push_back(base + (uint32_t)m.Indices[i+0]);
            outTris.push_back(base + (uint32_t)m.Indices[i+1]);
            outTris.push_back(base + (uint32_t)m.Indices[i+2]);
        }
    }
}

// Build a trivial navmesh: use the gathered triangles directly; build simple adjacency if triangles share identical edges
void BuildRuntimeFromTriangles(const std::vector<glm::vec3>& verts, const std::vector<uint32_t>& tris, const Bounds& bounds, std::shared_ptr<NavMeshRuntime>& out)
{
    auto rt = std::make_shared<NavMeshRuntime>();
    rt->m_Vertices = verts;
    rt->m_Polys.reserve(tris.size()/3);
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        NavMeshRuntime::Poly p{}; p.i0 = tris[i]; p.i1 = tris[i+1]; p.i2 = tris[i+2]; p.area = 1; p.flags = 0;
        rt->m_Polys.push_back(p);
    }
    // adjacency via edge map
    struct Edge { uint32_t a,b; uint32_t tri;};
    std::vector<Edge> edges; edges.reserve(rt->m_Polys.size()*3);
    for (uint32_t t = 0; t < (uint32_t)rt->m_Polys.size(); ++t) {
        const auto& p = rt->m_Polys[t];
        auto add = [&](uint32_t i, uint32_t j){ Edge e{ std::min(i,j), std::max(i,j), t }; edges.push_back(e); };
        add(p.i0, p.i1); add(p.i1, p.i2); add(p.i2, p.i0);
    }
    std::sort(edges.begin(), edges.end(), [](const Edge& x, const Edge& y){ if (x.a!=y.a) return x.a<y.a; if (x.b!=y.b) return x.b<y.b; return x.tri<y.tri; });
    rt->m_Adjacency.assign(rt->m_Polys.size(), {});
    for (size_t i = 1; i < edges.size(); ++i) {
        if (edges[i].a==edges[i-1].a && edges[i].b==edges[i-1].b && edges[i].tri!=edges[i-1].tri) {
            rt->m_Adjacency[edges[i].tri].push_back(edges[i-1].tri);
            rt->m_Adjacency[edges[i-1].tri].push_back(edges[i].tri);
        }
    }
    rt->m_Bounds = bounds;
    rt->RebuildBVH();
    out = std::move(rt);
}

void nav::jobs::SubmitBake(NavMeshComponent* comp, Scene* scene)
{
    // Run on background thread so we don't block main thread
    std::thread([comp, scene](){
        std::vector<glm::vec3> verts; std::vector<uint32_t> tris; Bounds b;
        comp->BakingProgress.store(0.05f);
        GatherSourceTriangles(*scene, *comp, verts, tris, b);
        if (comp->BakingCancel.load()) { comp->Baking.store(false); return; }
        comp->BakingProgress.store(0.35f);

        std::shared_ptr<NavMeshRuntime> rt;
        BuildRuntimeFromTriangles(verts, tris, b, rt);
        if (comp->BakingCancel.load()) { comp->Baking.store(false); return; }
        comp->BakingProgress.store(0.6f);

        // Serialize deterministically to .navbin
        uint64_t bakeHash = comp->ComputeBakeHash(*scene);
        std::filesystem::path outDir = std::filesystem::current_path() / "assets" / "Nav";
        std::error_code ec; std::filesystem::create_directories(outDir, ec);
        std::filesystem::path outPath = outDir / (std::to_string((uint64_t)reinterpret_cast<uintptr_t>(comp)) + std::string(".navbin"));
        if (!io::WriteNavbin(*rt, bakeHash, outPath.string())) {
            Logger::LogError("[Nav] Failed to write navbin");
            comp->Baking.store(false); return;
        }
        comp->BakingProgress.store(0.85f);

        // Register as asset and update component
        AssetReference ref; ref.guid = ClaymoreGUID::Generate(); ref.fileID = 0;
        AssetLibrary::Instance().RegisterAsset(ref, (AssetType)999 /*NavMesh*/, outPath.string(), "NavMesh");
        comp->BakedAsset = ref;
        comp->BakeHash = bakeHash;

        comp->Runtime = rt; // hot-swap
        comp->BakingProgress.store(1.0f);
        comp->Baking.store(false);
    }).detach();
}

// Provide symbol used by NavMeshComponent::RequestBake
extern "C" void SubmitBake(nav::NavMeshComponent* comp, Scene* scene) { nav::jobs::SubmitBake(comp, scene); }


