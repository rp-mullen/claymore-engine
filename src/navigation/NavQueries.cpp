#define GLM_ENABLE_EXPERIMENTAL
#include "navigation/NavQueries.h"
#include "navigation/NavMesh.h"
#include <glm/gtx/norm.hpp>
#include <random>

using namespace nav;

namespace nav::queries
{
    static glm::vec3 TriCenter(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        return (a + b + c) / 3.0f;
    }

    static float Heuristic(const glm::vec3& a, const glm::vec3& b) {
        return glm::length(a - b);
    }

    bool FindPath(const NavMeshRuntime& nm, const glm::vec3& start, const glm::vec3& end,
                  const NavAgentParams& /*params*/, NavFlags /*include*/, NavFlags /*exclude*/, NavPath& out)
    {
        out.points.clear(); out.valid = false;
        if (nm.m_Polys.empty()) return false;

        // For simplicity pick start/end triangle by nearest center
        auto nearestPoly = [&](const glm::vec3& p){
            float best = std::numeric_limits<float>::max();
            uint32_t bestIdx = 0;
            for (uint32_t i = 0; i < (uint32_t)nm.m_Polys.size(); ++i) {
                const auto& poly = nm.m_Polys[i];
                glm::vec3 c = TriCenter(nm.m_Vertices[poly.i0], nm.m_Vertices[poly.i1], nm.m_Vertices[poly.i2]);
                float d = glm::distance2(p, c);
                if (d < best) { best = d; bestIdx = i; }
            }
            return bestIdx;
        };

        const uint32_t sIdx = nearestPoly(start);
        const uint32_t eIdx = nearestPoly(end);

        struct Node { uint32_t idx; float g, f; uint32_t parent; bool hasParent; };
        std::vector<Node> nodes(nm.m_Polys.size());
        std::vector<uint8_t> open(nm.m_Polys.size(), 0), closed(nm.m_Polys.size(), 0);
        auto triCenterAt = [&](uint32_t i){ const auto& p = nm.m_Polys[i]; return TriCenter(nm.m_Vertices[p.i0], nm.m_Vertices[p.i1], nm.m_Vertices[p.i2]); };

        auto cmp = [&](uint32_t a, uint32_t b){ return nodes[a].f > nodes[b].f; };
        std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(cmp)> pq(cmp);

        nodes[sIdx] = { sIdx, 0.0f, Heuristic(triCenterAt(sIdx), triCenterAt(eIdx)), 0, false };
        pq.push(sIdx); open[sIdx] = 1;

        bool found = false;
        while (!pq.empty()) {
            uint32_t cur = pq.top(); pq.pop(); open[cur] = 0; closed[cur] = 1;
            if (cur == eIdx) { found = true; break; }
            const auto& neigh = nm.m_Adjacency[cur];
            for (uint32_t nb : neigh) {
                if (closed[nb]) continue;
                float tentativeG = nodes[cur].g + glm::distance(triCenterAt(cur), triCenterAt(nb));
                if (!open[nb] || tentativeG < nodes[nb].g) {
                    nodes[nb].g = tentativeG;
                    nodes[nb].f = tentativeG + Heuristic(triCenterAt(nb), triCenterAt(eIdx));
                    nodes[nb].parent = cur; nodes[nb].hasParent = true;
                    if (!open[nb]) { pq.push(nb); open[nb] = 1; }
                }
            }
        }

        if (!found) return false;

        // Reconstruct path of centers
        std::vector<uint32_t> rev;
        for (uint32_t at = eIdx; ; ) {
            rev.push_back(at);
            if (!nodes[at].hasParent) break;
            at = nodes[at].parent;
        }
        std::reverse(rev.begin(), rev.end());
        out.points.reserve(rev.size() + 2);
        out.points.push_back(start);
        for (uint32_t i : rev) out.points.push_back(triCenterAt(i));
        out.points.push_back(end);
        out.valid = true;
        return true;
    }

