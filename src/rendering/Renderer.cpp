#include "Renderer.h"
#include <bgfx/platform.h>
#include <glm/gtc/type_ptr.hpp>
#include "ShaderManager.h"
#include "Picking.h"
#include "MaterialManager.h"
#include "VertexTypes.h"
#include "ecs/ParticleEmitterSystem.h"
#include <bx/math.h>
#include "rendering/MaterialPropertyBlock.h"
#include "rendering/SkinnedPBRMaterial.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <cstring>
#include "ecs/Components.h"
#include "Environment.h"

#include "TextRenderer.h"
#include "pipeline/AssetLibrary.h"
#include "editor/Input.h"

#include <core/application.h>
#include "Terrain.h"
#include <limits>


// ---------------- Initialization ----------------
void Renderer::Init(uint32_t width, uint32_t height, void* windowHandle) {

   // Set viewport size
   m_Width = width;
   m_Height = height;

   // Initialize bgfx with the provided window handle
   bgfx::Init init;
   init.platformData.nwh = windowHandle;
   init.type = bgfx::RendererType::Count;
   init.resolution.width = width;
   init.resolution.height = height;
   init.resolution.reset = BGFX_RESET_VSYNC;
   bgfx::init(init);
   bgfx::setDebug(BGFX_DEBUG_TEXT);

   // Set default camera
   m_RendererCamera = new Camera(60.0f, float(width) / float(height), 0.1f, 100.0f);

   // Set up Render Texture
   const uint64_t texFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
   m_SceneTexture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::BGRA8, texFlags);

   // Set up depth texture
   m_SceneDepthTexture = bgfx::createTexture2D(
      width, height, false, 1,
      bgfx::TextureFormat::D24S8, // Or D32F for float depth
      BGFX_TEXTURE_RT_WRITE_ONLY
   );

   // Create the offscreen framebuffer with color and depth textures
   bgfx::TextureHandle fbTextures[] = { m_SceneTexture, m_SceneDepthTexture };
   m_SceneFrameBuffer = bgfx::createFrameBuffer(2, fbTextures, true);

   // In editor mode, render to the offscreen framebuffer
   // In standalone mode, render directly to the backbuffer
   if (m_RenderToOffscreen) {
      bgfx::setViewFrameBuffer(0, m_SceneFrameBuffer);
      }
   else {
      bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);
      }

   // Default clear
   bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff);
   bgfx::setViewClear(1, 0);


   // Initialize vertex layouts globally
   PBRVertex::Init();
   GridVertex::Init();
   TerrainVertex::Init();
   ParticleVertex::Init();
   UIVertex::Init();

   // Debug line program
   m_DebugLineProgram = ShaderManager::Instance().LoadProgram("vs_debug", "fs_debug");

   // Outline program (fullscreen composite)
   m_OutlineProgram = ShaderManager::Instance().LoadProgram("vs_outline", "fs_outline");
   u_outlineColor = bgfx::createUniform("u_outlineColor", bgfx::UniformType::Vec4);
   // Selection pipeline resources
   m_SelectMaskProgram = ShaderManager::Instance().LoadProgram("vs_pbr", "fs_select_mask");
   m_SelectMaskProgramSkinned = ShaderManager::Instance().LoadProgram("vs_pbr_skinned", "fs_select_mask");
   m_OutlineCompositeProgram = ShaderManager::Instance().LoadProgram("vs_fullscreen", "fs_outline");
   if (!bgfx::isValid(u_TexelSize)) u_TexelSize = bgfx::createUniform("uTexelSize", bgfx::UniformType::Vec4);
   if (!bgfx::isValid(u_OutlineColor)) u_OutlineColor = bgfx::createUniform("uColor", bgfx::UniformType::Vec4);
   if (!bgfx::isValid(u_OutlineParams)) u_OutlineParams = bgfx::createUniform("uParams", bgfx::UniformType::Vec4);
   if (!bgfx::isValid(s_MaskVis)) s_MaskVis = bgfx::createUniform("sMaskVis", bgfx::UniformType::Sampler);
   if (!bgfx::isValid(s_MaskOcc)) s_MaskOcc = bgfx::createUniform("sMaskOcc", bgfx::UniformType::Sampler);
   m_TintProgram = ShaderManager::Instance().LoadProgram("vs_pbr", "fs_tint");
   if (!bgfx::isValid(u_TintColor)) u_TintColor = bgfx::createUniform("uTintColor", bgfx::UniformType::Vec4);

   // New screen-space outline programs and uniforms
   m_ObjectIdProgram        = ShaderManager::Instance().LoadProgram("vs_pbr", "fs_object_id");
   m_ObjectIdProgramSkinned = ShaderManager::Instance().LoadProgram("vs_pbr_skinned", "fs_object_id");
   m_OutlineEdgeProgram     = ShaderManager::Instance().LoadProgram("vs_fullscreen", "fs_outline_edge");
   m_OutlineCompositeProgram2 = ShaderManager::Instance().LoadProgram("vs_fullscreen", "fs_outline_composite");
   if (!bgfx::isValid(u_ObjectIdPacked)) u_ObjectIdPacked = bgfx::createUniform("uObjectId", bgfx::UniformType::Vec4);
   if (!bgfx::isValid(u_SelectedIdPacked)) u_SelectedIdPacked = bgfx::createUniform("uSelectedId", bgfx::UniformType::Vec4);
   if (!bgfx::isValid(s_ObjectId)) s_ObjectId = bgfx::createUniform("sObjectId", bgfx::UniformType::Sampler);
   if (!bgfx::isValid(s_EdgeMask)) s_EdgeMask = bgfx::createUniform("sEdgeMask", bgfx::UniformType::Sampler);
   if (!bgfx::isValid(s_SceneColor)) s_SceneColor = bgfx::createUniform("sSceneColor", bgfx::UniformType::Sampler);

   // Create selection mask targets
   const uint64_t rFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
   m_VisMaskTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::BGRA8, rFlags);
   m_OccMaskTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::BGRA8, rFlags);
   {
      bgfx::TextureHandle visAttachments[] = { m_VisMaskTex, m_SceneDepthTexture };
      // Do not destroy attached textures; depth is shared
      m_VisMaskFB = bgfx::createFrameBuffer(2, visAttachments, false);
   }
   m_OccMaskFB = bgfx::createFrameBuffer(1, &m_OccMaskTex, true);

   // Create ObjectID and Edge mask render targets
   {
      const uint64_t idFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
      m_ObjectIdTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::BGRA8, idFlags);
      bgfx::TextureHandle idAttachments[] = { m_ObjectIdTex, m_SceneDepthTexture };
      // Don't destroy depth; it's shared with the scene framebuffer
      m_ObjectIdFB = bgfx::createFrameBuffer(2, idAttachments, false);
   }
   {
      const uint64_t edgeFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
      m_EdgeMaskTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::R8, edgeFlags);
      m_EdgeMaskFB = bgfx::createFrameBuffer(1, &m_EdgeMaskTex, true);
   }
   InitGrid(20.0f, 1.0f);

   // Create uniforms for lighting and environment
   u_LightColors = bgfx::createUniform("u_lightColors", bgfx::UniformType::Vec4, 4);
   u_LightPositions = bgfx::createUniform("u_lightPositions", bgfx::UniformType::Vec4, 4);
   u_LightParams = bgfx::createUniform("u_lightParams", bgfx::UniformType::Vec4, 4);
   u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);

   // CPU-provided normal matrix for skinned and static meshes
   u_normalMat = bgfx::createUniform("u_normalMat", bgfx::UniformType::Mat4);
   u_AmbientFog = bgfx::createUniform("u_ambientFog", bgfx::UniformType::Vec4);
   u_FogParams = bgfx::createUniform("u_fogParams", bgfx::UniformType::Vec4);
   u_SkyParams = bgfx::createUniform("u_skyParams", bgfx::UniformType::Vec4);
   u_SkyZenith = bgfx::createUniform("u_skyZenith", bgfx::UniformType::Vec4);
   u_SkyHorizon = bgfx::createUniform("u_skyHorizon", bgfx::UniformType::Vec4);


   // Terrain resources
   m_TerrainProgram = ShaderManager::Instance().LoadProgram("vs_pbr", "fs_pbr");
   m_TerrainHeightTexProgram = ShaderManager::Instance().LoadProgram("vs_terrain_height_texture", "fs_terrain");
   s_TerrainHeightTexture = bgfx::createUniform("s_heightTexture", bgfx::UniformType::Sampler);

   // Procedural sky program (fullscreen triangle)
   m_SkyProgram = ShaderManager::Instance().LoadProgram("vs_sky", "fs_sky");

   // Initialize text renderer (self-contained, stb-based)
   m_TextRenderer = std::make_unique<TextRenderer>();
   // Use dedicated text shaders that sample the atlas alpha
   bgfx::ProgramHandle fontProgram = ShaderManager::Instance().LoadProgram("vs_text", "fs_text");
   if (!m_TextRenderer->Init("assets/fonts/Roboto-Regular.ttf", fontProgram, 512, 512, 48.0f)) {
      std::cerr << "[Renderer] Failed to initialize TextRenderer (font bake). Continuing without text." << std::endl;
      }

   // UI rendering init
   m_UIProgram = ShaderManager::Instance().LoadProgram("vs_ui", "fs_ui");
   if (!bgfx::isValid(m_UISampler)) m_UISampler = bgfx::createUniform("s_uiTex", bgfx::UniformType::Sampler);
   if (!bgfx::isValid(m_UIProgram)) {
      std::cerr << "[Renderer] UI shader program invalid; UI overlay disabled." << std::endl;
      m_ShowUIOverlay = false;
      }
   // Fallback white texture for panels without texture
   try {
      m_UIWhiteTex = TextureLoader::Load2D("assets/debug/white.png");
      }
   catch (const std::exception& e) {
      std::cerr << "[Renderer] Failed to load UI white texture: " << e.what() << std::endl;
      m_UIWhiteTex.idx = bgfx::kInvalidHandle;
      }
   }


