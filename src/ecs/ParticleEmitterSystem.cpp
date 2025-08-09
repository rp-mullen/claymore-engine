#include "ParticleEmitterSystem.h"
#include "Scene.h"
#include "Components.h"
#include "Entity.h"

namespace ecs 
{
    ParticleEmitterSystem& ParticleEmitterSystem::Get()
    {
        static ParticleEmitterSystem s_Instance;
        return s_Instance;
    }

    void ParticleEmitterSystem::Init()
    {
        if (!m_Initialized)
        {
            ps::init(128); // Allow up to 128 emitters by default.
            m_Initialized = true;
        }
    }

    void ParticleEmitterSystem::Shutdown()
    {
        if (m_Initialized)
        {
            ps::shutdown();
            m_Initialized = false;
        }
    }

    void ParticleEmitterSystem::Update(Scene& scene, float dt)
    {
        if (!m_Initialized)
            Init();
        // Iterate all entities and sync emitter uniforms with transform.
        const auto& entityList = scene.GetEntities();
        for (const auto& entity : entityList)
        {
            EntityID id = entity.GetID();
            auto* dataPtr = scene.GetEntityData(id);
            if (!dataPtr || !dataPtr->Emitter) continue;
            auto& emitterComp = *dataPtr->Emitter;
            if (!emitterComp.Enabled) continue;

            // Respect entity visibility for particle systems in editor and play mode
            if (!dataPtr->Visible) continue;

            // Create emitter lazily.
            if (!ps::isValid(emitterComp.Handle))
            {
                emitterComp.Handle = ps::createEmitter(ps::EmitterShape::Sphere, ps::EmitterDirection::Up, emitterComp.MaxParticles);
                emitterComp.Uniforms.m_handle = emitterComp.SpriteHandle;
                // Give a sane default so particles actually spawn if user hasn't set it yet.
                if (emitterComp.Uniforms.m_particlesPerSecond == 0)
                    emitterComp.Uniforms.m_particlesPerSecond = 100;
            }

            auto& data = *dataPtr;

            // Update position from TransformComponent.
            emitterComp.Uniforms.m_position[0] = data.Transform.Position.x;
            emitterComp.Uniforms.m_position[1] = data.Transform.Position.y;
            emitterComp.Uniforms.m_position[2] = data.Transform.Position.z;

            ps::updateEmitter(emitterComp.Handle, &emitterComp.Uniforms);
        }

        // Step particle simulation once per frame.
        ps::update(dt);
    }

    void ParticleEmitterSystem::Render(uint8_t viewId, const float* mtxView, const bx::Vec3& eye)
    {
        if (!m_Initialized) return;
        ps::render(viewId, mtxView, eye);
    }
}
