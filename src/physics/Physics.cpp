// Physics.cpp
#include "Physics.h"
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <iostream>
#include <Jolt/Math/Mat44.h>
#include <glm/gtc/type_ptr.hpp> // for glm::value_ptr
#include <Jolt/Physics/Body/BodyCreationSettings.h>

// ---- Layer Definitions ----
static const JPH::ObjectLayer OBJECT_LAYER_NON_MOVING = 0;
static const JPH::ObjectLayer OBJECT_LAYER_MOVING = 1;

// --- Static member definitions ---
// Public Jolt members
JPH::TempAllocatorImpl* Physics::s_TempAllocator = nullptr;
JPH::JobSystemThreadPool* Physics::s_JobSystem = nullptr;
JPH::PhysicsSystem* Physics::s_PhysicsSystem = nullptr;

// Your custom classes used for filtering
BroadPhaseLayerInterfaceImpl* Physics::s_BroadPhaseInterface = nullptr;
ObjectVsBroadPhaseLayerFilterImpl* Physics::s_ObjectVsBroadPhaseFilter = nullptr;
ObjectLayerPairFilterImpl* Physics::s_ObjectLayerPairFilter = nullptr;


class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        return true; // Allow everything for now
    }
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        return true; // Allow everything for now
    }
};

// Maps object layers to broad phase layers
class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterfaceImpl() {
        mObjectToBroadPhase[OBJECT_LAYER_NON_MOVING] = JPH::BroadPhaseLayer(0);
        mObjectToBroadPhase[OBJECT_LAYER_MOVING] = JPH::BroadPhaseLayer(1);
    }

    virtual JPH::uint GetNumBroadPhaseLayers() const override {

        return 2;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch (inLayer) {
        case 0: return "NonMoving";
        case 1: return "Moving";
        default: return "Unknown";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[2];
};


void Physics::Init() {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    s_TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
    s_JobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, JPH::thread::hardware_concurrency() - 1);

    s_BroadPhaseInterface = new BroadPhaseLayerInterfaceImpl();
    s_ObjectVsBroadPhaseFilter = new ObjectVsBroadPhaseLayerFilterImpl();
    s_ObjectLayerPairFilter = new ObjectLayerPairFilterImpl();

    s_PhysicsSystem = new JPH::PhysicsSystem();
    s_PhysicsSystem->Init(
        1024, 0, 1024, 1024,
        *s_BroadPhaseInterface,
        *s_ObjectVsBroadPhaseFilter,
        *s_ObjectLayerPairFilter
    );

    std::cout << "[Physics] Jolt Physics initialized.\n";
}


void Physics::Shutdown() {
    delete s_PhysicsSystem;
    delete s_JobSystem;
    delete s_TempAllocator;
    delete JPH::Factory::sInstance;
    std::cout << "[Physics] Jolt Physics shut down.\n";
}

void Physics::Step(float deltaTime) {
    s_PhysicsSystem->Update(deltaTime, 1, s_TempAllocator, s_JobSystem);
}

void Physics::DestroyBody(JPH::BodyID bodyID) {
    if (!s_PhysicsSystem) return;

    JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
    bodyInterface.RemoveBody(bodyID);
    bodyInterface.DestroyBody(bodyID);
}


JPH::BodyID Physics::CreateBody(const glm::mat4& transform, JPH::RefConst<JPH::Shape> shape, bool isStatic) {
    if (!s_PhysicsSystem || !shape)
        return JPH::BodyID(); // Invalid

    // Extract position and rotation from glm::mat4
    glm::vec3 position = glm::vec3(transform[3]); // Extract translation
    glm::quat rotation = glm::quat_cast(transform); // Convert matrix to quaternion

    // Convert to Jolt types
    JPH::Vec3 joltPosition(position.x, position.y, position.z);
    JPH::Quat joltRotation(rotation.x, rotation.y, rotation.z, rotation.w);

    // Create body settings
    JPH::BodyCreationSettings settings(
        shape,
        joltPosition,
        joltRotation,
        isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
        isStatic ? OBJECT_LAYER_NON_MOVING : OBJECT_LAYER_MOVING
    );

    JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();

    // Create and add the body to the simulation
    JPH::Body* body = bodyInterface.CreateBody(settings);
    if (!body) {
        std::cerr << "[Physics] Failed to create body\n";
        return JPH::BodyID();
    }

    bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
    return body->GetID();
}