Renderer::~Renderer() {
   m_TextRenderer.reset();
   }


void Renderer::Shutdown() {
   if (bgfx::isValid(m_DebugLineProgram)) bgfx::destroy(m_DebugLineProgram);
   bgfx::shutdown();
   }

// ---------------- Frame Lifecycle ----------------
void Renderer::BeginFrame(float r, float g, float b) {

   // Debug View (0)
   bgfx::setViewRect(0, 0, 0, m_Width, m_Height);
   bgfx::setViewTransform(0, m_view, m_proj);
   if (m_RenderToOffscreen) {
      bgfx::setViewFrameBuffer(0, m_SceneFrameBuffer);
      }
   else {
      bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);
      }
   bgfx::touch(0);

   // Mesh view (1)
   bgfx::setViewRect(1, 0, 0, m_Width, m_Height);
   bgfx::setViewTransform(1, m_view, m_proj);
   if (m_RenderToOffscreen) {
      bgfx::setViewFrameBuffer(1, m_SceneFrameBuffer);
      }
   else {
      bgfx::setViewFrameBuffer(1, BGFX_INVALID_HANDLE);
      }
   bgfx::touch(1);

   // Screen-space UI/Text view (2) on the same framebuffer, rendered after 0/1
   bgfx::setViewRect(2, 0, 0, m_Width, m_Height);
   if (m_RenderToOffscreen) {
      bgfx::setViewFrameBuffer(2, m_SceneFrameBuffer);
      }
   else {
      bgfx::setViewFrameBuffer(2, BGFX_INVALID_HANDLE);
      }
   bgfx::touch(2);
   }


void Renderer::EndFrame() {
   bgfx::frame();
   }

void Renderer::Resize(uint32_t width, uint32_t height) {
   m_Width = width;
   m_Height = height;

   m_RendererCamera->SetViewportSize((float)width, (float)height);


   if (bgfx::isValid(m_SceneFrameBuffer)) {
      bgfx::destroy(m_SceneFrameBuffer);
      bgfx::destroy(m_SceneTexture);
   }
   const uint64_t texFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
   m_SceneTexture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::BGRA8, texFlags);
   if (bgfx::isValid(m_SceneDepthTexture)) {
      bgfx::destroy(m_SceneDepthTexture);
   }
   m_SceneDepthTexture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY);
   {
      bgfx::TextureHandle fbTex[] = { m_SceneTexture, m_SceneDepthTexture };
      m_SceneFrameBuffer = bgfx::createFrameBuffer(2, fbTex, true);
   }
   bgfx::setViewFrameBuffer(0, m_SceneFrameBuffer);

   // Recreate selection mask RTs
   if (bgfx::isValid(m_VisMaskFB)) { bgfx::destroy(m_VisMaskFB); m_VisMaskFB = BGFX_INVALID_HANDLE; }
   if (bgfx::isValid(m_OccMaskFB)) { bgfx::destroy(m_OccMaskFB); m_OccMaskFB = BGFX_INVALID_HANDLE; }
   if (bgfx::isValid(m_VisMaskTex)) { bgfx::destroy(m_VisMaskTex); m_VisMaskTex = BGFX_INVALID_HANDLE; }
   if (bgfx::isValid(m_OccMaskTex)) { bgfx::destroy(m_OccMaskTex); m_OccMaskTex = BGFX_INVALID_HANDLE; }
   const uint64_t rFlags2 = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
   m_VisMaskTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::BGRA8, rFlags2);
   m_OccMaskTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::BGRA8, rFlags2);
   {
      bgfx::TextureHandle visAttachments[] = { m_VisMaskTex, m_SceneDepthTexture };
      m_VisMaskFB = bgfx::createFrameBuffer(2, visAttachments, false);
   }
   m_OccMaskFB = bgfx::createFrameBuffer(1, &m_OccMaskTex, true);

   // Recreate ObjectID and Edge mask RTs
   if (bgfx::isValid(m_ObjectIdFB)) { bgfx::destroy(m_ObjectIdFB); m_ObjectIdFB = BGFX_INVALID_HANDLE; }
   if (bgfx::isValid(m_EdgeMaskFB)) { bgfx::destroy(m_EdgeMaskFB); m_EdgeMaskFB = BGFX_INVALID_HANDLE; }
   if (bgfx::isValid(m_ObjectIdTex)) { bgfx::destroy(m_ObjectIdTex); m_ObjectIdTex = BGFX_INVALID_HANDLE; }
   if (bgfx::isValid(m_EdgeMaskTex)) { bgfx::destroy(m_EdgeMaskTex); m_EdgeMaskTex = BGFX_INVALID_HANDLE; }
   {
      const uint64_t idFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
      m_ObjectIdTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::BGRA8, idFlags);
      bgfx::TextureHandle idAttachments[] = { m_ObjectIdTex, m_SceneDepthTexture };
      m_ObjectIdFB = bgfx::createFrameBuffer(2, idAttachments, false);
   }
   {
      const uint64_t edgeFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
      m_EdgeMaskTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::R8, edgeFlags);
      m_EdgeMaskFB = bgfx::createFrameBuffer(1, &m_EdgeMaskTex, true);
   }
}

