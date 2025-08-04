#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "ecs/Scene.h"
#include "Camera.h"
#include "Mesh.h"

struct Ray {
    glm::vec3 Origin;
    glm::vec3 Direction;
};

class Picking {
public:
    struct PickRequest {
        float nx, ny; // Normalized coordinates
    };

    // Original API (unchanged)
    static Ray ScreenPointToRay(float nx, float ny, Camera* cam);
    static int PickEntity(float nx, float ny, Scene& scene, Camera* cam);

    // Additional API for queued picking
    static void QueuePick(float nx, float ny);
    static void Process(Scene& scene, Camera* cam);
    static int GetLastPick();

private:
    // Existing intersection methods (unchanged)
    static bool RayIntersectsAABB(const Ray& ray, const glm::vec3& min, const glm::vec3& max, float& t);
    static bool RayIntersectsOBB(const Ray& ray, const glm::mat4& transform, const glm::vec3& min, const glm::vec3& max, float& t);
    static bool RayIntersectsTriangle(const glm::vec3& origin, const glm::vec3& dir, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& t);
    static bool RayIntersectsMesh(const Ray& ray, const Mesh& mesh, const glm::mat4& transform, float& closestT);

    // Internal helpers
    static int PickEntityRay(const Ray& ray, Scene& scene);

    // Queue state
    static inline std::vector<PickRequest> s_PickQueue;
    static inline int s_LastPick = -1;
};
