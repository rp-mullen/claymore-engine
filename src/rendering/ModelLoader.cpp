#include "ModelLoader.h"
#include "VertexTypes.h"
#include "MaterialManager.h"
#include "SkinnedPBRMaterial.h"
#include "TextureLoader.h"
#include "ShaderManager.h"
#include "ecs/AnimationComponents.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

Model ModelLoader::LoadModel(const std::string& filepath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath,
       aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices |
        aiProcess_FixInfacingNormals | aiProcess_FlipWindingOrder);

   float importScale = 1.f;

   // Ensure vertex layouts are initialized once
   static bool layoutsInit = false;
   if (!layoutsInit) {
       PBRVertex::Init();
       SkinnedPBRVertex::Init();
       layoutsInit = true;
   }

   Model result;
   if (!scene || !scene->mRootNode) {
      std::cerr << "Failed to load model: " << filepath << std::endl;
      return result;
      }

   std::string directory = std::filesystem::path(filepath).parent_path().string();

   for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
      aiMesh* aMesh = scene->mMeshes[m];
      std::vector<PBRVertex> vertices;
      std::vector<SkinnedPBRVertex> skVertices;
      std::vector<uint16_t> indices;

      bool hasSkin = aMesh->mNumBones > 0;
      if (hasSkin) skVertices.reserve(aMesh->mNumVertices);
      else         vertices.reserve(aMesh->mNumVertices);
        std::vector<glm::vec4> vertWeights(aMesh->mNumVertices, glm::vec4(0.0f));
        std::vector<glm::ivec4> vertIndices(aMesh->mNumVertices, glm::ivec4(0));
      for (unsigned int i = 0; i < aMesh->mNumVertices; i++) {
         aiVector3D pos = aMesh->mVertices[i] ;
         aiVector3D norm = aMesh->mNormals[i];
         aiVector3D uv = aMesh->HasTextureCoords(0) ? aMesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);

         if (!aMesh->HasNormals())
             std::cerr << "[ModelLoader] WARNING: Mesh '" << aMesh->mName.C_Str() << "' is missing normals.\n";

         if (!aMesh->HasTextureCoords(0))
             std::cerr << "[ModelLoader] WARNING: Mesh '" << aMesh->mName.C_Str() << "' is missing UV0.\n";


                   // Ensure UV coordinates are valid (not NaN or infinite)
          float u = std::isfinite(uv.x) ? uv.x : 0.0f;
          float v = std::isfinite(uv.y) ? uv.y : 0.0f;
          
          // Ensure normal is valid and normalized
          glm::vec3 normal(norm.x, norm.y, norm.z);
          if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z) || 
              glm::length(normal) < 0.001f) {
              normal = glm::vec3(0.0f, 1.0f, 0.0f); // Default up normal
          } else {
              normal = glm::normalize(normal);
          }
          
          if (!hasSkin) {
            vertices.push_back({ pos.x, pos.y, pos.z, normal.x, normal.y, normal.z, u, v });
        }
         }

      // ---------------- Skinning data ----------------
        std::unordered_map<std::string,int> boneIndexMap;
        auto getBoneIndex=[&](const std::string& name)->int{
            auto it = std::find(result.BoneNames.begin(), result.BoneNames.end(), name);
            if(it!=result.BoneNames.end()) return int(it - result.BoneNames.begin());
            int idx = (int)result.BoneNames.size();
            result.BoneNames.push_back(name);
            result.InverseBindPoses.push_back(glm::mat4(1.0f)); // placeholder, will set later
            return idx;
        };
        if (hasSkin) {
            for (unsigned int b = 0; b < aMesh->mNumBones; ++b) {
                aiBone* bone = aMesh->mBones[b];
                int boneIndex = getBoneIndex(bone->mName.C_Str());
                // store inverse bind pose
                glm::mat4 offset = glm::make_mat4(&bone->mOffsetMatrix.a1);
                result.InverseBindPoses[boneIndex] = offset;
                for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
                    const aiVertexWeight& vw = bone->mWeights[w];
                    unsigned int vId = vw.mVertexId;
                    float weight = vw.mWeight;

                    // Pack top-4 influences – naive implementation
                    for (int slot = 0; slot < 4; ++slot) {
                        if (vertWeights[vId][slot] == 0.0f) {
                            vertWeights[vId][slot] = weight;
                            vertIndices[vId][slot] = boneIndex;
                            break;
                        }
                    }
                }
            }

            // --- Normalise weights so they sum to 1 ---
            for(size_t v = 0; v < vertWeights.size(); ++v)
            {
                float sum = vertWeights[v].x + vertWeights[v].y + vertWeights[v].z + vertWeights[v].w;
                if(sum > 0.0001f)
                {
                    vertWeights[v] /= sum;
                }
                else
                {
                    vertWeights[v].x = 1.0f; // fallback to first bone full weight
                }
            }
            }

        // ---------------- Build final vertex arrays ----------------
        if (hasSkin) {
            skVertices.clear();
            skVertices.reserve(aMesh->mNumVertices);
            for (unsigned int i = 0; i < aMesh->mNumVertices; ++i) {
                aiVector3D pos = aMesh->mVertices[i];
                aiVector3D norm = aMesh->mNormals ? aMesh->mNormals[i] : aiVector3D(0, 1, 0);
                aiVector3D uv = aMesh->HasTextureCoords(0) ? aMesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);
                float u = std::isfinite(uv.x) ? uv.x : 0.0f;
                float v = std::isfinite(uv.y) ? uv.y : 0.0f;
                glm::vec3 normal(norm.x, norm.y, norm.z);
                if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z) || glm::length(normal) < 0.001f)
                    normal = glm::vec3(0.0f, 1.0f, 0.0f);
                else
                    normal = glm::normalize(normal);

                const glm::ivec4& bIdx = vertIndices[i];
                const glm::vec4& bW  = vertWeights[i];
                skVertices.push_back({ pos.x, pos.y, pos.z,
                                       normal.x, normal.y, normal.z,
                                       u, v,
                                       (uint8_t)bIdx.x, (uint8_t)bIdx.y, (uint8_t)bIdx.z, (uint8_t)bIdx.w,
                                       bW.x, bW.y, bW.z, bW.w });
            }
        } else {
            vertices.clear();
            vertices.reserve(aMesh->mNumVertices);
            for (unsigned int i = 0; i < aMesh->mNumVertices; ++i) {
                aiVector3D pos = aMesh->mVertices[i];
                aiVector3D norm = aMesh->mNormals ? aMesh->mNormals[i] : aiVector3D(0, 1, 0);
                aiVector3D uv = aMesh->HasTextureCoords(0) ? aMesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);
                float u = std::isfinite(uv.x) ? uv.x : 0.0f;
                float v = std::isfinite(uv.y) ? uv.y : 0.0f;
                glm::vec3 normal(norm.x, norm.y, norm.z);
                if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z) || glm::length(normal) < 0.001f)
                    normal = glm::vec3(0.0f, 1.0f, 0.0f);
                else
                    normal = glm::normalize(normal);

                vertices.push_back({ pos.x, pos.y, pos.z, normal.x, normal.y, normal.z, u, v });
            }
        }

        for (unsigned int f = 0; f < aMesh->mNumFaces; f++) {
         const aiFace& face = aMesh->mFaces[f];
         for (unsigned int i = 0; i < face.mNumIndices; i++) {
            uint32_t originalIndex = face.mIndices[i];
            if (originalIndex > 65535) {
                std::cerr << "[ModelLoader] ERROR: Index " << originalIndex << " exceeds uint16_t range!\n";
                continue; // Skip this face
            }
            indices.push_back(static_cast<uint16_t>(originalIndex));
         }
         }

        // ---------- Create GPU buffers ----------
        const bgfx::Memory* vbMem = nullptr;
        const bgfx::VertexLayout* layoutPtr = nullptr;
        if (hasSkin) {
            vbMem = bgfx::copy(skVertices.data(), sizeof(SkinnedPBRVertex) * skVertices.size());
            layoutPtr = &SkinnedPBRVertex::layout;
        } else {
            vbMem = bgfx::copy(vertices.data(), sizeof(PBRVertex) * vertices.size());
            layoutPtr = &PBRVertex::layout;
        }

        std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();

        if (hasSkin || aMesh->mNumAnimMeshes > 0) {
            // Need a dynamic buffer so CPU can update (skinning or blendshapes)
            bgfx::DynamicVertexBufferHandle dvh = bgfx::createDynamicVertexBuffer(vbMem, *layoutPtr);
            mesh->dvbh = dvh;
            mesh->Dynamic = true;
        } else {
            bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(vbMem, *layoutPtr);
            mesh->vbh = vbh;
        }

      const bgfx::Memory* ibhMem = bgfx::copy(indices.data(), sizeof(uint16_t) * indices.size());
      bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(ibhMem);

      // handles assigned
      
      mesh->ibh = ibh;
      mesh->numVertices = hasSkin ? static_cast<uint32_t>(skVertices.size()) : static_cast<uint32_t>(vertices.size());
      mesh->numIndices = static_cast<uint32_t>(indices.size());

      bool vbValid = mesh->Dynamic ? bgfx::isValid(mesh->dvbh) : bgfx::isValid(mesh->vbh);
      if (!vbValid || !bgfx::isValid(ibh)) {
          std::cerr << "[ModelLoader] ERROR: Failed to create GPU buffers for mesh '" << aMesh->mName.C_Str() << "'\n";
          continue; // Skip this mesh
      }


      // CPU-side positions (for AABB)
      mesh->Vertices.reserve(aMesh->mNumVertices);
      mesh->Normals.reserve(aMesh->mNumVertices);
      mesh->BoneWeights = vertWeights;
      mesh->BoneIndices = vertIndices;
      for (unsigned int i = 0; i < aMesh->mNumVertices; i++) {
         aiVector3D pos = aMesh->mVertices[i];
         mesh->Vertices.push_back(glm::vec3(pos.x, pos.y, pos.z));
         aiVector3D nrm = aMesh->mNormals ? aMesh->mNormals[i] : aiVector3D(0,0,1);
         mesh->Normals.push_back(glm::vec3(nrm.x, nrm.y, nrm.z));
         }

	  std::cout << "[ModelLoader] Loaded mesh: " << aMesh->mName.C_Str() << " with "
		  << aMesh->mNumVertices << " vertices and " << indices.size() << " indices and" << aMesh->mNumFaces << " faces." << std::endl;
      
      // Debug: Print first few vertex positions
      // Debug: Print first few vertex positions
      if (hasSkin) {
          if (!skVertices.empty()) {
              std::cout << "[ModelLoader] First 3 vertices: ";
              size_t count = std::min<size_t>(3, skVertices.size());
              for (size_t i = 0; i < count; ++i) {
                  std::cout << "(" << skVertices[i].x << ", " << skVertices[i].y << ", " << skVertices[i].z << ") ";
              }
              std::cout << std::endl;
          }
      } else {
          if (!vertices.empty()) {
              std::cout << "[ModelLoader] First 3 vertices: ";
              size_t count = std::min<size_t>(3, vertices.size());
              for (size_t i = 0; i < count; ++i) {
                  std::cout << "(" << vertices[i].x << ", " << vertices[i].y << ", " << vertices[i].z << ") ";
              }
              std::cout << std::endl;
          }
      }

      mesh->Indices = indices;

      // Sanity check: No indices should exceed the number of vertices
      uint16_t maxIndex = 0;
      for (uint16_t i : indices)
          maxIndex = std::max(maxIndex, i);

      size_t vertCountForCheck = hasSkin ? skVertices.size() : vertices.size();
      if (maxIndex >= vertCountForCheck) {
          std::cerr << "[ModelLoader] ERROR: Mesh '" << aMesh->mName.C_Str()
              << "' has out-of-bounds index " << maxIndex
              << " (vertex count = " << vertCountForCheck << ")\n";
      }


      mesh->ComputeBounds();

      // ---------------- Blend Shapes ----------------
      BlendShapeComponent blendComp;
      if (aMesh->mNumAnimMeshes > 0) {
          for (unsigned int a = 0; a < aMesh->mNumAnimMeshes; ++a) {
              aiAnimMesh* anim = aMesh->mAnimMeshes[a];
              BlendShape bs;
              bs.Name = anim->mName.C_Str();
              bs.DeltaPos.reserve(aMesh->mNumVertices);
              bs.DeltaNormal.reserve(aMesh->mNumVertices);
              for (unsigned int v = 0; v < aMesh->mNumVertices; ++v) {
                  aiVector3D dp = anim->mVertices[v];
                  bs.DeltaPos.push_back(glm::vec3(dp.x, dp.y, dp.z));
                  aiVector3D dn = anim->mNormals ? anim->mNormals[v] : aiVector3D(0,0,0);
                  bs.DeltaNormal.push_back(glm::vec3(dn.x, dn.y, dn.z));
              }
              blendComp.Shapes.push_back(std::move(bs));
          }
      }

      result.Meshes.push_back(mesh);

      // Load material
      std::shared_ptr<Material> mat;
      if (hasSkin) {
          // Real skinning (bones) needs the skinned shader.
          auto prog = ShaderManager::Instance().LoadProgram("vs_pbr_skinned", "fs_pbr_skinned");
          mat = std::make_shared<SkinnedPBRMaterial>("SkinnedPBR", prog);
      }
      else {
          // No skeleton – regular PBR shader is sufficient; blend-shapes work on CPU.
          mat = MaterialManager::Instance().CreateDefaultPBRMaterial();
      }

      
      // Debug: Check if material is valid
      std::cout << "[ModelLoader] Created material: " << mat->GetName() 
                << " program valid: " << bgfx::isValid(mat->GetProgram()) << std::endl;

      aiMaterial* material = scene->mMaterials[aMesh->mMaterialIndex];
      aiString texPath;

      /*if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
         std::string texFile = texPath.C_Str();
         mat->m_AlbedoTex = TextureLoader::Load2D(texFile);
         }
      if (material->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS) {
         std::string texFile = texPath.C_Str();
         mat->m_MetallicRoughnessTex = TextureLoader::Load2D(texFile);
         }
      if (material->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS) {
         std::string texFile = texPath.C_Str();
         mat->m_NormalTex = TextureLoader::Load2D(texFile);
         }*/

      result.Materials.push_back(mat);
      result.BlendShapes.push_back(std::move(blendComp));
      }

   return result;
   }
