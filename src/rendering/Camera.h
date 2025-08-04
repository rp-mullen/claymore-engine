#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera {
public:
    Camera(float fov = 60.0f, float aspect = 1.6f, float nearPlane = 0.1f, float farPlane = 1000.0f);

    // Viewport & Projection
    void SetViewportSize(float width, float height);
    void UpdateProjection();
    glm::mat4 GetProjectionMatrix() const { return m_ProjectionMatrix; }
    glm::mat4 GetViewMatrix() const { return m_ViewMatrix; }

    // Camera transform
    void SetPosition(const glm::vec3& pos);
    void SetRotation(const glm::vec3& rot);
    void SetPerspective(float fovDegrees, float aspect, float nearClip, float farClip);

    void LookAt(const glm::vec3& target);

    // Accessors
    glm::vec3 GetPosition() const { return m_Position; }
    glm::vec3 GetRotation() const { return m_Rotation; }

    // bgfx compatibility
    float* GetViewArray();
    float* GetProjectionArray();

private:
    glm::vec3 m_Position{ 0.0f, 5.0f, 10.0f };
    glm::vec3 m_Rotation{ 0.0f, 0.0f, 0.0f }; // Pitch, Yaw, Roll in degrees
    glm::mat4 m_ViewMatrix{ 1.0f };
    glm::mat4 m_ProjectionMatrix{ 1.0f };

    float m_FOV;
    float m_Aspect;
    float m_Near;
    float m_Far;

    void RecalculateView();
};
