#pragma once
#include <bgfx/bgfx.h>
#include <memory>
#include <unordered_map>
#include <string>
#include "Mesh.h"

class StandardMeshManager {
public:
    static StandardMeshManager& Instance();

    // Accessors for standard meshes
    std::shared_ptr<Mesh> GetCubeMesh();
    Mesh& GetPlaneMesh();
    Mesh& GetSphereMesh();

private:
    StandardMeshManager() = default;
    ~StandardMeshManager();

    void CreateCubeMesh();
    void CreatePlaneMesh();
    void CreateSphereMesh();

    std::unique_ptr<Mesh> m_CubeMesh;
    std::unique_ptr<Mesh> m_PlaneMesh;
    std::unique_ptr<Mesh> m_SphereMesh;
};
