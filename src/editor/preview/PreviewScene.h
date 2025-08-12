// PreviewScene.h
#pragma once

#include <bgfx/bgfx.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include "ecs/Scene.h"
#include "rendering/Camera.h"

struct Model;
struct SkeletonComponent;

class PreviewScene {
public:
    bool Init(int width, int height);
    void Resize(int width, int height);
    void Shutdown();

    bgfx::TextureHandle GetColorTexture() const { return m_Color; }
    void SetModelPath(const std::string& path);
    Scene* GetScene() { return &m_Scene; }
    SkeletonComponent* GetSkeleton();
    void Render(float dt);
    void EnsureInView(float padding = 0.2f);

    void Orbit(float dx, float dy);
    void Dolly(float dz);
    void Pan(float dx, float dy);
    void ResetCamera();

private:
    uint16_t m_ViewId = 211; // dedicated inspector preview view
    bgfx::FrameBufferHandle m_FBO = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_Color = BGFX_INVALID_HANDLE;
    int m_Width = 0;
    int m_Height = 0;

    // Minimal scene: local ECS clone would be heavy; hold references/pointers as needed
    glm::vec3 m_CamTarget{0.0f};
    float m_Distance = 3.0f;
    float m_Yaw = 0.0f;
    float m_Pitch = 15.0f;

    Scene m_Scene;
    Camera m_Camera{60.0f, 1.6f, 0.1f, 100.0f};
    EntityID m_ModelRoot = -1;
};


