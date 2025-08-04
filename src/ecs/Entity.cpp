#include "Entity.h"
#include "Scene.h"

const std::string& Entity::GetName() const {
    return m_Scene->GetEntityData(m_ID)->Name;
}

void Entity::SetName(const std::string& name) {
    if (auto* data = m_Scene->GetEntityData(m_ID)) {
        data->Name = name;
    }
}
