#pragma once

#include <imgui.h>
#include <cstddef>

struct SkeletonComponent;
namespace cm { namespace animation { struct PreviewContext; struct PoseBuffer; } }
struct Mesh; struct SkinningData;

namespace cm { namespace animation {

void Begin(PreviewContext& ctx, const ImVec2& topLeft, const ImVec2& size);
void DrawSkeleton(PreviewContext& ctx, const SkeletonComponent& skeleton);
void DrawSkinned(PreviewContext& ctx, const Mesh& mesh, const SkinningData& skin, const PoseBuffer& pose);
void End(PreviewContext& ctx);

} }


