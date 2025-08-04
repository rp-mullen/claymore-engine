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
    static Model LoadModel(const std::string& filepath);
};