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
    __declspec(dllexport) int GetEntityByID(int entityID);
    __declspec(dllexport) int* GetEntities();
    __declspec(dllexport) int GetEntityCount();

    // Rotation & Scale
    __declspec(dllexport) void GetEntityRotation(int entityID, float* outX, float* outY, float* outZ);
    __declspec(dllexport) void SetEntityRotation(int entityID, float x, float y, float z);
    // Quaternion rotation (x, y, z, w)
    __declspec(dllexport) void GetEntityRotationQuat(int entityID, float* outX, float* outY, float* outZ, float* outW);
    __declspec(dllexport) void SetEntityRotationQuat(int entityID, float x, float y, float z, float w);
    __declspec(dllexport) void GetEntityScale(int entityID, float* outX, float* outY, float* outZ);
    __declspec(dllexport) void SetEntityScale(int entityID, float x, float y, float z);

    // Physics
    __declspec(dllexport) void SetLinearVelocity(int entityID, float x, float y, float z);
    __declspec(dllexport) void SetAngularVelocity(int entityID, float x, float y, float z);


#ifdef __cplusplus
}
#endif