// ---------------- Scene Rendering ----------------
void Renderer::RenderScene(Scene& scene) {

   // Prepare camera matrices
   Camera* activeCamera = GetCamera();
   glm::mat4 view = activeCamera->GetViewMatrix();
   glm::mat4 proj = activeCamera->GetProjectionMatrix();
   memcpy(m_view, glm::value_ptr(view), sizeof(float) * 16);
   memcpy(m_proj, glm::value_ptr(proj), sizeof(float) * 16);

   bgfx::setViewTransform(0, m_view, m_proj);
   bgfx::setViewTransform(1, m_view, m_proj);
   
   // Also update the preview view (210) when used by offscreen renders
   bgfx::setViewTransform(210, m_view, m_proj);
   
   // Ensure views are touched so other view state changes don't clear them unexpectedly
   bgfx::touch(0);
   bgfx::touch(1);
   bgfx::touch(210);
   bgfx::touch(2);

   glm::vec4 camPos(activeCamera->GetPosition(), 1.0f);
   bgfx::setUniform(u_cameraPos, &camPos);

   // Upload environment
   UploadEnvironmentToShader(scene.GetEnvironment());

   // --------------------------------------
   // Procedural Sky Pass
   // --------------------------------------
   const Environment& envForSky = scene.GetEnvironment();
   if (envForSky.ProceduralSky) {
      // Fullscreen triangle for background on view 0
      float id[16]; bx::mtxIdentity(id);
      bgfx::setTransform(id);

      // No geometry buffers; use screen-space triangle trick via transient buffers
      
      struct PosCoord { 
         float x, y, z; 
         };
      
      const PosCoord verts[3] = { 
            {-1.0f,-1.0f,0.0f}, 
            {3.0f,-1.0f,0.0f}, 
            {-1.0f,3.0f,0.0f} 
         };
      const uint16_t idx[3] = { 0,1,2 };
      
      bgfx::TransientVertexBuffer tvb; bgfx::TransientIndexBuffer tib;
      bgfx::VertexLayout layout; layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
      
      const uint32_t needVerts = 3u;
      const uint32_t needIdx = 3u;
      const uint32_t availVerts = bgfx::getAvailTransientVertexBuffer(needVerts, layout);
      const uint32_t availIdx = bgfx::getAvailTransientIndexBuffer(needIdx);
      if (availVerts >= needVerts && availIdx >= needIdx) {
         bgfx::allocTransientVertexBuffer(&tvb, needVerts, layout);
         bgfx::allocTransientIndexBuffer(&tib, needIdx);
         memcpy(tvb.data, verts, sizeof(verts));
         memcpy(tib.data, idx, sizeof(idx));
         bgfx::setVertexBuffer(0, &tvb);
         bgfx::setIndexBuffer(&tib);
         bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_ALWAYS);
         bgfx::submit(0, m_SkyProgram);
         }
      }

   // --------------------------------------
   // Collect lights from ECS
   // --------------------------------------
   std::vector<LightData> lights;
   lights.reserve(4); // Max 4 lights for now
   for (auto& entity : scene.GetEntities()) {
      auto* data = scene.GetEntityData(entity.GetID());
      if (!data || !data->Light || !data->Visible) continue;

      LightData ld;
      ld.type = data->Light->Type;
      ld.color = data->Light->Color * data->Light->Intensity;
      ld.position = data->Transform.Position;

      // Compute direction for directional lights
      if (data->Light->Type == LightType::Directional) {
         float yaw = glm::radians(data->Transform.Rotation.y);
         float pitch = glm::radians(data->Transform.Rotation.x);
         ld.direction = glm::normalize(glm::vec3(
            cos(pitch) * sin(yaw),
            sin(pitch),
            cos(pitch) * cos(yaw)
         ));
         // Directional lights don't need attenuation parameters
         ld.range = 0.0f;
         ld.constant = 1.0f;
         ld.linear = 0.0f;
         ld.quadratic = 0.0f;
         }
      else {
         ld.direction = glm::vec3(0.0f); // Not used for point lights
         // Default point light parameters
         ld.range = 50.0f;  // Default range
         ld.constant = 1.0f;
         ld.linear = 0.09f;
         ld.quadratic = 0.032f;
         }

      lights.push_back(ld);
      if (lights.size() == 4) break; // Hard limit
      }

   if (Application::Get().m_RunEditorUI && m_ShowGrid) {
      DrawGrid();
      }

   // Upload light data to shaders
   UploadLightsToShader(lights);

   // --------------------------------------
   // Draw all meshes
   // --------------------------------------
   // Take a snapshot of entity IDs to avoid iterator invalidation during deletions
   std::vector<EntityID> entityIds;
   entityIds.reserve(scene.GetEntities().size());
   for (const auto& eSnap : scene.GetEntities()) entityIds.push_back(eSnap.GetID());

   for (EntityID eid : entityIds) {
      auto* data = scene.GetEntityData(eid);
      if (!data || !data->Visible || !data->Mesh || !data->Mesh->mesh) continue;

      // Hold a local strong ref to guard against concurrent resets
      std::shared_ptr<Mesh> meshPtr = data->Mesh->mesh; // local strong ref
      if (!meshPtr) continue;

      bool meshValid = meshPtr->Dynamic ? bgfx::isValid(meshPtr->dvbh) : bgfx::isValid(meshPtr->vbh);
      if (!meshValid || !bgfx::isValid(meshPtr->ibh)) {
         std::cerr << "Invalid mesh for entity " << eid << "\n";
         continue;
         }

      float transform[16];
      memcpy(transform, glm::value_ptr(data->Transform.WorldMatrix), sizeof(float) * 16);


      DrawMesh(*meshPtr.get(), transform, *data->Mesh->material, &data->Mesh->PropertyBlock);
      }

   // --------------------------------------
   // Draw all terrains
   // --------------------------------------
   for (auto& entity : scene.GetEntities())
      {
      auto* data = scene.GetEntityData(entity.GetID());
      if (!data || !data->Visible || !data->Terrain) continue;

      TerrainComponent& terrain = *data->Terrain;

      if (terrain.Dirty)
         {
         Terrain::UpdateTerrainBuffers(terrain);
         terrain.Dirty = false;
         }

      float transform[16];
      memcpy(transform, glm::value_ptr(data->Transform.WorldMatrix), sizeof(float) * 16);
      bgfx::setTransform(transform);

      switch (terrain.Mode)
         {
            case 0:
            default:
               if (bgfx::isValid(terrain.vbh) && bgfx::isValid(terrain.ibh))
                  {
                  bgfx::setVertexBuffer(0, terrain.vbh);
                  bgfx::setIndexBuffer(terrain.ibh);
                  bgfx::setState(BGFX_STATE_DEFAULT);
                  bgfx::submit(1, m_TerrainProgram);
                  }
               break;
            case 1:
               if (bgfx::isValid(terrain.dvbh) && bgfx::isValid(terrain.dibh))
                  {
                  bgfx::setVertexBuffer(0, terrain.dvbh);
                  bgfx::setIndexBuffer(terrain.dibh);
                  bgfx::setState(BGFX_STATE_DEFAULT);
                  bgfx::submit(1, m_TerrainProgram);
                  }
               break;
            case 2:
               if (bgfx::isValid(terrain.vbh) && bgfx::isValid(terrain.ibh) && bgfx::isValid(terrain.HeightTexture))
                  {
                  bgfx::setVertexBuffer(0, terrain.vbh);
                  bgfx::setIndexBuffer(terrain.ibh);
                  bgfx::setTexture(0, s_TerrainHeightTexture, terrain.HeightTexture);
                  bgfx::setState(BGFX_STATE_DEFAULT);
                  bgfx::submit(1, m_TerrainHeightTexProgram);
                  }
               break;
         }
      }

   // --------------------------------------
   // Draw particle emitters (new system)
   // --------------------------------------
   {
   bx::Vec3 eye = { camPos.x, camPos.y, camPos.z };
   ecs::ParticleEmitterSystem::Get().Render(1, m_view, eye);
   }

   // --------------------------------------
   // Draw colliders in editor mode
   // --------------------------------------
   if (!scene.m_IsPlaying) {
      // Colliders
      if (m_ShowColliders) {
         for (auto& entity : scene.GetEntities()) {
            auto* data = scene.GetEntityData(entity.GetID());
            if (!data || !data->Visible || !data->Collider) continue;
            DrawCollider(*data->Collider, data->Transform);
            }
         }

      // Picking AABBs (world-space) around meshes
      if (m_ShowAABBs) {
         for (auto& entity : scene.GetEntities()) {
            auto* data = scene.GetEntityData(entity.GetID());
            if (!data || !data->Visible || !data->Mesh || !data->Mesh->mesh) continue;
            std::shared_ptr<Mesh> meshPtr = data->Mesh->mesh;
            if (!meshPtr) continue;
            // Transform local AABB to world-space by transforming the 8 corners and recomputing min/max
            const glm::vec3 lmin = meshPtr->BoundsMin;
            const glm::vec3 lmax = meshPtr->BoundsMax;
            glm::vec3 corners[8] = {
               {lmin.x, lmin.y, lmin.z}, {lmax.x, lmin.y, lmin.z}, {lmin.x, lmax.y, lmin.z}, {lmax.x, lmax.y, lmin.z},
               {lmin.x, lmin.y, lmax.z}, {lmax.x, lmin.y, lmax.z}, {lmin.x, lmax.y, lmax.z}, {lmax.x, lmax.y, lmax.z}
            };
            glm::vec3 wmin( std::numeric_limits<float>::max());
            glm::vec3 wmax(-std::numeric_limits<float>::max());
            for (int i = 0; i < 8; ++i) {
               glm::vec3 w = glm::vec3(data->Transform.WorldMatrix * glm::vec4(corners[i], 1.0f));
               wmin = glm::min(wmin, w);
               wmax = glm::max(wmax, w);
            }
            DrawAABB(wmin, wmax, 0);
         }
      }
   }

   // --------------------------------------
   // Draw text components (world or screen space)
   // --------------------------------------
   if (m_TextRenderer) {
      // worldViewId=1, screenViewId=2 to layer correctly
      m_TextRenderer->RenderTexts(scene, m_view, m_proj, m_Width, m_Height, 1, 2);
      }

   // --------------------------------------
   // UI Rendering (Canvas/Panel/Button)
   // --------------------------------------
   if (m_ShowUIOverlay && bgfx::isValid(m_UIProgram)) {
      // Setup orthographic projection for view 2 (top-left origin)
      const bgfx::Caps* caps = bgfx::getCaps();
      float ortho[16];
      bx::mtxOrtho(ortho, 0.0f, float(m_Width), float(m_Height), 0.0f, 0.0f, 100.0f, 0.0f, caps->homogeneousDepth);
      float viewIdMat[16]; bx::mtxIdentity(viewIdMat);
      bgfx::setViewTransform(2, viewIdMat, ortho);
      bgfx::setViewRect(2, 0, 0, (uint16_t)m_Width, (uint16_t)m_Height);

      // Mouse for hit-testing (prefer viewport-reported framebuffer coords)
      float mx = 0.0f, my = 0.0f;
      if (m_UIMouseValid) {
         mx = m_UIMouseX;
         my = m_UIMouseY;
         }
      else {
         auto mp = Input::GetMousePosition();
         mx = mp.first; my = mp.second;
         }
      bool mouseDown = Input::IsMouseButtonPressed(0);
      m_UIInputConsumed = false;

      // Simple per-entity pass: draw panels; drive buttons; text already handled by TextRenderer screen path
      for (auto& e : scene.GetEntities()) {
         auto* d = scene.GetEntityData(e.GetID());
         if (!d || !d->Visible) continue;

         // If entity has a Canvas and is screen space, it just acts as a scope for children; we currently use global backbuffer size
         // Draw Panel
         if (d->Panel && d->Panel->Visible) {
            PanelComponent& p = *d->Panel;
            // Compute anchor-based top-left position
            float ax = 0.0f, ay = 0.0f;
            if (p.AnchorEnabled) {
               switch (p.Anchor) {
                     case UIAnchorPreset::TopLeft:    ax = 0;              ay = 0;               break;
                     case UIAnchorPreset::Top:        ax = m_Width * 0.5f;   ay = 0;               break;
                     case UIAnchorPreset::TopRight:   ax = (float)m_Width;  ay = 0;               break;
                     case UIAnchorPreset::Left:       ax = 0;              ay = m_Height * 0.5f;  break;
                     case UIAnchorPreset::Center:     ax = m_Width * 0.5f;   ay = m_Height * 0.5f;  break;
                     case UIAnchorPreset::Right:      ax = (float)m_Width; ay = m_Height * 0.5f;  break;
                     case UIAnchorPreset::BottomLeft: ax = 0;              ay = (float)m_Height; break;
                     case UIAnchorPreset::Bottom:     ax = m_Width * 0.5f;   ay = (float)m_Height; break;
                     case UIAnchorPreset::BottomRight:ax = (float)m_Width; ay = (float)m_Height; break;
                  }
               ax += p.AnchorOffset.x;
               ay += p.AnchorOffset.y;
               }
            else {
               ax = p.Position.x;
               ay = p.Position.y;
               }
            float x0 = ax;
            float y0 = ay;
            float x1 = x0 + p.Size.x * p.Scale.x;
            float y1 = y0 + p.Size.y * p.Scale.y;

            // Base tint: panel tint
            uint32_t abgr = 0xffffffffu;
            // Pack tint including opacity
            auto clamp01 = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };
            glm::vec4 tint = p.TintColor;
            // Apply button state tint if present
            if (d->Button) {
               if (d->Button->Pressed)      tint *= d->Button->PressedTint;
               else if (d->Button->Hovered) tint *= d->Button->HoverTint;
               else                          tint *= d->Button->NormalTint;
               }
            uint8_t r = (uint8_t)(clamp01(tint.r) * 255.0f);
            uint8_t g = (uint8_t)(clamp01(tint.g) * 255.0f);
            uint8_t b = (uint8_t)(clamp01(tint.b) * 255.0f);
            uint8_t a = (uint8_t)(clamp01(tint.a * p.Opacity) * 255.0f);
            abgr = (a << 24) | (b << 16) | (g << 8) | (r);

            UIVertex verts[4];
            uint16_t idx[6] = { 0,1,2, 0,2,3 };
            if (p.Mode == PanelComponent::FillMode::NineSlice && p.Texture.IsValid()) {
               float L = x0, T = y0, R = x1, B = y1;
               float w = (x1 - x0), h = (y1 - y0);
               float uL = p.UVRect.x, vT = p.UVRect.y, uR = p.UVRect.z, vB = p.UVRect.w;
               float du = (uR - uL);
               float dv = (vB - vT);
               // Convert absolute UV slice margins into fractions of the selected rect
               float lFrac = (du != 0.0f) ? (p.SliceUV.x / du) : 0.0f;
               float rFrac = (du != 0.0f) ? (p.SliceUV.z / du) : 0.0f;
               float tFrac = (dv != 0.0f) ? (p.SliceUV.y / dv) : 0.0f;
               float bFrac = (dv != 0.0f) ? (p.SliceUV.w / dv) : 0.0f;

               float lpx = w * lFrac;
               float rpx = w * rFrac;
               float tpx = h * tFrac;
               float bpx = h * bFrac;

               float xL = L;
               float xM = L + lpx;
               float xR = R - rpx;
               float yT = T;
               float yM = T + tpx;
               float yB = B - bpx;

               // Compute UV splits using absolute slice margins inside the rect
               float uL2 = uL + p.SliceUV.x;
               float uR2 = uR - p.SliceUV.z;
               float vT2 = vT + p.SliceUV.y;
               float vB2 = vB - p.SliceUV.w;

               auto submitQuad = [&](float xa, float ya, float xb, float yb, float ua, float va, float ub, float vb) {
                  UIVertex vv[4] = {
                      { xa, ya, 0.0f, ua, va, abgr },
                      { xb, ya, 0.0f, ub, va, abgr },
                      { xb, yb, 0.0f, ub, vb, abgr },
                      { xa, yb, 0.0f, ua, vb, abgr }
                     };
                  uint16_t ii[6] = { 0,1,2, 0,2,3 };
                  const bgfx::Memory* vmem2 = bgfx::copy(vv, sizeof(vv));
                  const bgfx::Memory* imem2 = bgfx::copy(ii, sizeof(ii));
                  bgfx::VertexBufferHandle vbh2 = bgfx::createVertexBuffer(vmem2, UIVertex::layout);
                  bgfx::IndexBufferHandle  ibh2 = bgfx::createIndexBuffer(imem2);
                  float id2[16]; bx::mtxIdentity(id2); bgfx::setTransform(id2);
                  bgfx::setVertexBuffer(0, vbh2);
                  bgfx::setIndexBuffer(ibh2);
                  bgfx::TextureHandle th2 = m_UIWhiteTex;
                  if (p.Texture.IsValid()) {
                     if (auto* entry = AssetLibrary::Instance().GetAsset(p.Texture)) {
                        // Lazy-load like the non-9slice path to ensure the texture is available
                        if (!entry->texture || !bgfx::isValid(*entry->texture)) {
                           auto tex = AssetLibrary::Instance().LoadTexture(p.Texture);
                           (void)tex;
                           }
                        if (entry->texture && bgfx::isValid(*entry->texture)) th2 = *entry->texture;
                        }
                     }
                  bgfx::setTexture(0, m_UISampler, th2);
                  bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
                  bgfx::submit(2, m_UIProgram);
                  bgfx::destroy(vbh2);
                  bgfx::destroy(ibh2);
                  };

               submitQuad(xL, yT, xM, yM, uL, vT, uL2, vT2);
               submitQuad(xM, yT, xR, yM, uL2, vT, uR2, vT2);
               submitQuad(xR, yT, R, yM, uR2, vT, uR, vT2);
               submitQuad(xL, yM, xM, yB, uL, vT2, uL2, vB2);
               submitQuad(xM, yM, xR, yB, uL2, vT2, uR2, vB2);
               submitQuad(xR, yM, R, yB, uR2, vT2, uR, vB2);
               submitQuad(xL, yB, xM, B, uL, vB2, uL2, vB);
               submitQuad(xM, yB, xR, B, uL2, vB2, uR2, vB);
               submitQuad(xR, yB, R, B, uR2, vB2, uR, vB);
               continue;
               }
            else if (p.Mode == PanelComponent::FillMode::Tile) {
               float u0 = p.UVRect.x, v0 = p.UVRect.y;
               float u1 = p.UVRect.z * p.TileRepeat.x, v1 = p.UVRect.w * p.TileRepeat.y;
               verts[0] = { x0, y0, 0.0f, u0, v0, abgr };
               verts[1] = { x1, y0, 0.0f, u1, v0, abgr };
               verts[2] = { x1, y1, 0.0f, u1, v1, abgr };
               verts[3] = { x0, y1, 0.0f, u0, v1, abgr };
               }
            else {
               verts[0] = { x0, y0, 0.0f, p.UVRect.x, p.UVRect.y, abgr };
               verts[1] = { x1, y0, 0.0f, p.UVRect.z, p.UVRect.y, abgr };
               verts[2] = { x1, y1, 0.0f, p.UVRect.z, p.UVRect.w, abgr };
               verts[3] = { x0, y1, 0.0f, p.UVRect.x, p.UVRect.w, abgr };
               }
            const bgfx::Memory* vmem = bgfx::copy(verts, sizeof(verts));
            const bgfx::Memory* imem = bgfx::copy(idx, sizeof(idx));
            bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(vmem, UIVertex::layout);
            bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(imem);
            float id[16]; bx::mtxIdentity(id); bgfx::setTransform(id);
            bgfx::setVertexBuffer(0, vbh);
            bgfx::setIndexBuffer(ibh);
            bgfx::TextureHandle th = m_UIWhiteTex;
            if (p.Texture.IsValid()) {
               if (auto* entry = AssetLibrary::Instance().GetAsset(p.Texture)) {
                  // Lazy-load the texture if needed so drops immediately show up
                  if (!entry->texture || !bgfx::isValid(*entry->texture)) {
                     auto tex = AssetLibrary::Instance().LoadTexture(p.Texture);
                     (void)tex;
                     }
                  if (entry->texture && bgfx::isValid(*entry->texture)) th = *entry->texture;
                  }
               }
            bgfx::setTexture(0, m_UISampler, th);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(2, m_UIProgram);
            bgfx::destroy(vbh);
            bgfx::destroy(ibh);

            // Button hit-testing overlay
            if (d->Button && d->Button->Interactable) {
               bool inside = (mx >= x0 && mx <= x1 && my >= y0 && my <= y1);
               d->Button->Hovered = inside;
               if (inside) m_UIInputConsumed = true;
               bool wasPressed = d->Button->Pressed;
               d->Button->Pressed = inside && mouseDown;
               d->Button->Clicked = (!mouseDown && wasPressed && inside);
               if (d->Button->Toggle && d->Button->Clicked) d->Button->Toggled = !d->Button->Toggled;
               }
            }
         }
      }

   }

