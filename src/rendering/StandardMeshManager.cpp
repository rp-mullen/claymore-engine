#include "StandardMeshManager.h"
#include "VertexTypes.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdio>
#include "MaterialManager.h"

StandardMeshManager& StandardMeshManager::Instance() {
    static StandardMeshManager instance;
    return instance;
}

StandardMeshManager::~StandardMeshManager() {
    if (m_CubeMesh && m_CubeMesh->vbh.idx != bgfx::kInvalidHandle) bgfx::destroy(m_CubeMesh->vbh);
    if (m_CubeMesh && m_CubeMesh->ibh.idx != bgfx::kInvalidHandle) bgfx::destroy(m_CubeMesh->ibh);
    if (m_PlaneMesh && m_PlaneMesh->vbh.idx != bgfx::kInvalidHandle) bgfx::destroy(m_PlaneMesh->vbh);
    if (m_PlaneMesh && m_PlaneMesh->ibh.idx != bgfx::kInvalidHandle) bgfx::destroy(m_PlaneMesh->ibh);
    if (m_SphereMesh && m_SphereMesh->vbh.idx != bgfx::kInvalidHandle) bgfx::destroy(m_SphereMesh->vbh);
    if (m_SphereMesh && m_SphereMesh->ibh.idx != bgfx::kInvalidHandle) bgfx::destroy(m_SphereMesh->ibh);
}

std::shared_ptr<Mesh> StandardMeshManager::GetCubeMesh() {  
   if (!m_CubeMesh) CreateCubeMesh();  
   return std::shared_ptr<Mesh>(m_CubeMesh.release());  
}

Mesh& StandardMeshManager::GetPlaneMesh() {
    if (!m_PlaneMesh) CreatePlaneMesh();
    return *m_PlaneMesh;
}

Mesh& StandardMeshManager::GetSphereMesh() {
    if (!m_SphereMesh) CreateSphereMesh();
    return *m_SphereMesh;
}

void StandardMeshManager::CreateCubeMesh() {
    static PBRVertex cubeVertices[] = {
        // Front
        {-1,  1,  1,  0, 0, 1,  0, 0},
        { 1,  1,  1,  0, 0, 1,  1, 0},
        {-1, -1,  1,  0, 0, 1,  0, 1},
        { 1, -1,  1,  0, 0, 1,  1, 1},

        // Back
        {-1,  1, -1,  0, 0, -1, 0, 0},
        { 1,  1, -1,  0, 0, -1, 1, 0},
        {-1, -1, -1,  0, 0, -1, 0, 1},
        { 1, -1, -1,  0, 0, -1, 1, 1},

        // Left
        {-1,  1, -1, -1, 0, 0,  0, 0},
        {-1,  1,  1, -1, 0, 0,  1, 0},
        {-1, -1, -1, -1, 0, 0,  0, 1},
        {-1, -1,  1, -1, 0, 0,  1, 1},

        // Right
        { 1,  1,  1,  1, 0, 0,  0, 0},
        { 1,  1, -1,  1, 0, 0,  1, 0},
        { 1, -1,  1,  1, 0, 0,  0, 1},
        { 1, -1, -1,  1, 0, 0,  1, 1},

        // Top
        {-1,  1, -1,  0, 1, 0,  0, 0},
        { 1,  1, -1,  0, 1, 0,  1, 0},
        {-1,  1,  1,  0, 1, 0,  0, 1},
        { 1,  1,  1,  0, 1, 0,  1, 1},

        // Bottom
        {-1, -1,  1,  0, -1, 0, 0, 0},
        { 1, -1,  1,  0, -1, 0, 1, 0},
        {-1, -1, -1,  0, -1, 0, 0, 1},
        { 1, -1, -1,  0, -1, 0, 1, 1}
    };

    static const uint16_t cubeIndices[] = {
        0, 1, 2, 1, 3, 2,      // Front
        4, 6, 5, 5, 6, 7,      // Back
        8, 9,10, 9,11,10,      // Left
       12,13,14,13,15,14,      // Right
       16,17,18,17,19,18,      // Top
       20,21,22,21,23,22       // Bottom
    };


    m_CubeMesh = std::make_unique<Mesh>();

    m_CubeMesh->vbh = bgfx::createVertexBuffer(bgfx::makeRef(cubeVertices, sizeof(cubeVertices)), PBRVertex::layout);
    m_CubeMesh->ibh = bgfx::createIndexBuffer(bgfx::makeRef(cubeIndices, sizeof(cubeIndices)));

    // CPU-side storage for picking
    m_CubeMesh->Vertices.reserve(8);
    for (auto& v : cubeVertices) {
        m_CubeMesh->Vertices.push_back(glm::vec3(v.x, v.y, v.z));
    }

    size_t indexCount = sizeof(cubeIndices) / sizeof(uint16_t);
    m_CubeMesh->Indices.assign(cubeIndices, cubeIndices + indexCount);
    m_CubeMesh->numIndices = (uint32_t)indexCount;

    m_CubeMesh->ComputeBounds();
    printf("[StandardMeshManager] Cube Mesh created (PBR).\n");
}


void StandardMeshManager::CreatePlaneMesh() {
    // TODO: Implement similar to cube (flat quad)
}

void StandardMeshManager::CreateSphereMesh() {
    // TODO: Implement UV sphere or icosphere
}
