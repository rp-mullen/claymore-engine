#pragma once

#ifdef __cplusplus
extern "C" {
#endif

	__declspec(dllexport) void GetEntityPosition(int entityID, float* outX, float* outY, float* outZ);
	__declspec(dllexport) void SetEntityPosition(int entityID, float x, float y, float z);
	__declspec(dllexport) int FindEntityByName(const char* name);

#ifdef __cplusplus
}
#endif
