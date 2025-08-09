#include "animation/AnimatorRuntime.h"

namespace cm {
namespace animation {

void Animator::ResetToDefaults() {
    m_Blackboard = AnimatorBlackboard{};
    if (!m_Controller) { m_Playback = {}; return; }
    for (const auto& p : m_Controller->Parameters) {
        switch (p.Type) {
            case AnimatorParamType::Bool:   m_Blackboard.Bools[p.Name] = p.DefaultBool; break;
            case AnimatorParamType::Int:    m_Blackboard.Ints[p.Name] = p.DefaultInt; break;
            case AnimatorParamType::Float:  m_Blackboard.Floats[p.Name] = p.DefaultFloat; break;
            case AnimatorParamType::Trigger:m_Blackboard.Triggers[p.Name] = false; break;
        }
    }
    m_Playback = {};
    m_Playback.CurrentStateId = m_Controller->DefaultState;
    m_Playback.StateTime = 0.0f;
}

static bool EvaluateCondition(const AnimatorCondition& c, const AnimatorBlackboard& bb) {
    switch (c.Mode) {
        case ConditionMode::If: {
            auto it = bb.Bools.find(c.Parameter); return it != bb.Bools.end() && it->second;
        }
        case ConditionMode::IfNot: {
            auto it = bb.Bools.find(c.Parameter); return it != bb.Bools.end() && !it->second;
        }
        case ConditionMode::Greater: {
            auto it = bb.Floats.find(c.Parameter); if (it != bb.Floats.end()) return it->second > c.Threshold;
            auto iti = bb.Ints.find(c.Parameter); if (iti != bb.Ints.end()) return iti->second > c.IntThreshold;
            return false;
        }
        case ConditionMode::Less: {
            auto it = bb.Floats.find(c.Parameter); if (it != bb.Floats.end()) return it->second < c.Threshold;
            auto iti = bb.Ints.find(c.Parameter); if (iti != bb.Ints.end()) return iti->second < c.IntThreshold;
            return false;
        }
        case ConditionMode::Equals: {
            auto it = bb.Floats.find(c.Parameter); if (it != bb.Floats.end()) return it->second == c.Threshold;
            auto iti = bb.Ints.find(c.Parameter); if (iti != bb.Ints.end()) return iti->second == c.IntThreshold;
            return false;
        }
        case ConditionMode::NotEquals: {
            auto it = bb.Floats.find(c.Parameter); if (it != bb.Floats.end()) return it->second != c.Threshold;
            auto iti = bb.Ints.find(c.Parameter); if (iti != bb.Ints.end()) return iti->second != c.IntThreshold;
            return false;
        }
        case ConditionMode::Trigger: {
            auto it = bb.Triggers.find(c.Parameter); return it != bb.Triggers.end() && it->second;
        }
    }
    return false;
}

int Animator::ChooseNextState() const {
    if (!m_Controller) return -1;
    const int current = m_Playback.CurrentStateId;
    for (const auto& t : m_Controller->Transitions) {
        if (t.FromState != -1 && t.FromState != current) continue;
        bool ok = true;
        for (const auto& cond : t.Conditions) {
            if (!EvaluateCondition(cond, m_Blackboard)) { ok = false; break; }
        }
        if (!ok) continue;
        // Respect exit time if required
        if (t.HasExitTime) {
            if (m_Playback.StateNormalized + 1e-4f < t.ExitTime) continue;
        }
        return t.ToState;
    }
    return -1;
}

void Animator::ConsumeTriggers() {
    for (auto& kv : m_Blackboard.Triggers) kv.second = false;
}

void Animator::Update(float deltaTime, float clipDuration) {
    if (!m_Controller) return;
    if (m_Playback.CurrentStateId < 0) m_Playback.CurrentStateId = m_Controller->DefaultState;
    m_Playback.StateTime += deltaTime;
    if (clipDuration > 0.0f)
        m_Playback.StateNormalized = fmod(m_Playback.StateTime, clipDuration) / clipDuration;
    else
        m_Playback.StateNormalized = 0.0f;
}

} // namespace animation
} // namespace cm