void Renderer::RenderScene(Scene& scene, uint16_t viewId)
   {
   // Prepare camera matrices (use current camera already set via SetCamera)
   Camera* activeCamera = GetCamera();
   glm::mat4 view = activeCamera->GetViewMatrix();
   glm::mat4 proj = activeCamera->GetProjectionMatrix();
   bgfx::setViewTransform(viewId, glm::value_ptr(view), glm::value_ptr(proj));
   bgfx::touch(viewId);

   glm::vec4 camPos(activeCamera->GetPosition(), 1.0f);
   bgfx::setUniform(u_cameraPos, &camPos);

   UploadEnvironmentToShader(scene.GetEnvironment());

   std::vector<LightData> lights;
   lights.reserve(4);
   for (auto& entity : scene.GetEntities()) {
      auto* data = scene.GetEntityData(entity.GetID());
      if (!data || !data->Light || !data->Visible) continue;

      // Collect light data from the entity
      LightData ld; ld.type = data->Light->Type; 
      ld.color = data->Light->Color * data->Light->Intensity;
      ld.position = data->Transform.Position;

      if (data->Light->Type == LightType::Directional) 
         {
         float yaw = glm::radians(data->Transform.Rotation.y);
         float pitch = glm::radians(data->Transform.Rotation.x);
         ld.direction = glm::normalize(glm::vec3(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw)));
         ld.range = 0.0f; ld.constant = 1.0f; ld.linear = 0.0f; ld.quadratic = 0.0f;
         }
      else 
         { 
         ld.direction = glm::vec3(0.0f); 
         ld.range = 50.0f; ld.constant = 1.0f;
         ld.linear = 0.09f; ld.quadratic = 0.032f; 
         }
      lights.push_back(ld); if (lights.size() == 4) break;
      }

   if (m_ShowGrid) {
      DrawGrid(viewId);
   }
   UploadLightsToShader(lights);

   std::vector<EntityID> entityIds; entityIds.reserve(scene.GetEntities().size());
   for (const auto& eSnap : scene.GetEntities()) entityIds.push_back(eSnap.GetID());
   for (EntityID eid : entityIds) {

      auto* data = scene.GetEntityData(eid);
      if (!data || !data->Visible || !data->Mesh || !data->Mesh->mesh) continue;
      std::shared_ptr<Mesh> meshPtr = data->Mesh->mesh; if (!meshPtr) continue;

      bool meshValid = meshPtr->Dynamic ? bgfx::isValid(meshPtr->dvbh) : bgfx::isValid(meshPtr->vbh);
      if (!meshValid || !bgfx::isValid(meshPtr->ibh)) continue;

      float transform[16]; memcpy(transform, glm::value_ptr(data->Transform.WorldMatrix), sizeof(float) * 16);

      DrawMesh(*meshPtr.get(), transform, *data->Mesh->material, viewId, &data->Mesh->PropertyBlock);
      }
   }


