#include "StandardMeshManager.h"
#include "VertexTypes.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdio>
#include "MaterialManager.h"
#include "pipeline/AssetLibrary.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/scalar_constants.hpp>

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
   return std::shared_ptr<Mesh>(m_CubeMesh.get(), [](Mesh*) {}); // Non-owning shared_ptr
}

std::shared_ptr<Mesh> StandardMeshManager::GetPlaneMesh() {
    if (!m_PlaneMesh) CreatePlaneMesh();
    return std::shared_ptr<Mesh>(m_PlaneMesh.get(), [](Mesh*) {}); // Non-owning shared_ptr
}

std::shared_ptr<Mesh> StandardMeshManager::GetSphereMesh() {
    if (!m_SphereMesh) CreateSphereMesh();
    return std::shared_ptr<Mesh>(m_SphereMesh.get(), [](Mesh*) {}); // Non-owning shared_ptr
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
    static PBRVertex planeVertices[] = {
        // Front face (facing +Z)
        {-1,  1,  0,  0, 0, 1,  0, 0},  // Top-left
        { 1,  1,  0,  0, 0, 1,  1, 0},  // Top-right
        {-1, -1,  0,  0, 0, 1,  0, 1},  // Bottom-left
        { 1, -1,  0,  0, 0, 1,  1, 1}   // Bottom-right
    };

    static const uint16_t planeIndices[] = {
        0, 1, 2, 1, 3, 2  // Two triangles forming a quad
    };

    m_PlaneMesh = std::make_unique<Mesh>();

    m_PlaneMesh->vbh = bgfx::createVertexBuffer(bgfx::makeRef(planeVertices, sizeof(planeVertices)), PBRVertex::layout);
    m_PlaneMesh->ibh = bgfx::createIndexBuffer(bgfx::makeRef(planeIndices, sizeof(planeIndices)));

    // CPU-side storage for picking
    m_PlaneMesh->Vertices.reserve(4);
    for (auto& v : planeVertices) {
        m_PlaneMesh->Vertices.push_back(glm::vec3(v.x, v.y, v.z));
    }

    size_t indexCount = sizeof(planeIndices) / sizeof(uint16_t);
    m_PlaneMesh->Indices.assign(planeIndices, planeIndices + indexCount);
    m_PlaneMesh->numIndices = (uint32_t)indexCount;

    m_PlaneMesh->ComputeBounds();
    printf("[StandardMeshManager] Plane Mesh created (PBR).\n");
}

void StandardMeshManager::CreateSphereMesh() {
    const int segments = 32;  // Horizontal segments
    const int rings = 16;     // Vertical rings
    const float radius = 1.0f;
    
    std::vector<PBRVertex> sphereVertices;
    std::vector<uint16_t> sphereIndices;
    
    // Generate vertices
    for (int ring = 0; ring <= rings; ++ring) {
        float phi = (float)ring / rings * glm::pi<float>();
        float y = radius * cos(phi);
        float ringRadius = radius * sin(phi);
        
        for (int segment = 0; segment <= segments; ++segment) {
            float theta = (float)segment / segments * 2.0f * glm::pi<float>();
            float x = ringRadius * cos(theta);
            float z = ringRadius * sin(theta);
            
            // Position
            float px = x;
            float py = y;
            float pz = z;
            
            // Normal (normalized position for unit sphere)
            float nx = x / radius;
            float ny = y / radius;
            float nz = z / radius;
            
            // UV coordinates
            float u = (float)segment / segments;
            float v = (float)ring / rings;
            
            sphereVertices.push_back({px, py, pz, nx, ny, nz, u, v});
        }
    }
    
    // Generate indices
    for (int ring = 0; ring < rings; ++ring) {
        for (int segment = 0; segment < segments; ++segment) {
            uint16_t current = ring * (segments + 1) + segment;
            uint16_t next = current + segments + 1;
            
            // First triangle
            sphereIndices.push_back(current);
            sphereIndices.push_back(next);
            sphereIndices.push_back(current + 1);
            
            // Second triangle
            sphereIndices.push_back(next);
            sphereIndices.push_back(next + 1);
            sphereIndices.push_back(current + 1);
        }
    }
    
    m_SphereMesh = std::make_unique<Mesh>();
    
    m_SphereMesh->vbh = bgfx::createVertexBuffer(
        bgfx::makeRef(sphereVertices.data(), sphereVertices.size() * sizeof(PBRVertex)), 
        PBRVertex::layout
    );
    m_SphereMesh->ibh = bgfx::createIndexBuffer(
        bgfx::makeRef(sphereIndices.data(), sphereIndices.size() * sizeof(uint16_t))
    );
    
    // CPU-side storage for picking
    m_SphereMesh->Vertices.reserve(sphereVertices.size());
    for (auto& v : sphereVertices) {
        m_SphereMesh->Vertices.push_back(glm::vec3(v.x, v.y, v.z));
    }
    
    m_SphereMesh->Indices = sphereIndices;
    m_SphereMesh->numIndices = (uint32_t)sphereIndices.size();
    
    m_SphereMesh->ComputeBounds();
    printf("[StandardMeshManager] Sphere Mesh created (PBR) - %zu vertices, %zu indices.\n", 
           sphereVertices.size(), sphereIndices.size());
}

void StandardMeshManager::RegisterPrimitiveMeshes() {
    // Register primitive meshes with AssetLibrary
    // Use special GUIDs for primitives
    static const ClaymoreGUID PRIMITIVE_GUID = ClaymoreGUID::FromString("00000000000000000000000000000001");
    
    // Register Cube primitive
    AssetReference cubeRef(PRIMITIVE_GUID, 0, static_cast<int32_t>(AssetType::Mesh));
    AssetLibrary::Instance().RegisterAsset(cubeRef, AssetType::Mesh, "", "Cube");
    
    // Register Sphere primitive
    AssetReference sphereRef(PRIMITIVE_GUID, 1, static_cast<int32_t>(AssetType::Mesh));
    AssetLibrary::Instance().RegisterAsset(sphereRef, AssetType::Mesh, "", "Sphere");
    
    // Register Plane primitive
    AssetReference planeRef(PRIMITIVE_GUID, 2, static_cast<int32_t>(AssetType::Mesh));
    AssetLibrary::Instance().RegisterAsset(planeRef, AssetType::Mesh, "", "Plane");
    
    std::cout << "[StandardMeshManager] Registered primitive meshes with AssetLibrary" << std::endl;
}
