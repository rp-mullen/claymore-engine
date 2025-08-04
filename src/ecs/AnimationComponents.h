#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "ecs/Entity.h" // assumes EntityID typedef lives there; adjust path if different

// ------------ Blend Shapes ------------
struct BlendShape {
    std::string Name;
    std::vector<glm::vec3> DeltaPos;
    std::vector<glm::vec3> DeltaNormal;
    float Weight = 0.0f;
};

struct BlendShapeComponent {
    std::vector<BlendShape> Shapes;
    bool Dirty = false;
};

// ------------ Skeleton & Skinning ------------
struct SkeletonComponent {
    std::vector<glm::mat4> InverseBindPoses;  // inverse bind matrices per bone
    std::vector<EntityID>  BoneEntities;      // entity for each bone
};

struct SkinningComponent {
    EntityID SkeletonRoot = -1;
    std::vector<glm::mat4> Palette;           // current frame palette
};
