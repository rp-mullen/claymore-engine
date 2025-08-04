#include "Renderer.h"
#include <bgfx/platform.h>
#include <glm/gtc/type_ptr.hpp>
#include "ShaderManager.h"
#include "Picking.h"
#include "MaterialManager.h"
#include "VertexTypes.h"
#include <bx/math.h>

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

    m_Camera = new Camera(60.0f, float(width) / float(height), 0.1f, 100.0f);


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
    
    // Debug line program
    m_DebugLineProgram = ShaderManager::Instance().LoadProgram("vs_debug", "fs_debug");
	InitGrid(20.0f, 1.0f);

    u_LightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4, 4);
    u_LightDirection = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4, 4);
    u_LightPosition = bgfx::createUniform("u_lightPos", bgfx::UniformType::Vec4, 4);
    u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);


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

    m_Camera->SetViewportSize((float)width, (float)height);


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
    glm::mat4 view = m_Camera->GetViewMatrix();
    glm::mat4 proj = m_Camera->GetProjectionMatrix();
    memcpy(m_view, glm::value_ptr(view), sizeof(float) * 16);
    memcpy(m_proj, glm::value_ptr(proj), sizeof(float) * 16);
    bgfx::setViewTransform(0, m_view, m_proj);
    bgfx::setViewTransform(1, m_view, m_proj);  // Set camera matrices for view 1 too!

    glm::vec4 camPos(Renderer::Get().GetCamera()->GetPosition(), 1.0f);
    bgfx::setUniform(u_cameraPos, &camPos);

    // --------------------------------------
    // Collect lights from ECS
    // --------------------------------------
    std::vector<LightData> lights;
    lights.reserve(4); // Max 4 lights for now
    for (auto& entity : scene.GetEntities()) {
        auto* data = scene.GetEntityData(entity.GetID());
        if (!data || !data->Light) continue;

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
        }
        else {
            ld.direction = glm::vec3(0.0f); // Not used for point lights
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
    for (auto& entity : scene.GetEntities()) {
        auto* data = scene.GetEntityData(entity.GetID());
        if (!data || !data->Mesh) continue;

		bool meshValid = data->Mesh->mesh->Dynamic ? bgfx::isValid(data->Mesh->mesh->dvbh) : bgfx::isValid(data->Mesh->mesh->vbh);
		if (!meshValid || !bgfx::isValid(data->Mesh->mesh->ibh)) {
			std::cerr << "Invalid mesh for entity " << entity.GetID() << "\n";
			continue;
		}

        float transform[16];
        memcpy(transform, glm::value_ptr(data->Transform.WorldMatrix), sizeof(float) * 16);
        

        
        // Only print debug info for FBX entities (ID > 1) and only once
        if (entity.GetID() > 1) {
            static bool printedDebug = false;
            if (!printedDebug) {
                std::cout << "[Renderer] FBX Entity " << entity.GetID() 
                          << " - Vertices: " << data->Mesh->mesh->Vertices.size()
                          << ", Indices: " << data->Mesh->mesh->Indices.size()
                          << ", Position: (" << data->Transform.Position.x << ", " 
                          << data->Transform.Position.y << ", " << data->Transform.Position.z << ")"
                          << ", Bounds: (" << data->Mesh->mesh->BoundsMin.x << ", " 
                          << data->Mesh->mesh->BoundsMin.y << ", " << data->Mesh->mesh->BoundsMin.z << ") to ("
                          << data->Mesh->mesh->BoundsMax.x << ", " 
                          << data->Mesh->mesh->BoundsMax.y << ", " << data->Mesh->mesh->BoundsMax.z << ")" << std::endl;
                printedDebug = true;
            }
        }
 
        DrawMesh(*data->Mesh->mesh.get(), transform, *data->Mesh->material);
    }
	
}


// ---------------- Mesh Submission ----------------
void Renderer::DrawMesh(const Mesh& mesh, const float* transform, const Material& material) {
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

// ---------------- Light Management ----------------
void Renderer::UploadLightsToShader(const std::vector<LightData>& lights) {
    glm::vec4 colors[4], dirs[4];
    for (int i = 0; i < 4; ++i) {
        if (i < lights.size()) {
            colors[i] = glm::vec4(lights[i].color, 1.0f);
            dirs[i] = glm::vec4(lights[i].direction, 0.0f);
        }
        else {
            colors[i] = glm::vec4(0.0f);
            dirs[i] = glm::vec4(0.0f);
        }
    }
    if (!lights.empty()) {
        glm::vec4 color(lights[0].color, 1.0f);
        glm::vec4 dir(lights[0].direction, 0.0f);
        bgfx::setUniform(u_LightColor, &color);
        bgfx::setUniform(u_LightDirection, &dir);
    }
    else {
        glm::vec4 black(0.0f);
        bgfx::setUniform(u_LightColor, &black);
        bgfx::setUniform(u_LightDirection, &black);
    }

}

void Renderer::DrawGrid() {
    if (!bgfx::isValid(m_GridVB)) return;

    /*bgfx::setViewFrameBuffer(0, m_SceneFrameBuffer);*/
    bgfx::setViewTransform(0, m_view, m_proj);
    bgfx::setViewRect(0, 0, 0, m_Width, m_Height);

    float identity[16];
    bx::mtxIdentity(identity);
    bgfx::setTransform(identity);

    bgfx::setVertexBuffer(0, m_GridVB);

    auto debugMat = MaterialManager::Instance().CreateDefaultDebugMaterial();
    debugMat->BindUniforms();
    bgfx::setState(
       BGFX_STATE_WRITE_RGB |
       BGFX_STATE_WRITE_Z |              // ← Enable depth write!
       BGFX_STATE_DEPTH_TEST_LEQUAL |      // ← Enable proper depth testing
       BGFX_STATE_PT_LINES);
    bgfx::submit(0, debugMat->GetProgram());
}





void Renderer::DrawDebugRay(const glm::vec3& origin, const glm::vec3& dir, float length) {
    // TODO: Implement line vertex buffer for debug rendering
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