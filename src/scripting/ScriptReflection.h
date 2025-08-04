#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <functional>
#include <glm/glm.hpp>

// Supported property types for reflection
using PropertyValue = std::variant<int, float, bool, std::string, glm::vec3>;

enum class PropertyType {
    Int,
    Float,
    Bool,
    String,
    Vector3
};

struct PropertyInfo {
    std::string name;
    PropertyType type;
    PropertyValue defaultValue;
    PropertyValue currentValue;
    
    // Callbacks for getting/setting values from managed scripts
    std::function<PropertyValue()> getter;
    std::function<void(const PropertyValue&)> setter;
};

class ScriptReflection {
public:
    // Static registry for script properties
    static void RegisterScriptProperty(const std::string& scriptClass, const PropertyInfo& property);
    static std::vector<PropertyInfo> GetScriptProperties(const std::string& scriptClass);
    static bool HasProperties(const std::string& scriptClass);
    
    // Property type utilities
    static std::string PropertyTypeToString(PropertyType type);
    static PropertyType StringToPropertyType(const std::string& typeStr);
    
    // Property value utilities
    static std::string PropertyValueToString(const PropertyValue& value);
    static PropertyValue StringToPropertyValue(const std::string& str, PropertyType type);

private:
    static std::unordered_map<std::string, std::vector<PropertyInfo>> s_ScriptProperties;
};

// Macro for easy property registration (would be used in managed code interop)
#define REGISTER_SCRIPT_PROPERTY(scriptClass, propertyName, propertyType, defaultVal) \
    ScriptReflection::RegisterScriptProperty(scriptClass, {propertyName, propertyType, defaultVal, defaultVal, nullptr, nullptr});