#include "ComponentUtils.h"
#include "ecs/Components.h"
#include "ecs/EntityData.h"

void EnsureCollider(RigidBodyComponent* rigidBody, EntityData* entityData) {
	if (!entityData->Collider) {
        entityData->Collider = std::make_unique<ColliderComponent>();
		entityData->Collider->ShapeType = ColliderShape::Box;
		entityData->Collider->Size = glm::vec3(1.0f);
	}
}

void EnsureCollider(StaticBodyComponent* staticBody, EntityData* entityData) {
	if (!entityData->Collider) {
        entityData->Collider = std::make_unique<ColliderComponent>();
		entityData->Collider->ShapeType = ColliderShape::Box;
		entityData->Collider->Size = glm::vec3(1.0f);
	}
} 