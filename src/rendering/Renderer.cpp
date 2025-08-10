#include "Renderer.h"
#include <bgfx/platform.h>
#include <glm/gtc/type_ptr.hpp>
#include "ShaderManager.h"
#include "Picking.h"
#include "MaterialManager.h"
#include "VertexTypes.h"
#include "ecs/ParticleEmitterSystem.h"
#include <bx/math.h>
#include <glm/gtx/transform.hpp>
#include <cstring>
#include "ecs/Components.h"
#include "Environment.h"

#include "TextRenderer.h"

// ---------------- Particle Vertex ----------------
struct ParticleVertex {
    float x, y, z, size;
    uint32_t abgr;
    static bgfx::VertexLayout layout;
    static void Init() {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 1, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true, true)
            .end();
    }
};
bgfx::VertexLayout ParticleVertex::layout;

// ---------------- Terrain Support ----------------
static void BuildTerrainMesh(TerrainComponent& terrain)
{
    const uint32_t size = terrain.Size;

    terrain.Vertices.resize(size * size);

    for (uint32_t y = 0; y < size; ++y)
    {
        for (uint32_t x = 0; x < size; ++x)
        {
            uint32_t idx = y * size + x;
            TerrainVertex& vert = terrain.Vertices[idx];
            vert.x = static_cast<float>(x);
            vert.y = static_cast<float>(terrain.HeightMap[idx]);
            vert.z = static_cast<float>(y);

            // Upward normal (will be wrong on cliffs but fine for now)
            vert.nx = 0.0f; vert.ny = 1.0f; vert.nz = 0.0f;

            vert.u = (x + 0.5f) / static_cast<float>(size);
            vert.v = (y + 0.5f) / static_cast<float>(size);
        }
    }

    terrain.Indices.resize((size - 1) * (size - 1) * 6);
    uint32_t index = 0;
    for (uint16_t y = 0; y < size - 1; ++y)
    {
        uint16_t y_offset = y * size;
        for (uint16_t x = 0; x < size - 1; ++x)
        {
            // Use CCW winding when looking from +Y (up) so top faces render
            terrain.Indices[index + 0] = y_offset + x;
            terrain.Indices[index + 1] = y_offset + x + size;
            terrain.Indices[index + 2] = y_offset + x + 1;
            terrain.Indices[index + 3] = y_offset + x + 1;
            terrain.Indices[index + 4] = y_offset + x + size;
            terrain.Indices[index + 5] = y_offset + x + size + 1;
            index += 6;
        }
    }
}

static void UpdateTerrainBuffers(TerrainComponent& terrain)
{
    // Rebuild CPU mesh data
    BuildTerrainMesh(terrain);

    const bgfx::Memory* vMem = bgfx::copy(terrain.Vertices.data(), (uint32_t)(terrain.Vertices.size() * sizeof(TerrainVertex)));
    const bgfx::Memory* iMem = bgfx::copy(terrain.Indices.data(), (uint32_t)(terrain.Indices.size() * sizeof(uint16_t)));

    switch (terrain.Mode)
    {
        default:
        case 0: // Static VB
            if (bgfx::isValid(terrain.vbh)) bgfx::destroy(terrain.vbh);
            if (bgfx::isValid(terrain.ibh)) bgfx::destroy(terrain.ibh);
            terrain.vbh = bgfx::createVertexBuffer(vMem, TerrainVertex::layout);
            terrain.ibh = bgfx::createIndexBuffer(iMem);
            break;
        case 1: // Dynamic VB
            if (!bgfx::isValid(terrain.dvbh))
                terrain.dvbh = bgfx::createDynamicVertexBuffer((uint32_t)terrain.Vertices.size(), TerrainVertex::layout);
            if (!bgfx::isValid(terrain.dibh))
                terrain.dibh = bgfx::createDynamicIndexBuffer((uint32_t)terrain.Indices.size());
            bgfx::update(terrain.dvbh, 0, vMem);
            bgfx::update(terrain.dibh, 0, iMem);
            break;
        case 2: // Height texture mode (not fully implemented yet)
            // Build initial geometry once
            if (!bgfx::isValid(terrain.vbh) || !bgfx::isValid(terrain.ibh))
            {
                terrain.vbh = bgfx::createVertexBuffer(vMem, TerrainVertex::layout);
                terrain.ibh = bgfx::createIndexBuffer(iMem);
            }
            // Update height texture
            if (!bgfx::isValid(terrain.HeightTexture))
            {
                terrain.HeightTexture = bgfx::createTexture2D(terrain.Size, terrain.Size, false, 1, bgfx::TextureFormat::R8);
            }
            const bgfx::Memory* tMem = bgfx::copy(terrain.HeightMap.data(), (uint32_t)terrain.HeightMap.size());
            bgfx::updateTexture2D(terrain.HeightTexture, 0, 0, 0, 0, terrain.Size, terrain.Size, tMem);
            break;
    }
}


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