    static bool RayTri(const glm::vec3& ro, const glm::vec3& rd, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t, glm::vec3& n)
    {
        const float EPS = 1e-6f;
        glm::vec3 ab = b - a, ac = c - a;
        n = glm::normalize(glm::cross(ab, ac));
        glm::vec3 pvec = glm::cross(rd, ac);
        float det = glm::dot(ab, pvec);
        if (fabs(det) < EPS) return false;
        float invDet = 1.0f / det;
        glm::vec3 tvec = ro - a;
        float u = glm::dot(tvec, pvec) * invDet; if (u < 0 || u > 1) return false;
        glm::vec3 qvec = glm::cross(tvec, ab);
        float v = glm::dot(rd, qvec) * invDet; if (v < 0 || u + v > 1) return false;
        float tt = glm::dot(ac, qvec) * invDet; if (tt < 0) return false;
        t = tt; return true;
    }

    bool RaycastPolyMesh(const NavMeshRuntime& nm, const glm::vec3& start, const glm::vec3& end, float& tHit, glm::vec3& hitNormal)
    {
        glm::vec3 ro = start; glm::vec3 rd = glm::normalize(end - start); float maxT = glm::length(end - start);
        bool any = false; float best = maxT;
        for (const auto& p : nm.m_Polys) {
            float t; glm::vec3 n;
            if (RayTri(ro, rd, nm.m_Vertices[p.i0], nm.m_Vertices[p.i1], nm.m_Vertices[p.i2], t, n)) {
                if (t < best) { best = t; hitNormal = n; any = true; }
            }
        }
        if (any) { tHit = best / maxT; return true; }
        return false;
    }

    bool NearestPointOnNavmesh(const NavMeshRuntime& nm, const glm::vec3& pos, float /*maxDist*/, glm::vec3& outOnMesh)
    {
        float best = std::numeric_limits<float>::max();
        glm::vec3 bestP = pos;
        for (const auto& p : nm.m_Polys) {
            glm::vec3 a = nm.m_Vertices[p.i0];
            glm::vec3 b = nm.m_Vertices[p.i1];
            glm::vec3 c = nm.m_Vertices[p.i2];
            // Barycentric projection
            glm::vec3 v0 = b - a, v1 = c - a, v2 = pos - a;
            float d00 = glm::dot(v0, v0);
            float d01 = glm::dot(v0, v1);
            float d11 = glm::dot(v1, v1);
            float d20 = glm::dot(v2, v0);
            float d21 = glm::dot(v2, v1);
            float denom = d00 * d11 - d01 * d01;
            if (fabs(denom) < 1e-6f) continue;
            float v = (d11 * d20 - d01 * d21) / denom;
            float w = (d00 * d21 - d01 * d20) / denom;
            float u = 1.0f - v - w;
            glm::vec3 pProj;
            if (u >= 0 && v >= 0 && w >= 0) {
                pProj = u * a + v * b + w * c;
            } else {
                // clamp to nearest edge
                auto closestSeg = [](const glm::vec3& p, const glm::vec3& x, const glm::vec3& y){
                    glm::vec3 d = y - x; float t = glm::dot(p - x, d) / glm::dot(d, d); t = glm::clamp(t, 0.0f, 1.0f); return x + d * t; };
                glm::vec3 p0 = closestSeg(pos, a, b);
                glm::vec3 p1 = closestSeg(pos, b, c);
                glm::vec3 p2 = closestSeg(pos, c, a);
                float d0 = glm::distance2(p0, pos), d1 = glm::distance2(p1, pos), d2 = glm::distance2(p2, pos);
                if (d0 < d1 && d0 < d2) pProj = p0; else if (d1 < d2) pProj = p1; else pProj = p2;
            }
            float d2 = glm::distance2(pProj, pos);
            if (d2 < best) { best = d2; bestP = pProj; }
        }
        outOnMesh = bestP; return true;
    }

    bool RandomPointInRadius(const NavMeshRuntime& nm, const glm::vec3& pos, float r, glm::vec3& out)
    {
        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> ang(0, 6.28318f);
        std::uniform_real_distribution<float> rad(0, r);
        for (int i = 0; i < 64; ++i) {
            float a = ang(rng), rr = rad(rng);
            glm::vec3 p = pos + glm::vec3(cos(a) * rr, 0.0f, sin(a) * rr);
            glm::vec3 on; if (NearestPointOnNavmesh(nm, p, r, on)) { out = on; return true; }
        }
        return false;
    }
}


