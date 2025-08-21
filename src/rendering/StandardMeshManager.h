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
    std::shared_ptr<Mesh> GetPlaneMesh();
    std::shared_ptr<Mesh> GetSphereMesh();
    std::shared_ptr<Mesh> GetCapsuleMesh();
    
    // Register primitive meshes with AssetLibrary
    void RegisterPrimitiveMeshes();

private:
    StandardMeshManager() = default;
    ~StandardMeshManager();

    void CreateCubeMesh();
    void CreatePlaneMesh();
    void CreateSphereMesh();
    void CreateCapsuleMesh();

    std::unique_ptr<Mesh> m_CubeMesh;
    std::unique_ptr<Mesh> m_PlaneMesh;
    std::unique_ptr<Mesh> m_SphereMesh;
    std::unique_ptr<Mesh> m_CapsuleMesh;
};