// ---------------- Initialization ----------------
void Renderer::Init(uint32_t width, uint32_t height, void* windowHandle) {
    m_Width = width;
    m_Height = height;

    bgfx::Init init;
    init.platformData.nwh = windowHandle;
    init.type = bgfx::RendererType::Count;
    init.resolution.width = width;
    init.resolution.height = height;
    init.resolution.reset = BGFX_RESET_VSYNC;
    bgfx::init(init);
    bgfx::setDebug(BGFX_DEBUG_TEXT);

    m_RendererCamera = new Camera(60.0f, float(width) / float(height), 0.1f, 100.0f);


    const uint64_t texFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    m_SceneTexture = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::BGRA8, texFlags);

    bgfx::TextureHandle depthTexture = bgfx::createTexture2D(
        width, height, false, 1,
        bgfx::TextureFormat::D24S8, // Or D32F for float depth
        BGFX_TEXTURE_RT_WRITE_ONLY
    );

    bgfx::TextureHandle fbTextures[] = { m_SceneTexture, depthTexture };

    m_SceneFrameBuffer = bgfx::createFrameBuffer(2, fbTextures, true);
    
    bgfx::setViewFrameBuffer(0, m_SceneFrameBuffer);

    // Default clear
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff);
    bgfx::setViewClear(1, 0);


    // Initialize vertex layouts globally
    PBRVertex::Init();
    GridVertex::Init();
    TerrainVertex::Init(); 
    ParticleVertex::Init();
    

    // Debug line program
    m_DebugLineProgram = ShaderManager::Instance().LoadProgram("vs_debug", "fs_debug");
    // Outline program (color via uniform)
    m_OutlineProgram = ShaderManager::Instance().LoadProgram("vs_outline", "fs_outline");
    u_outlineColor = bgfx::createUniform("u_outlineColor", bgfx::UniformType::Vec4);
	InitGrid(20.0f, 1.0f);

    u_LightColors = bgfx::createUniform("u_lightColors", bgfx::UniformType::Vec4, 4);
    u_LightPositions = bgfx::createUniform("u_lightPositions", bgfx::UniformType::Vec4, 4);
    u_LightParams = bgfx::createUniform("u_lightParams", bgfx::UniformType::Vec4, 4);
    u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
    // CPU-provided normal matrix for skinned and static meshes
    u_normalMat = bgfx::createUniform("u_normalMat", bgfx::UniformType::Mat4);
    u_AmbientFog = bgfx::createUniform("u_ambientFog", bgfx::UniformType::Vec4);
    u_FogParams  = bgfx::createUniform("u_fogParams",  bgfx::UniformType::Vec4);


    // Terrain resources
    m_TerrainProgram = ShaderManager::Instance().LoadProgram("vs_pbr", "fs_pbr");
    m_TerrainHeightTexProgram = ShaderManager::Instance().LoadProgram("vs_terrain_height_texture", "fs_terrain");
    s_TerrainHeightTexture = bgfx::createUniform("s_heightTexture", bgfx::UniformType::Sampler);

    // Initialize text renderer (self-contained, stb-based)
    m_TextRenderer = std::make_unique<TextRenderer>();
    // Create a simple program from existing debug shaders (2D textured quads)
    bgfx::ProgramHandle fontProgram = ShaderManager::Instance().LoadProgram("vs_debug", "fs_debug");
    m_TextRenderer->Init("assets/fonts/Roboto-Regular.ttf", fontProgram, 512, 512, 48.0f);
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

   // Grid view (0)
   bgfx::setViewRect(0, 0, 0, m_Width, m_Height);
   bgfx::setViewTransform(0, m_view, m_proj);
   bgfx::setViewFrameBuffer(0, m_SceneFrameBuffer);  // ← this was missing before!
   bgfx::touch(0);

   // Mesh view (1)
   bgfx::setViewRect(1, 0, 0, m_Width, m_Height);
   bgfx::setViewTransform(1, m_view, m_proj);
   bgfx::setViewFrameBuffer(1, m_SceneFrameBuffer);  // ← this was missing!
   bgfx::touch(1);
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
    m_SceneFrameBuffer = bgfx::createFrameBuffer(1, &m_SceneTexture, true);
    bgfx::setViewFrameBuffer(0, m_SceneFrameBuffer);
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
    bgfx::setViewTransform(1, m_view, m_proj);  // Set camera matrices for view 1 too!

    glm::vec4 camPos(activeCamera->GetPosition(), 1.0f);
    bgfx::setUniform(u_cameraPos, &camPos);

    // Upload environment
    UploadEnvironmentToShader(scene.GetEnvironment());

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

    DrawGrid();

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
        

        
        // Only print debug info for FBX entities (ID > 1) and only once
        if (eid > 1) {
            static bool printedDebug = false;
            if (!printedDebug) {
                std::cout << "[Renderer] FBX Entity " << eid 
                          << " - Vertices: " << meshPtr->Vertices.size()
                          << ", Indices: " << meshPtr->Indices.size()
                          << ", Position: (" << data->Transform.Position.x << ", " 
                          << data->Transform.Position.y << ", " << data->Transform.Position.z << ")"
                          << ", Bounds: (" << meshPtr->BoundsMin.x << ", " 
                          << meshPtr->BoundsMin.y << ", " << meshPtr->BoundsMin.z << ") to ("
                          << meshPtr->BoundsMax.x << ", " 
                          << meshPtr->BoundsMax.y << ", " << meshPtr->BoundsMax.z << ")" << std::endl;
                printedDebug = true;
            }
        }
 
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
            UpdateTerrainBuffers(terrain);
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
        for (auto& entity : scene.GetEntities()) {
            auto* data = scene.GetEntityData(entity.GetID());
            if (!data || !data->Visible || !data->Collider) continue;

            DrawCollider(*data->Collider, data->Transform);
        }
    }

    // --------------------------------------
    // Draw text components (world or screen space)
    // --------------------------------------
    if (m_TextRenderer) {
        m_TextRenderer->RenderTexts(scene, m_view, m_proj, m_Width, m_Height, 1, 0);
    }

}


