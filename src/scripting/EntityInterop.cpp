#include "EntityInterop.h"
#include "ecs/Scene.h"

GetEntityPosition_fn GetEntityPositionPtr = &GetEntityPosition;
SetEntityPosition_fn SetEntityPositionPtr = &SetEntityPosition;
FindEntityByName_fn  FindEntityByNamePtr = &FindEntityByName;

extern "C"
{
    __declspec(dllexport) void GetEntityPosition(int entityID, float* outX, float* outY, float* outZ)
    {
        auto pos = Scene::Get().GetEntityData(entityID)->Transform.Position;
        *outX = pos.x;
        *outY = pos.y;
        *outZ = pos.z;
    }

    __declspec(dllexport) void SetEntityPosition(int entityID, float x, float y, float z)
    {
        auto data = Scene::Get().GetEntityData(entityID);
        data->Transform.Position = glm::vec3(x, y, z);
        Scene::Get().MarkTransformDirty(entityID);
    }

    __declspec(dllexport) int FindEntityByName(const char* name)
    {
        for (const Entity& e : Scene::Get().GetEntities())
            if (e.GetName() == name)
                return e.GetID();
        return -1;
    }
}
