#include "Picking.h"
#include <limits>
#include <cfloat>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
// =============================
// Convert screen point to world-space ray
// =============================
Ray Picking::ScreenPointToRay(float nx, float ny, Camera* cam) {
    float x = nx * 2.0f - 1.0f;
    float y = 1.0f - ny * 2.0f;

    glm::vec4 rayClip(x, y, -1.0f, 1.0f);

    glm::mat4 invProj = glm::inverse(cam->GetProjectionMatrix());
    glm::vec4 rayEye = invProj * rayClip;
    rayEye.z = -1.0f;
    rayEye.w = 0.0f;

    glm::mat4 invView = glm::inverse(cam->GetViewMatrix());
    glm::vec3 rayDir = glm::normalize(glm::vec3(invView * rayEye));
    glm::vec3 origin = cam->GetPosition();

    return { origin, rayDir };
}

// =============================
// Pick entity at normalized coords
// =============================
int Picking::PickEntity(float nx, float ny, Scene& scene, Camera* cam) {
    Ray ray = ScreenPointToRay(nx, ny, cam);
    return PickEntityRay(ray, scene);
}

// =============================
// Core logic for ray picking
// =============================
int Picking::PickEntityRay(const Ray& ray, Scene& scene) {
    int pickedEntity = -1;
    float closestT = FLT_MAX;

    for (auto& entity : scene.GetEntities()) {
        auto* data = scene.GetEntityData(entity.GetID());
        if (!data || !data->Mesh) continue;

        // Use precomputed world matrix (includes parent hierarchy)
        glm::mat4 transform = data->Transform.WorldMatrix;

        float tTri;
        // Take a strong reference to guard against entity deletion during this loop
        std::shared_ptr<Mesh> meshRef = data->Mesh->mesh;
        if (!meshRef) continue;
        if (RayIntersectsMesh(ray, *meshRef.get(), transform, tTri)) {
            if (tTri < closestT && tTri > 0.0f) {
                closestT = tTri;
                pickedEntity = entity.GetID();
            }
        }
    }
    return pickedEntity;
}

// =============================
// Queuing system
// =============================
void Picking::QueuePick(float nx, float ny) {
    s_PickQueue.push_back({ nx, ny });
}

void Picking::Process(Scene& scene, Camera* cam) {
    s_LastPick = -1;
    s_ProcessedThisFrame = !s_PickQueue.empty();
    s_AnyHitThisFrame = false;
    for (auto& req : s_PickQueue) {
        int entity = PickEntity(req.nx, req.ny, scene, cam);
        if (entity != -1) { s_LastPick = entity; s_AnyHitThisFrame = true; }
    }
    s_PickQueue.clear();
}

int Picking::GetLastPick() {
    return s_LastPick;
}

bool Picking::HadPickThisFrame() { return s_ProcessedThisFrame; }
bool Picking::HadHitThisFrame() { return s_AnyHitThisFrame; }

bool Picking::RayIntersectsAABB(const Ray& ray, const glm::vec3& min, const glm::vec3& max, float& t) {
    float tMin = (min.x - ray.Origin.x) / ray.Direction.x;
    float tMax = (max.x - ray.Origin.x) / ray.Direction.x;
    if (tMin > tMax) std::swap(tMin, tMax);

    float tyMin = (min.y - ray.Origin.y) / ray.Direction.y;
    float tyMax = (max.y - ray.Origin.y) / ray.Direction.y;
    if (tyMin > tyMax) std::swap(tyMin, tyMax);

    if ((tMin > tyMax) || (tyMin > tMax)) return false;
    if (tyMin > tMin) tMin = tyMin;
    if (tyMax < tMax) tMax = tyMax;

    float tzMin = (min.z - ray.Origin.z) / ray.Direction.z;
    float tzMax = (max.z - ray.Origin.z) / ray.Direction.z;
    if (tzMin > tzMax) std::swap(tzMin, tzMax);

    if ((tMin > tzMax) || (tzMin > tMax)) return false;
    if (tzMin > tMin) tMin = tzMin;
    if (tzMax < tMax) tMax = tzMax;

    t = tMin;
    return true;
}

bool Picking::RayIntersectsOBB(const Ray& ray, const glm::mat4& transform,
    const glm::vec3& min, const glm::vec3& max, float& t) {
    glm::vec3 obbPos(transform[3]);
    glm::vec3 axisX(transform[0]);
    glm::vec3 axisY(transform[1]);
    glm::vec3 axisZ(transform[2]);

    glm::vec3 delta = obbPos - ray.Origin;
    float tMin = 0.0f;
    float tMax = FLT_MAX;

    const glm::vec3 axes[3] = { axisX, axisY, axisZ };
    const float bounds[3][2] = { {min.x, max.x}, {min.y, max.y}, {min.z, max.z} };

    for (int i = 0; i < 3; i++) {
        float e = glm::dot(axes[i], delta);
        float f = glm::dot(ray.Direction, axes[i]);

        if (fabs(f) > 1e-6f) {
            float t1 = (e + bounds[i][0]) / f;
            float t2 = (e + bounds[i][1]) / f;
            if (t1 > t2) std::swap(t1, t2);
            if (t1 > tMin) tMin = t1;
            if (t2 < tMax) tMax = t2;
            if (tMin > tMax) return false;
        }
        else if (-e + bounds[i][0] > 0.0f || -e + bounds[i][1] < 0.0f) {
            return false;
        }
    }

    t = tMin;
    return true;
}

bool Picking::RayIntersectsTriangle(const glm::vec3& origin, const glm::vec3& dir,
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& t) {
    const float EPSILON = 1e-6f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;

    glm::vec3 h = glm::cross(dir, edge2);
    float a = glm::dot(edge1, h);

    if (fabs(a) < EPSILON) return false;
    float f = 1.0f / a;
    glm::vec3 s = origin - v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(dir, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    t = f * glm::dot(edge2, q);
    return t > EPSILON;
}

bool Picking::RayIntersectsMesh(const Ray& ray, const Mesh& mesh, const glm::mat4& transform, float& closestT) {
    closestT = FLT_MAX;
    bool hit = false;

    glm::mat4 invTransform = glm::inverse(transform);
    glm::vec3 localOrigin = glm::vec3(invTransform * glm::vec4(ray.Origin, 1.0f));
    glm::vec3 localDir = glm::normalize(glm::vec3(invTransform * glm::vec4(ray.Direction, 0.0f)));

    for (size_t i = 0; i < mesh.Indices.size(); i += 3) {
        glm::vec3 v0 = mesh.Vertices[mesh.Indices[i]];
        glm::vec3 v1 = mesh.Vertices[mesh.Indices[i + 1]];
        glm::vec3 v2 = mesh.Vertices[mesh.Indices[i + 2]];

        float t;
        if (RayIntersectsTriangle(localOrigin, localDir, v0, v1, v2, t)) {
            if (t < closestT && t > 0.0f) {
                closestT = t;
                hit = true;
            }
        }
    }
    return hit;
}
