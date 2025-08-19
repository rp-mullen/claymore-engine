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
#include "TextRenderer.h"

// TextRenderer is used via unique_ptr; include full type to avoid incomplete-type destructor issues

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
    void SetRenderToOffscreen(bool enable) { m_RenderToOffscreen = enable; }

    // Scene rendering
    void RenderScene(Scene& scene);
    void RenderScene(Scene& scene, uint16_t viewId);

    // Mesh submission
    void DrawMesh(const Mesh& mesh, const float* transform, const Material& material, const struct MaterialPropertyBlock* propertyBlock = nullptr);
    void DrawMesh(const Mesh& mesh, const float* transform, const Material& material, uint16_t viewId, const struct MaterialPropertyBlock* propertyBlock = nullptr);

    // Camera
    Camera* GetCamera() const {
        // Prefer the scene's active camera whenever available (both in editor play and standalone)
        if (Scene::CurrentScene) {
            Camera* cam = Scene::CurrentScene->GetActiveCamera();
            if (cam) return cam;
        }
        return m_RendererCamera;
    }
    void SetCamera(Camera* cam) { m_RendererCamera = cam; }

    // Viewport info
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    bgfx::TextureHandle GetSceneTexture() const { return m_SceneTexture; }
    // Render any scene into a temporary texture using a dedicated offscreen view.
    // The provided camera is used for this render only and does not affect the main viewport.
    bgfx::TextureHandle RenderSceneToTexture(Scene* scene, uint32_t width, uint32_t height, class Camera* camera);

    // Debug utilities
	void InitGrid(float size, float step);
    void DrawGrid();
    void DrawGrid(uint16_t viewId);
    void DrawDebugRay(const glm::vec3& origin, const glm::vec3& dir, float length = 10.0f);
    void DrawCollider(const ColliderComponent& collider, const TransformComponent& transform);

    void UploadLightsToShader(const std::vector<LightData>& lights);
    void UploadEnvironmentToShader(const Environment& env);

    std::vector<glm::mat4> ComputeFinalBoneMatrices(Entity entity, Scene& scene);

    // Editor helpers
    void DrawEntityOutline(Scene& scene, EntityID selectedEntity);


private:
    Renderer() = default;
    ~Renderer();

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
    bgfx::UniformHandle u_AmbientFog   = BGFX_INVALID_HANDLE; // xyz=color/intensity, w=flags
    bgfx::UniformHandle u_FogParams    = BGFX_INVALID_HANDLE; // x=fogDensity, y=unused
    // u_SkyParams: x = proceduralSky (1.0 or 0.0)
    bgfx::UniformHandle u_SkyParams    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_SkyZenith    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_SkyHorizon   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_normalMat    = BGFX_INVALID_HANDLE; // CPU-provided normal matrix


    bgfx::ProgramHandle m_DebugLineProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_OutlineProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_outlineColor = BGFX_INVALID_HANDLE;


    // Terrain rendering resources
    bgfx::ProgramHandle m_TerrainProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_TerrainHeightTexProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_TerrainHeightTexture = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_SkyProgram = BGFX_INVALID_HANDLE;

    bgfx::VertexBufferHandle m_GridVB = BGFX_INVALID_HANDLE;
    uint32_t m_GridVertexCount = 0;

    // Text rendering state
    std::unique_ptr<TextRenderer> m_TextRenderer;

    // UI rendering
    bgfx::UniformHandle m_UISampler = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_UIProgram = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_UIWhiteTex = BGFX_INVALID_HANDLE;
    bool m_ShowUIOverlay = true;
    bool m_UIInputConsumed = false;
    // Viewport-reported mouse position in scene framebuffer space (pixels)
    float m_UIMouseX = 0.0f;
    float m_UIMouseY = 0.0f;
    bool  m_UIMouseValid = false;

    // When true, views 0/1/2 render to the offscreen scene framebuffer; otherwise, they render directly to the backbuffer
    bool m_RenderToOffscreen = true;

public:
    void SetShowUIOverlay(bool v){ m_ShowUIOverlay = v; }
    bool WasUIInputConsumedThisFrame() const { return m_UIInputConsumed; }
    void SetUIMode(bool enabled){ m_ShowUIOverlay = enabled; }
    void SetUIMousePosition(float x, float y, bool valid){ m_UIMouseX = x; m_UIMouseY = y; m_UIMouseValid = valid; }

 
};
