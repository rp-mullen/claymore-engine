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
    int NextStateId = -1; // for cross-fade (MVP unused)
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

private:
    std::shared_ptr<AnimatorController> m_Controller;
    AnimatorBlackboard m_Blackboard;
    AnimatorPlayback m_Playback;
};

} // namespace animation
} // namespace cm


