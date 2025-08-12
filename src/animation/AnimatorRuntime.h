#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "animation/AnimatorController.h"
#include "animation/AnimationTypes.h"

namespace cm {
namespace animation {

struct AnimatorBlackboard {
    std::unordered_map<std::string, bool> Bools;
    std::unordered_map<std::string, int> Ints;
    std::unordered_map<std::string, float> Floats;
    std::unordered_map<std::string, bool> Triggers; // consumed when read
};

struct AnimatorPlayback {
    int CurrentStateId = -1;
    float StateTime = 0.0f; // seconds
    float StateNormalized = 0.0f; // 0..1 (cached)
    int NextStateId = -1; // for cross-fade
    float CrossfadeTime = 0.0f;
    float CrossfadeDuration = 0.0f; // seconds; 0 means no crossfade
    float NextStateTime = 0.0f; // seconds accumulator for next state during crossfade
};

class Animator {
public:
    Animator() = default;

    void SetController(std::shared_ptr<AnimatorController> controller) { m_Controller = std::move(controller); }
    std::shared_ptr<AnimatorController> GetController() const { return m_Controller; }

    AnimatorBlackboard& Blackboard() { return m_Blackboard; }
    const AnimatorPlayback& Playback() const { return m_Playback; }

    void ResetToDefaults();
    void Update(float deltaTime, float clipDuration);
    int ChooseNextState() const; // returns -1 if none
    void ConsumeTriggers();

    // Crossfade control (MVP): call when a transition with duration is selected
    void BeginCrossfade(int toStateId, float durationSeconds);
    bool IsCrossfading() const { return m_Playback.CrossfadeDuration > 0.0f && m_Playback.CrossfadeTime < m_Playback.CrossfadeDuration; }
    float CrossfadeAlpha() const { return m_Playback.CrossfadeDuration > 0.0f ? glm::clamp(m_Playback.CrossfadeTime / m_Playback.CrossfadeDuration, 0.0f, 1.0f) : 1.0f; }

    // Advance internal crossfade timers (used by systems to tick without touching privates)
    void AdvanceCrossfade(float deltaSeconds) {
        if (m_Playback.CrossfadeDuration <= 0.0f) return;
        m_Playback.CrossfadeTime += deltaSeconds;
        m_Playback.NextStateTime += deltaSeconds;
        if (m_Playback.CrossfadeTime >= m_Playback.CrossfadeDuration) {
            m_Playback.CrossfadeTime = m_Playback.CrossfadeDuration;
        }
    }

private:
    std::shared_ptr<AnimatorController> m_Controller;
    AnimatorBlackboard m_Blackboard;
    AnimatorPlayback m_Playback;
};

} // namespace animation
} // namespace cm