// ---------------- Mesh Submission ----------------

void Renderer::DrawMesh(const Mesh& mesh, const float* transform, const Material& material, const MaterialPropertyBlock* propertyBlock) {
   bgfx::setTransform(transform);
   if (mesh.Dynamic) {
      if (bgfx::isValid(mesh.dvbh)) 
         {
         bgfx::setVertexBuffer(0, mesh.dvbh, 0, mesh.numVertices);
         }
      else 
         {
         std::cerr << "[Renderer] Tried to draw with invalid dynamic VBO!\n";
         return;
         }
      }
   else 
      {
      bgfx::setVertexBuffer(0, mesh.vbh);
      }

   bgfx::setIndexBuffer(mesh.ibh);

   // Bind shared material defaults first, then overlay per-entity overrides
   // Provide normal matrix to shaders that use it (skinned shader expects u_normalMat)
   // Compute transpose(inverse(mat3(model))) once on CPU
   glm::mat4 modelMtx = glm::make_mat4(transform);
   glm::mat3 n3 = glm::transpose(glm::inverse(glm::mat3(modelMtx)));
   glm::mat4 normalMat4(1.0f);
   normalMat4[0] = glm::vec4(n3[0], 0.0f);
   normalMat4[1] = glm::vec4(n3[1], 0.0f);
   normalMat4[2] = glm::vec4(n3[2], 0.0f);
   bgfx::setUniform(u_normalMat, glm::value_ptr(normalMat4));

   material.BindUniforms();
   if (propertyBlock && !propertyBlock->Empty()) {
      material.ApplyPropertyBlock(*propertyBlock);
      }
   // Use the material's depth state as-is
   bgfx::setState(material.GetStateFlags());
   auto materialProgram = material.GetProgram();

   if (!bgfx::isValid(material.GetProgram())) {
      std::cerr << "Invalid PBR shader program!\n";
      return;
      }

   bgfx::submit(1, materialProgram);
   }

