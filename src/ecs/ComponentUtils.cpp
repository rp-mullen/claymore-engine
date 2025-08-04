#include "ComponentUtils.h"
#include "Components.h"
#include "EntityData.h"

void EnsureCollider(RigidBodyComponent* rigidBody, EntityData* entityData) {
	if (!entityData->Collider) {
		entityData->Collider = new ColliderComponent();
		entityData->Collider->ShapeType = ColliderShape::Box;
		entityData->Collider->Size = glm::vec3(1.0f);
	}
}

void EnsureCollider(StaticBodyComponent* staticBody, EntityData* entityData) {
	if (!entityData->Collider) {
		entityData->Collider = new ColliderComponent();
		entityData->Collider->ShapeType = ColliderShape::Box;
		entityData->Collider->Size = glm::vec3(1.0f);
	}
} 