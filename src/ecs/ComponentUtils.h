#pragma once

// Forward declarations
struct EntityData;
struct RigidBodyComponent;
struct StaticBodyComponent;

// Utility functions for components that need EntityData
void EnsureCollider(RigidBodyComponent* rigidBody, EntityData* entityData);
void EnsureCollider(StaticBodyComponent* staticBody, EntityData* entityData); 