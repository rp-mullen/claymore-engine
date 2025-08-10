/*
 * ParticleSystem.cpp - Integrated BGFX particle system for Claymore engine.
 * This file is largely based on the BGFX example particle system by Branimir Karadzic.
 * It has been adapted to fit Claymore's build system and code style.
 */

#include "ParticleSystem.h"

#include <bgfx/bgfx.h>
#include <bx/easing.h>
#include <bx/rng.h>
#include <bx/handlealloc.h>
#include <cfloat>
#include <bx/math.h>
#include "rendering/ShaderManager.h"
extern bgfx::ProgramHandle LoadParticleProgram();

// Local utilities (implemented elsewhere)
#include "utils/packrect.h" // Rectangle packing for sprite atlas.

namespace ps
{
    // --------------------------------------------------------------
    // Vertex layout for particles
    // --------------------------------------------------------------
    struct PosColorTexCoord0Vertex
    {
        float m_x;
        float m_y;
        float m_z;
        uint32_t m_abgr;
        float m_u;
        float m_v;
        float m_blend;
        float m_angle;

        static bgfx::VertexLayout ms_layout;
        static void init()
        {
            ms_layout
                .begin()
                .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
                .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
                .end();
        }
    };

    bgfx::VertexLayout PosColorTexCoord0Vertex::ms_layout;

    // Helper conversions ------------------------------------------------------
    inline uint32_t toAbgr(float _rr, float _gg, float _bb, float _aa)
    {
        return 0
            | (uint8_t(_rr*255.0f)<< 0)
            | (uint8_t(_gg*255.0f)<< 8)
            | (uint8_t(_bb*255.0f)<<16)
            | (uint8_t(_aa*255.0f)<<24);
    }

    inline uint32_t toAbgr(const float* _rgba)
    {
        return toAbgr(_rgba[0], _rgba[1], _rgba[2], _rgba[3]);
    }

    // Emitter uniforms default ------------------------------------------------
    void EmitterUniforms::reset()
    {
        m_position[0] = m_position[1] = m_position[2] = 0.0f;
        m_angle[0] = m_angle[1] = m_angle[2] = 0.0f;

        m_particlesPerSecond = 0;

        m_offsetStart[0] = 0.0f; m_offsetStart[1] = 1.0f;
        m_offsetEnd[0]   = 2.0f; m_offsetEnd[1]   = 3.0f;

        // Colours: starting transparent, then opaque white, etc.
        m_rgba[0] = 0x00ffffff;
        m_rgba[1] = UINT32_MAX;
        m_rgba[2] = UINT32_MAX;
        m_rgba[3] = UINT32_MAX;
        m_rgba[4] = 0x00ffffff;

        m_blendStart[0] = 0.8f; m_blendStart[1] = 1.0f;
        m_blendEnd[0]   = 0.0f; m_blendEnd[1]   = 0.2f;

        m_scaleStart[0] = 0.1f; m_scaleStart[1] = 0.2f;
        m_scaleEnd[0]   = 0.3f; m_scaleEnd[1]   = 0.4f;

        m_lifeSpan[0] = 1.0f; m_lifeSpan[1] = 2.0f;

        m_gravityScale = 0.0f;

        m_easePos   = bx::Easing::Linear;
        m_easeRgba  = bx::Easing::Linear;
        m_easeBlend = bx::Easing::Linear;
        m_easeScale = bx::Easing::Linear;

        m_handle.idx = UINT16_MAX; // invalid
        m_blendMode = 0; // Alpha by default
    }

    // -------------------------------------------------------------------------
    // Forward declarations
    struct Emitter;

    static int32_t particleSortFn(const void* _lhs, const void* _rhs);

    // -------------------------------------------------------------------------
    // Sprite Atlas handling (rect pack + texture)
    // -------------------------------------------------------------------------
    #define SPRITE_TEXTURE_SIZE 1024

    template<uint16_t MaxHandlesT = 256, uint16_t TextureSizeT = 1024>
    struct SpriteT
    {
        SpriteT() : m_ra(TextureSizeT, TextureSizeT) {}

        EmitterSpriteHandle create(uint16_t _width, uint16_t _height)
        {
            EmitterSpriteHandle handle = { UINT16_MAX };

            if (m_handleAlloc.getNumHandles() < m_handleAlloc.getMaxHandles())
            {
                Pack2D pack;
                if (m_ra.find(_width, _height, pack))
                {
                    handle.idx = m_handleAlloc.alloc();
                    m_pack[handle.idx] = pack;
                }
            }
            return handle;
        }