void Renderer::DrawMesh(const Mesh& mesh, const float* transform, const Material& material, uint16_t viewId, const MaterialPropertyBlock* propertyBlock) {
   bgfx::setTransform(transform);
   if (mesh.Dynamic) {
      if (bgfx::isValid(mesh.dvbh)) bgfx::setVertexBuffer(0, mesh.dvbh, 0, mesh.numVertices); else { std::cerr << "[Renderer] Invalid dynamic VBO" << std::endl; return; }
      }
   else {
      bgfx::setVertexBuffer(0, mesh.vbh);
      }
   bgfx::setIndexBuffer(mesh.ibh);

   // Bind shared material defaults then overlay overrides so they win
   // Note: normal matrix set below applies regardless
   glm::mat4 modelMtx = glm::make_mat4(transform);
   glm::mat3 n3 = glm::transpose(glm::inverse(glm::mat3(modelMtx)));
   glm::mat4 normalMat4(1.0f); normalMat4[0] = glm::vec4(n3[0], 0.0f); normalMat4[1] = glm::vec4(n3[1], 0.0f); normalMat4[2] = glm::vec4(n3[2], 0.0f);
   
   bgfx::setUniform(u_normalMat, glm::value_ptr(normalMat4));
   material.BindUniforms();
   
   if (propertyBlock && !propertyBlock->Empty()) 
      { 
      material.ApplyPropertyBlock(*propertyBlock); 
      }

   bgfx::setState(material.GetStateFlags());
   if (!bgfx::isValid(material.GetProgram())) 
      { 
      std::cerr << "Invalid material program" << std::endl;
      return; 
      }

   bgfx::submit(viewId, material.GetProgram());
   }


// ---------------- Light Management ----------------
void Renderer::UploadLightsToShader(const std::vector<LightData>&lights) {
   glm::vec4 colors[4], positions[4], params[4];

   for (int i = 0; i < 4; ++i) {
      if (i < lights.size()) {
         const LightData& light = lights[i];

         // Color with intensity in alpha
         colors[i] = glm::vec4(light.color, 1.0f);

         if (light.type == LightType::Directional) {
            // For directional lights: xyz = direction, w = 0 (directional)
            positions[i] = glm::vec4(light.direction, 0.0f);
            }
         else {
            // For point lights: xyz = position, w = 1 (point)
            positions[i] = glm::vec4(light.position, 1.0f);
            }

         // Light parameters: x = range, y = constant, z = linear, w = quadratic
         params[i] = glm::vec4(light.range, light.constant, light.linear, light.quadratic);
         }
      else {
         // Disabled light
         colors[i] = glm::vec4(0.0f);
         positions[i] = glm::vec4(0.0f);
         params[i] = glm::vec4(0.0f);
         }
      }

   bgfx::setUniform(u_LightColors, colors, 4);
   bgfx::setUniform(u_LightPositions, positions, 4);
   bgfx::setUniform(u_LightParams, params, 4);
   }

void Renderer::UploadEnvironmentToShader(const Environment & env)
   {
   // Pack ambient color * intensity in xyz, w = flags (bit0: fog enabled)
   glm::vec3 ambient = env.AmbientColor * env.AmbientIntensity;
   float flags = env.EnableFog ? 1.0f : 0.0f;
   glm::vec4 ambientFog(ambient, flags);
   bgfx::setUniform(u_AmbientFog, &ambientFog);

   // Fog params: x = density, yzw = fog color
   glm::vec4 fogParams(env.FogDensity, env.FogColor.r, env.FogColor.g, env.FogColor.b);
   bgfx::setUniform(u_FogParams, &fogParams);

   // Sky params: x = proceduralSky flag
   glm::vec4 skyParams(env.ProceduralSky ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
   bgfx::setUniform(u_SkyParams, &skyParams);
   glm::vec4 zen(env.SkyZenithColor, 1.0f);
   glm::vec4 hor(env.SkyHorizonColor, 1.0f);
   bgfx::setUniform(u_SkyZenith, &zen);
   bgfx::setUniform(u_SkyHorizon, &hor);
   }

void Renderer::DrawGrid() {
   // View setup
   bgfx::setViewTransform(0, m_view, m_proj);
   bgfx::setViewRect(0, 0, 0, m_Width, m_Height);

   // Fixed, angle-independent bounds: square around camera projection on ground
   Camera* cam = GetCamera();
   if (!cam) return;

   glm::vec3 camPos = cam->GetPosition();
   glm::vec2 groundCenter = { camPos.x, camPos.z };
   float height = std::max(0.001f, fabs(camPos.y));
   float extent = std::clamp(height * 8.0f, 10.0f, 400.0f);

   float minX = groundCenter.x - extent;
   float maxX = groundCenter.x + extent;
   float minZ = groundCenter.y - extent;
   float maxZ = groundCenter.y + extent;

   // Pad a little
   const float padding = 1.0f;
   minX -= padding; maxX += padding; minZ -= padding; maxZ += padding;

   // Snap to step
   const float step = 1.0f;
   auto floorTo = [&](float v) { return std::floor(v / step) * step; };
   auto ceilTo = [&](float v) { return std::ceil(v / step) * step; };
   minX = floorTo(minX); maxX = ceilTo(maxX); minZ = floorTo(minZ); maxZ = ceilTo(maxZ);

   // Build grid vertices inside bounds on y=0 plane
   std::vector<GridVertex> vertices;
   for (float x = minX; x <= maxX + 1e-4f; x += step) {
      vertices.push_back({ x, 0.0f, minZ });
      vertices.push_back({ x, 0.0f, maxZ });
      }
   for (float z = minZ; z <= maxZ + 1e-4f; z += step) {
      vertices.push_back({ minX, 0.0f, z });
      vertices.push_back({ maxX, 0.0f, z });
      }
   if (vertices.empty()) return;

   const bgfx::Memory* mem = bgfx::copy(vertices.data(), (uint32_t)(vertices.size() * sizeof(GridVertex)));
   bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(mem, GridVertex::layout);

   float identity[16];
   bx::mtxIdentity(identity);
   bgfx::setTransform(identity);
   bgfx::setVertexBuffer(0, vbh);

   // Semi-transparent gray, depth-tested but not writing depth
   auto debugMat = MaterialManager::Instance().CreateDefaultDebugMaterial();
   debugMat->BindUniforms();
   bgfx::setState(
      BGFX_STATE_WRITE_RGB |
      BGFX_STATE_DEPTH_TEST_LEQUAL |
      BGFX_STATE_PT_LINES |
      BGFX_STATE_BLEND_ALPHA
   );
   bgfx::submit(0, debugMat->GetProgram());
   bgfx::destroy(vbh);
   }

void Renderer::DrawGrid(uint16_t viewId) {
   if (!bgfx::isValid(m_GridVB)) return;

   Camera* cam = GetCamera(); if (!cam) return;
   glm::vec3 camPos = cam->GetPosition();
   glm::vec2 groundCenter = { camPos.x, camPos.z };

   float height = std::max(0.001f, fabs(camPos.y));
   float extent = std::clamp(height * 8.0f, 10.0f, 400.0f);
   float minX = groundCenter.x - extent; float maxX = groundCenter.x + extent;
   float minZ = groundCenter.y - extent; float maxZ = groundCenter.y + extent;

   const float step = 1.0f;
   auto floorTo = [&](float v) { return std::floor(v / step) * step; };
   auto ceilTo = [&](float v) { return std::ceil(v / step) * step; };

   minX = floorTo(minX); maxX = ceilTo(maxX); minZ = floorTo(minZ); maxZ = ceilTo(maxZ);
   std::vector<GridVertex> vertices;

   for (float x = minX; x <= maxX + 1e-4f; x += step)
      {
      vertices.push_back({ x, 0.0f, minZ });
      vertices.push_back({ x, 0.0f, maxZ });
      }
   for (float z = minZ; z <= maxZ + 1e-4f; z += step) 
      { 
      vertices.push_back({ minX, 0.0f, z }); 
      vertices.push_back({ maxX, 0.0f, z }); 
      }

   if (vertices.empty()) return;
   const bgfx::Memory* mem = bgfx::copy(vertices.data(), (uint32_t)(vertices.size() * sizeof(GridVertex)));
   
   bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(mem, GridVertex::layout);
   float identity[16]; bx::mtxIdentity(identity); 
   bgfx::setTransform(identity);
   
   bgfx::setVertexBuffer(0, vbh);
   auto debugMat = MaterialManager::Instance().CreateDefaultDebugMaterial(); 
   debugMat->BindUniforms();
   
   bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_PT_LINES | BGFX_STATE_BLEND_ALPHA);
   bgfx::submit(viewId, debugMat->GetProgram());
   bgfx::destroy(vbh);
   }