// ---------------- Mesh Submission ----------------
#include "rendering/MaterialPropertyBlock.h"
void Renderer::DrawMesh(const Mesh& mesh, const float* transform, const Material& material, const MaterialPropertyBlock* propertyBlock) {
    bgfx::setTransform(transform);
    if (mesh.Dynamic)
        if (bgfx::isValid(mesh.dvbh))
            bgfx::setVertexBuffer(0, mesh.dvbh, 0, mesh.numVertices);
        else {
            std::cerr << "[Renderer] Tried to draw with invalid dynamic VBO!\n";
            return;
        }
    else
        bgfx::setVertexBuffer(0, mesh.vbh);
    bgfx::setIndexBuffer(mesh.ibh);
    if (propertyBlock && !propertyBlock->Empty()) {
        material.ApplyPropertyBlock(*propertyBlock);
    }
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
    // Use the material’s depth state as-is
    bgfx::setState(material.GetStateFlags());
	auto materialProgram = material.GetProgram();
     
    if (!bgfx::isValid(material.GetProgram())) {
        std::cerr << "Invalid PBR shader program!\n";
        return;
    }

    // Debug: Check if textures are valid (only once for FBX)
    if (material.GetName() == "DefaultPBR" && mesh.Vertices.size() > 24) {
        static bool printedTextureDebug = false;
        if (!printedTextureDebug) {
            const PBRMaterial* pbrMat = static_cast<const PBRMaterial*>(&material);
            std::cout << "[Renderer] FBX PBR Material textures - Albedo: " 
                      << (bgfx::isValid(pbrMat->m_AlbedoTex) ? "valid" : "invalid")
                      << ", MR: " << (bgfx::isValid(pbrMat->m_MetallicRoughnessTex) ? "valid" : "invalid")
                      << ", Normal: " << (bgfx::isValid(pbrMat->m_NormalTex) ? "valid" : "invalid") << std::endl;
            printedTextureDebug = true;
        }
    }

    bgfx::submit(1, materialProgram);
}

#if 0


        return;
#endif

    #if 0
