#include "animation/AnimationSystem.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include <cmath>
#include <glm/gtx/transform.hpp>
#include "ecs/EntityData.h"
#include "animation/AnimationSerializer.h"

namespace cm {
namespace animation {

void AnimationSystem::Update(::Scene& scene, float deltaTime) {
    for (const auto& ent : scene.GetEntities()) {
        auto* data = scene.GetEntityData(ent.GetID());
        if (!data || !data->AnimationPlayer || !data->Skeleton || !data->Skinning) continue;
        auto& player   = *data->AnimationPlayer;
        auto& skeleton = *data->Skeleton;
        auto& skin     = *data->Skinning;

        // Animator controller update (if set)
        if (player.Controller) {
            // Load default state clip if needed
            if (player.CurrentStateId < 0) {
                player.AnimatorInstance.SetController(player.Controller);
                player.AnimatorInstance.ResetToDefaults();
                player.CurrentStateId = player.Controller->DefaultState;
            }

            const auto* st = player.Controller->FindState(player.CurrentStateId);
            if (!st) continue;
            // Load or get cached clip
            std::shared_ptr<cm::animation::AnimationClip> clip;
            auto itc = player.CachedClips.find(st->Id);
            if (itc != player.CachedClips.end()) clip = itc->second; else {
                // Load from file path
                clip = std::make_shared<cm::animation::AnimationClip>(cm::animation::LoadAnimationClip(st->ClipPath));
                player.CachedClips[st->Id] = clip;
            }
            if (!clip) continue;
            // Advance animator time
            player.AnimatorInstance.Update(deltaTime * st->Speed * player.PlaybackSpeed, clip->Duration);
            // Check transitions
            int next = player.AnimatorInstance.ChooseNextState();
            if (next >= 0 && next != player.CurrentStateId) {
                player.CurrentStateId = next;
                // Reset time for new state
                player.AnimatorInstance.Update(0.0f, 0.0f);
                player.AnimatorInstance.ConsumeTriggers();
            }

            // Evaluate current state clip at time
            std::vector<glm::mat4> localTransforms;
            // For now, write into first active state for compatibility with skinning path below
            if (player.ActiveStates.empty()) player.ActiveStates.push_back({clip.get(), 0.0f, 1.0f, st->Loop});
            AnimationState& s0 = player.ActiveStates.front();
            s0.Clip = clip.get();
            s0.Loop = st->Loop;
            // Reconstruct time from normalized in AnimatorPlayback
            float time = player.AnimatorInstance.Playback().StateNormalized * (clip->Duration > 0.0f ? clip->Duration : 1.0f);
            s0.Time = time;
        }

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