void Renderer::DrawDebugRay(const glm::vec3 & origin, const glm::vec3 & dir, float length) {
   // TODO: Implement line vertex buffer for debug rendering
   }

void Renderer::DrawCollider(const ColliderComponent & collider, const TransformComponent & transform) {
   if (!bgfx::isValid(m_DebugLineProgram)) return;

   // Calculate world transform including collider offset
   glm::mat4 worldTransform = transform.WorldMatrix * glm::translate(glm::mat4(1.0f), collider.Offset);

   float transformMatrix[16];
   memcpy(transformMatrix, glm::value_ptr(worldTransform), sizeof(float) * 16);
   bgfx::setTransform(transformMatrix);

   // Set debug material state
   bgfx::setState(
      BGFX_STATE_WRITE_RGB |
      BGFX_STATE_WRITE_Z |
      BGFX_STATE_DEPTH_TEST_LEQUAL |
      BGFX_STATE_PT_LINES
   );

   // Draw different shapes based on collider type
   switch (collider.ShapeType) {
         case ColliderShape::Box: {
         // Create wireframe box vertices
         std::vector<GridVertex> boxVertices;
         float halfSizeX = collider.Size.x * 0.5f;
         float halfSizeY = collider.Size.y * 0.5f;
         float halfSizeZ = collider.Size.z * 0.5f;

         // Front face
         boxVertices.push_back({ -halfSizeX, -halfSizeY, -halfSizeZ });
         boxVertices.push_back({ halfSizeX, -halfSizeY, -halfSizeZ });
         boxVertices.push_back({ halfSizeX, -halfSizeY, -halfSizeZ });
         boxVertices.push_back({ halfSizeX,  halfSizeY, -halfSizeZ });
         boxVertices.push_back({ halfSizeX,  halfSizeY, -halfSizeZ });
         boxVertices.push_back({ -halfSizeX,  halfSizeY, -halfSizeZ });
         boxVertices.push_back({ -halfSizeX,  halfSizeY, -halfSizeZ });
         boxVertices.push_back({ -halfSizeX, -halfSizeY, -halfSizeZ });

         // Back face
         boxVertices.push_back({ -halfSizeX, -halfSizeY,  halfSizeZ });
         boxVertices.push_back({ halfSizeX, -halfSizeY,  halfSizeZ });
         boxVertices.push_back({ halfSizeX, -halfSizeY,  halfSizeZ });
         boxVertices.push_back({ halfSizeX,  halfSizeY,  halfSizeZ });
         boxVertices.push_back({ halfSizeX,  halfSizeY,  halfSizeZ });
         boxVertices.push_back({ -halfSizeX,  halfSizeY,  halfSizeZ });
         boxVertices.push_back({ -halfSizeX,  halfSizeY,  halfSizeZ });
         boxVertices.push_back({ -halfSizeX, -halfSizeY,  halfSizeZ });

         // Connecting lines
         boxVertices.push_back({ -halfSizeX, -halfSizeY, -halfSizeZ });
         boxVertices.push_back({ -halfSizeX, -halfSizeY,  halfSizeZ });
         boxVertices.push_back({ halfSizeX, -halfSizeY, -halfSizeZ });
         boxVertices.push_back({ halfSizeX, -halfSizeY,  halfSizeZ });
         boxVertices.push_back({ halfSizeX,  halfSizeY, -halfSizeZ });
         boxVertices.push_back({ halfSizeX,  halfSizeY,  halfSizeZ });
         boxVertices.push_back({ -halfSizeX,  halfSizeY, -halfSizeZ });
         boxVertices.push_back({ -halfSizeX,  halfSizeY,  halfSizeZ });

         const bgfx::Memory* mem = bgfx::copy(boxVertices.data(), sizeof(GridVertex) * boxVertices.size());
         bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(mem, GridVertex::layout);
         bgfx::setVertexBuffer(0, vbh);
         bgfx::submit(0, m_DebugLineProgram);
         bgfx::destroy(vbh);
         break;
         }
         case ColliderShape::Capsule: {
         // For now, draw as a box approximation
         // TODO: Implement proper capsule wireframe
         std::vector<GridVertex> capsuleVertices;
         float radius = collider.Radius;
         float height = collider.Height;
         float halfHeight = height * 0.5f;

         // Draw as a cylinder approximation
         const int segments = 16;
         for (int i = 0; i < segments; ++i) {
            float angle1 = (float)i / segments * 2.0f * 3.14159f;
            float angle2 = (float)(i + 1) / segments * 2.0f * 3.14159f;

            float x1 = cos(angle1) * radius;
            float z1 = sin(angle1) * radius;
            float x2 = cos(angle2) * radius;
            float z2 = sin(angle2) * radius;

            // Top circle
            capsuleVertices.push_back({ x1, halfHeight, z1 });
            capsuleVertices.push_back({ x2, halfHeight, z2 });

            // Bottom circle
            capsuleVertices.push_back({ x1, -halfHeight, z1 });
            capsuleVertices.push_back({ x2, -halfHeight, z2 });

            // Connecting lines
            capsuleVertices.push_back({ x1, halfHeight, z1 });
            capsuleVertices.push_back({ x1, -halfHeight, z1 });
            }

         const bgfx::Memory* mem = bgfx::copy(capsuleVertices.data(), sizeof(GridVertex) * capsuleVertices.size());
         bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(mem, GridVertex::layout);
         bgfx::setVertexBuffer(0, vbh);
         bgfx::submit(0, m_DebugLineProgram);
         bgfx::destroy(vbh);
         break;
         }
         case ColliderShape::Mesh: {
         // For mesh colliders, we could draw the mesh bounds
         // For now, skip mesh collider debug drawing
         break;
         }
      }
   }

void Renderer::DrawAABB(const glm::vec3& worldMin, const glm::vec3& worldMax, uint16_t viewId) {
   glm::vec3 v000 = {worldMin.x, worldMin.y, worldMin.z};
   glm::vec3 v100 = {worldMax.x, worldMin.y, worldMin.z};
   glm::vec3 v010 = {worldMin.x, worldMax.y, worldMin.z};
   glm::vec3 v110 = {worldMax.x, worldMax.y, worldMin.z};
   glm::vec3 v001 = {worldMin.x, worldMin.y, worldMax.z};
   glm::vec3 v101 = {worldMax.x, worldMin.y, worldMax.z};
   glm::vec3 v011 = {worldMin.x, worldMax.y, worldMax.z};
   glm::vec3 v111 = {worldMax.x, worldMax.y, worldMax.z};

   std::vector<GridVertex> lines = {
      {v000.x, v000.y, v000.z}, {v100.x, v100.y, v100.z},
      {v100.x, v100.y, v100.z}, {v110.x, v110.y, v110.z},
      {v110.x, v110.y, v110.z}, {v010.x, v010.y, v010.z},
      {v010.x, v010.y, v010.z}, {v000.x, v000.y, v000.z},

      {v001.x, v001.y, v001.z}, {v101.x, v101.y, v101.z},
      {v101.x, v101.y, v101.z}, {v111.x, v111.y, v111.z},
      {v111.x, v111.y, v111.z}, {v011.x, v011.y, v011.z},
      {v011.x, v011.y, v011.z}, {v001.x, v001.y, v001.z},

      {v000.x, v000.y, v000.z}, {v001.x, v001.y, v001.z},
      {v100.x, v100.y, v100.z}, {v101.x, v101.y, v101.z},
      {v110.x, v110.y, v110.z}, {v111.x, v111.y, v111.z},
      {v010.x, v010.y, v010.z}, {v011.x, v011.y, v011.z},
   };

   const bgfx::Memory* mem = bgfx::copy(lines.data(), (uint32_t)(lines.size() * sizeof(GridVertex)));
   bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(mem, GridVertex::layout);
   float identity[16]; bx::mtxIdentity(identity);
   bgfx::setTransform(identity);
   bgfx::setVertexBuffer(0, vbh);

   auto debugMat = MaterialManager::Instance().CreateDefaultDebugMaterial();
   debugMat->BindUniforms();
   bgfx::setState(
      BGFX_STATE_WRITE_RGB |
      BGFX_STATE_DEPTH_TEST_LEQUAL |
      BGFX_STATE_PT_LINES |
      BGFX_STATE_BLEND_ALPHA
   );
   bgfx::submit(viewId, debugMat->GetProgram());
   bgfx::destroy(vbh);
}

