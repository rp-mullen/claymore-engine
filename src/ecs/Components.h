#pragma once
#include <string>
#include <glm/glm.hpp>

#include "rendering/Material.h"
#include "rendering/Mesh.h"
#include <memory>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <physics/Physics.h>

struct TransformComponent {
	glm::vec3 Position = glm::vec3(0.0f);
	glm::vec3 Rotation = glm::vec3(0.0f); // Euler degrees (XYZ order)
	glm::vec3 Scale = glm::vec3(1.0f);

	glm::mat4 LocalMatrix = glm::mat4(1.0f);  // Local transform
	glm::mat4 WorldMatrix = glm::mat4(1.0f);  // Computed

	bool TransformDirty = true;

	inline glm::mat4 CalculateLocalMatrix() {
		glm::mat4 translation = glm::translate(glm::mat4(1.0f), Position);

		glm::mat4 rotation =
			glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.x), glm::vec3(1, 0, 0)) *
			glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.y), glm::vec3(0, 1, 0)) *
			glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.z), glm::vec3(0, 0, 1));

		glm::mat4 scale = glm::scale(glm::mat4(1.0f), Scale);

		LocalMatrix = translation * rotation * scale;
		return LocalMatrix;
	}
};



struct BlendShapeComponent; // forward decl

struct MeshComponent {
	std::shared_ptr<Mesh> mesh;
	std::string MeshName;
	std::shared_ptr<Material> material;

	BlendShapeComponent* BlendShapes = nullptr;

	MeshComponent(std::shared_ptr<Mesh> m, const std::string& name, std::shared_ptr<Material> mat)
		: mesh(std::move(m)), MeshName(name), material(std::move(mat)) {
	}

	MeshComponent() = default;
};

#include "AnimationComponents.h"



enum class LightType {
	Directional,
	Point
};

struct LightComponent {
	LightType Type = LightType::Directional;
	glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
	float Intensity = 1.0f;

	LightComponent(LightType type = LightType::Directional,
		const glm::vec3& color = { 1.0f, 1.0f, 1.0f },
		float intensity = 1.0f)
		: Type(type), Color(color), Intensity(intensity) {
	}
	// For directional: use rotation from TransformComponent
	// For point: use position from TransformComponent
};

struct ColliderComponent {
	ColliderShape ShapeType = ColliderShape::Box;

	glm::vec3 Offset = glm::vec3(0.0f);         // Local offset from entity transform
	glm::vec3 Size = glm::vec3(1.0f);           // For Box
	float Radius = 0.5f, Height = 1.0f;         // For Capsule
	std::string MeshPath;                      // For Mesh

	bool IsTrigger = false;

	// Cached Jolt shape
	JPH::RefConst<JPH::Shape> Shape;

	ColliderComponent() = default;

	// Runtime shape generation
	void BuildShape(const Mesh* mesh = nullptr) {
		switch (ShapeType) {
		case ColliderShape::Box: {
			JPH::BoxShapeSettings settings(JPH::Vec3(Size.x * 0.5f, Size.y * 0.5f, Size.z * 0.5f));
			Shape = settings.Create().Get();
			break;
		}
		case ColliderShape::Capsule: {
			JPH::CapsuleShapeSettings settings(Radius, Height * 0.5f);
			Shape = settings.Create().Get();
			break;
		}
		case ColliderShape::Mesh: {
			if (!mesh || mesh->Vertices.empty() || mesh->Indices.empty()) {
				std::cerr << "[Collider] Mesh collider requires valid mesh data!\n";
				Shape = nullptr;
				break;
			}

			const auto& verts = mesh->Vertices;
			const auto& indices = mesh->Indices;

			JPH::TriangleList triangles;
			for (size_t i = 0; i + 2 < indices.size(); i += 3) {
				const glm::vec3& v0 = verts[indices[i + 0]];
				const glm::vec3& v1 = verts[indices[i + 1]];
				const glm::vec3& v2 = verts[indices[i + 2]];
				triangles.emplace_back(
					JPH::Float3(v0.x, v0.y, v0.z),
					JPH::Float3(v1.x, v1.y, v1.z),
					JPH::Float3(v2.x, v2.y, v2.z)
				);
			}

			JPH::MeshShapeSettings settings(std::move(triangles));
			auto result = settings.Create();
			if (result.HasError()) {
				std::cerr << "[Collider] MeshShape creation failed: " << result.GetError().c_str() << "\n";
				Shape = nullptr;
			}
			else {
				Shape = result.Get();
			}
			break;
		}
		}
	}
};
