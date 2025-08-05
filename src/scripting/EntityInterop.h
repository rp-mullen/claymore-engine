#pragma once

#ifdef __cplusplus
extern "C" {
#endif

	__declspec(dllexport) void GetEntityPosition(int entityID, float* outX, float* outY, float* outZ);
	__declspec(dllexport) void SetEntityPosition(int entityID, float x, float y, float z);
	__declspec(dllexport) int  FindEntityByName(const char* name);

    // Entity management
    __declspec(dllexport) int  CreateEntity(const char* name);
    __declspec(dllexport) void DestroyEntity(int entityID);

    // Rotation & Scale
    __declspec(dllexport) void GetEntityRotation(int entityID, float* outX, float* outY, float* outZ);
    __declspec(dllexport) void SetEntityRotation(int entityID, float x, float y, float z);
    __declspec(dllexport) void GetEntityScale(int entityID, float* outX, float* outY, float* outZ);
    __declspec(dllexport) void SetEntityScale(int entityID, float x, float y, float z);

    // Physics
    __declspec(dllexport) void SetLinearVelocity(int entityID, float x, float y, float z);
    __declspec(dllexport) void SetAngularVelocity(int entityID, float x, float y, float z);

    // Lighting
    __declspec(dllexport) void SetLightColor(int entityID, float r, float g, float b);
    __declspec(dllexport) void SetLightIntensity(int entityID, float intensity);

    // BlendShapes
    __declspec(dllexport) void SetBlendShapeWeight(int entityID, const char* shapeName, float weight);

#ifdef __cplusplus
}
#endif
