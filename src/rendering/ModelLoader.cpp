// ModelLoader.cpp
#include "ModelLoader.h"
#include "VertexTypes.h"
#include "MaterialManager.h"
#include "PBRMaterial.h"
#include "SkinnedPBRMaterial.h"
#include "TextureLoader.h"
#include "ShaderManager.h"
#include "ecs/AnimationComponents.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include "io/FileSystem.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

// --------------------------------- Helpers ---------------------------------
static glm::mat4 AiToGlmTransposed(const aiMatrix4x4& m)
{
    // Assimp matrices are row-major; GLM is column-major -> transpose the constructed mat.
    glm::mat4 mat(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
    return glm::transpose(mat);
}

static inline glm::mat4 AiToGlm(const aiMatrix4x4& m)
{
    // GLM's 16-float ctor is column-major: we pass columns (a*, b*, c*, d*)
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}


static std::string GetTexPath(const aiMaterial* mat, aiTextureType type)
{
    if (!mat) return {};
    aiString str;
    if (AI_SUCCESS == mat->GetTexture(type, 0, &str))
        return std::string(str.C_Str());
    return {};
}

// Some glTF 2.0 packs ORM; this tries common slots in reasonable order.
static void ExtractPbrTextures(const aiMaterial* aim,
    std::string& albedo,
    std::string& metallicRoughness,
    std::string& normal)
{
    // Base color or Diffuse
    albedo = GetTexPath(aim, aiTextureType_BASE_COLOR);
    if (albedo.empty()) albedo = GetTexPath(aim, aiTextureType_DIFFUSE);

    // Metallic-Roughness (try specific then fallbacks)
    metallicRoughness = GetTexPath(aim, aiTextureType_UNKNOWN);
    if (metallicRoughness.empty()) metallicRoughness = GetTexPath(aim, aiTextureType_METALNESS);
    if (metallicRoughness.empty()) metallicRoughness = GetTexPath(aim, aiTextureType_DIFFUSE_ROUGHNESS);

    // Normal map
    normal = GetTexPath(aim, aiTextureType_NORMALS);
    if (normal.empty()) normal = GetTexPath(aim, aiTextureType_HEIGHT); // some exporters misuse this
}

static void ApplyTexturesToMaterial(Material* mat, const std::string& baseDir,
    const std::string& albedo,
    const std::string& mr,
    const std::string& normal)
{
    auto* pbr = dynamic_cast<PBRMaterial*>(mat);
    if (!pbr) return;

    auto resolve = [&](const std::string& rel) -> std::string
        {
            if (rel.empty()) return {};
            if (baseDir.empty()) return rel;
            return (std::filesystem::path(baseDir) / rel).string();
        };

    if (!albedo.empty())
    {
        std::string p = resolve(albedo);
        auto t = TextureLoader::Load2D(p.c_str());
        if (bgfx::isValid(t)) { pbr->SetAlbedoTexture(t); pbr->SetAlbedoTextureFromPath(p); }
    }
    if (!mr.empty())
    {
        std::string p = resolve(mr);
        auto t = TextureLoader::Load2D(p.c_str());
        if (bgfx::isValid(t)) { pbr->SetMetallicRoughnessTexture(t); pbr->SetMetallicRoughnessTextureFromPath(p); }
    }
    if (!normal.empty())
    {
        std::string p = resolve(normal);
        auto t = TextureLoader::Load2D(p.c_str());
        if (bgfx::isValid(t)) { pbr->SetNormalTexture(t); pbr->SetNormalTextureFromPath(p); }
    }
}

static inline bool IsFinite3(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// --------------------------------- Loader ----------------------------------
Model ModelLoader::LoadModel(const std::string& filepath)
{
    // Initialize the predefined layouts once (from VertexTypes.h)
    static bool layoutsInit = false;
    if (!layoutsInit)
    {
        PBRVertex::Init();
        SkinnedPBRVertex::Init();
        layoutsInit = true;
    }

    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    // If file is inside a mounted pak, extract to temp cache before loading via Assimp
    std::string openPath = filepath;
    if (!std::filesystem::exists(openPath)) {
        std::vector<uint8_t> bytes;
        if (FileSystem::Instance().ReadFile(filepath, bytes)) {
            std::filesystem::path cacheDir = std::filesystem::temp_directory_path() / "claymore_pak_cache";
            std::error_code ec; std::filesystem::create_directories(cacheDir, ec);
            size_t h = std::hash<std::string>{}(filepath);
            std::string ext = std::filesystem::path(filepath).extension().string();
            std::filesystem::path outPath = cacheDir / ("model_" + std::to_string(h) + ext);
            std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
            if (out.is_open()) {
                if (!bytes.empty()) out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                out.close();
                openPath = outPath.string();
            }
        }
    }

    const aiScene* scene = importer.ReadFile(
        openPath.c_str(),
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
		aiProcess_LimitBoneWeights |
        aiProcess_GlobalScale
    );

    Model result;
    if (!scene || !scene->mRootNode)
    {
        std::cerr << "[ModelLoader] Failed to load: " << filepath
            << " (" << importer.GetErrorString() << ")\n";
        return result;
    }

    // Determine unit scale from source (e.g., FBX UnitScaleFactor). Default to 1.0.
    float importScale = 1.0f;
    if (scene->mMetaData)
    {
        double unitScaleDouble = 1.0;
        if (scene->mMetaData->Get("UnitScaleFactor", unitScaleDouble))
            importScale = static_cast<float>(unitScaleDouble);
    }

    const std::string baseDir = std::filesystem::path(filepath).parent_path().string();

    // ---------------- Scene-wide bone prepass (stable indices across submeshes) ----------------
    std::unordered_map<std::string, uint32_t> boneIndexMap;
    result.BoneNames.clear();
    result.InverseBindPoses.clear();

    auto registerBone = [&](const aiBone* abone) -> uint32_t
        {
            std::string name = abone->mName.C_Str();
            auto it = boneIndexMap.find(name);
            if (it != boneIndexMap.end()) return it->second;

            uint32_t idx = (uint32_t)result.BoneNames.size();
            boneIndexMap.emplace(name, idx);
            result.BoneNames.push_back(name);

            // Use raw construction (no extra transpose) to match skinning convention elsewhere
            glm::mat4 offset = AiToGlm(abone->mOffsetMatrix);
            result.InverseBindPoses.push_back(offset);

            return idx;
        };

    for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi)
    {
        const aiMesh* m = scene->mMeshes[mi];
        if (m->HasBones())
        {
            for (unsigned b = 0; b < m->mNumBones; ++b)
                (void)registerBone(m->mBones[b]);
        }
    }

    // ---------------- Convert meshes ----------------
    result.Meshes.reserve(scene->mNumMeshes);
    result.Materials.reserve(scene->mNumMeshes);
    result.BlendShapes.reserve(scene->mNumMeshes);

    // Import option: flip Y coordinate to convert between up-axis conventions (non-skinned meshes only)
    constexpr bool kFlipYOnImport = true;

    for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi)
    {
        aiMesh* aMesh = scene->mMeshes[mi];
        const bool hasSkin = (aMesh->mNumBones > 0);
        const bool flipThisMesh = (kFlipYOnImport && !hasSkin);

        // ---- Material (create first so we can attach textures later)
        std::shared_ptr<Material> mat;
        if (hasSkin)
        {
            auto prog = ShaderManager::Instance().LoadProgram("vs_pbr_skinned", "fs_pbr_skinned");
            mat = std::make_shared<SkinnedPBRMaterial>("SkinnedPBR", prog);
        }
        else
        {
            // Regular PBR; CPU morphs are fine with static shader
            mat = MaterialManager::Instance().CreateDefaultPBRMaterial();
        }

        // Extract & apply textures (glTF/FBX common slots)
        if (scene->HasMaterials() && aMesh->mMaterialIndex < scene->mNumMaterials)
        {
            std::string albedo, mr, normal;
            ExtractPbrTextures(scene->mMaterials[aMesh->mMaterialIndex], albedo, mr, normal);

            // Attempt to map material textures to extracted assets under assets/textures/<modelName>
            auto* pbr = dynamic_cast<PBRMaterial*>(mat.get());
            if (pbr)
            {
                const std::string modelName = std::filesystem::path(filepath).stem().string();
                const std::filesystem::path texDir = std::filesystem::path("assets") / "textures" / modelName;
                auto mapToExtracted = [&](const std::string& src) -> std::string {
                    if (src.empty()) return {};
                    std::string fname = std::filesystem::path(src).filename().string();
                    std::filesystem::path candidate = texDir / fname;
                    std::error_code ec; // avoid exceptions on missing paths
                    if (std::filesystem::exists(candidate, ec)) return candidate.string();
                    return {};
                };

                auto trySet = [&](const std::string& mapped, void(PBRMaterial::*setTexPath)(const std::string&)) {
                    if (!mapped.empty()) (pbr->*setTexPath)(mapped);
                };

                // Prefer mapped extracted textures; fall back to model-relative paths
                std::string albedoMapped = mapToExtracted(albedo);
                std::string mrMapped     = mapToExtracted(mr);
                std::string normalMapped = mapToExtracted(normal);

                if (!albedoMapped.empty() || !mrMapped.empty() || !normalMapped.empty())
                {
                    trySet(albedoMapped, &PBRMaterial::SetAlbedoTextureFromPath);
                    trySet(mrMapped,     &PBRMaterial::SetMetallicRoughnessTextureFromPath);
                    trySet(normalMapped, &PBRMaterial::SetNormalTextureFromPath);
                }
                else
                {
                    // No extracted match; use original relative references if present
                    ApplyTexturesToMaterial(mat.get(), baseDir, albedo, mr, normal);
                }
            }
        }

        // ---- CPU vertex arrays (final packed types for GPU upload)
        std::vector<PBRVertex>           vertices;
        std::vector<SkinnedPBRVertex>    skVertices;
        std::vector<uint32_t>            indices32;
        // CPU caches for morph blending (non-skinned meshes store base data here)
        std::vector<glm::vec3>           meshVerticesCPU; meshVerticesCPU.reserve(aMesh->mNumVertices);
        std::vector<glm::vec3>           meshNormalsCPU;  meshNormalsCPU.reserve(aMesh->mNumVertices);
        std::vector<glm::vec2>           meshUVsCPU;      meshUVsCPU.reserve(aMesh->mNumVertices);

        vertices.reserve(hasSkin ? 0 : aMesh->mNumVertices);
        skVertices.reserve(hasSkin ? aMesh->mNumVertices : 0);
        indices32.reserve(aMesh->mNumFaces * 3);

        // ---- Gather raw per-vertex base attributes
        // Also collect skinning weights/indices as top-4
        std::vector<glm::vec4>   vertWeights(aMesh->mNumVertices, glm::vec4(0.0f));
        std::vector<glm::ivec4>  vertIndices(aMesh->mNumVertices, glm::ivec4(0));

        // Pre-fill base attributes (pos/norm/uv)
        for (unsigned i = 0; i < aMesh->mNumVertices; ++i)
        {
            aiVector3D pos = aMesh->mVertices[i];
            aiVector3D norm = aMesh->mNormals ? aMesh->mNormals[i] : aiVector3D(0, 1, 0);
            aiVector3D uv = aMesh->HasTextureCoords(0) ? aMesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);

            float u = std::isfinite(uv.x) ? uv.x : 0.0f;
            float v = std::isfinite(uv.y) ? uv.y : 0.0f;

            glm::vec3 normal(norm.x, norm.y, norm.z);
            if (!IsFinite3(normal) || glm::length(normal) < 0.001f)
                normal = glm::vec3(0.0f, 1.0f, 0.0f);
            else
                normal = glm::normalize(normal);

            if (flipThisMesh) {
                pos.y = -pos.y;
                normal.y = -normal.y;
            }

            // Always keep a CPU copy of base data for morph blending (skinned and non-skinned)
            meshVerticesCPU.push_back(glm::vec3(pos.x, pos.y, pos.z));
            meshNormalsCPU.push_back(glm::vec3(normal.x, normal.y, normal.z));
            meshUVsCPU.push_back(glm::vec2(u, v));

            if (!hasSkin)
            {
                // PBRVertex from VertexTypes.h: (x,y,z, nx,ny,nz, u,v)
                vertices.push_back({
                    pos.x , pos.y , pos.z,
                    normal.x, normal.y, normal.z, u, v
                    });
            }
        }

        // ---- Skinning data: accumulate top-4 weights using scene-wide bone indices
        if (hasSkin)
        {
            for (unsigned b = 0; b < aMesh->mNumBones; ++b)
            {
                const aiBone* bone = aMesh->mBones[b];
                uint32_t boneIndex = boneIndexMap[bone->mName.C_Str()];

                for (unsigned w = 0; w < bone->mNumWeights; ++w)
                {
                    const aiVertexWeight& vw = bone->mWeights[w];
                    const unsigned vId = vw.mVertexId;
                    const float weight = vw.mWeight;
                    if (vId >= vertWeights.size() || weight <= 0.0f) continue;

                    // Replace smallest slot if this weight is larger
                    int slot = 0;
                    float smallest = vertWeights[vId][0];
                    for (int s = 1; s < 4; ++s)
                    {
                        if (vertWeights[vId][s] < smallest) { smallest = vertWeights[vId][s]; slot = s; }
                    }
                    if (weight > smallest)
                    {
                        vertWeights[vId][slot] = weight;
                        vertIndices[vId][slot] = (int)boneIndex;
                    }
                }
            }

            // Clamp to palette & normalize weights
            constexpr int kMaxBones = (int)SkinnedPBRMaterial::MaxBones;
            for (size_t v = 0; v < vertWeights.size(); ++v)
            {
                for (int s = 0; s < 4; ++s)
                {
                    if (vertIndices[v][s] < 0 || vertIndices[v][s] >= kMaxBones)
                    {
                        vertWeights[v][s] = 0.0f;
                        vertIndices[v][s] = 0;
                    }
                }
                float sum = vertWeights[v].x + vertWeights[v].y + vertWeights[v].z + vertWeights[v].w;
                if (sum > 0.0001f) vertWeights[v] /= sum;
                else { vertWeights[v].x = 1.0f; vertIndices[v].x = 0; }
            }

            // Build SkinnedPBRVertex array
            skVertices.reserve(aMesh->mNumVertices);
            for (unsigned i = 0; i < aMesh->mNumVertices; ++i)
            {
                aiVector3D pos = aMesh->mVertices[i];
                aiVector3D norm = aMesh->mNormals ? aMesh->mNormals[i] : aiVector3D(0, 1, 0);
                aiVector3D uv = aMesh->HasTextureCoords(0) ? aMesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);

                float u = std::isfinite(uv.x) ? uv.x : 0.0f;
                float v = std::isfinite(uv.y) ? uv.y : 0.0f;

                glm::vec3 normal(norm.x, norm.y, norm.z);
                if (!IsFinite3(normal) || glm::length(normal) < 0.001f)
                    normal = glm::vec3(0.0f, 1.0f, 0.0f);
                else
                    normal = glm::normalize(normal);

                if (flipThisMesh) {
                    pos.y = -pos.y;
                    normal.y = -normal.y;
                }

                const glm::ivec4& bi = vertIndices[i];
                const glm::vec4& bw = vertWeights[i];

                // SkinnedPBRVertex from VertexTypes.h:
                // (x,y,z, nx,ny,nz, u,v, i0,i1,i2,i3, w0,w1,w2,w3)
                skVertices.push_back({
                    pos.x , pos.y, pos.z,
                    normal.x, normal.y, normal.z,
                    u, v,
                    (uint8_t)bi.x, (uint8_t)bi.y, (uint8_t)bi.z, (uint8_t)bi.w,
                    bw.x, bw.y, bw.z, bw.w
                    });
            }
        }

        // ---- Indices
        for (unsigned f = 0; f < aMesh->mNumFaces; ++f)
        {
            const aiFace& face = aMesh->mFaces[f];
            if (face.mNumIndices != 3) continue;
            if (flipThisMesh) {
                // Preserve front-face after axis flip by reversing winding
                indices32.push_back((uint32_t)face.mIndices[0]);
                indices32.push_back((uint32_t)face.mIndices[2]);
                indices32.push_back((uint32_t)face.mIndices[1]);
            } else {
                indices32.push_back((uint32_t)face.mIndices[0]);
                indices32.push_back((uint32_t)face.mIndices[1]);
                indices32.push_back((uint32_t)face.mIndices[2]);
            }
        }

        // ---- Create GPU buffers (use predefined layouts from VertexTypes.h)
        const bgfx::Memory* vbMem = nullptr;
        const bgfx::VertexLayout* layoutPtr = nullptr;

        if (hasSkin)
        {
            vbMem = bgfx::copy(skVertices.data(), (uint32_t)(sizeof(SkinnedPBRVertex) * skVertices.size()));
            layoutPtr = &SkinnedPBRVertex::layout;
        }
        else
        {
            vbMem = bgfx::copy(vertices.data(), (uint32_t)(sizeof(PBRVertex) * vertices.size()));
            layoutPtr = &PBRVertex::layout;
        }

        std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();

        // Dynamic if skinned or has blendshapes (CPU updates)
        if (hasSkin || aMesh->mNumAnimMeshes > 0)
        {
            mesh->dvbh = bgfx::createDynamicVertexBuffer(vbMem, *layoutPtr);
            mesh->Dynamic = true;
        }
        else
        {
            mesh->vbh = bgfx::createVertexBuffer(vbMem, *layoutPtr);
            mesh->Dynamic = false;
        }

        // Choose 16-bit vs 32-bit indices
        uint32_t maxIndex = 0;
        for (uint32_t idx : indices32) maxIndex = std::max(maxIndex, idx);
        const bool use32 = (maxIndex >= 65536u);

        if (use32)
        {
            const bgfx::Memory* imem = bgfx::copy(indices32.data(), (uint32_t)(indices32.size() * sizeof(uint32_t)));
            mesh->ibh = bgfx::createIndexBuffer(imem, BGFX_BUFFER_INDEX32);
        }
        else
        {
            std::vector<uint16_t> idx16; idx16.reserve(indices32.size());
            for (uint32_t v : indices32) idx16.push_back((uint16_t)v);
            const bgfx::Memory* imem = bgfx::copy(idx16.data(), (uint32_t)(idx16.size() * sizeof(uint16_t)));
            mesh->ibh = bgfx::createIndexBuffer(imem);
        }

        mesh->numVertices = hasSkin ? (uint32_t)skVertices.size() : (uint32_t)vertices.size();
        mesh->numIndices = (uint32_t)indices32.size();

        // Validate handles
        bool vbValid = mesh->Dynamic ? bgfx::isValid(mesh->dvbh) : bgfx::isValid(mesh->vbh);
        if (!vbValid || !bgfx::isValid(mesh->ibh))
        {
            std::cerr << "[ModelLoader] ERROR: Failed to create GPU buffers for mesh '"
                << aMesh->mName.C_Str() << "'\n";
            continue;
        }

        // ---- CPU-side data (for picking/AABB/debug)
        mesh->Vertices.reserve(aMesh->mNumVertices);
        mesh->Normals.reserve(aMesh->mNumVertices);
        mesh->BoneWeights = vertWeights;
        mesh->BoneIndices = vertIndices;

        for (unsigned i = 0; i < aMesh->mNumVertices; ++i)
        {
            aiVector3D p = aMesh->mVertices[i];
            if (flipThisMesh) p.y = -p.y;
            mesh->Vertices.emplace_back(p.x , p.y , p.z );

            aiVector3D n = aMesh->mNormals ? aMesh->mNormals[i] : aiVector3D(0, 0, 1);
            if (flipThisMesh) n.y = -n.y;
            mesh->Normals.emplace_back(n.x, n.y, n.z);
        }
        mesh->Indices = indices32;

        // Sanity: indices within range
        if (!indices32.empty())
        {
            uint32_t maxIdxCpu = *std::max_element(indices32.begin(), indices32.end());
            const size_t vcount = mesh->numVertices;
            if (maxIdxCpu >= vcount)
            {
                std::cerr << "[ModelLoader] ERROR: Mesh '" << aMesh->mName.C_Str()
                    << "' has out-of-bounds index " << maxIdxCpu
                    << " (vertex count = " << vcount << ")\n";
            }
        }

        mesh->ComputeBounds();

        // ---- Blend Shapes (Morph Targets)
        BlendShapeComponent blendComp;
        if (aMesh->mNumAnimMeshes > 0)
        {
            for (unsigned a = 0; a < aMesh->mNumAnimMeshes; ++a)
            {
                aiAnimMesh* anim = aMesh->mAnimMeshes[a];
                BlendShape bs;
                bs.Name = anim->mName.C_Str();
                bs.DeltaPos.reserve(aMesh->mNumVertices);
                bs.DeltaNormal.reserve(aMesh->mNumVertices);

                for (unsigned v = 0; v < aMesh->mNumVertices; ++v)
                {
                    // Assimp stores target positions; convert to deltas relative to base mesh
                    const glm::vec3 baseP = (v < meshVerticesCPU.size()) ? meshVerticesCPU[v] : glm::vec3(0);
                    aiVector3D ap = anim->mVertices[v];
                    if (flipThisMesh) ap.y = -ap.y; // match axis flip applied to base
                    const glm::vec3 tgtP(ap.x, ap.y, ap.z);
                    bs.DeltaPos.emplace_back(tgtP - baseP);

                    const glm::vec3 baseN = (v < meshNormalsCPU.size()) ? meshNormalsCPU[v] : glm::vec3(0, 1, 0);
                    aiVector3D an = anim->mNormals ? anim->mNormals[v] : aiVector3D(0, 0, 0);
                    if (flipThisMesh) an.y = -an.y;
                    const glm::vec3 tgtN(an.x, an.y, an.z);
                    bs.DeltaNormal.emplace_back(tgtN - baseN);
                }
                blendComp.Shapes.push_back(std::move(bs));
            }
        }

        // Fill CPU caches for morph blending (both skinned and non-skinned)
        mesh->Vertices = std::move(meshVerticesCPU);
        mesh->Normals  = std::move(meshNormalsCPU);
        mesh->UVs      = std::move(meshUVsCPU);

        // ---- Append to result
        result.Meshes.push_back(mesh);
        result.Materials.push_back(mat);
        result.BlendShapes.push_back(std::move(blendComp));

        // Debug (optional):
        std::cout << "[ModelLoader] Mesh '" << aMesh->mName.C_Str()
            << "' verts=" << mesh->numVertices
            << " indices=" << mesh->numIndices
            << " faces=" << aMesh->mNumFaces
            << " skinned=" << (hasSkin ? "yes" : "no")
            << " animMeshes=" << aMesh->mNumAnimMeshes
            << "\n";
    }

    return result;
}
