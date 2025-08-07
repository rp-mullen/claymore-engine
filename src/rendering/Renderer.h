#pragma once
#include <bgfx/bgfx.h>
#include <glm/glm.hpp>
#include <memory>
#include "ecs/Scene.h"
#include "Camera.h"
#include "Mesh.h"
#include "ecs/Components.h"
#include "Material.h"
#include "DebugMaterial.h"

struct LightData {
    LightType type;
    glm::vec3 color;
    glm::vec3 position;
    glm::vec3 direction;
    float range;           // For point lights
    float constant;        // Attenuation constant
    float linear;          // Attenuation linear
    float quadratic;       // Attenuation quadratic
};



class Renderer {
public:
    static Renderer& Get() {
        static Renderer instance;
        return instance;
    }

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Initialization & lifecycle
    void Init(uint32_t width, uint32_t height, void* windowHandle);
    void Shutdown();
    void BeginFrame(float r, float g, float b);
    void EndFrame();
    void Resize(uint32_t width, uint32_t height);

    // Scene rendering
    void RenderScene(Scene& scene);

    // Mesh submission
    void DrawMesh(const Mesh& mesh, const float* transform, const Material& material, const struct MaterialPropertyBlock* propertyBlock = nullptr);

    // Camera
    Camera* GetCamera() const { 
       
       if (Scene::CurrentScene->m_IsPlaying) {
          Camera* cam = Scene::CurrentScene->GetActiveCamera();
          if (cam) {
             return cam;
             }
          }
       return m_RendererCamera; 
       }
    void SetCamera(Camera* cam) { m_RendererCamera = cam; }

    // Viewport info
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    bgfx::TextureHandle GetSceneTexture() const { return m_SceneTexture; }
    // Render any scene into a temporary texture (simple fallback to main scene texture for now)
    bgfx::TextureHandle RenderSceneToTexture(Scene* scene, uint32_t width, uint32_t height);

    // Debug utilities
	void InitGrid(float size, float step);
    void DrawGrid();
    void DrawDebugRay(const glm::vec3& origin, const glm::vec3& dir, float length = 10.0f);
    void DrawCollider(const ColliderComponent& collider, const TransformComponent& transform);

    void UploadLightsToShader(const std::vector<LightData>& lights);

    std::vector<glm::mat4> ComputeFinalBoneMatrices(Entity entity, Scene& scene);


private:
    Renderer() = default;
    ~Renderer() = default;

    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    Camera* m_RendererCamera = nullptr;

    bgfx::FrameBufferHandle m_SceneFrameBuffer = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_SceneTexture = BGFX_INVALID_HANDLE;

    float m_view[16]{};
    float m_proj[16]{};

    // Multi-light uniforms (support up to 4 lights)
    bgfx::UniformHandle u_LightColors = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_LightPositions = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_LightParams = BGFX_INVALID_HANDLE;
	    bgfx::UniformHandle u_cameraPos = BGFX_INVALID_HANDLE;


    bgfx::ProgramHandle m_DebugLineProgram = BGFX_INVALID_HANDLE;


    // Terrain rendering resources
    bgfx::ProgramHandle m_TerrainProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_TerrainHeightTexProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_TerrainHeightTexture = BGFX_INVALID_HANDLE;

    bgfx::VertexBufferHandle m_GridVB = BGFX_INVALID_HANDLE;
    uint32_t m_GridVertexCount = 0;


};
