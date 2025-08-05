#pragma once

#include <bgfx/bgfx.h>
#include "particles/ParticleSystem.h"

// Forward declarations
class Scene;

namespace ecs
{
    class ParticleEmitterSystem
    {
    public:
        static ParticleEmitterSystem& Get();

        void Init();
        void Shutdown();

        // Tick emitters and underlying particle system.
        void Update(Scene& scene, float dt);

        // Submit draw calls for all emitters.
        void Render(uint8_t viewId, const float* mtxView, const bx::Vec3& eye);
    private:
        bool m_Initialized = false;
    };
}