        void destroy(EmitterSpriteHandle _sprite)
        {
            const Pack2D& pack = m_pack[_sprite.idx];
            m_ra.clear(pack);
            m_handleAlloc.free(_sprite.idx);
        }

        const Pack2D& get(EmitterSpriteHandle _sprite) const { return m_pack[_sprite.idx]; }

        bx::HandleAllocT<MaxHandlesT> m_handleAlloc;
        Pack2D                        m_pack[MaxHandlesT];
        RectPack2DT<256>              m_ra; // 256 free rects
    };

    // -------------------------------------------------------------------------
    // Emitter definition
    // -------------------------------------------------------------------------
    struct Particle
    {
        bx::Vec3 start;
        bx::Vec3 end[2];
        float blendStart;
        float blendEnd;
        float scaleStart;
        float scaleEnd;

        uint32_t rgba[5];

        float life;     // progress 0..1 where 1 is dead
        float lifeSpan; // seconds
    };

    struct ParticleSort
    {
        float    dist;
        uint32_t idx;
    };

    struct Emitter
    {
        void create(EmitterShape::Enum _shape, EmitterDirection::Enum _direction, uint32_t _maxParticles);
        void destroy();

        void reset();
        void update(float _dt);
        void spawn(float _dt);

        uint32_t render(const float _uv[4], const float* _mtxView, const bx::Vec3& _eye,
                         uint32_t _first, uint32_t _max, ParticleSort* _outSort,
                         PosColorTexCoord0Vertex* _outVertices);

        EmitterShape::Enum     m_shape{EmitterShape::Sphere};
        EmitterDirection::Enum m_direction{EmitterDirection::Up};

        float           m_dt{0.0f};
        bx::RngMwc      m_rng;
        EmitterUniforms m_uniforms;

        bx::Aabb        m_aabb;

        Particle*       m_particles{nullptr};
        uint32_t        m_num{0};
        uint32_t        m_max{0};
    };

    // -------------------------------------------------------------------------
    // ParticleSystem context
    // -------------------------------------------------------------------------
    struct ParticleSystem
    {
        void init(uint16_t _maxEmitters, bx::AllocatorI* _allocator);
        void shutdown();

        EmitterSpriteHandle createSprite(uint16_t _width, uint16_t _height, const void* _data);
        void destroySprite(EmitterSpriteHandle _handle);

        void update(float _dt);
        void render(uint8_t _view, const float* _mtxView, const bx::Vec3& _eye);

        EmitterHandle createEmitter(EmitterShape::Enum _shape, EmitterDirection::Enum _direction, uint32_t _maxParticles);
        void updateEmitter(EmitterHandle _handle, const EmitterUniforms* _uniforms);
        void getAabb(EmitterHandle _handle, bx::Aabb& _outAabb);
        void destroyEmitter(EmitterHandle _handle);

        // members
        bx::AllocatorI*  m_allocator{nullptr};
        bx::HandleAlloc* m_emitterAlloc{nullptr};
        Emitter*         m_emitter{nullptr};

        typedef SpriteT<256, SPRITE_TEXTURE_SIZE> Sprite;
        Sprite           m_sprite;

        // BGFX resources
        bgfx::UniformHandle s_texColor = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle m_texture  = BGFX_INVALID_HANDLE;
        bgfx::ProgramHandle m_program  = BGFX_INVALID_HANDLE;

        uint32_t m_numParticles{0};
    };

    static ParticleSystem s_ctx; // global context

    // ----------------------------------------------------------------------------------------
    // Implementation details
    // ----------------------------------------------------------------------------------------

    void Emitter::reset()
    {
        m_dt = 0.0f;
        m_uniforms.reset();
        m_num = 0;
        bx::memSet(&m_aabb, 0, sizeof(bx::Aabb));
        m_rng.reset();
    }

    void Emitter::create(EmitterShape::Enum _shape, EmitterDirection::Enum _direction, uint32_t _maxParticles)
    {
        reset();
        m_shape     = _shape;
        m_direction = _direction;
        m_max       = _maxParticles;
        m_particles = (Particle*)bx::alloc(s_ctx.m_allocator, sizeof(Particle)*m_max);
    }

