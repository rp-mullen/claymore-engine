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

    // Set gravity explicitly (Jolt defaults to (0, -9.81, 0) but let's be explicit)
    s_PhysicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

    std::cout << "[Physics] Jolt Physics initialized with gravity (0, -9.81, 0).\n";
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

glm::vec3 Physics::GetGravity() {
    if (!s_PhysicsSystem) return glm::vec3(0.0f, -9.81f, 0.0f);
    JPH::Vec3 gravity = s_PhysicsSystem->GetGravity();
    return glm::vec3(gravity.GetX(), gravity.GetY(), gravity.GetZ());
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

    // Normalize the quaternion to ensure it's valid for Jolt
    rotation = glm::normalize(rotation);

    // Debug: Verify quaternion normalization
    float quatLength = glm::length(rotation);
    if (std::abs(quatLength - 1.0f) > 0.001f) {
        std::cout << "[Physics] Warning: Quaternion not properly normalized, length: " << quatLength << std::endl;
    }

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

    // Set mass for dynamic bodies (default to 1.0 kg)
    if (!isStatic) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = 1.0f;
        settings.mGravityFactor = 1.0f; // Ensure gravity is applied
    }

    JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();

    // Create and add the body to the simulation
    JPH::Body* body = bodyInterface.CreateBody(settings);
    if (!body) {
        std::cerr << "[Physics] Failed to create body\n";
        return JPH::BodyID();
    }

    bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
    
    // Debug: Print body info
    if (!isStatic) {
        std::cout << "[Physics] Created dynamic body with ID " << body->GetID().GetIndex() 
                  << " at position (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
    }
    
    return body->GetID();
}

void Physics::SetBodyLinearVelocity(JPH::BodyID bodyID, const glm::vec3& velocity) {
    if (!bodyID.IsInvalid() && s_PhysicsSystem) {
        JPH::Vec3 joltVelocity(velocity.x, velocity.y, velocity.z);
        s_PhysicsSystem->GetBodyInterface().SetLinearVelocity(bodyID, joltVelocity);
    }
}

void Physics::SetBodyAngularVelocity(JPH::BodyID bodyID, const glm::vec3& velocity) {
    if (!bodyID.IsInvalid() && s_PhysicsSystem) {
        JPH::Vec3 joltVelocity(velocity.x, velocity.y, velocity.z);
        s_PhysicsSystem->GetBodyInterface().SetAngularVelocity(bodyID, joltVelocity);
    }
}

// -----------------------------------------------------------------------------
// Public helper to retrieve BodyInterface so that other systems (e.g. Scene)
// can create / manipulate bodies without touching the Physics internals.
// -----------------------------------------------------------------------------
JPH::BodyInterface& Physics::GetBodyInterface() {
    return s_PhysicsSystem->GetBodyInterface();
}

glm::mat4 Physics::GetBodyTransform(JPH::BodyID bodyID) {
    if (bodyID.IsInvalid() || !s_PhysicsSystem) {
        return glm::mat4(0.0f); // Return invalid transform
    }

    JPH::Mat44 joltTransform = s_PhysicsSystem->GetBodyInterface().GetWorldTransform(bodyID);
    
    // Convert Jolt (row-major) matrix to GLM (column-major) matrix by transposing during copy
    glm::mat4 glmTransform(1.0f);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            glmTransform[col][row] = joltTransform(row, col);
        }
    }
    return glmTransform;
}

void Physics::SetBodyTransform(JPH::BodyID bodyID, const glm::vec3& position, const glm::vec3& eulerDegrees) {
    if (bodyID.IsInvalid() || !s_PhysicsSystem) return;
    JPH::BodyInterface& bi = s_PhysicsSystem->GetBodyInterface();
    // Convert Euler degrees to quaternion
    glm::quat rot = glm::quat(glm::radians(eulerDegrees));
    JPH::RVec3 pos(position.x, position.y, position.z);
    JPH::Quat q(rot.x, rot.y, rot.z, rot.w);
    bi.SetPositionAndRotation(bodyID, pos, q, JPH::EActivation::Activate);
}

