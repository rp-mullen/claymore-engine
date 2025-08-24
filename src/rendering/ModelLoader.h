#pragma once
#include <vector>
#include <string>
#include "Material.h"
#include "Mesh.h"
#include "ecs/AnimationComponents.h"

struct Model {
   std::vector<std::shared_ptr<Mesh>> Meshes;
   std::vector<std::shared_ptr<Material>> Materials;
   std::vector<BlendShapeComponent> BlendShapes;
   std::vector<std::string> BoneNames;
   std::vector<glm::mat4> InverseBindPoses;
   };

class ModelLoader {
public:
    // Global import configuration (applies to all loads)
    static void SetFlipYAxis(bool enabled);
    static void SetFlipZAxis(bool enabled);
    static bool GetFlipYAxis();
    static bool GetFlipZAxis();
    static void SetRotateY180(bool enabled);
    static bool GetRotateY180();

    // Load a model using the current global configuration
    static Model LoadModel(const std::string& filepath);

private:
    static bool s_FlipY;
    static bool s_FlipZ;
    static bool s_RotateY180;
};