    void Emitter::destroy()
    {
        bx::free(s_ctx.m_allocator, m_particles);
        m_particles = nullptr;
    }

    // The majority of update, spawn, render functions are directly ported from BGFX sample.
    // For brevity and maintainability, please refer to the original source if you need to
    // modify the underlying behaviour.

    static inline bx::Vec3 aabbExpand(bx::Aabb& _aabb, const bx::Vec3& _point)
    {
        _aabb.min = bx::min(_aabb.min, _point);
        _aabb.max = bx::max(_aabb.max, _point);
        return _point;
    }

    // Helper function to spawn new particles (partial port for brevity)
    void Emitter::spawn(float _dt)
    {
        const float timePerParticle = 1.0f / bx::max<float>(1.0f, (float)m_uniforms.m_particlesPerSecond);
        m_dt += _dt;
        const uint32_t numParticlesToSpawn = (uint32_t)(m_dt / timePerParticle);
        m_dt -= numParticlesToSpawn * timePerParticle;

        if (numParticlesToSpawn == 0) return;

        // Precompute emitter transform
        float mtx[16];
        bx::mtxSRT(mtx, 1.0f, 1.0f, 1.0f,
                   m_uniforms.m_angle[0], m_uniforms.m_angle[1], m_uniforms.m_angle[2],
                   m_uniforms.m_position[0], m_uniforms.m_position[1], m_uniforms.m_position[2]);

        constexpr bx::Vec3 up = { 0.0f, 1.0f, 0.0f };

        float emitTime = 0.0f;
        for (uint32_t ii = 0; ii < numParticlesToSpawn && m_num < m_max; ++ii)
        {
            Particle& p = m_particles[m_num++];

            // Random position depending on shape
            bx::Vec3 pos(bx::InitNone);
            switch (m_shape)
            {
                case EmitterShape::Sphere:     pos = bx::randUnitSphere(&m_rng); break;
                case EmitterShape::Hemisphere: pos = bx::randUnitHemisphere(&m_rng, up); break;
                case EmitterShape::Circle:     pos = bx::randUnitCircle(&m_rng); break;
                case EmitterShape::Disc:
                {
                    bx::Vec3 tmp = bx::randUnitCircle(&m_rng);
                    pos = bx::mul(tmp, bx::frnd(&m_rng));
                    break;
                }
                case EmitterShape::Rect:
                {
                    pos = { bx::frndh(&m_rng), 0.0f, bx::frndh(&m_rng) };
                    break;
                }
                default: break;
            }

            // Direction
            bx::Vec3 dir(bx::InitNone);
            switch (m_direction)
            {
                case EmitterDirection::Up:      dir = up; break;
                case EmitterDirection::Outward: dir = bx::normalize(pos); break;
                default: break;
            }

            const float startOffset = bx::lerp(m_uniforms.m_offsetStart[0], m_uniforms.m_offsetStart[1], bx::frnd(&m_rng));
            const bx::Vec3 start = bx::mul(pos, startOffset);

            const float endOffset = bx::lerp(m_uniforms.m_offsetEnd[0], m_uniforms.m_offsetEnd[1], bx::frnd(&m_rng));
            const bx::Vec3 end = bx::add(bx::mul(dir, endOffset), start);

            p.life = emitTime;
            p.lifeSpan = bx::lerp(m_uniforms.m_lifeSpan[0], m_uniforms.m_lifeSpan[1], bx::frnd(&m_rng));

            const bx::Vec3 gravity = { 0.0f, -9.81f * m_uniforms.m_gravityScale * bx::square(p.lifeSpan), 0.0f };

            p.start  = bx::mul(start, mtx);
            p.end[0] = bx::mul(end,   mtx);
            p.end[1] = bx::add(p.end[0], gravity);

            bx::memCopy(p.rgba, m_uniforms.m_rgba, sizeof(p.rgba));

            p.blendStart = bx::lerp(m_uniforms.m_blendStart[0], m_uniforms.m_blendStart[1], bx::frnd(&m_rng));
            p.blendEnd   = bx::lerp(m_uniforms.m_blendEnd[0],   m_uniforms.m_blendEnd[1],   bx::frnd(&m_rng));

            p.scaleStart = bx::lerp(m_uniforms.m_scaleStart[0], m_uniforms.m_scaleStart[1], bx::frnd(&m_rng));
            p.scaleEnd   = bx::lerp(m_uniforms.m_scaleEnd[0],   m_uniforms.m_scaleEnd[1],   bx::frnd(&m_rng));

            emitTime += timePerParticle;
        }
    }

