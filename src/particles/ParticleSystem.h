/*
 * Adapted from BGFX example particle system (copyright Branimir Karadzic).
 * Integrated into Claymore engine.
 */
#pragma once

#include <bgfx/bgfx.h>
#include <bx/allocator.h>
#include <bx/bounds.h>
#include <bx/easing.h>
#include <bx/rng.h>
#include <cstdint>

// Forward declare pack rect types (implemented in packrect.h)
struct Pack2D;

namespace ps // short namespace for particle system
{
    struct EmitterHandle       { uint16_t idx; };
    struct EmitterSpriteHandle { uint16_t idx; };

    template<typename Ty>
    inline bool isValid(Ty _handle) { return _handle.idx != UINT16_MAX; }

    struct EmitterShape
    {
        enum Enum
        {
            Sphere,
            Hemisphere,
            Circle,
            Disc,
            Rect,

            Count
        };
    };

    struct EmitterDirection
    {
        enum Enum
        {
            Up,
            Outward,

            Count
        };
    };

    struct EmitterUniforms
    {
        void reset();

        float m_position[3];
        float m_angle[3];

        float m_blendStart[2];
        float m_blendEnd[2];
        float m_offsetStart[2];
        float m_offsetEnd[2];
        float m_scaleStart[2];
        float m_scaleEnd[2];
        float m_lifeSpan[2];
        float m_gravityScale;

        uint32_t m_rgba[5];
        uint32_t m_particlesPerSecond;

        bx::Easing::Enum m_easePos;
        bx::Easing::Enum m_easeRgba;
        bx::Easing::Enum m_easeBlend;
        bx::Easing::Enum m_easeScale;

        EmitterSpriteHandle m_handle;
    };

    // API functions (implemented in ParticleSystem.cpp)
    void init(uint16_t _maxEmitters = 64, bx::AllocatorI* _allocator = nullptr);
    void shutdown();

    EmitterSpriteHandle createSprite(uint16_t _width, uint16_t _height, const void* _data);
    void destroySprite(EmitterSpriteHandle _handle);

    EmitterHandle createEmitter(EmitterShape::Enum _shape, EmitterDirection::Enum _direction, uint32_t _maxParticles);
    void updateEmitter(EmitterHandle _handle, const EmitterUniforms* _uniforms = nullptr);
    void getAabb(EmitterHandle _handle, bx::Aabb& _outAabb);
    void destroyEmitter(EmitterHandle _handle);

    void update(float _dt);

    // Renders all emitters using internal BGFX resources.
    // _view must be a valid BGFX view id, _mtxView is current view matrix, _eye is eye position in world space.
    void render(uint8_t _view, const float* _mtxView, const bx::Vec3& _eye);

} // namespace ps
