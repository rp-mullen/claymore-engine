#include "ScriptReflectionInterop.h"

SetManagedField_fn SetManagedFieldPtr = nullptr;

extern "C" __declspec(dllexport)
void RegisterScriptPropertyNative(const char* className,
                                  const char* fieldName,
                                  int propType,
                                  void* boxedDefault)
{
    PropertyInfo info;
    info.name = fieldName;
    info.type = static_cast<PropertyType>(propType);
    info.currentValue = ScriptReflection::BoxToValue(boxedDefault, info.type);
    // defaultValue can mirror currentValue for now
    info.defaultValue = info.currentValue;

    ScriptReflection::RegisterScriptProperty(className, info);
}