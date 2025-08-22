#include "navigation/NavMeshBake.h"
#include "navigation/NavMesh.h"
#include "ecs/Scene.h"
#include "ecs/Components.h"
#include <algorithm>

using namespace nav;

bool nav::bake::BuildFromScene(Scene& scene, const NavMeshComponent& comp, NavMeshBinary& out)
{
    out.vertices.clear(); out.indices.clear(); out.bounds.min = glm::vec3(FLT_MAX); out.bounds.max = glm::vec3(-FLT_MAX);
    std::vector<EntityID> sources; comp.GetEffectiveSources(scene, sources);
    for (EntityID id : sources) {
        auto* d = scene.GetEntityData(id);
        if (!d || !d->Mesh || !d->Mesh->mesh) continue;
        const Mesh& m = *d->Mesh->mesh; const glm::mat4& M = d->Transform.WorldMatrix;
        uint32_t base = (uint32_t)out.vertices.size();
        out.vertices.reserve(out.vertices.size() + m.Vertices.size());
        for (const auto& v : m.Vertices) { glm::vec3 w = glm::vec3(M * glm::vec4(v,1)); out.vertices.push_back(w); out.bounds.expand(w); }
        for (size_t i = 0; i + 2 < m.Indices.size(); i += 3) { out.indices.push_back(base + m.Indices[i+0]); out.indices.push_back(base + m.Indices[i+1]); out.indices.push_back(base + m.Indices[i+2]); }
    }
    return !out.vertices.empty() && !out.indices.empty();
}

bool nav::bake::BuildRuntime(const NavMeshBinary& bin, std::shared_ptr<NavMeshRuntime>& out)
{
    // Build a trivial runtime directly from triangles (mirrors helper in NavJobs.cpp)
    auto rt = std::make_shared<NavMeshRuntime>();
    rt->m_Vertices = bin.vertices;
    rt->m_Polys.reserve(bin.indices.size() / 3);
    for (size_t i = 0; i + 2 < bin.indices.size(); i += 3) {
        NavMeshRuntime::Poly p{}; p.i0 = bin.indices[i]; p.i1 = bin.indices[i+1]; p.i2 = bin.indices[i+2]; p.area = 1; p.flags = 0;
        rt->m_Polys.push_back(p);
    }
    struct Edge { uint32_t a,b; uint32_t tri; };
    std::vector<Edge> edges; edges.reserve(rt->m_Polys.size() * 3);
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
    rt->m_Bounds = bin.bounds;
    rt->RebuildBVH();
    out = std::move(rt);
    return out != nullptr;
}


