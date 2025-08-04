#include "ScriptReflection.h"
#include <glm/glm.hpp>
#include <sstream>

// Static member definition
std::unordered_map<std::string, std::vector<PropertyInfo>> ScriptReflection::s_ScriptProperties;

void ScriptReflection::RegisterScriptProperty(const std::string& scriptClass, const PropertyInfo& property) {
    s_ScriptProperties[scriptClass].push_back(property);
}

std::vector<PropertyInfo> ScriptReflection::GetScriptProperties(const std::string& scriptClass) {
    auto it = s_ScriptProperties.find(scriptClass);
    if (it != s_ScriptProperties.end()) {
        return it->second;
    }
    return {};
}

bool ScriptReflection::HasProperties(const std::string& scriptClass) {
    auto it = s_ScriptProperties.find(scriptClass);
    return it != s_ScriptProperties.end() && !it->second.empty();
}

std::string ScriptReflection::PropertyTypeToString(PropertyType type) {
    switch (type) {
        case PropertyType::Int: return "int";
        case PropertyType::Float: return "float";
        case PropertyType::Bool: return "bool";
        case PropertyType::String: return "string";
        case PropertyType::Vector3: return "Vector3";
        default: return "unknown";
    }
}

PropertyType ScriptReflection::StringToPropertyType(const std::string& typeStr) {
    if (typeStr == "int") return PropertyType::Int;
    if (typeStr == "float") return PropertyType::Float;
    if (typeStr == "bool") return PropertyType::Bool;
    if (typeStr == "string") return PropertyType::String;
    if (typeStr == "Vector3") return PropertyType::Vector3;
    return PropertyType::Int; // Default fallback
}

std::string ScriptReflection::PropertyValueToString(const PropertyValue& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, int>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, float>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            std::ostringstream oss;
            oss << v.x << "," << v.y << "," << v.z;
            return oss.str();
        }
        return "";
    }, value);
}

PropertyValue ScriptReflection::StringToPropertyValue(const std::string& str, PropertyType type) {
    switch (type) {
        case PropertyType::Int:
            try { return std::stoi(str); }
            catch (...) { return 0; }
            
        case PropertyType::Float:
            try { return std::stof(str); }
            catch (...) { return 0.0f; }
            
        case PropertyType::Bool:
            return str == "true" || str == "1";
            
        case PropertyType::String:
            return str;
            
        case PropertyType::Vector3: {
            std::istringstream iss(str);
            std::string token;
            glm::vec3 vec(0.0f);
            int component = 0;
            
            while (std::getline(iss, token, ',') && component < 3) {
                try {
                    vec[component] = std::stof(token);
                } catch (...) {
                    vec[component] = 0.0f;
                }
                component++;
            }
            return vec;
        }
        
        default:
            return 0;
    }
}