bgfx::allocInstanceDataBuffer(&idb, (uint16_t)ps.Particles.size(), sizeof(float) * 16);
    if (idb.data == nullptr)
        return; // not enough space 

    float* mtx = (float*)idb.data;
     
    // Build billboard transform matrices per particle
    glm::mat4 viewMat = glm::make_mat4(m_view);
    glm::mat3 billboardRot = glm::transpose(glm::mat3(viewMat)); // inverse of view rotation

    for (size_t i = 0; i < ps.Particles.size(); ++i)
    {
        const auto& p = ps.Particles[i];
        float t = p.Age / p.Lifetime;
        float size = glm::mix(ps.StartSize, ps.EndSize, t);
        glm::vec3 posWorld = systemPos + p.Position;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, posWorld);
        model *= glm::mat4(billboardRot);
        model = glm::scale(model, glm::vec3(size));
        memcpy(&mtx[i * 16], glm::value_ptr(model), sizeof(float) * 16);
    }

    
    // Use a unit quad mesh (generated once)
    if (!bgfx::isValid(m_ParticleQuadVbh))
    {
        // Create simple quad (-0.5..0.5)
        struct QuadVert { float x, y, z; float u, v; uint32_t abgr; };
        uint32_t whiteAbgr = 0xffffffff;
        QuadVert verts[4] = {
            { -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, whiteAbgr },
            {  0.5f, -0.5f, 0.0f, 1.0f, 1.0f, whiteAbgr },
            {  0.5f,  0.5f, 0.0f, 1.0f, 0.0f, whiteAbgr },
            { -0.5f,  0.5f, 0.0f, 0.0f, 0.0f, whiteAbgr },
        };
        uint16_t indices[6] = { 0,1,2, 0,2,3 };
        const bgfx::Memory* vmem = bgfx::copy(verts, sizeof(verts));
        const bgfx::Memory* imem = bgfx::copy(indices, sizeof(indices));
        bgfx::VertexLayout quadLayout;
        quadLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true, true)
            .end();
        m_ParticleQuadVbh = bgfx::createVertexBuffer(vmem, quadLayout);
        m_ParticleQuadIbh = bgfx::createIndexBuffer(imem);
    }

    bgfx::setVertexBuffer(0, m_ParticleQuadVbh);
    bgfx::setInstanceDataBuffer(&idb);
    bgfx::setIndexBuffer(m_ParticleQuadIbh);

    // Bind texture (per system) or white
    bgfx::TextureHandle tex = bgfx::isValid(ps.Texture) ? ps.Texture : m_WhiteTexture;
    bgfx::setTexture(0, s_ParticleTexture, tex);

#endif