// --------------------------------------
// Draw simple wireframe outline around selected entity's mesh (editor only)
// --------------------------------------
void Renderer::DrawEntityOutline(Scene & scene, EntityID selectedEntity) {
   if (selectedEntity < 0 || scene.m_IsPlaying) return;
   auto* data = scene.GetEntityData(selectedEntity);
   if (!data || !data->Visible || !data->Mesh || !data->Mesh->mesh) return;

   // New pipeline: ObjectID -> Edge -> Composite
   const uint16_t kView_ObjectId = 210;
   const uint16_t kView_OutlineEdge = 211;
   const uint16_t kView_OutlineComposite = 212;

   // Ensure RTs exist
   if (!bgfx::isValid(m_ObjectIdFB) || !bgfx::isValid(m_EdgeMaskFB)) {
      Resize(m_Width, m_Height);
   }

   // 1) ObjectID pass: clear and draw only the selected entity (fast path)
   {
      bgfx::setViewRect(kView_ObjectId, 0, 0, (uint16_t)m_Width, (uint16_t)m_Height);
      bgfx::setViewTransform(kView_ObjectId, m_view, m_proj);
      bgfx::setViewFrameBuffer(kView_ObjectId, m_ObjectIdFB);
      bgfx::setViewClear(kView_ObjectId, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
      bgfx::touch(kView_ObjectId);

      std::shared_ptr<Mesh> meshPtr2 = data->Mesh->mesh;
      if (meshPtr2 && (meshPtr2->Dynamic ? bgfx::isValid(meshPtr2->dvbh) : bgfx::isValid(meshPtr2->vbh)) && bgfx::isValid(meshPtr2->ibh)) {
         float transform[16]; memcpy(transform, glm::value_ptr(data->Transform.WorldMatrix), sizeof(float) * 16);
         bgfx::setTransform(transform);
         if (meshPtr2->Dynamic) bgfx::setVertexBuffer(0, meshPtr2->dvbh, 0, meshPtr2->numVertices); else bgfx::setVertexBuffer(0, meshPtr2->vbh);
         bgfx::setIndexBuffer(meshPtr2->ibh);
         uint64_t st = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LEQUAL;
         bgfx::setState(st);
         // Pack selected entity id into RGB
         uint32_t id = (uint32_t)selectedEntity;
         glm::vec4 packed((float)(id & 255u) / 255.0f,
                          (float)((id >> 8) & 255u) / 255.0f,
                          (float)((id >> 16) & 255u) / 255.0f,
                          0.0f);
         bgfx::setUniform(u_ObjectIdPacked, &packed);
         bgfx::submit(kView_ObjectId, std::dynamic_pointer_cast<SkinnedPBRMaterial>(data->Mesh->material) ? m_ObjectIdProgramSkinned : m_ObjectIdProgram);
      }
   }

   // 2) Edge pass: fullscreen sampling ObjectId -> EdgeMask
   {
      bgfx::setViewRect(kView_OutlineEdge, 0, 0, (uint16_t)m_Width, (uint16_t)m_Height);
      bgfx::setViewFrameBuffer(kView_OutlineEdge, m_EdgeMaskFB);
      bgfx::setViewClear(kView_OutlineEdge, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
      float idM[16]; bx::mtxIdentity(idM); bgfx::setTransform(idM);
      struct Pos { float x,y,z; };
      const Pos verts[3] = { {-1.0f,-1.0f,0.0f}, {3.0f,-1.0f,0.0f}, {-1.0f,3.0f,0.0f} };
      const uint16_t idx[3] = {0,1,2};
      bgfx::TransientVertexBuffer tvb; bgfx::TransientIndexBuffer tib; bgfx::VertexLayout layout; layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
      if (bgfx::getAvailTransientVertexBuffer(3, layout) >= 3 && bgfx::getAvailTransientIndexBuffer(3) >= 3) {
         bgfx::allocTransientVertexBuffer(&tvb, 3, layout);
         bgfx::allocTransientIndexBuffer(&tib, 3);
         memcpy(tvb.data, verts, sizeof(verts)); memcpy(tib.data, idx, sizeof(idx));
         bgfx::setVertexBuffer(0, &tvb); bgfx::setIndexBuffer(&tib);
         bgfx::setTexture(0, s_ObjectId, m_ObjectIdTex);
         glm::vec4 texelSize(1.0f / float(m_Width), 1.0f / float(m_Height), 0.0f, 0.0f);
         bgfx::setUniform(u_TexelSize, &texelSize);
         // Selected id packed matches the object id we wrote
         uint32_t id = (uint32_t)selectedEntity;
         glm::vec4 selPacked((float)(id & 255u) / 255.0f,
                             (float)((id >> 8) & 255u) / 255.0f,
                             (float)((id >> 16) & 255u) / 255.0f,
                             0.0f);
         bgfx::setUniform(u_SelectedIdPacked, &selPacked);
         glm::vec4 edgeParams(m_OutlineThicknessPx, 0.0f, 0.0f, 0.0f);
         bgfx::setUniform(u_OutlineParams, &edgeParams);
         bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
         bgfx::submit(kView_OutlineEdge, m_OutlineEdgeProgram);
      }
   }

   // 3) Composite pass: blend onto scene color/backbuffer
   {
      bgfx::setViewRect(kView_OutlineComposite, 0, 0, (uint16_t)m_Width, (uint16_t)m_Height);
      if (m_RenderToOffscreen) bgfx::setViewFrameBuffer(kView_OutlineComposite, m_SceneFrameBuffer); else bgfx::setViewFrameBuffer(kView_OutlineComposite, BGFX_INVALID_HANDLE);
      float idM[16]; bx::mtxIdentity(idM); bgfx::setTransform(idM);
      struct Pos { float x,y,z; };
      const Pos verts[3] = { {-1.0f,-1.0f,0.0f}, {3.0f,-1.0f,0.0f}, {-1.0f,3.0f,0.0f} };
      const uint16_t idx[3] = {0,1,2};
      bgfx::TransientVertexBuffer tvb; bgfx::TransientIndexBuffer tib; bgfx::VertexLayout layout; layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
      if (bgfx::getAvailTransientVertexBuffer(3, layout) >= 3 && bgfx::getAvailTransientIndexBuffer(3) >= 3) {
         bgfx::allocTransientVertexBuffer(&tvb, 3, layout);
         bgfx::allocTransientIndexBuffer(&tib, 3);
         memcpy(tvb.data, verts, sizeof(verts)); memcpy(tib.data, idx, sizeof(idx));
         bgfx::setVertexBuffer(0, &tvb); bgfx::setIndexBuffer(&tib);
         // Bind edge mask only; blending will overlay over the existing scene color
         bgfx::setTexture(0, s_EdgeMask, m_EdgeMaskTex);
         glm::vec4 texelSize(1.0f / float(m_Width), 1.0f / float(m_Height), 0.0f, 0.0f);
         bgfx::setUniform(u_TexelSize, &texelSize);
         bgfx::setUniform(u_OutlineColor, &m_OutlineColor);
         bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ALPHA);
         bgfx::submit(kView_OutlineComposite, m_OutlineCompositeProgram2);
      }
   }
}

void Renderer::InitGrid(float size, float step) {
   std::vector<GridVertex> vertices;
   float half = size / 2.0f;
   for (float i = -half; i <= half; i += step) {
      vertices.push_back({ i, 0.0f, -half });
      vertices.push_back({ i, 0.0f,  half });
      vertices.push_back({ -half, 0.0f, i });
      vertices.push_back({ half, 0.0f, i });
      }
   m_GridVertexCount = (uint32_t)vertices.size();

   const bgfx::Memory* mem = bgfx::copy(vertices.data(), sizeof(GridVertex) * vertices.size());
   m_GridVB = bgfx::createVertexBuffer(mem, GridVertex::layout);
   }