    void Emitter::update(float _dt)
    {
        // Update existing particles and remove dead ones
        for (uint32_t ii = 0; ii < m_num; )
        {
            Particle& p = m_particles[ii];
            p.life += _dt * (1.0f / p.lifeSpan);
            if (p.life > 1.0f)
            {
                // kill this particle by swap with last
                if (ii != m_num - 1)
                {
                    m_particles[ii] = m_particles[m_num - 1];
                }
                --m_num;
                continue; // don't increment ii, reprocess swapped
            }
            ++ii;
        }

        // Spawn new ones if needed
        if (m_uniforms.m_particlesPerSecond > 0)
        {
            spawn(_dt);
        }
    }

    // Render returns number of particles processed (similar to sample). This is a partial implementation that
    // fills vertex buffer and sort mask.
    uint32_t Emitter::render(const float _uv[4], const float* _mtxView, const bx::Vec3& _eye,
                             uint32_t _first, uint32_t _max, ParticleSort* _outSort,
                             PosColorTexCoord0Vertex* _outVertices)
    {
        // For brevity of this integration, we'll not implement full detailed interpolation and sorting here.
        // Instead, we simply output billboards with constant size and color.
        const uint32_t count = bx::uint32_min(m_num, _max - _first);
        for (uint32_t ii = 0; ii < count; ++ii)
        {
            const Particle& p = m_particles[ii];
            const bx::Vec3 pos = bx::lerp(p.start, p.end[0], p.life);

            // simple constant scale
            const float scale = bx::lerp(p.scaleStart, p.scaleEnd, p.life);
            const float blend = bx::lerp(p.blendStart, p.blendEnd, p.life);

            // Use view matrix columns (inverse rotation axes): right = col0, up = col1
            const bx::Vec3 udir = { _mtxView[0]*scale, _mtxView[4]*scale, _mtxView[8]*scale };
            const bx::Vec3 vdir = { _mtxView[1]*scale, _mtxView[5]*scale, _mtxView[9]*scale };

            PosColorTexCoord0Vertex* vertex = &_outVertices[(_first + ii)*4];

            const bx::Vec3 ul = bx::sub(bx::sub(pos, udir), vdir);
            bx::store(&vertex[0].m_x, ul);
            vertex[0].m_abgr = p.rgba[0];
            vertex[0].m_u = _uv[0]; vertex[0].m_v = _uv[1]; vertex[0].m_blend = blend;
            const bx::Vec3 ur = bx::sub(bx::add(pos, udir), vdir);
            bx::store(&vertex[1].m_x, ur);
            vertex[1].m_abgr = p.rgba[0];
            vertex[1].m_u = _uv[2]; vertex[1].m_v = _uv[1]; vertex[1].m_blend = blend;
            const bx::Vec3 br = bx::add(bx::add(pos, udir), vdir);
            bx::store(&vertex[2].m_x, br);
            vertex[2].m_abgr = p.rgba[0];
            vertex[2].m_u = _uv[2]; vertex[2].m_v = _uv[3]; vertex[2].m_blend = blend;
            const bx::Vec3 bl = bx::add(bx::sub(pos, udir), vdir);
            bx::store(&vertex[3].m_x, bl);
            vertex[3].m_abgr = p.rgba[0];
            vertex[3].m_u = _uv[0]; vertex[3].m_v = _uv[3]; vertex[3].m_blend = blend;

            // sort info
            const bx::Vec3 tmp = bx::sub(_eye, pos);
            _outSort[_first + ii].dist = bx::length(tmp);
            _outSort[_first + ii].idx  = _first + ii;
        }
        // update aabb (approx, expanded bounds)
        m_aabb.min = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        m_aabb.max = {  FLT_MAX,  FLT_MAX,  FLT_MAX };

        return count;
    }

    // -------------------------------------------------------------------------
    // Sorting function for qsort.
    static int32_t particleSortFn(const void* _lhs, const void* _rhs)
    {
        const ParticleSort& lhs = *(const ParticleSort*)_lhs;
        const ParticleSort& rhs = *(const ParticleSort*)_rhs;
        return lhs.dist > rhs.dist ? -1 : 1;
    }

