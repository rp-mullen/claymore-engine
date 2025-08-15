#include "animation/AnimationSystem.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include <cmath>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include "ecs/EntityData.h"
#include "animation/AnimationSerializer.h"
#include "animation/AnimatorController.h"
#include "animation/AnimationAsset.h"
#include "animation/AnimationEvaluator.h"
#include "animation/BindingCache.h"
#include "animation/HumanoidRetargeter.h"
#include "animation/AvatarSerializer.h"
// Script event dispatch to managed C# scripts
#include "scripting/ManagedScriptComponent.h"
#include "scripting/DotNetHost.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace cm {
namespace animation {

    void decomposeTRS(const glm::mat4& m, glm::vec3& T, glm::quat& R, glm::vec3& S) {
        T = glm::vec3(m[3]);
        glm::vec3 X = glm::vec3(m[0]);
        glm::vec3 Y = glm::vec3(m[1]);
        glm::vec3 Z = glm::vec3(m[2]);
        S = glm::vec3(glm::length(X), glm::length(Y), glm::length(Z));
        if (S.x > 1e-6f) X /= S.x;
        if (S.y > 1e-6f) Y /= S.y;
        if (S.z > 1e-6f) Z /= S.z;
        glm::mat3 rotMat(X, Y, Z);
        R = glm::quat_cast(rotMat);
    }

void AnimationSystem::Update(::Scene& scene, float deltaTime) {
    for (const auto& ent : scene.GetEntities()) {
        auto* data = scene.GetEntityData(ent.GetID());
        // Drive animation from entities that own an AnimationPlayer and a Skeleton (skeleton root)
        if (!data || !data->AnimationPlayer || !data->Skeleton) continue;
        auto& player   = *data->AnimationPlayer;
        auto& skeleton = *data->Skeleton;

        // Auto-load controller if path is set but runtime controller not yet created
        if (player.AnimatorMode == AnimationPlayerComponent::Mode::ControllerAnimated && !player.Controller && !player.ControllerPath.empty()) {
            try {
                std::ifstream in(player.ControllerPath);
                if (in) {
                    nlohmann::json j; in >> j;
                    auto ctrl = std::make_shared<cm::animation::AnimatorController>();
                    nlohmann::from_json(j, *ctrl);
                    player.Controller = ctrl;
                    player.AnimatorInstance.SetController(ctrl);
                    player.AnimatorInstance.ResetToDefaults();
                    player.CurrentStateId = ctrl->DefaultState;
                }
            } catch (...) {
                // Ignore errors; will remain unloaded
            }
        }

        // If in Controller mode but still no controller loaded, do not drive animation
        if (player.AnimatorMode == AnimationPlayerComponent::Mode::ControllerAnimated && !player.Controller) {
            if (!player.ActiveStates.empty()) {
                player.ActiveStates.front().Asset = nullptr;
                player.ActiveStates.front().LegacyClip = nullptr;
            }
            player.Debug_CurrentAnimationName.clear();
            player.Debug_CurrentControllerStateName.clear();
            continue;
        }

        // Predeclare evaluation context shared across phases (needed for Blend1D sampling later)
        const cm::animation::AnimatorState* stNowForEval = nullptr;
        std::shared_ptr<cm::animation::AnimationAsset> assetNow;
        std::shared_ptr<cm::animation::AnimationClip> clipNow;
        std::shared_ptr<cm::animation::AnimationAsset> assetB0, assetB1;
        std::shared_ptr<cm::animation::AnimationClip> clipB0, clipB1;
        float durationNow = 0.0f;
        float blendT = 0.0f;
        bool useBlend1D = false;

        // Animator controller update (if set) and in ControllerAnimated mode
        if (player.AnimatorMode == AnimationPlayerComponent::Mode::ControllerAnimated && player.Controller) {
            // Load default state clip if needed
            if (player.CurrentStateId < 0) {
                player.AnimatorInstance.SetController(player.Controller);
                player.AnimatorInstance.ResetToDefaults();
                player.CurrentStateId = player.Controller->DefaultState;
            }

            const auto* st = player.Controller->FindState(player.CurrentStateId);
            if (!st) continue;
            // Load or get cached unified asset (prefer) or legacy clip (fallback)
            std::shared_ptr<cm::animation::AnimationAsset> asset;
            if (!st->AnimationAssetPath.empty()) {
                auto ita = player.CachedAssets.find(st->Id);
                if (ita != player.CachedAssets.end()) asset = ita->second; else {
                    asset = std::make_shared<cm::animation::AnimationAsset>(cm::animation::LoadAnimationAsset(st->AnimationAssetPath));
                    player.CachedAssets[st->Id] = asset;
                }
            }
            std::shared_ptr<cm::animation::AnimationClip> clip;
            if (!asset && !st->ClipPath.empty()) {
                auto itc = player.CachedClips.find(st->Id);
                if (itc != player.CachedClips.end()) clip = itc->second; else {
                    clip = std::make_shared<cm::animation::AnimationClip>(cm::animation::LoadAnimationClip(st->ClipPath));
                    player.CachedClips[st->Id] = clip;
                }
            }
            // Advance animator time; if Blend1D, use blended duration so normalized time progresses
            float currentDuration = 0.0f;
            if (st->Kind == cm::animation::AnimatorStateKind::Blend1D && !st->Blend1DEntries.empty()) {
                float x = 0.0f;
                auto itf = player.AnimatorInstance.Blackboard().Floats.find(st->Blend1DParam);
                if (itf != player.AnimatorInstance.Blackboard().Floats.end()) x = glm::clamp(itf->second, 0.0f, 1.0f);
                const auto& e = st->Blend1DEntries;
                int i1 = 0, i2 = (int)e.size()-1;
                for (int i=0;i<(int)e.size();++i) { if (e[i].Key <= x) i1 = i; if (e[i].Key >= x) { i2 = i; break; } }
                i1 = glm::clamp(i1, 0, (int)e.size()-1); i2 = glm::clamp(i2, 0, (int)e.size()-1);
                const auto& a = e[i1]; const auto& b = e[i2];
                float denom = std::max(1e-6f, (b.Key - a.Key));
                float t = glm::clamp((x - a.Key) / denom, 0.0f, 1.0f);
                // Resolve durations for a/b (load or get cached)
                std::shared_ptr<cm::animation::AnimationAsset> aAsset, bAsset; std::shared_ptr<cm::animation::AnimationClip> aClip, bClip;
                if (!a.AssetPath.empty()) { auto ita = player.CachedAssets.find(st->Id * 1000 + i1); if (ita != player.CachedAssets.end()) aAsset = ita->second; else { aAsset = std::make_shared<cm::animation::AnimationAsset>(cm::animation::LoadAnimationAsset(a.AssetPath)); player.CachedAssets[st->Id * 1000 + i1] = aAsset; } }
                if (!aAsset && !a.ClipPath.empty()) { auto itc = player.CachedClips.find(st->Id * 1000 + i1); if (itc != player.CachedClips.end()) aClip = itc->second; else { aClip = std::make_shared<cm::animation::AnimationClip>(cm::animation::LoadAnimationClip(a.ClipPath)); player.CachedClips[st->Id * 1000 + i1] = aClip; } }
                if (!b.AssetPath.empty()) { auto ita = player.CachedAssets.find(st->Id * 1000 + i2); if (ita != player.CachedAssets.end()) bAsset = ita->second; else { bAsset = std::make_shared<cm::animation::AnimationAsset>(cm::animation::LoadAnimationAsset(b.AssetPath)); player.CachedAssets[st->Id * 1000 + i2] = bAsset; } }
                if (!bAsset && !b.ClipPath.empty()) { auto itc = player.CachedClips.find(st->Id * 1000 + i2); if (itc != player.CachedClips.end()) bClip = itc->second; else { bClip = std::make_shared<cm::animation::AnimationClip>(cm::animation::LoadAnimationClip(b.ClipPath)); player.CachedClips[st->Id * 1000 + i2] = bClip; } }
                float d0 = aAsset ? aAsset->Duration() : (aClip ? aClip->Duration : 0.0f);
                float d1 = bAsset ? bAsset->Duration() : (bClip ? bClip->Duration : 0.0f);
                currentDuration = glm::mix(d0, d1, t);
            } else {
                currentDuration = asset ? asset->Duration() : (clip ? clip->Duration : 0.0f);
            }
            player.AnimatorInstance.Update(deltaTime * st->Speed * player.PlaybackSpeed, currentDuration);
            // Check transitions
            int next = player.AnimatorInstance.ChooseNextState();
            if (next >= 0 && next != player.CurrentStateId) {
                // Query transition duration (MVP: first matching)
                float duration = 0.0f;
                for (const auto& tr : player.Controller->Transitions) {
                    if ((tr.FromState == -1 || tr.FromState == player.CurrentStateId) && tr.ToState == next) { duration = tr.Duration; break; }
                }
                if (duration > 0.0f) {
                    player.AnimatorInstance.BeginCrossfade(next, duration);
                    // Triggers should be consumed when a transition begins
                    player.AnimatorInstance.ConsumeTriggers();
                } else {
                    // Instant transition: synchronize Animator's internal state and player state
                    player.AnimatorInstance.SetCurrentState(next, /*resetTime*/true);
                    player.CurrentStateId = next;
                    player.AnimatorInstance.ConsumeTriggers();
                }
            }

            // Evaluate current (possibly updated) state asset at time – prefer unified asset if present
            stNowForEval = player.Controller->FindState(player.CurrentStateId);
            if (stNowForEval) {
                if (stNowForEval->Kind == cm::animation::AnimatorStateKind::Blend1D && !stNowForEval->Blend1DEntries.empty()) {
                    // Determine blend parameter value (0..1)
                    float x = 0.0f;
                    auto itf = player.AnimatorInstance.Blackboard().Floats.find(stNowForEval->Blend1DParam);
                    if (itf != player.AnimatorInstance.Blackboard().Floats.end()) x = glm::clamp(itf->second, 0.0f, 1.0f);
                    // Find two surrounding entries
                    const auto& e = stNowForEval->Blend1DEntries;
                    int i1 = 0, i2 = (int)e.size()-1;
                    for (int i=0;i<(int)e.size();++i) { if (e[i].Key <= x) i1 = i; if (e[i].Key >= x) { i2 = i; break; } }
                    i1 = glm::clamp(i1, 0, (int)e.size()-1); i2 = glm::clamp(i2, 0, (int)e.size()-1);
                    const auto& a = e[i1]; const auto& b = e[i2];
                    float denom = std::max(1e-6f, (b.Key - a.Key));
                    blendT = glm::clamp((x - a.Key) / denom, 0.0f, 1.0f);
                    // Resolve assets/clips for a and b
                    if (!a.AssetPath.empty()) { auto ita = player.CachedAssets.find(stNowForEval->Id * 1000 + i1); if (ita != player.CachedAssets.end()) assetB0 = ita->second; else { assetB0 = std::make_shared<cm::animation::AnimationAsset>(cm::animation::LoadAnimationAsset(a.AssetPath)); player.CachedAssets[stNowForEval->Id * 1000 + i1] = assetB0; } }
                    if (!assetB0 && !a.ClipPath.empty()) { auto itc = player.CachedClips.find(stNowForEval->Id * 1000 + i1); if (itc != player.CachedClips.end()) clipB0 = itc->second; else { clipB0 = std::make_shared<cm::animation::AnimationClip>(cm::animation::LoadAnimationClip(a.ClipPath)); player.CachedClips[stNowForEval->Id * 1000 + i1] = clipB0; } }
                    if (!b.AssetPath.empty()) { auto ita = player.CachedAssets.find(stNowForEval->Id * 1000 + i2); if (ita != player.CachedAssets.end()) assetB1 = ita->second; else { assetB1 = std::make_shared<cm::animation::AnimationAsset>(cm::animation::LoadAnimationAsset(b.AssetPath)); player.CachedAssets[stNowForEval->Id * 1000 + i2] = assetB1; } }
                    if (!assetB1 && !b.ClipPath.empty()) { auto itc = player.CachedClips.find(stNowForEval->Id * 1000 + i2); if (itc != player.CachedClips.end()) clipB1 = itc->second; else { clipB1 = std::make_shared<cm::animation::AnimationClip>(cm::animation::LoadAnimationClip(b.ClipPath)); player.CachedClips[stNowForEval->Id * 1000 + i2] = clipB1; } }
                    useBlend1D = true;
                    // duration as blend of two durations
                    float d0 = assetB0 ? assetB0->Duration() : (clipB0 ? clipB0->Duration : 0.0f);
                    float d1 = assetB1 ? assetB1->Duration() : (clipB1 ? clipB1->Duration : 0.0f);
                    durationNow = glm::mix(d0, d1, blendT);
                } else {
                    if (!stNowForEval->AnimationAssetPath.empty()) {
                        auto ita = player.CachedAssets.find(stNowForEval->Id);
                        if (ita != player.CachedAssets.end()) assetNow = ita->second; else {
                            assetNow = std::make_shared<cm::animation::AnimationAsset>(cm::animation::LoadAnimationAsset(stNowForEval->AnimationAssetPath));
                            player.CachedAssets[stNowForEval->Id] = assetNow;
                        }
                    }
                    if (!assetNow && !stNowForEval->ClipPath.empty()) {
                        auto itc = player.CachedClips.find(stNowForEval->Id);
                        if (itc != player.CachedClips.end()) clipNow = itc->second; else {
                            clipNow = std::make_shared<cm::animation::AnimationClip>(cm::animation::LoadAnimationClip(stNowForEval->ClipPath));
                            player.CachedClips[stNowForEval->Id] = clipNow;
                        }
                    }
                    durationNow = assetNow ? assetNow->Duration() : (clipNow ? clipNow->Duration : 0.0f);
                }
            }

            if (player.ActiveStates.empty()) player.ActiveStates.push_back({});
            AnimationState& s0 = player.ActiveStates.front();
            s0.Asset = assetNow.get();
            s0.LegacyClip = clipNow.get();
            s0.Loop = stNowForEval ? stNowForEval->Loop : true;
            // Derive time from absolute state time so parameter changes (which alter duration) don't cause jumps
            float baseT = player.AnimatorInstance.Playback().StateTime;
            float time = (durationNow > 0.0f) ? fmod(baseT, durationNow) : 0.0f;
            if (!std::isfinite(time) || time < 0.0f) time = 0.0f;
            s0.Time = time;

            // Debug info
            if (stNowForEval) player.Debug_CurrentControllerStateName = stNowForEval->Name;
            player.Debug_CurrentAnimationName = assetNow ? assetNow->name : (clipNow ? clipNow->Name : std::string());
        }

        // Animation Player mode (single clip, no controller)
        if (player.AnimatorMode == AnimationPlayerComponent::Mode::AnimationPlayerAnimated) {
            // Ensure ActiveStates[0] is bound to the selected SingleClipPath if provided
            if (!player.SingleClipPath.empty()) {
                // Resolve cached asset at key 0
                std::shared_ptr<cm::animation::AnimationAsset> asset;
                auto it = player.CachedAssets.find(0);
                if (it == player.CachedAssets.end()) {
                    cm::animation::AnimationAsset a = cm::animation::LoadAnimationAsset(player.SingleClipPath);
                    asset = std::make_shared<cm::animation::AnimationAsset>(std::move(a));
                    player.CachedAssets[0] = asset;
                } else {
                    asset = it->second;
                }
                if (player.ActiveStates.empty()) player.ActiveStates.push_back({});
                player.ActiveStates.front().Asset = player.CachedAssets[0].get();
                player.ActiveStates.front().LegacyClip = nullptr;
                player.Debug_CurrentAnimationName = player.ActiveStates.front().Asset ? player.ActiveStates.front().Asset->name : std::string();
            }

            // Apply PlayOnStart once
            if (!player._InitApplied) {
                player._InitApplied = true;
                if (player.PlayOnStart) {
                    player.IsPlaying = true;
                    if (!player.ActiveStates.empty()) player.ActiveStates.front().Time = 0.0f;
                }
            }
        }

        if (player.ActiveStates.empty()) continue;

        // Evaluate pose; if crossfading, blend between two states linearly
        const AnimationState& state = player.ActiveStates.front();
        // Allow Blend1D path to evaluate even when no single clip/asset bound in state container
        if (!useBlend1D && !state.LegacyClip && !state.Asset) continue;

        // Advance time
        AnimationState& mutableState = player.ActiveStates.front();
        // In Animation Player mode, advance only if IsPlaying; in Controller mode, always driven
        bool shouldAdvance = (player.AnimatorMode == AnimationPlayerComponent::Mode::ControllerAnimated) ? (player.Controller != nullptr) : player.IsPlaying;
        if (shouldAdvance) {
            mutableState.Time += deltaTime * player.PlaybackSpeed;
        }
        float clipDuration = state.LegacyClip ? state.LegacyClip->Duration : (state.Asset ? state.Asset->Duration() : 0.0f);
        if (clipDuration > 0.0f && mutableState.Loop) {
            if (shouldAdvance) mutableState.Time = fmod(mutableState.Time, clipDuration);
        } else if (clipDuration > 0.0f && player.AnimatorMode == AnimationPlayerComponent::Mode::AnimationPlayerAnimated) {
            // Stop at end in single-clip mode if not looping
            if (shouldAdvance && mutableState.Time >= clipDuration) {
                mutableState.Time = clipDuration;
                player.IsPlaying = false;
            }
        }
        if (player.AnimatorInstance.IsCrossfading()) {
            player.AnimatorInstance.AdvanceCrossfade(deltaTime * player.PlaybackSpeed);
        }

        std::vector<glm::mat4> localTransforms;
        // Helper to compute local bind transform for a bone index
        auto computeLocalBind = [&](int boneIndex) -> glm::mat4 {
            if (boneIndex < 0 || boneIndex >= (int)skeleton.InverseBindPoses.size()) return glm::mat4(1.0f);
            glm::mat4 invBind = skeleton.InverseBindPoses[boneIndex];
            glm::mat4 globalBind = glm::inverse(invBind);
            int parent = (boneIndex < (int)skeleton.BoneParents.size()) ? skeleton.BoneParents[boneIndex] : -1;
            glm::mat4 parentGlobal = (parent >= 0 && parent < (int)skeleton.InverseBindPoses.size()) ? glm::inverse(skeleton.InverseBindPoses[parent]) : glm::mat4(1.0f);
            glm::mat4 localBind = glm::inverse(parentGlobal) * globalBind;
            return localBind;
        };

        if (stNowForEval && stNowForEval->Kind == cm::animation::AnimatorStateKind::Blend1D && useBlend1D) {
            // Evaluate two samples then blend; drive time from Animator's normalized state
            std::vector<glm::mat4> A(skeleton.BoneEntities.size(), glm::mat4(1.0f));
            std::vector<glm::mat4> B(skeleton.BoneEntities.size(), glm::mat4(1.0f));
            float d0 = assetB0 ? assetB0->Duration() : (clipB0 ? clipB0->Duration : 0.0f);
            float d1 = assetB1 ? assetB1->Duration() : (clipB1 ? clipB1->Duration : 0.0f);
            float baseT = player.AnimatorInstance.Playback().StateTime;
            float tA = (d0 > 0.0f) ? fmod(baseT, d0) : 0.0f;
            float tB = (d1 > 0.0f) ? fmod(baseT, d1) : 0.0f;
            // Sample A
            if (assetB0) {
                PoseBuffer pose; pose.local.resize(skeleton.BoneEntities.size(), glm::mat4(1.0f)); pose.touched.resize(skeleton.BoneEntities.size(), false);
                static BindingCache s_bindings; s_bindings.SetSkeleton(&skeleton);
                EvalInputs in{ assetB0.get(), tA, stNowForEval->Loop };
                EvalTargets tgt{ &pose };
                EvalContext ctx{ &s_bindings, skeleton.Avatar.get(), &skeleton };
                SampleAsset(in, ctx, tgt, nullptr, nullptr);
                A = std::move(pose.local);
            } else if (clipB0) {
                EvaluateAnimation(*clipB0, tA, skeleton, A);
            }
            // Sample B
            if (assetB1) {
                PoseBuffer pose; pose.local.resize(skeleton.BoneEntities.size(), glm::mat4(1.0f)); pose.touched.resize(skeleton.BoneEntities.size(), false);
                static BindingCache s_bindings; s_bindings.SetSkeleton(&skeleton);
                EvalInputs in{ assetB1.get(), tB, stNowForEval->Loop };
                EvalTargets tgt{ &pose };
                EvalContext ctx{ &s_bindings, skeleton.Avatar.get(), &skeleton };
                SampleAsset(in, ctx, tgt, nullptr, nullptr);
                B = std::move(pose.local);
            } else if (clipB1) {
                EvaluateAnimation(*clipB1, tB, skeleton, B);
            }
            // Fill untouched like elsewhere
            auto ensureBind = [&](std::vector<glm::mat4>& buf){ for (int i=0;i<(int)buf.size();++i) { if (buf[i] == glm::mat4(1.0f)) buf[i] = computeLocalBind(i); } };
            ensureBind(A); ensureBind(B);
            // Lerp pose A->B by blendT
            localTransforms.resize(skeleton.BoneEntities.size(), glm::mat4(1.0f));
            for (size_t i=0;i<localTransforms.size();++i) {
                glm::vec3 T0, S0, T1, S1; glm::quat R0, R1;
                decomposeTRS(A[i], T0, R0, S0);
                decomposeTRS(B[i], T1, R1, S1);
                glm::vec3 T = glm::mix(T0, T1, blendT);
                glm::quat R = glm::slerp(R0, R1, blendT);
                glm::vec3 S = glm::mix(S0, S1, blendT);
                localTransforms[i] = glm::translate(T) * glm::mat4_cast(glm::normalize(R)) * glm::scale(S);
            }
        } else if (state.Asset) {
            // Unified evaluation into a temporary pose buffer sized to skeleton
            PoseBuffer pose; pose.local.resize(skeleton.BoneEntities.size(), glm::mat4(1.0f)); pose.touched.resize(skeleton.BoneEntities.size(), false);
            static BindingCache s_bindings; s_bindings.SetSkeleton(&skeleton);
            EvalInputs in{ state.Asset, mutableState.Time, mutableState.Loop };
            EvalTargets tgt{ &pose };
            EvalContext ctx{ &s_bindings, skeleton.Avatar.get(), &skeleton };
            // Collect script events and property writes (if any)
            std::vector<cm::animation::ScriptEvent> firedEvents;
            nlohmann::json propWrites;
            SampleAsset(in, ctx, tgt, &firedEvents, &propWrites);
            localTransforms = std::move(pose.local);
            // Fill untouched bones with bind pose locals
            for (int i = 0; i < (int)localTransforms.size(); ++i) {
                if (i < (int)pose.touched.size() && !pose.touched[i]) {
                    localTransforms[i] = computeLocalBind(i);
                }
            }

            // Dispatch script events to managed scripts attached to the skeleton root entity
            if (!firedEvents.empty()) {
                auto* rootData = scene.GetEntityData(ent.GetID());
                if (rootData) {
                    for (const auto& ev : firedEvents) {
                        const std::string& targetClass  = ev.className;
                        const std::string& targetMethod = ev.method;
                        for (auto& script : rootData->Scripts) {
                            if (!script.Instance) continue;
                            if (script.ClassName != targetClass) continue;
                            if (script.Instance->GetBackend() == ScriptBackend::Managed) {
                                auto managed = std::dynamic_pointer_cast<ManagedScriptComponent>(script.Instance);
                                if (managed && g_Script_Invoke) {
                                    g_Script_Invoke(managed->GetHandle(), targetMethod.c_str());
                                }
                            }
                        }
                    }
                }
            }
        } else if (state.LegacyClip) {
            EvaluateAnimation(*state.LegacyClip, mutableState.Time, skeleton, localTransforms);
            // EvaluateAnimation leaves non-animated bones as identity; replace with bind locals
            for (int i = 0; i < (int)localTransforms.size(); ++i) {
                // Identity check: compare to mat4 identity exactly (inputs are exact identity)
                if (localTransforms[i] == glm::mat4(1.0f)) {
                    localTransforms[i] = computeLocalBind(i);
                } 
            } 
        }
         
        // Crossfade blend if active: sample next state and blend matrices linearly (local-space)
        if (player.AnimatorInstance.IsCrossfading() && player.Controller && player.AnimatorMode == AnimationPlayerComponent::Mode::ControllerAnimated) {
            int nextId = player.AnimatorInstance.Playback().NextStateId;
            const auto* nextSt = player.Controller->FindState(nextId);
            if (nextSt) {
                const AnimationAsset* nextAsset = nullptr;
                std::shared_ptr<AnimationAsset> nextAssetPtr;
                if (!nextSt->AnimationAssetPath.empty()) { 
                    auto it = player.CachedAssets.find(nextSt->Id);
                    if (it != player.CachedAssets.end()) nextAssetPtr = it->second; else {
                        nextAssetPtr = std::make_shared<AnimationAsset>(LoadAnimationAsset(nextSt->AnimationAssetPath));
                        player.CachedAssets[nextSt->Id] = nextAssetPtr;
                    }
                    nextAsset = nextAssetPtr.get();
                }
                std::shared_ptr<AnimationClip> nextClip;
                if (!nextAsset && !nextSt->ClipPath.empty()) {
                    auto itc = player.CachedClips.find(nextSt->Id);
                    if (itc != player.CachedClips.end()) nextClip = itc->second; else {
                        nextClip = std::make_shared<AnimationClip>(LoadAnimationClip(nextSt->ClipPath));
                        player.CachedClips[nextSt->Id] = nextClip;
                    }
                }

                std::vector<glm::mat4> nextLocal(localTransforms.size(), glm::mat4(1.0f));
                float nextTime = player.AnimatorInstance.Playback().NextStateTime;
                if (nextAsset) {
                    PoseBuffer pose; pose.local.resize(skeleton.BoneEntities.size(), glm::mat4(1.0f)); pose.touched.resize(skeleton.BoneEntities.size(), false);
                    static BindingCache s_bindings; s_bindings.SetSkeleton(&skeleton);
                    EvalInputs in{ nextAsset, nextTime, nextSt->Loop };
                    EvalTargets tgt{ &pose };
                    EvalContext ctx{ &s_bindings, skeleton.Avatar.get(), &skeleton };
                    SampleAsset(in, ctx, tgt, nullptr, nullptr);
                    nextLocal = std::move(pose.local);
                } else if (nextClip) {
                    EvaluateAnimation(*nextClip, nextTime, skeleton, nextLocal);
                }

                float a = player.AnimatorInstance.CrossfadeAlpha();
                if (!nextLocal.empty() && nextLocal.size() == localTransforms.size()) {
                    for (size_t i = 0; i < localTransforms.size(); ++i) {
                        glm::vec3 T0, S0, T1, S1; glm::quat R0, R1;
                        auto decompose = [](const glm::mat4& m, glm::vec3& T, glm::quat& R, glm::vec3& S){
                            T = glm::vec3(m[3]);
                            glm::vec3 X = glm::vec3(m[0]);
                            glm::vec3 Y = glm::vec3(m[1]);
                            glm::vec3 Z = glm::vec3(m[2]);
                            S = glm::vec3(glm::length(X), glm::length(Y), glm::length(Z));
                            if (S.x > 1e-6f) X /= S.x; if (S.y > 1e-6f) Y /= S.y; if (S.z > 1e-6f) Z /= S.z;
                            glm::mat3 rotMat(X, Y, Z);
                            R = glm::quat_cast(rotMat);
                        };
                        decompose(localTransforms[i], T0, R0, S0);
                        decompose(nextLocal[i], T1, R1, S1);
                        glm::vec3 T = glm::mix(T0, T1, a);
                        glm::quat R = glm::slerp(R0, R1, a);
                        glm::vec3 S = glm::mix(S0, S1, a);
                        localTransforms[i] = glm::translate(T) * glm::mat4_cast(glm::normalize(R)) * glm::scale(S);
                    }
                }
                if (a >= 1.0f) {
                    // Crossfade complete: ensure Animator's current state is updated as well
                    player.AnimatorInstance.SetCurrentState(nextId, /*resetTime*/true);
                    player.CurrentStateId = nextId;
                }
            }
        }

        // Humanoid constraint: keep translation/scale only on root/hips; others use bind T/S, animated rotation
        if (skeleton.Avatar) {
            int hipsIdx = skeleton.Avatar->GetMappedBoneIndex(cm::animation::HumanoidBone::Hips);
            int rootIdx = skeleton.Avatar->GetMappedBoneIndex(cm::animation::HumanoidBone::Root);
            for (int i = 0; i < (int)localTransforms.size(); ++i) {
                if (i == hipsIdx || i == rootIdx) continue;
                glm::vec3 Ta, Sa; glm::quat Ra;
                decomposeTRS(localTransforms[i], Ta, Ra, Sa);
                glm::mat4 bindLocal = computeLocalBind(i);
                glm::vec3 Tb, Sb; glm::quat Rb; // Rb unused
                decomposeTRS(bindLocal, Tb, Rb, Sb);
                localTransforms[i] = glm::translate(Tb) * glm::mat4_cast(glm::normalize(Ra)) * glm::scale(Sb);
            }
        }

        // Root motion handling and in-place playback
        if (skeleton.Avatar) {
            // Compose model matrix from locals up the parent chain
            auto composeModel = [&](int boneIndex) -> glm::mat4 {
                glm::mat4 model(1.0f);
                int bi = boneIndex;
                while (bi >= 0) {
                    model = localTransforms[bi] * model;
                    bi = (bi < (int)skeleton.BoneParents.size()) ? skeleton.BoneParents[bi] : -1;
                }
                return model;
            };

            // Replace a bone's local translation with its bind local translation, preserve animated R/S
            auto zeroLocalTranslationToBind = [&](int boneIndex) {
                if (boneIndex < 0 || boneIndex >= (int)localTransforms.size()) return;
                glm::vec3 Ta, Sa; glm::quat Ra;
                decomposeTRS(localTransforms[boneIndex], Ta, Ra, Sa);
                glm::vec3 Tb, Sb; glm::quat Rb;
                decomposeTRS(computeLocalBind(boneIndex), Tb, Rb, Sb);
                localTransforms[boneIndex] = glm::translate(Tb) * glm::mat4_cast(glm::normalize(Ra)) * glm::scale(Sb);
            };

            int hipsIdx = skeleton.Avatar->GetMappedBoneIndex(cm::animation::HumanoidBone::Hips);
            int rootIdx = skeleton.Avatar->GetMappedBoneIndex(cm::animation::HumanoidBone::Root);

            switch (player.RootMotion) {
                case AnimationPlayerComponent::RootMotionMode::None: {
                    // Keep rig in-place: zero translation on hips and root back to bind
                    zeroLocalTranslationToBind(hipsIdx);
                    zeroLocalTranslationToBind(rootIdx);
                    player._PrevRootValid = false;
                } break;

                case AnimationPlayerComponent::RootMotionMode::FromHipsToEntity:
                case AnimationPlayerComponent::RootMotionMode::FromRootToEntity: {
                    const int src = (player.RootMotion == AnimationPlayerComponent::RootMotionMode::FromHipsToEntity) ? hipsIdx : rootIdx;
                    if (src >= 0) {
                        glm::vec3 curPos = glm::vec3(composeModel(src)[3]);
                        if (player._PrevRootValid) {
                            glm::vec3 delta = curPos - player._PrevRootModelPos;
                            if (auto* rootData = scene.GetEntityData(ent.GetID())) {
                                rootData->Transform.Position += delta;
                                rootData->Transform.TransformDirty = true;
                            }
                        }
                        player._PrevRootModelPos = curPos;
                        player._PrevRootValid = true;

                        // After extracting root motion, keep the animated bone in-place
                        zeroLocalTranslationToBind(src);
                    } else {
                        player._PrevRootValid = false;
                    }
                } break;
            }
        }

        // Write evaluated local pose into bone entities as TRS so transform system recomputes matrices
        auto decomposeTRS = [](const glm::mat4& m, glm::vec3& T, glm::quat& R, glm::vec3& S){
            T = glm::vec3(m[3]);
            glm::vec3 X = glm::vec3(m[0]);
            glm::vec3 Y = glm::vec3(m[1]);
            glm::vec3 Z = glm::vec3(m[2]);
            S = glm::vec3(glm::length(X), glm::length(Y), glm::length(Z));
            if (S.x > 1e-6f) X /= S.x; if (S.y > 1e-6f) Y /= S.y; if (S.z > 1e-6f) Z /= S.z;
            glm::mat3 rotMat(X, Y, Z);
            R = glm::quat_cast(rotMat);
        };
        for (size_t i = 0; i < localTransforms.size() && i < skeleton.BoneEntities.size(); ++i) {
            EntityID boneId = skeleton.BoneEntities[i];
            if (boneId == (EntityID)-1) continue;
            if (auto* bd = scene.GetEntityData(boneId)) {
                glm::vec3 T, S; glm::quat R;
                decomposeTRS(localTransforms[i], T, R, S);
                bd->Transform.Position = T;
                bd->Transform.Scale    = S;
                bd->Transform.RotationQ = glm::normalize(R);
                bd->Transform.UseQuatRotation = true;
                // Keep Euler for inspector display
                bd->Transform.Rotation = glm::degrees(glm::eulerAngles(bd->Transform.RotationQ));
                bd->Transform.TransformDirty = true;
            }
        }
    }
}

} // namespace animation
} // namespace cm
