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
    // Hook to allow external debug overlays like navigation to draw after scene
    void AddOverlayCallback(void(*fn)(uint16_t));
    void RemoveOverlayCallback(void(*fn)(uint16_t));

    void UploadLightsToShader(const std::vector<LightData>& lights);
    void UploadEnvironmentToShader(const Environment& env);

    std::vector<glm::mat4> ComputeFinalBoneMatrices(Entity entity, Scene& scene);

    // Editor helpers
    void DrawEntityOutline(Scene& scene, EntityID selectedEntity);

    // Debug draw toggles
    void SetShowGrid(bool v) { m_ShowGrid = v; }
    bool GetShowGrid() const { return m_ShowGrid; }
    void SetShowColliders(bool v) { m_ShowColliders = v; }
    bool GetShowColliders() const { return m_ShowColliders; }
    void SetShowAABBs(bool v) { m_ShowAABBs = v; }
    bool GetShowAABBs() const { return m_ShowAABBs; }

private:
    Renderer() = default;
    ~Renderer();

    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    Camera* m_RendererCamera = nullptr;

    bgfx::FrameBufferHandle m_SceneFrameBuffer = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_SceneTexture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_SceneDepthTexture = BGFX_INVALID_HANDLE;

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

    // Outline/mask resources
    bgfx::TextureHandle m_VisMaskTex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_OccMaskTex = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_VisMaskFB = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_OccMaskFB = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_TexelSize = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_OutlineColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_OutlineParams = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_SelectMaskProgram = BGFX_INVALID_HANDLE; // static mask (vs_pbr)
    bgfx::ProgramHandle m_SelectMaskProgramSkinned = BGFX_INVALID_HANDLE; // skinned mask (vs_pbr_skinned)
    bgfx::ProgramHandle m_OutlineCompositeProgram = BGFX_INVALID_HANDLE; // fullscreen (legacy masks)
    bgfx::UniformHandle s_MaskVis = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_MaskOcc = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_TintProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_TintColor = BGFX_INVALID_HANDLE;

    // New screen-space outline pipeline: ObjectID -> Edge -> Composite
    bgfx::TextureHandle m_ObjectIdTex = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_ObjectIdFB = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_EdgeMaskTex = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_EdgeMaskFB = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_ObjectIdProgram = BGFX_INVALID_HANDLE;            // vs_pbr + fs_object_id
    bgfx::ProgramHandle m_ObjectIdProgramSkinned = BGFX_INVALID_HANDLE;     // vs_pbr_skinned + fs_object_id
    bgfx::ProgramHandle m_OutlineEdgeProgram = BGFX_INVALID_HANDLE;         // vs_fullscreen + fs_outline_edge
    bgfx::ProgramHandle m_OutlineCompositeProgram2 = BGFX_INVALID_HANDLE;   // vs_fullscreen + fs_outline_composite
    bgfx::UniformHandle u_ObjectIdPacked = BGFX_INVALID_HANDLE;             // per-draw ID (packed into rgb)
    bgfx::UniformHandle u_SelectedIdPacked = BGFX_INVALID_HANDLE;           // selected entity id (packed)
    bgfx::UniformHandle s_ObjectId = BGFX_INVALID_HANDLE;                   // sampler for ObjectIdTex
    bgfx::UniformHandle s_EdgeMask = BGFX_INVALID_HANDLE;                   // sampler for EdgeMaskTex
    bgfx::UniformHandle s_SceneColor = BGFX_INVALID_HANDLE;                 // sampler for scene color in composite
 

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
    // Toggle: when false, use legacy geometry-scaled outline; when true, use screen-space mask+dilate
    
    // Outline parameters (editor defaults)
    float m_OutlineThicknessPx = 3.0f;
    glm::vec4 m_OutlineColor = glm::vec4(1.0f, 0.6f, 0.0f, 1.0f);
 
public:
    void SetShowUIOverlay(bool v){ m_ShowUIOverlay = v; }
    bool WasUIInputConsumedThisFrame() const { return m_UIInputConsumed; }
    void SetUIMode(bool enabled){ m_ShowUIOverlay = enabled; }
    void SetUIMousePosition(float x, float y, bool valid){ m_UIMouseX = x; m_UIMouseY = y; m_UIMouseValid = valid; }
    void SetOutlineThickness(float px){ m_OutlineThicknessPx = px; }
    void SetOutlineColor(const glm::vec4& color){ m_OutlineColor = color; }
 
 private:
    // Helper: draw world-space axis-aligned bounding box on a given view (default debug view 0)
    void DrawAABB(const glm::vec3& worldMin, const glm::vec3& worldMax, uint16_t viewId = 0);

    // Debug draw flags (editor)
    bool m_ShowGrid = true;
    bool m_ShowColliders = true;   // enabled by default per requirement
    bool m_ShowAABBs = false;
    std::vector<void(*)(uint16_t)> m_OverlayCallbacks;
};
