#pragma once
#include "ScriptReflection.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Managed -> Native reflection registration
// ---------------------------------------------------------------------------
// propType is integer value of PropertyType enum
__declspec(dllexport) void RegisterScriptPropertyNative(const char* className,
                                                        const char* fieldName,
                                                        int propType,
                                                        void* boxedDefault);

// Native -> Managed script type registration
__declspec(dllexport) void NativeRegisterScriptType(const char* className);

// Native keeps a pointer to managed setter so Inspector can push values back
using SetManagedField_fn = void(*)(void* /*GCHandle*/, const char* /*field*/, void* /*boxed*/);
extern SetManagedField_fn SetManagedFieldPtr;

#ifdef __cplusplus
}
#endif