#pragma once
#include "ScriptReflection.h"
#include <glm/glm.hpp>

// This file demonstrates how script reflection would be set up
// In a real implementation, this would be called from the managed code interop

inline void RegisterSampleScriptProperties() {
    // Example: PlayerController script with common properties
    ScriptReflection::RegisterScriptProperty("PlayerController", {
        "Speed", PropertyType::Float, 5.0f, 5.0f, nullptr, nullptr
    });
    
    ScriptReflection::RegisterScriptProperty("PlayerController", {
        "JumpHeight", PropertyType::Float, 2.0f, 2.0f, nullptr, nullptr
    });
    
    ScriptReflection::RegisterScriptProperty("PlayerController", {
        "CanDoubleJump", PropertyType::Bool, true, true, nullptr, nullptr
    });

    // Example: EnemyAI script
    ScriptReflection::RegisterScriptProperty("EnemyAI", {
        "PatrolRadius", PropertyType::Float, 10.0f, 10.0f, nullptr, nullptr
    });
    
    ScriptReflection::RegisterScriptProperty("EnemyAI", {
        "DetectionRange", PropertyType::Float, 5.0f, 5.0f, nullptr, nullptr
    });
    
    ScriptReflection::RegisterScriptProperty("EnemyAI", {
        "PatrolPoints", PropertyType::Int, 4, 4, nullptr, nullptr
    });

    // Example: Transform Rotator script
    ScriptReflection::RegisterScriptProperty("TransformRotator", {
        "RotationSpeed", PropertyType::Vector3, glm::vec3(0, 90, 0), glm::vec3(0, 90, 0), nullptr, nullptr
    });
    
    ScriptReflection::RegisterScriptProperty("TransformRotator", {
        "RotateInLocalSpace", PropertyType::Bool, true, true, nullptr, nullptr
    });

    // Example: UI Text Display script
    ScriptReflection::RegisterScriptProperty("UITextDisplay", {
        "DisplayText", PropertyType::String, std::string("Hello World"), std::string("Hello World"), nullptr, nullptr
    });
    
    ScriptReflection::RegisterScriptProperty("UITextDisplay", {
        "UpdateInterval", PropertyType::Float, 1.0f, 1.0f, nullptr, nullptr
    });
}