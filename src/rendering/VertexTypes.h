#pragma once
#include <bgfx/bgfx.h>

struct PosColorVertex {
    float x, y, z;
    uint32_t abgr;

    static void Init() {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
    }

    static bgfx::VertexLayout layout;
};

struct PBRVertex {
    float x, y, z;    // Position
    float nx, ny, nz; // Normal
    float u, v;       // UV

    static void Init() {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }

    static bgfx::VertexLayout layout;
};

struct SkinnedPBRVertex {
    float  x,  y,  z;    // Position
    float  nx, ny, nz;   // Normal
    float  u,  v;        // UV
    uint8_t i0, i1, i2, i3; // Bone indices
    float  w0, w1, w2, w3;  // Bone weights

	static void Init() {
		layout.begin()
            .add(bgfx::Attrib::Position,     3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal,       3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0,    2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8, false, true)
            .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
            .end();
	}

    static bgfx::VertexLayout layout;
};


struct TerrainVertex {
    float x, y, z;    // Position
    float nx, ny, nz; // Normal
    float u, v;       // UV

    static void Init() {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal,   3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }

    static bgfx::VertexLayout layout;
};

struct GridVertex {
    float x, y, z;

    static void Init() {
        GridVertex::layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .end();
    }

    static bgfx::VertexLayout layout;
	
};