    // -------------------------------------------------------------------------
    // ParticleSystem methods
    void ParticleSystem::init(uint16_t _maxEmitters, bx::AllocatorI* _allocator)
    {
        m_allocator = _allocator;
        if (!m_allocator)
        {
            static bx::DefaultAllocator defaultAlloc;
            m_allocator = &defaultAlloc;
        }

        m_emitterAlloc = bx::createHandleAlloc(m_allocator, _maxEmitters);
        m_emitter      = (Emitter*)bx::alloc(m_allocator, sizeof(Emitter)*_maxEmitters);

        PosColorTexCoord0Vertex::init();

        // uniform, texture and program
        s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
        m_texture  = bgfx::createTexture2D(SPRITE_TEXTURE_SIZE, SPRITE_TEXTURE_SIZE, false, 1, bgfx::TextureFormat::BGRA8);

        // Initialize atlas with opaque white so quads are visible even without a sprite uploaded
        {
            const uint32_t pixelCount = SPRITE_TEXTURE_SIZE * SPRITE_TEXTURE_SIZE;
            std::vector<uint32_t> white(pixelCount, 0xffffffffu);
            const bgfx::Memory* mem = bgfx::copy(white.data(), uint32_t(white.size() * sizeof(uint32_t)));
            bgfx::updateTexture2D(m_texture, 0, 0, 0, 0, SPRITE_TEXTURE_SIZE, SPRITE_TEXTURE_SIZE, mem);
        }

        // Load particle shaders via global ShaderManager (requires include)
        extern bgfx::ProgramHandle LoadParticleProgram();
        m_program = LoadParticleProgram();

        // Ensure default blending-friendly render state is used in shader; we set at draw time.
    }

    void ParticleSystem::shutdown()
    {
        if (bgfx::isValid(m_program)) bgfx::destroy(m_program);
        if (bgfx::isValid(m_texture)) bgfx::destroy(m_texture);
        if (bgfx::isValid(s_texColor)) bgfx::destroy(s_texColor);

        bx::destroyHandleAlloc(m_allocator, m_emitterAlloc);
        bx::free(m_allocator, m_emitter);

        m_allocator = nullptr;
    }

    EmitterSpriteHandle ParticleSystem::createSprite(uint16_t _width, uint16_t _height, const void* _data)
    {
        EmitterSpriteHandle h = m_sprite.create(_width, _height);
        if (isValid(h))
        {
            const Pack2D& pack = m_sprite.get(h);
            bgfx::updateTexture2D(m_texture, 0, 0, pack.m_x, pack.m_y, pack.m_width, pack.m_height, bgfx::copy(_data, pack.m_width*pack.m_height*4));
        }
        return h;
    }

    void ParticleSystem::destroySprite(EmitterSpriteHandle _handle)
    {
        m_sprite.destroy(_handle);
    }

    void ParticleSystem::update(float _dt)
    {
        if (m_emitterAlloc == nullptr)
        {
            m_numParticles = 0;
            return;
        }
        uint32_t total = 0;
        for (uint16_t ii = 0, nh = m_emitterAlloc->getNumHandles(); ii < nh; ++ii)
        {
            uint16_t idx = m_emitterAlloc->getHandleAt(ii);
            m_emitter[idx].update(_dt);
            total += m_emitter[idx].m_num;
        }
        m_numParticles = total;
    }

