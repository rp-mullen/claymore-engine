#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <editor/Project.h>

namespace cm {
namespace animation {

// Node kinds
enum class AnimatorStateKind {
    Single = 0,
    Blend1D = 1
};

struct Blend1DEntry {
    float Key = 0.0f; // normalized 0..1
    std::string ClipPath; // Legacy .anim
    std::string AssetPath; // Unified .anim
};

// Parameters
enum class AnimatorParamType {
    Bool,
    Int,
    Float,
    Trigger
};

struct AnimatorParameter {
    std::string Name;
    AnimatorParamType Type = AnimatorParamType::Float;
    // Defaults
    bool DefaultBool = false;
    int DefaultInt = 0;
    float DefaultFloat = 0.0f;
};

// Conditions
enum class ConditionMode {
    If,
    IfNot,
    Greater,
    Less,
    Equals,
    NotEquals,
    Trigger
};

struct AnimatorCondition {
    std::string Parameter;
    ConditionMode Mode = ConditionMode::If;
    // For numeric comparisons
    float Threshold = 0.0f;
    int IntThreshold = 0;
};

// State & Transition
struct AnimatorState {
    int Id = -1;
    std::string Name;
    std::string ClipPath; // Legacy .anim
    std::string AnimationAssetPath; // Unified .anim (new)
    float Speed = 1.0f;
    bool Loop = true;
    AnimatorStateKind Kind = AnimatorStateKind::Single;
    // Blend1D specific
    std::string Blend1DParam; // name of float parameter
    std::vector<Blend1DEntry> Blend1DEntries; // sorted by Key
    // Editor visualization
    float EditorPosX = 0.0f;
    float EditorPosY = 0.0f;
};

struct AnimatorTransition {
    int Id = -1;               // Stable link id for editor selection
    int FromState = -1; // -1 means AnyState
    int ToState = -1;
    bool HasExitTime = false;
    float ExitTime = 0.0f; // normalized 0..1
    float Duration = 0.0f; // seconds (MVP: instant if 0)
    std::vector<AnimatorCondition> Conditions;
};

struct AnimatorController {
    std::string Name;
    std::vector<AnimatorParameter> Parameters;
    std::vector<AnimatorState> States;
    std::vector<AnimatorTransition> Transitions;
    int DefaultState = -1;

    const AnimatorState* FindState(int id) const {
        for (const auto& s : States) if (s.Id == id) return &s; return nullptr;
    }
    AnimatorState* FindState(int id) {
        for (auto& s : States) if (s.Id == id) return &s; return nullptr;
    }
};

// Serialization helpers for nlohmann::json (must be in same namespace for ADL)
    inline std::string ToString(AnimatorParamType t) {
        switch (t) {
            case AnimatorParamType::Bool: return "bool";
            case AnimatorParamType::Int: return "int";
            case AnimatorParamType::Float: return "float";
            case AnimatorParamType::Trigger: return "trigger";
        }
        return "float";
    }
    inline AnimatorParamType ParamTypeFromString(const std::string& s) {
        if (s == "bool") return AnimatorParamType::Bool;
        if (s == "int") return AnimatorParamType::Int;
        if (s == "float") return AnimatorParamType::Float;
        if (s == "trigger") return AnimatorParamType::Trigger;
        return AnimatorParamType::Float;
    }

    inline std::string ToString(ConditionMode m) {
        switch (m) {
            case ConditionMode::If: return "if";
            case ConditionMode::IfNot: return "if_not";
            case ConditionMode::Greater: return "greater";
            case ConditionMode::Less: return "less";
            case ConditionMode::Equals: return "equals";
            case ConditionMode::NotEquals: return "not_equals";
            case ConditionMode::Trigger: return "trigger";
        }
        return "if";
    }
    inline ConditionMode ConditionModeFromString(const std::string& s) {
        if (s == "if") return ConditionMode::If;
        if (s == "if_not") return ConditionMode::IfNot;
        if (s == "greater") return ConditionMode::Greater;
        if (s == "less") return ConditionMode::Less;
        if (s == "equals") return ConditionMode::Equals;
        if (s == "not_equals") return ConditionMode::NotEquals;
        if (s == "trigger") return ConditionMode::Trigger;
        return ConditionMode::If;
    }

    inline void to_json(nlohmann::json& j, const AnimatorParameter& p) {
        j = nlohmann::json{{"name", p.Name}, {"type", ToString(p.Type)}, {"defaultBool", p.DefaultBool}, {"defaultInt", p.DefaultInt}, {"defaultFloat", p.DefaultFloat}};
    }
    inline void from_json(const nlohmann::json& j, AnimatorParameter& p) {
        p.Name = j.value("name", "");
        p.Type = ParamTypeFromString(j.value("type", "float"));
        p.DefaultBool = j.value("defaultBool", false);
        p.DefaultInt = j.value("defaultInt", 0);
        p.DefaultFloat = j.value("defaultFloat", 0.0f);
    }

