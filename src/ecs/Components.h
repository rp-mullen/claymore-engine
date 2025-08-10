#pragma once
#include <string>
#include <iostream>
#include <vector>
#include <bgfx/bgfx.h>
#include "rendering/VertexTypes.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "particles/ParticleSystem.h"

#include "rendering/Material.h"
#include "rendering/Mesh.h"
#include "rendering/Camera.h"
#include "pipeline/AssetReference.h"
#include "rendering/MaterialPropertyBlock.h"
#include <memory>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <physics/Physics.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>


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
	std::string MeshName;  // Keep for backward compatibility
	AssetReference meshReference;  // New asset reference system
	    std::shared_ptr<Material> material;
    bool UniqueMaterial = false; // If true, this entity uses its own material instance
    MaterialPropertyBlock PropertyBlock;

	BlendShapeComponent* BlendShapes = nullptr;

	MeshComponent(std::shared_ptr<Mesh> m, const std::string& name, std::shared_ptr<Material> mat)
		: mesh(std::move(m)), MeshName(name), material(std::move(mat)) {
	}

	MeshComponent() = default;
};

#include "AnimationComponents.h"
#include <rendering/MaterialPropertyBlock.h>



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

	void BuildShape(const Mesh* mesh = nullptr) {
		switch (ShapeType) {
		case ColliderShape::Box: {
			JPH::BoxShapeSettings settings(JPH::Vec3(Size.x * 0.5f, Size.y * 0.5f, Size.z * 0.5f));
			Shape = settings.Create().Get();
			std::cout << "[Collider] Created box shape with size (" << Size.x << ", " << Size.y << ", " << Size.z << ")" << std::endl;
			break;
		}
		case ColliderShape::Capsule: {
			JPH::CapsuleShapeSettings settings(Radius, Height * 0.5f);
			Shape = settings.Create().Get();
			break;
		}
		case ColliderShape::Mesh: {
			if (mesh) {
				// Auto-detect appropriate collision shape based on mesh bounds
				glm::vec3 boundsSize = mesh->BoundsMax - mesh->BoundsMin;
				glm::vec3 boundsCenter = (mesh->BoundsMax + mesh->BoundsMin) * 0.5f;
				
				// Auto-detect shape type based on bounds
				if (boundsSize.x > 0.9f && boundsSize.x < 1.1f && 
					boundsSize.y > 0.9f && boundsSize.y < 1.1f && 
					boundsSize.z > 0.9f && boundsSize.z < 1.1f) {
					// Use sphere shape for approximately unit sphere meshes
					float radius = std::max({boundsSize.x, boundsSize.y, boundsSize.z}) * 0.5f;
					JPH::SphereShapeSettings settings(radius);
					Shape = settings.Create().Get();
				} else if (boundsSize.y < 0.1f && boundsSize.x > 0.5f && boundsSize.z > 0.5f) {
					// Use box shape for plane meshes (thin box)
					JPH::BoxShapeSettings settings(JPH::Vec3(boundsSize.x * 0.5f, 0.01f, boundsSize.z * 0.5f));
					Shape = settings.Create().Get();
				} else {
					// Default to box shape for other meshes
					JPH::BoxShapeSettings settings(JPH::Vec3(boundsSize.x * 0.5f, boundsSize.y * 0.5f, boundsSize.z * 0.5f));
					Shape = settings.Create().Get();
				}
			} else {
				// Fallback to box shape if no mesh provided
				JPH::BoxShapeSettings settings(JPH::Vec3(Size.x * 0.5f, Size.y * 0.5f, Size.z * 0.5f));
				Shape = settings.Create().Get();
			}
			break;
		}
		}
	}
};

struct RigidBodyComponent {
	float Mass = 1.0f;
	float Friction = 0.5f;
	float Restitution = 0.0f; // Bounciness
	bool UseGravity = true;
	bool IsKinematic = false;
	
	// Physics body reference
	JPH::BodyID BodyID = JPH::BodyID();
	
	// Velocity and angular velocity (for kinematic bodies)
	glm::vec3 LinearVelocity = glm::vec3(0.0f);
	glm::vec3 AngularVelocity = glm::vec3(0.0f);
	
	RigidBodyComponent() = default;
};

struct StaticBodyComponent {
	float Friction = 0.5f;
	float Restitution = 0.0f;
	
	// Physics body reference
	JPH::BodyID BodyID = JPH::BodyID();
	
	StaticBodyComponent() = default;
};

struct CameraComponent {
	Camera Camera; // Your existing Camera class
	bool Active = false; 

   int priority = 0; // Lower values render first, higher values render last

	// Settings
	float FieldOfView = 60.0f;
	float NearClip = 0.1f;
	float FarClip = 1000.0f;
	bool IsPerspective = true;

	CameraComponent() = default;

	void UpdateProjection(float aspectRatio) {
		if (IsPerspective)
			Camera.SetPerspective(FieldOfView, aspectRatio, NearClip, FarClip);
		else {
			// Orthographic projection
			float orthoSize = 10.0f; // Default ortho size
			Camera.SetPerspective(orthoSize, aspectRatio, NearClip, FarClip);
		}
	}

	void SyncWithTransform(const TransformComponent& transform) {
		Camera.SetPosition(transform.Position);
		Camera.SetRotation(transform.Rotation);
		// Note: SetPosition and SetRotation automatically call RecalculateView()
	}
};

// ---------------- Terrain ----------------
struct TerrainBrush
{
    bool Raise = true;
    int Size = 10;
    float Power = 0.5f;
};

struct TerrainComponent
{
    // 0 = VertexBuffer, 1 = DynamicVertexBuffer, 2 = HeightTexture
    int Mode = 0;
    bool Dirty = true;

    uint32_t Size = 256; // Grid resolution (Size x Size)

    std::vector<uint8_t> HeightMap;
    std::vector<TerrainVertex> Vertices;
    std::vector<uint16_t> Indices;

    // GPU resources
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    bgfx::DynamicVertexBufferHandle dvbh = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle dibh = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle HeightTexture = BGFX_INVALID_HANDLE;

    TerrainBrush Brush;

    bool PaintMode = false;

    TerrainComponent()
        : HeightMap(Size * Size, 0)
    {
    }
};

// ---------------- Particle System ----------------
/* Legacy particle system component removed in favour of new emitter-based particle system. */
struct ParticleEmitterComponent
{
    ps::EmitterHandle Handle{ uint16_t{UINT16_MAX} };
    ps::EmitterUniforms Uniforms; // customised per emitter

    uint32_t MaxParticles = 1024;

    // Sprite for this emitter (created via ParticleSystem utility functions)
    ps::EmitterSpriteHandle SpriteHandle{ uint16_t{UINT16_MAX} };
    // Optional path to sprite image used to (re)create atlas sprite on load/UI selection
    std::string SpritePath;

    bool Enabled = true;

    ParticleEmitterComponent()
    {
        Uniforms.reset();
    }
};

// ---------------- Text Rendering ----------------
struct TextRendererComponent
{
    // UTF-8 text to render
    std::string Text = "Hello World";

    // Approximate pixel height used when creating the font
    float PixelSize = 32.0f;

    // ABGR color packed as 0xAABBGGRR (bgfx convention)
    uint32_t ColorAbgr = 0xffffffffu;

    // If true, text is rendered in world space using the entity transform.
    // If false, text is rendered in screen space (top-left origin) at the entity's Position.xy
    bool WorldSpace = true;
};