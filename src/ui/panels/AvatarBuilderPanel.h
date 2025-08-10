#pragma once

#include <string>
#include <memory>
struct Scene;
struct SkeletonComponent;

namespace cm { namespace animation { struct AvatarDefinition; } }

class AvatarBuilderPanel {
public:
    AvatarBuilderPanel(Scene* scene);
    void SetContext(Scene* scene);
    void OnImGuiRender();
    bool IsOpen() const { return m_Open; }
    void OpenForEntity(int entityId);

private:
    Scene* m_Scene = nullptr;
    int m_TargetEntity = -1;
    bool m_Open = false;
    std::unique_ptr<cm::animation::AvatarDefinition> m_Working;

    void DrawMappingUI(SkeletonComponent& skel);
    void AutoMap(SkeletonComponent& skel);
    void Validate(SkeletonComponent& skel);
    void SaveAvatar(SkeletonComponent& skel);
};