    void ParticleSystem::render(uint8_t _view, const float* _mtxView, const bx::Vec3& _eye)
    {
        if (m_numParticles == 0 || !bgfx::isValid(m_program))
            return;

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer  tib;

        const uint32_t availVB = bgfx::getAvailTransientVertexBuffer(m_numParticles*4, PosColorTexCoord0Vertex::ms_layout);
        const uint32_t availIB = bgfx::getAvailTransientIndexBuffer(m_numParticles*6);
        const uint32_t maxDraw = bx::uint32_min(availVB/4, availIB/6);
        if (maxDraw == 0) return;

        bgfx::allocTransientVertexBuffer(&tvb, maxDraw*4, PosColorTexCoord0Vertex::ms_layout);
        bgfx::allocTransientIndexBuffer(&tib, maxDraw*6);

        PosColorTexCoord0Vertex* vertices = (PosColorTexCoord0Vertex*)tvb.data;
        ParticleSort* sortBuf = (ParticleSort*)bx::alloc(m_allocator, sizeof(ParticleSort)*maxDraw);
        std::vector<uint8_t> modePerQuad; modePerQuad.resize(maxDraw, 0);

        uint32_t pos = 0;
        for (uint16_t ii = 0, nh = m_emitterAlloc->getNumHandles(); ii < nh && pos < maxDraw; ++ii)
        {
            uint16_t idx = m_emitterAlloc->getHandleAt(ii);
            Emitter& emitter = m_emitter[idx];

            float uv[4];
            if (isValid(emitter.m_uniforms.m_handle)) {
                const Pack2D& pack = m_sprite.get(emitter.m_uniforms.m_handle);
                const float invTex = 1.0f / SPRITE_TEXTURE_SIZE;
                uv[0] = pack.m_x * invTex;
                uv[1] = pack.m_y * invTex;
                uv[2] = (pack.m_x + pack.m_width) * invTex;
                uv[3] = (pack.m_y + pack.m_height) * invTex;
            } else {
                // Default to a small white quad in the corner of the atlas
                uv[0] = 0.0f; uv[1] = 0.0f; uv[2] = 8.0f / SPRITE_TEXTURE_SIZE; uv[3] = 8.0f / SPRITE_TEXTURE_SIZE;
            }

            uint32_t start = pos;
            pos += emitter.render(uv, _mtxView, _eye, pos, maxDraw, sortBuf, vertices);
            for (uint32_t q = start; q < pos; ++q) modePerQuad[q] = (uint8_t)bx::uint32_min(emitter.m_uniforms.m_blendMode, 2u);
        }

        // sort particles back-to-front
        qsort(sortBuf, pos, sizeof(ParticleSort), particleSortFn);

        uint16_t* indices = (uint16_t*)tib.data;
        for (uint32_t ii = 0; ii < pos; ++ii)
        {
            const ParticleSort& s = sortBuf[ii];
            uint16_t idx = (uint16_t)s.idx;
            uint16_t* idxPtr = &indices[ii*6];
            idxPtr[0] = idx*4 + 0;
            idxPtr[1] = idx*4 + 1;
            idxPtr[2] = idx*4 + 2;
            idxPtr[3] = idx*4 + 2;
            idxPtr[4] = idx*4 + 3;
            idxPtr[5] = idx*4 + 0;
        }

        // Select blend state based on emitter blend mode. Default depth test less, write RGB/A.
        uint64_t blendState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS;
        // We will submit once per emitter range to avoid state change per particle. For now, a single pass with state that fits most.
        // If multiple emitters use different modes in the same frame, we change state before submit below.
        // Here we pick a conservative default and override per-emitter just before submit.
        // Note: actual override is done in the submit per emitter loop below.
        (void)blendState;
        // We'll render all particles in a single draw for now. Since emitters might have different blend modes,
        // we conservatively sort by blend mode segments and submit up to three draws.
        // For simplicity, split the index buffer into three buckets by blend mode.

        // Build remapped indices by mode, preserving back-to-front order
        std::vector<uint16_t> indicesAlpha;
        std::vector<uint16_t> indicesAdd;
        std::vector<uint16_t> indicesMul;
        indicesAlpha.reserve(pos*6);
        indicesAdd.reserve(pos*6);
        indicesMul.reserve(pos*6);

        for (uint32_t ii = 0; ii < pos; ++ii)
        {
            const ParticleSort& s = sortBuf[ii];
            uint16_t qidx = (uint16_t)s.idx;
            uint16_t base = (uint16_t)(qidx*4);
            uint16_t local[6] = { (uint16_t)(base+0), (uint16_t)(base+1), (uint16_t)(base+2), (uint16_t)(base+2), (uint16_t)(base+3), (uint16_t)(base+0) };
            uint8_t mode = (qidx < modePerQuad.size()) ? modePerQuad[qidx] : 0;
            switch (mode)
            {
                default:
                case 0: indicesAlpha.insert(indicesAlpha.end(), local, local+6); break;
                case 1: indicesAdd.insert(indicesAdd.end(), local, local+6); break;
                case 2: indicesMul.insert(indicesMul.end(), local, local+6); break;
            }
        }

        bx::free(m_allocator, sortBuf);

        // Submit draws per mode bucket
        float idMtx[16]; bx::mtxIdentity(idMtx);
        bgfx::setTransform(idMtx);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setTexture(0, s_texColor, m_texture);

        auto submitBucket = [&](const std::vector<uint16_t>& idxList, uint64_t blendFlags)
        {
            if (idxList.empty()) return;
            bgfx::TransientIndexBuffer tibLocal;
            bgfx::allocTransientIndexBuffer(&tibLocal, (uint32_t)idxList.size());
            memcpy(tibLocal.data, idxList.data(), idxList.size()*sizeof(uint16_t));
            bgfx::setIndexBuffer(&tibLocal);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | blendFlags);
            bgfx::submit(_view, m_program);
        };

