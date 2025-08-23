#include "SkinningSystem.h"
#include "ecs/AnimationComponents.h"
#include "rendering/VertexTypes.h"
#include "rendering/SkinnedPBRMaterial.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <algorithm>
#include <bgfx/bgfx.h>

#include "jobs/JobSystem.h"   
#include "jobs/ParallelFor.h"
#include "jobs/Jobs.h"

// ---------- Palette kernel (assumes pose[i] = boneWorld[i] * invBind[i]) ----------
struct PaletteArgs {
   const glm::mat4* pose;      // size = boneCount
   glm::mat4        invMesh;   // per mesh
   glm::mat4* out;       // palette, size = boneCount
   int start, count;
   };
static inline void PaletteKernel(const PaletteArgs& a) {
   const int end = a.start + a.count;
   for (int i = a.start; i < end; ++i) {
      a.out[i] = a.invMesh * a.pose[i]; // one mul per bone
      }

   }

// ---------- Blendshape kernel (adds pre-accumulated deltas to base) ----------
struct MorphBlendArgs {
   const glm::vec3* basePos; const glm::vec3* baseNrm;
   const glm::vec3* accDP;   const glm::vec3* accDN; // weighted sums per vertex
   size_t           vCount;
   // Output contiguous arrays (avoid assuming interleaved vertex stride)
   glm::vec3* outPos; glm::vec3* outNrm;
   int start, count;
   };
static inline void MorphBlendKernel(const MorphBlendArgs& a) {
   const int end = a.start + a.count;
   for (int i = a.start; i < end; ++i) {
      a.outPos[i] = a.basePos[i] + a.accDP[i];
      a.outNrm[i] = a.baseNrm[i] + a.accDN[i];
      }
   }

static inline glm::mat4 GetWorldOrIdentity(Scene& scene, EntityID id)
{
	auto* data = scene.GetEntityData(id);
	if (data)
	{
		return data->Transform.WorldMatrix;
	}
	return glm::mat4(1.0f);
}