    inline void to_json(nlohmann::json& j, const AnimatorCondition& c) {
        j = nlohmann::json{{"param", c.Parameter}, {"mode", ToString(c.Mode)}, {"threshold", c.Threshold}, {"iThreshold", c.IntThreshold}};
    }
    inline void from_json(const nlohmann::json& j, AnimatorCondition& c) {
        c.Parameter = j.value("param", "");
        c.Mode = ConditionModeFromString(j.value("mode", "if"));
        c.Threshold = j.value("threshold", 0.0f);
        c.IntThreshold = j.value("iThreshold", 0);
    }

    inline static std::string _NormalizeSlashes(const std::string& in) {
        std::string s = in; for (char& c : s) if (c == '\\') c = '/'; return s;
    }
    inline static std::string _MakeProjectRelative(const std::string& path) {
        if (path.empty()) return path;
        try {
            std::filesystem::path p(path);
            std::filesystem::path base = Project::GetProjectDirectory();
            if (!base.empty()) {
                std::filesystem::path rel = std::filesystem::relative(p, base);
                return _NormalizeSlashes(rel.string());
            }
        } catch (...) {}
        return _NormalizeSlashes(path);
    }
    inline static std::string _ResolveProjectRelative(const std::string& path) {
        if (path.empty()) return path;
        try {
            std::filesystem::path p(path);
            if (p.is_absolute()) return _NormalizeSlashes(p.string());
            std::filesystem::path base = Project::GetProjectDirectory();
            if (!base.empty()) return _NormalizeSlashes((base / p).string());
        } catch (...) {}
        return _NormalizeSlashes(path);
    }

    inline void to_json(nlohmann::json& j, const Blend1DEntry& e) {
        j = nlohmann::json{{"key", e.Key}, {"clip", _MakeProjectRelative(e.ClipPath)}, {"asset", _MakeProjectRelative(e.AssetPath)}};
    }
    inline void from_json(const nlohmann::json& j, Blend1DEntry& e) {
        e.Key = j.value("key", 0.0f);
        e.ClipPath = _NormalizeSlashes(j.value("clip", ""));
        e.AssetPath = _NormalizeSlashes(j.value("asset", ""));
    }

    inline void to_json(nlohmann::json& j, const AnimatorState& s) {
        j = nlohmann::json{{"id", s.Id}, {"name", s.Name}, {"clip", _MakeProjectRelative(s.ClipPath)}, {"asset", _MakeProjectRelative(s.AnimationAssetPath)}, {"speed", s.Speed}, {"loop", s.Loop}, {"x", s.EditorPosX}, {"y", s.EditorPosY}, {"kind", (int)s.Kind}, {"blendParam", s.Blend1DParam}, {"entries", s.Blend1DEntries}};
    }
    inline void from_json(const nlohmann::json& j, AnimatorState& s) {
        s.Id = j.value("id", -1);
        s.Name = j.value("name", "");
        s.ClipPath = _NormalizeSlashes(j.value("clip", ""));
        s.AnimationAssetPath = _NormalizeSlashes(j.value("asset", ""));
        s.Speed = j.value("speed", 1.0f);
        s.Loop = j.value("loop", true);
        s.EditorPosX = j.value("x", 0.0f);
        s.EditorPosY = j.value("y", 0.0f);
        s.Kind = (AnimatorStateKind)j.value("kind", 0);
        s.Blend1DParam = j.value("blendParam", "");
        if (j.contains("entries")) s.Blend1DEntries = j["entries"].get<std::vector<Blend1DEntry>>();
    }

    inline void to_json(nlohmann::json& j, const AnimatorTransition& t) {
        j = nlohmann::json{{"id", t.Id}, {"from", t.FromState}, {"to", t.ToState}, {"exit", t.HasExitTime}, {"exitTime", t.ExitTime}, {"duration", t.Duration}, {"conditions", t.Conditions}};
    }
    inline void from_json(const nlohmann::json& j, AnimatorTransition& t) {
        t.Id = j.value("id", -1);
        t.FromState = j.value("from", -1);
        t.ToState = j.value("to", -1);
        t.HasExitTime = j.value("exit", false);
        t.ExitTime = j.value("exitTime", 0.0f);
        t.Duration = j.value("duration", 0.0f);
        if (j.contains("conditions")) t.Conditions = j["conditions"].get<std::vector<AnimatorCondition>>();
    }

    inline void to_json(nlohmann::json& j, const AnimatorController& c) {
        j = nlohmann::json{{"name", c.Name}, {"defaultState", c.DefaultState}, {"parameters", c.Parameters}, {"states", c.States}, {"transitions", c.Transitions}};
    }
    inline void from_json(const nlohmann::json& j, AnimatorController& c) {
        c.Name = j.value("name", "");
        c.DefaultState = j.value("defaultState", -1);
        if (j.contains("parameters")) c.Parameters = j["parameters"].get<std::vector<AnimatorParameter>>();
        if (j.contains("states")) c.States = j["states"].get<std::vector<AnimatorState>>();
        if (j.contains("transitions")) c.Transitions = j["transitions"].get<std::vector<AnimatorTransition>>();
    }
// end serialization helpers

} // namespace animation
} // namespace cm


