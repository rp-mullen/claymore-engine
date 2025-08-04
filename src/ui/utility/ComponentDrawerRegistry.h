#pragma once
#include <unordered_map>
#include <functional>
#include <string>

class ComponentDrawerRegistry {
public:
    using DrawFunc = std::function<void(void*)>;

    static ComponentDrawerRegistry& Instance() {
        static ComponentDrawerRegistry instance;
        return instance;
    }

    template<typename T>
    void Register(const std::string& name, std::function<void(T&)> drawFn) {
        m_Drawers[name] = [drawFn](void* comp) {
            drawFn(*static_cast<T*>(comp));
            };
    }

    void DrawComponentUI(const std::string& name, void* ptr) const {
        auto it = m_Drawers.find(name);
        if (it != m_Drawers.end()) {
            it->second(ptr);
        }
    }

    const std::unordered_map<std::string, DrawFunc>& GetAllDrawers() const {
        return m_Drawers;
    }

private:
    std::unordered_map<std::string, DrawFunc> m_Drawers;
};
