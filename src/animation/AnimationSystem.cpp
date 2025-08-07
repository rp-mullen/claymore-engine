#include "animation/AnimationSystem.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include <cmath>
#include <glm/gtx/transform.hpp>
#include "ecs/EntityData.h"

namespace cm {
namespace animation {

void AnimationSystem::Update(::Scene& scene, float deltaTime) {
    for (const auto& ent : scene.GetEntities()) {
        auto* data = scene.GetEntityData(ent.GetID());
        if (!data || !data->AnimationPlayer || !data->Skeleton || !data->Skinning) continue;
        auto& player   = *data->AnimationPlayer;
        auto& skeleton = *data->Skeleton;
        auto& skin     = *data->Skinning;

        if (player.ActiveStates.empty()) continue;

        // Ensure palette size matches bone count
        skin.Palette.resize(skeleton.BoneEntities.size(), glm::mat4(1.0f));

        // Evaluate blended pose (currently supports single state)
        const AnimationState& state = player.ActiveStates.front();
        if (!state.Clip) continue;

        // Advance time
        AnimationState& mutableState = player.ActiveStates.front();
        mutableState.Time += deltaTime * player.PlaybackSpeed;
        if (state.Clip->Duration > 0.0f && mutableState.Loop) {
            mutableState.Time = fmod(mutableState.Time, state.Clip->Duration);
        }

        std::vector<glm::mat4> localTransforms;
        EvaluateAnimation(*state.Clip, mutableState.Time, skeleton, localTransforms);

        // --------- Build global transforms and palette ---------
        std::vector<glm::mat4> globalTransforms(localTransforms.size());
        for (size_t i = 0; i < localTransforms.size(); ++i) {
            int parent = (i < skeleton.BoneParents.size()) ? skeleton.BoneParents[i] : -1;
            if (parent < 0) globalTransforms[i] = localTransforms[i];
            else            globalTransforms[i] = globalTransforms[parent] * localTransforms[i];
        }

        for (size_t i = 0; i < skin.Palette.size(); ++i) {
            glm::mat4 invBind = (i < skeleton.InverseBindPoses.size()) ? skeleton.InverseBindPoses[i] : glm::mat4(1.0f);
            skin.Palette[i] = globalTransforms[i] * invBind;
        }
    }
}

} // namespace animation
} // namespace cm
