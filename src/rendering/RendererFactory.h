#pragma once
#include "rendering/Mesh.h"

enum class RendererKind { Static, Skinned };

inline RendererKind ClassifyRenderer(const Mesh& mesh) {
    return mesh.HasSkinning() ? RendererKind::Skinned : RendererKind::Static;
}