        submitBucket(indicesAlpha, BGFX_STATE_BLEND_ALPHA);
        submitBucket(indicesAdd,   BGFX_STATE_BLEND_ADD);
        const uint64_t BLEND_MULTIPLY = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR, BGFX_STATE_BLEND_ZERO);
        submitBucket(indicesMul, BLEND_MULTIPLY);
    }

    EmitterHandle ParticleSystem::createEmitter(EmitterShape::Enum _shape, EmitterDirection::Enum _direction, uint32_t _maxParticles)
    {
        EmitterHandle h = { m_emitterAlloc->alloc() };
        if (h.idx != UINT16_MAX) m_emitter[h.idx].create(_shape, _direction, _maxParticles);
        return h;
    }

    void ParticleSystem::updateEmitter(EmitterHandle _handle, const EmitterUniforms* _uniforms)
    {
        if (!isValid(_handle)) return;
        Emitter& e = m_emitter[_handle.idx];
        if (_uniforms)
            bx::memCopy(&e.m_uniforms, _uniforms, sizeof(EmitterUniforms));
        else
            e.reset();
    }

    void ParticleSystem::getAabb(EmitterHandle _handle, bx::Aabb& _outAabb)
    {
        if (!isValid(_handle)) return;
        _outAabb = m_emitter[_handle.idx].m_aabb;
    }

    void ParticleSystem::destroyEmitter(EmitterHandle _handle)
    {
        if (!isValid(_handle)) return;
        m_emitter[_handle.idx].destroy();
        m_emitterAlloc->free(_handle.idx);
    }

    // -------------------------------------------------------------------------
    // Public API wrappers

    void init(uint16_t _maxEmitters, bx::AllocatorI* _allocator)    { s_ctx.init(_maxEmitters, _allocator); }
    void shutdown()                                                { s_ctx.shutdown(); }
    EmitterSpriteHandle createSprite(uint16_t _width, uint16_t _height, const void* _data){ return s_ctx.createSprite(_width, _height, _data);}    
    void destroySprite(EmitterSpriteHandle _handle)                { s_ctx.destroySprite(_handle); }
    EmitterHandle createEmitter(EmitterShape::Enum _shape, EmitterDirection::Enum _direction, uint32_t _maxParticles){ return s_ctx.createEmitter(_shape, _direction, _maxParticles);}    
    void updateEmitter(EmitterHandle _handle, const EmitterUniforms* _uniforms){ s_ctx.updateEmitter(_handle, _uniforms);}    
    void getAabb(EmitterHandle _handle, bx::Aabb& _outAabb)       { s_ctx.getAabb(_handle, _outAabb);}    
    void destroyEmitter(EmitterHandle _handle)                     { s_ctx.destroyEmitter(_handle);}    
    void update(float _dt)                                         { s_ctx.update(_dt);}    
    void render(uint8_t _view, const float* _mtxView, const bx::Vec3& _eye) { s_ctx.render(_view, _mtxView, _eye);}    

    bool GetSpriteUV(EmitterSpriteHandle sprite, float uv[4])
    {
        if (!isValid(sprite))
            return false;

        const Pack2D& pack = s_ctx.m_sprite.get(sprite);
        const float invTex = 1.0f / SPRITE_TEXTURE_SIZE;
        uv[0] = pack.m_x * invTex;
        uv[1] = pack.m_y * invTex;
        uv[2] = (pack.m_x + pack.m_width) * invTex;
        uv[3] = (pack.m_y + pack.m_height) * invTex;
        return true;
    }

    bgfx::TextureHandle GetTexture()
    {
        return s_ctx.m_texture;
    }

} // namespace ps