// ---------------- Light Management ----------------
void Renderer::UploadLightsToShader(const std::vector<LightData>& lights) {
    glm::vec4 colors[4], positions[4], params[4];
    
    for (int i = 0; i < 4; ++i) {
        if (i < lights.size()) {
            const LightData& light = lights[i];
            
            // Color with intensity in alpha
            colors[i] = glm::vec4(light.color, 1.0f);
            
            if (light.type == LightType::Directional) {
                // For directional lights: xyz = direction, w = 0 (directional)
                positions[i] = glm::vec4(light.direction, 0.0f);
            } else {
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

void Renderer::UploadEnvironmentToShader(const Environment& env)
{
    // Pack ambient color * intensity in xyz, w = flags (bit0: fog enabled)
    glm::vec3 ambient = env.AmbientColor * env.AmbientIntensity;
    float flags = env.EnableFog ? 1.0f : 0.0f;
    glm::vec4 ambientFog(ambient, flags);
    bgfx::setUniform(u_AmbientFog, &ambientFog);

    // Fog params: x = density, yzw = fog color
    glm::vec4 fogParams(env.FogDensity, env.FogColor.r, env.FogColor.g, env.FogColor.b);
    bgfx::setUniform(u_FogParams, &fogParams);
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
    auto floorTo = [&](float v){ return std::floor(v / step) * step; };
    auto ceilTo  = [&](float v){ return std::ceil (v / step) * step; };
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





void Renderer::DrawDebugRay(const glm::vec3& origin, const glm::vec3& dir, float length) {
    // TODO: Implement line vertex buffer for debug rendering
}

void Renderer::DrawCollider(const ColliderComponent& collider, const TransformComponent& transform) {
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
            boxVertices.push_back({-halfSizeX, -halfSizeY, -halfSizeZ});
            boxVertices.push_back({ halfSizeX, -halfSizeY, -halfSizeZ});
            boxVertices.push_back({ halfSizeX, -halfSizeY, -halfSizeZ});
            boxVertices.push_back({ halfSizeX,  halfSizeY, -halfSizeZ});
            boxVertices.push_back({ halfSizeX,  halfSizeY, -halfSizeZ});
            boxVertices.push_back({-halfSizeX,  halfSizeY, -halfSizeZ});
            boxVertices.push_back({-halfSizeX,  halfSizeY, -halfSizeZ});
            boxVertices.push_back({-halfSizeX, -halfSizeY, -halfSizeZ});

            // Back face
            boxVertices.push_back({-halfSizeX, -halfSizeY,  halfSizeZ});
            boxVertices.push_back({ halfSizeX, -halfSizeY,  halfSizeZ});
            boxVertices.push_back({ halfSizeX, -halfSizeY,  halfSizeZ});
            boxVertices.push_back({ halfSizeX,  halfSizeY,  halfSizeZ});
            boxVertices.push_back({ halfSizeX,  halfSizeY,  halfSizeZ});
            boxVertices.push_back({-halfSizeX,  halfSizeY,  halfSizeZ});
            boxVertices.push_back({-halfSizeX,  halfSizeY,  halfSizeZ});
            boxVertices.push_back({-halfSizeX, -halfSizeY,  halfSizeZ});

            // Connecting lines
            boxVertices.push_back({-halfSizeX, -halfSizeY, -halfSizeZ});
            boxVertices.push_back({-halfSizeX, -halfSizeY,  halfSizeZ});
            boxVertices.push_back({ halfSizeX, -halfSizeY, -halfSizeZ});
            boxVertices.push_back({ halfSizeX, -halfSizeY,  halfSizeZ});
            boxVertices.push_back({ halfSizeX,  halfSizeY, -halfSizeZ});
            boxVertices.push_back({ halfSizeX,  halfSizeY,  halfSizeZ});
            boxVertices.push_back({-halfSizeX,  halfSizeY, -halfSizeZ});
            boxVertices.push_back({-halfSizeX,  halfSizeY,  halfSizeZ});

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
                capsuleVertices.push_back({x1, halfHeight, z1});
                capsuleVertices.push_back({x2, halfHeight, z2});

                // Bottom circle
                capsuleVertices.push_back({x1, -halfHeight, z1});
                capsuleVertices.push_back({x2, -halfHeight, z2});

                // Connecting lines
                capsuleVertices.push_back({x1, halfHeight, z1});
                capsuleVertices.push_back({x1, -halfHeight, z1});
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

// --------------------------------------
// Draw simple wireframe outline around selected entity's mesh (editor only)
// --------------------------------------
void Renderer::DrawEntityOutline(Scene& scene, EntityID selectedEntity) {
    if (selectedEntity < 0 || scene.m_IsPlaying) return;
    auto* data = scene.GetEntityData(selectedEntity);
    if (!data || !data->Visible || !data->Mesh || !data->Mesh->mesh) return;

    std::shared_ptr<Mesh> meshPtr = data->Mesh->mesh;
    if (!meshPtr) return;

    bool meshValid = meshPtr->Dynamic ? bgfx::isValid(meshPtr->dvbh) : bgfx::isValid(meshPtr->vbh);
    if (!meshValid || !bgfx::isValid(meshPtr->ibh)) return;

    // Slightly scale up to create visible thickness
    float transform[16];
    glm::mat4 scaled = data->Transform.WorldMatrix * glm::scale(glm::mat4(1.0f), glm::vec3(1.03f));
    memcpy(transform, glm::value_ptr(scaled), sizeof(float) * 16);
    bgfx::setTransform(transform);

    if (meshPtr->Dynamic)
        bgfx::setVertexBuffer(0, meshPtr->dvbh, 0, meshPtr->numVertices);
    else
        bgfx::setVertexBuffer(0, meshPtr->vbh);
    bgfx::setIndexBuffer(meshPtr->ibh);

    // Render backfaces only to show silhouette, depth test on
    uint64_t state =
        BGFX_STATE_WRITE_RGB |
        BGFX_STATE_DEPTH_TEST_LEQUAL |
        BGFX_STATE_CULL_CW;
    bgfx::setState(state);

    // Set outline color to orange
    glm::vec4 outlineColor = { 1.0f, 0.55f, 0.0f, 1.0f };
    bgfx::setUniform(u_outlineColor, &outlineColor);
    bgfx::submit(1, m_OutlineProgram);
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

