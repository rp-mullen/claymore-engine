// IKSystem.cpp
#include "animation/ik/IKSystem.h"
#include "ecs/Scene.h"
#include "ecs/EntityData.h"
#include "ecs/AnimationComponents.h"
#include "animation/ik/IKSolvers.h"
#include "animation/ik/IKDebugDraw.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace cm { namespace animation { namespace ik {

static inline void DecomposeTRS(const glm::mat4& m, glm::vec3& T, glm::quat& R, glm::vec3& S) {
    T = glm::vec3(m[3]);
    glm::vec3 X = glm::vec3(m[0]); glm::vec3 Y = glm::vec3(m[1]); glm::vec3 Z = glm::vec3(m[2]);
    S = glm::vec3(glm::length(X), glm::length(Y), glm::length(Z));
    if (S.x > 1e-6f) X /= S.x; if (S.y > 1e-6f) Y /= S.y; if (S.z > 1e-6f) Z /= S.z;
    glm::mat3 rotMat(X, Y, Z); R = glm::quat_cast(rotMat);
}

void IKSystem::SolveAndBlend(Scene& scene, float /*deltaTime*/) {
    // Iterate entities with Skeleton and potential IK authoring stored in Extra["ik"]
    for (const auto& ent : scene.GetEntities()) {
        auto* data = scene.GetEntityData(ent.GetID());
        if (!data || !data->Skeleton) continue;

        // Discover IK components stored in Extra under "ik" array; also support future native storage
        std::vector<IKComponent>* ikListPtr = nullptr;
        // Attach a scratch vector in Extra runtime map if needed
        static thread_local std::vector<IKComponent> s_runtimeIK;
        s_runtimeIK.clear();

        // If Extra carries authored IK blocks, materialize them into IKComponent instances
        if (data->Extra.is_object() && data->Extra.contains("ik") && data->Extra["ik"].is_array()) {
            const auto& arr = data->Extra["ik"];
            for (const auto& j : arr) {
                IKComponent c;
                c.Enabled = j.value("enabled", true);
                c.TargetEntity = j.value("target", (EntityID)0);
                c.PoleEntity = j.value("pole", (EntityID)0);
                c.Weight = j.value("weight", 1.0f);
                c.MaxIterations = j.value("maxIterations", 12.0f);
                c.Tolerance = j.value("tolerance", 0.001f);
                c.Damping = j.value("damping", 0.2f);
                c.UseTwoBone = j.value("useTwoBone", true);
                c.Visualize = j.value("visualize", false);
                if (j.contains("chain") && j["chain"].is_array()) {
                    for (auto& b : j["chain"]) c.Chain.push_back((BoneId)b.get<int>());
                }
                if (j.contains("constraints") && j["constraints"].is_array()) {
                    for (auto& cj : j["constraints"]) {
                        IKComponent::Constraint cc; cc.useHinge=cj.value("useHinge",false); cc.useTwist=cj.value("useTwist",false);
                        cc.hingeMinDeg=cj.value("hingeMinDeg",0.0f); cc.hingeMaxDeg=cj.value("hingeMaxDeg",0.0f);
                        cc.twistMinDeg=cj.value("twistMinDeg",0.0f); cc.twistMaxDeg=cj.value("twistMaxDeg",0.0f);
                        c.Constraints.push_back(cc);
                    }
                }
                c.Skeleton = data->Skeleton.get();
                s_runtimeIK.push_back(std::move(c));
            }
            ikListPtr = &s_runtimeIK;
        }

        if (!ikListPtr || ikListPtr->empty()) continue;

        auto& skeleton = *data->Skeleton;
        const size_t boneCount = skeleton.BoneEntities.size();

        // Build current local transforms buffer from bone entity TRS
        std::vector<glm::mat4> local(boneCount, glm::mat4(1.0f));
        for (size_t i=0;i<boneCount;++i) {
            EntityID be = skeleton.BoneEntities[i];
            if (auto* bd = scene.GetEntityData(be)) {
                glm::mat4 T = glm::translate(glm::mat4(1.0f), bd->Transform.Position);
                glm::mat4 R = glm::toMat4(glm::normalize(bd->Transform.RotationQ));
                glm::mat4 S = glm::scale(glm::mat4(1.0f), bd->Transform.Scale);
                local[i] = T * R * S;
            } else if (i < skeleton.InverseBindPoses.size()) {
                // fall back to bind local from inverse bind
                int parent = (i < skeleton.BoneParents.size()) ? skeleton.BoneParents[i] : -1;
                glm::mat4 invBind = skeleton.InverseBindPoses[i];
                glm::mat4 globalBind = glm::inverse(invBind);
                glm::mat4 parentGlobal = (parent>=0)? glm::inverse(skeleton.InverseBindPoses[parent]) : glm::mat4(1.0f);
                local[i] = glm::inverse(parentGlobal) * globalBind;
            }
        }

        // Compose world matrices for joints (model space = parent chain product)
        std::vector<glm::mat4> world(boneCount, glm::mat4(1.0f));
        for (size_t i=0;i<boneCount;++i) {
            int p = (i < skeleton.BoneParents.size()) ? skeleton.BoneParents[i] : -1;
            world[i] = (p>=0) ? (world[p] * local[i]) : local[i];
        }

        for (auto& ikc : *ikListPtr) {
            if (!ikc.Enabled || ikc.Weight <= 0.0f) continue;
            if (!ikc.ValidateChain(skeleton)) continue;
            const size_t m = ikc.Chain.size(); if (m < 2 || m > kMaxChainLen) continue;

            // Resolve target & pole world positions
            glm::vec3 targetW(0.0f); bool targetValid = false;
            if (ikc.TargetEntity != 0) {
                if (auto* td = scene.GetEntityData(ikc.TargetEntity)) { targetW = glm::vec3(td->Transform.WorldMatrix[3]); targetValid = true; }
            }
            if (!targetValid) { continue; }
            bool hasPole = false; glm::vec3 poleW(0.0f);
            if (ikc.PoleEntity != 0) { if (auto* pd = scene.GetEntityData(ikc.PoleEntity)) { poleW = glm::vec3(pd->Transform.WorldMatrix[3]); hasPole = true; } }

            // Assemble joint world positions for chain
            std::vector<glm::vec3> jw(m);
            for (size_t i=0;i<m;++i) jw[i] = glm::vec3(world[ikc.Chain[i]][3]);

            float outError = 0.0f; int outIter = 0;
            std::vector<glm::quat> desiredLocal(m, glm::quat(1,0,0,0));

            if (ikc.UseTwoBone && m == 3) {
                TwoBoneInputs in{};
                in.rootPos = jw[0]; in.midPos = jw[1]; in.endPos = jw[2]; in.targetPos = targetW;
                in.hasPole = hasPole; in.polePos = poleW;
                in.upperLen = glm::length(jw[1] - jw[0]); in.lowerLen = glm::length(jw[2] - jw[1]);
                glm::quat r0, r1; float err;
                SolveTwoBone(in, nullptr, r0, r1, err);
                outError = err; outIter = 1;
                desiredLocal[0] = r0; desiredLocal[1] = r1; desiredLocal[2] = glm::quat(1,0,0,0);
            } else {
                auto jwSolved = jw;
                const glm::vec3* polePtr = hasPole ? &poleW : nullptr;
                SolveFABRIK(jwSolved, targetW, (int)ikc.MaxIterations, ikc.Tolerance, polePtr, outError, outIter);
                // Convert solved positions to local delta rotations
                std::vector<glm::mat4> parentW(m);
                for (size_t i=0;i<m;++i) parentW[i] = world[ikc.Chain[i]];
                WorldChainToLocalRots(parentW, jwSolved, desiredLocal);
                jw.swap(jwSolved);
            }

            // Damping/blend: apply R_out = slerp(I, Delta, Weight*(1-Damping)) then accumulate on FK local
            float damp = glm::clamp(ikc.Damping, 0.0f, 1.0f);
            float blend = glm::clamp(ikc.Weight * (1.0f - damp), 0.0f, 1.0f);

            for (size_t i=0;i<m;++i) {
                int bi = ikc.Chain[i];
                // Decompose current local transform, apply rotation delta
                glm::vec3 T,S; glm::quat R;
                DecomposeTRS(local[bi], T, R, S);
                glm::quat delta = desiredLocal[i];
                glm::quat applied = glm::slerp(glm::quat(1,0,0,0), delta, blend);
                glm::quat newR = glm::normalize(applied * R);
                local[bi] = glm::translate(T) * glm::mat4_cast(newR) * glm::scale(S);
            }

            // Debug visualization on demand
            if (ikc.Visualize) {
                DebugChainViz viz; viz.jointWorld = jw; viz.targetWorld = targetW; viz.hasPole = hasPole; viz.poleWorld = poleW; viz.error = outError; viz.iterations = outIter;
                DrawChain(viz, 0);
            }
        }

        // Write back locals to bone entities
        for (size_t i=0;i<boneCount;++i) {
            EntityID be = skeleton.BoneEntities[i]; if (be == (EntityID)-1) continue;
            if (auto* bd = scene.GetEntityData(be)) {
                glm::vec3 T,S; glm::quat R; DecomposeTRS(local[i], T, R, S);
                bd->Transform.Position = T;
                bd->Transform.Scale = S;
                bd->Transform.RotationQ = glm::normalize(R);
                bd->Transform.UseQuatRotation = true;
                bd->Transform.Rotation = glm::degrees(glm::eulerAngles(bd->Transform.RotationQ));
                bd->Transform.TransformDirty = true;
            }
        }
    }
}

} } }