void SkinningSystem::Update(Scene& scene)
   { 
   auto& entities = scene.GetEntities();

   // 1) Group skinned meshes by SkeletonRoot and collect per-skeleton data
   struct MeshWork {
      EntityID meshId;
      glm::mat4 invMeshWorld;
      SkinningComponent* skin;
      std::vector<glm::mat4>* palette;
      std::shared_ptr<SkinnedPBRMaterial> skMat; // nullptr if not skinned PBR
      // Blendshape CPU data
      Mesh* meshPtr = nullptr;
      BlendShapeComponent* bs = nullptr;
      bool needsBlend = false;
      bool isSkinnedVB = false;
      };
   struct SkelGroup {
      EntityID root;
      EntityData* skelData;
      const SkeletonComponent* skel;
      bool inBindPose = true;
      std::vector<glm::mat4> pose; // size = boneCount (boneWorld * invBind)
      std::vector<MeshWork> meshes;
      };

   std::unordered_map<EntityID, SkelGroup> groups;
   struct NonSkinnedWork {
      EntityID meshId;
      Mesh* meshPtr = nullptr;
      BlendShapeComponent* bs = nullptr;
      bool needsBlend = false;
      };
   std::vector<NonSkinnedWork> nonSkinned;

   // Build map: skeleton root -> meshes using it; precompute invMeshWorld per mesh
   for (const auto& ent : entities) {
      EntityData* data = scene.GetEntityData(ent.GetID());
      if (!data || !data->Mesh) continue;

      // Collect non-skinned meshes for blendshape-only updates
      if (!data->Skinning) {
         auto meshPtr = data->Mesh->mesh.get();
         const bool bsDirty = (data->BlendShapes && meshPtr && meshPtr->Dynamic && data->Mesh->BlendShapes && data->Mesh->BlendShapes->Dirty);
         if (bsDirty) {
            NonSkinnedWork nw{};
            nw.meshId = ent.GetID();
            nw.meshPtr = meshPtr;
            nw.bs = data->BlendShapes.get();
            nw.needsBlend = true;
            nonSkinned.push_back(nw);
         }
         continue; // not part of skinning groups
      }

      EntityID root = data->Skinning->SkeletonRoot;
      if (root == (EntityID)-1) continue;
      EntityData* skelData = scene.GetEntityData(root);
      if (!skelData || !skelData->Skeleton) continue;

      auto& g = groups[root];
      g.root = root; g.skelData = skelData; g.skel = skelData->Skeleton.get();
      if (g.pose.empty()) {
         // detect bind vs animated
         g.inBindPose = (!skelData->AnimationPlayer) || (skelData->AnimationPlayer->ActiveStates.empty());
         }

      // Prepare mesh work item
      MeshWork w{};
      w.meshId = ent.GetID();
      const glm::mat4 meshWorld = data->Transform.WorldMatrix;          // you already compute this before skinning
      w.invMeshWorld = glm::inverse(meshWorld);                          // palette must be mesh-local :contentReference[oaicite:7]{index=7}
      w.skin = data->Skinning.get();
      w.palette = &w.skin->Palette;
      if (auto sk = std::dynamic_pointer_cast<SkinnedPBRMaterial>(data->Mesh->material))
         w.skMat = sk;                                                  // later: UploadBones if present :contentReference[oaicite:8]{index=8}

      // Blendshape info (dynamic meshes only)
      auto meshPtr = data->Mesh->mesh.get();
      const bool bsDirty = (data->BlendShapes && meshPtr && meshPtr->Dynamic && data->Mesh->BlendShapes->Dirty);
      w.meshPtr = meshPtr;
      w.bs = data->BlendShapes.get();
      w.needsBlend = bsDirty;
      // Decide buffer vertex format by mesh data, not material type
      w.isSkinnedVB = (w.meshPtr && w.meshPtr->HasSkinning()); // :contentReference[oaicite:9]{index=9}

      g.meshes.push_back(std::move(w));
      }

   // 2) For each skeleton group, compute pose once; then fill palettes in parallel
   for (auto& [root, g] : groups) {
      const size_t boneCountRaw = std::min(g.skel->InverseBindPoses.size(), g.skel->BoneEntities.size());
      if (boneCountRaw == 0) continue;                                   // :contentReference[oaicite:10]{index=10}
      const size_t boneCount = std::min(boneCountRaw, (size_t)SkinnedPBRMaterial::MaxBones); // :contentReference[oaicite:11]{index=11}

      // Ensure each mesh palette sized; also decide if we can use identity fast path
      for (auto& w : g.meshes) {
         if (w.palette->size() != boneCount) w.palette->assign(boneCount, glm::mat4(1.0f)); // :contentReference[oaicite:12]{index=12}
         }

      // Build pose matrices once (animated or bind-pose)
      std::vector<glm::mat4> boneWorld(boneCount);
      // Always source current bone entity world transforms so authored/rest poses (e.g., T-pose) are respected in Edit mode
      for (size_t i = 0; i < boneCount; ++i) {
         const EntityID be = g.skel->BoneEntities[i];
         const EntityData* bd = scene.GetEntityData(be);
         if (bd) boneWorld[i] = bd->Transform.WorldMatrix;
         else if (i < g.skel->BindPoseGlobals.size()) boneWorld[i] = g.skel->BindPoseGlobals[i];
         else boneWorld[i] = glm::inverse(g.skel->InverseBindPoses[i]);
      }

      g.pose.resize(boneCount);
      for (size_t i = 0; i < boneCount; ++i) {
         g.pose[i] = boneWorld[i] * g.skel->InverseBindPoses[i];
      }

      // Fill palettes across meshes: palette[i] = invMesh * pose[i]
      // Parallelize per mesh first (bone counts are modest)
      parallel_for(Jobs(), size_t{ 0 }, g.meshes.size(), size_t{ 1 },
         [&](size_t mStart, size_t mCount) {
         for (size_t m = mStart; m < mStart + mCount; ++m) {
            auto& w = g.meshes[m];
            PaletteArgs a{ g.pose.data(), w.invMeshWorld, w.palette->data(), 0, (int)boneCount };
            PaletteKernel(a);
            }
         });

      // Upload bones on main thread (after kernels done)
      for (auto& w : g.meshes) {
         if (!w.palette->empty()) {
            if (w.skMat) w.skMat->UploadBones(*w.palette);
         }
      }

      // 3) Blendshapes per mesh (dynamic; if dirty apply, else ensure base restored)
      for (auto& w : g.meshes) {
         if (!w.meshPtr) continue;
         const size_t vCountBase = w.meshPtr->Vertices.size();
         if (vCountBase == 0) continue;

         if (!w.needsBlend) { continue; }

         const size_t vCount = vCountBase;               // :contentReference[oaicite:15]{index=15}
         if (vCount == 0) { w.bs->Dirty = false; continue; }             // :contentReference[oaicite:16]{index=16}

         // Base copy of output vertex buffer (keeps UVs/indices/weights as before)
         if (w.isSkinnedVB) {
            std::vector<SkinnedPBRVertex> blended(vCount);
            // Copy base once (as your original code did) — keeps UVs/indices/weights intact :contentReference[oaicite:17]{index=17}
            for (size_t i = 0; i < vCount; ++i) {
               blended[i].x = w.meshPtr->Vertices[i].x;
               blended[i].y = w.meshPtr->Vertices[i].y;
               blended[i].z = w.meshPtr->Vertices[i].z;
               blended[i].nx = w.meshPtr->Normals[i].x;
               blended[i].ny = w.meshPtr->Normals[i].y;
               blended[i].nz = w.meshPtr->Normals[i].z;
               // Preserve base UVs to avoid UV drift when morph targets are applied
               if (i < w.meshPtr->UVs.size()) { blended[i].u = w.meshPtr->UVs[i].x; blended[i].v = w.meshPtr->UVs[i].y; }
               else { blended[i].u = 0.0f; blended[i].v = 0.0f; }
               const glm::ivec4 bi = (i < w.meshPtr->BoneIndices.size()) ? w.meshPtr->BoneIndices[i] : glm::ivec4(0);
               blended[i].i0 = (uint8_t)bi.x; blended[i].i1 = (uint8_t)bi.y;
               blended[i].i2 = (uint8_t)bi.z; blended[i].i3 = (uint8_t)bi.w;
               const glm::vec4 bw = (i < w.meshPtr->BoneWeights.size()) ? w.meshPtr->BoneWeights[i] : glm::vec4(1, 0, 0, 0);
               blended[i].w0 = bw.x; blended[i].w1 = bw.y; blended[i].w2 = bw.z; blended[i].w3 = bw.w;
               }

            // Pre-accumulate weighted deltas once
            std::vector<glm::vec3> accDP(vCount, glm::vec3(0));
            std::vector<glm::vec3> accDN(vCount, glm::vec3(0));
            for (const auto& shape : w.bs->Shapes) {
               const float weight = shape.Weight;
               if (weight == 0.0f) continue;
               if (shape.DeltaPos.size() != vCount || shape.DeltaNormal.size() != vCount) continue; // :contentReference[oaicite:19]{index=19}
               for (size_t i = 0; i < vCount; ++i) {
                  accDP[i] += shape.DeltaPos[i] * weight;
                  accDN[i] += shape.DeltaNormal[i] * weight;
                  }
               }

            // Kernel over vertices -> contiguous outputs, then copy back
            std::vector<glm::vec3> outPos(vCount), outNrm(vCount);
            MorphBlendArgs ba{
                w.meshPtr->Vertices.data(), w.meshPtr->Normals.data(),
                accDP.data(), accDN.data(), vCount,
                outPos.data(), outNrm.data(),
                0, (int)vCount
               };
            MorphBlendKernel(ba);
            for (size_t i = 0; i < vCount; ++i) {
               blended[i].x = outPos[i].x; blended[i].y = outPos[i].y; blended[i].z = outPos[i].z;
               blended[i].nx = outNrm[i].x; blended[i].ny = outNrm[i].y; blended[i].nz = outNrm[i].z;
            }

            // Upload once on main thread
            const bgfx::Memory* mem = bgfx::copy(blended.data(), uint32_t(sizeof(SkinnedPBRVertex) * blended.size()));
            if (bgfx::isValid(w.meshPtr->dvbh)) {
               bgfx::update(w.meshPtr->dvbh, 0, mem);                   // :contentReference[oaicite:20]{index=20}
            }
            }
         else {
            std::vector<PBRVertex> blended(vCount);
            for (size_t i = 0; i < vCount; ++i) {
               blended[i].x = w.meshPtr->Vertices[i].x;
               blended[i].y = w.meshPtr->Vertices[i].y;
               blended[i].z = w.meshPtr->Vertices[i].z;
               blended[i].nx = w.meshPtr->Normals[i].x;
               blended[i].ny = w.meshPtr->Normals[i].y;
               blended[i].nz = w.meshPtr->Normals[i].z;
               // Preserve base UVs to avoid warping when morph targets apply
               if (i < w.meshPtr->UVs.size()) { blended[i].u = w.meshPtr->UVs[i].x; blended[i].v = w.meshPtr->UVs[i].y; }
               else { blended[i].u = 0.0f; blended[i].v = 0.0f; }
               }
            std::vector<glm::vec3> accDP(vCount, glm::vec3(0));
            std::vector<glm::vec3> accDN(vCount, glm::vec3(0));
            for (const auto& shape : w.bs->Shapes) {
               const float weight = shape.Weight;
               if (weight == 0.0f) continue;
               if (shape.DeltaPos.size() != vCount || shape.DeltaNormal.size() != vCount) continue; // :contentReference[oaicite:22]{index=22}
               for (size_t i = 0; i < vCount; ++i) {
                  accDP[i] += shape.DeltaPos[i] * weight;
                  accDN[i] += shape.DeltaNormal[i] * weight;
                  }
               }
            std::vector<glm::vec3> outPos2(vCount), outNrm2(vCount);
            MorphBlendArgs ba{
                w.meshPtr->Vertices.data(), w.meshPtr->Normals.data(),
                accDP.data(), accDN.data(), vCount,
                outPos2.data(), outNrm2.data(),
                0, (int)vCount
               };
            MorphBlendKernel(ba);
            for (size_t i = 0; i < vCount; ++i) {
               blended[i].x = outPos2[i].x; blended[i].y = outPos2[i].y; blended[i].z = outPos2[i].z;
               blended[i].nx = outNrm2[i].x; blended[i].ny = outNrm2[i].y; blended[i].nz = outNrm2[i].z;
            }
            const bgfx::Memory* mem = bgfx::copy(blended.data(), uint32_t(sizeof(PBRVertex) * blended.size()));
            if (bgfx::isValid(w.meshPtr->dvbh)) {
               bgfx::update(w.meshPtr->dvbh, 0, mem);                   // :contentReference[oaicite:23]{index=23}
            }
            }

         w.bs->Dirty = false;                                            // :contentReference[oaicite:24]{index=24}
         }
      }
        
   // 4) Non-skinned meshes: apply blendshapes separately
   for (auto& w : nonSkinned) {
      if (!w.needsBlend || !w.meshPtr || !w.bs) continue;
      const size_t vCount = w.meshPtr->Vertices.size();
      if (vCount == 0) { w.bs->Dirty = false; continue; }

      std::vector<PBRVertex> blended(vCount);
      for (size_t i = 0; i < vCount; ++i) {
         blended[i].x = w.meshPtr->Vertices[i].x;
         blended[i].y = w.meshPtr->Vertices[i].y;
         blended[i].z = w.meshPtr->Vertices[i].z;
         blended[i].nx = w.meshPtr->Normals[i].x;
         blended[i].ny = w.meshPtr->Normals[i].y;
         blended[i].nz = w.meshPtr->Normals[i].z;
         blended[i].u = 0.0f; blended[i].v = 0.0f;
      }
      std::vector<glm::vec3> accDP(vCount, glm::vec3(0));
      std::vector<glm::vec3> accDN(vCount, glm::vec3(0));
      for (const auto& shape : w.bs->Shapes) {
         const float weight = shape.Weight;
         if (weight == 0.0f) continue;
         if (shape.DeltaPos.size() != vCount || shape.DeltaNormal.size() != vCount) continue;
         for (size_t i = 0; i < vCount; ++i) {
            accDP[i] += shape.DeltaPos[i] * weight;
            accDN[i] += shape.DeltaNormal[i] * weight;
         }
      }
      std::vector<glm::vec3> outPos(vCount), outNrm(vCount);
      MorphBlendArgs ba{
         w.meshPtr->Vertices.data(), w.meshPtr->Normals.data(),
         accDP.data(), accDN.data(), vCount,
         outPos.data(), outNrm.data(),
         0, (int)vCount
      };
      MorphBlendKernel(ba);
      for (size_t i = 0; i < vCount; ++i) {
         blended[i].x = outPos[i].x; blended[i].y = outPos[i].y; blended[i].z = outPos[i].z;
         blended[i].nx = outNrm[i].x; blended[i].ny = outNrm[i].y; blended[i].nz = outNrm[i].z;
      }
      const bgfx::Memory* mem = bgfx::copy(blended.data(), uint32_t(sizeof(PBRVertex) * blended.size()));
      if (bgfx::isValid(w.meshPtr->dvbh)) {
         bgfx::update(w.meshPtr->dvbh, 0, mem);
      }
      w.bs->Dirty = false;
   }
}
