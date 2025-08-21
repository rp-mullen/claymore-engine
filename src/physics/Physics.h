// Physics.h
#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <glm/glm.hpp>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <memory>
#include <string>

enum class ColliderShape {
    Box,
    Capsule,
    Mesh
};

class Physics {
public:
	static Physics& Get() {
		static Physics instance;

		return instance;
	}

    static void Init();
    static void Shutdown();
    static void Step(float deltaTime);

    static void DestroyBody(JPH::BodyID bodyID);
    static JPH::BodyID CreateBody(const glm::mat4& transform, JPH::RefConst<JPH::Shape> shape, bool isStatic = false);

    // Body control methods
    static void SetBodyLinearVelocity(JPH::BodyID bodyID, const glm::vec3& velocity);
    static void SetBodyAngularVelocity(JPH::BodyID bodyID, const glm::vec3& velocity);
    static void SetBodyTransform(JPH::BodyID bodyID, const glm::vec3& position, const glm::vec3& eulerDegrees);
    static glm::mat4 GetBodyTransform(JPH::BodyID bodyID);
    static glm::vec3 GetGravity();

    // New helper to expose Jolt's body interface
    static JPH::BodyInterface& GetBodyInterface();


private:
	Physics() = default;
	~Physics() = default;

    static JPH::TempAllocatorImpl* s_TempAllocator;
    static JPH::JobSystemThreadPool* s_JobSystem;
    static JPH::PhysicsSystem* s_PhysicsSystem;

    static class BroadPhaseLayerInterfaceImpl* s_BroadPhaseInterface;
    static class ObjectVsBroadPhaseLayerFilterImpl* s_ObjectVsBroadPhaseFilter;
    static class ObjectLayerPairFilterImpl* s_ObjectLayerPairFilter;
};