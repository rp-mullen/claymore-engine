#pragma once
#include <memory>
#include <string>
#include "ecs/Entity.h"

enum class ScriptBackend {
   None,
   Native,
	Managed
	};

class ScriptComponent {
public:
	virtual ~ScriptComponent() = default;
	virtual void OnCreate(Entity entity) {
		m_Entity = entity;
		}
	virtual void OnUpdate(float dt) {}

	virtual std::shared_ptr<ScriptComponent> Clone() const = 0;

	virtual ScriptBackend GetBackend() const { return ScriptBackend::Native; }

protected:
	Entity m_Entity;
	};

struct ScriptInstance {
	std::string ClassName;
	std::shared_ptr<ScriptComponent> Instance = nullptr;
	};
