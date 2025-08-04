#include "Camera.h"
#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/type_ptr.hpp>


Camera::Camera(float fov, float aspect, float nearPlane, float farPlane)
    : m_FOV(fov), m_Aspect(aspect), m_Near(nearPlane), m_Far(farPlane) {
    UpdateProjection();
    RecalculateView();
}

void Camera::SetViewportSize(float width, float height) {
    if (height <= 0.0f) height = 1.0f; // Prevent divide by zero
    m_Aspect = width / height;
    UpdateProjection();
}

void Camera::UpdateProjection() {
    m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_Aspect, m_Near, m_Far);
}

void Camera::SetPosition(const glm::vec3& pos) {
    m_Position = pos;
    RecalculateView();
}

void Camera::SetRotation(const glm::vec3& rot) {
    m_Rotation = rot;
    RecalculateView();
}

void Camera::LookAt(const glm::vec3& target) {
    m_ViewMatrix = glm::lookAt(m_Position, target, glm::vec3(0, 1, 0));
}

void Camera::RecalculateView() {
    glm::mat4 rotation = glm::yawPitchRoll(glm::radians(m_Rotation.y), glm::radians(m_Rotation.x), glm::radians(m_Rotation.z));
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), -m_Position);
    m_ViewMatrix = rotation * translation;
}

float* Camera::GetViewArray() {
    return glm::value_ptr(m_ViewMatrix);
}

float* Camera::GetProjectionArray() {
    return glm::value_ptr(m_ProjectionMatrix);
}

void Camera::SetPerspective(float fovDegrees, float aspect, float nearClip, float farClip) {
    m_FOV = fovDegrees;
    m_Aspect = aspect;
    m_Near = nearClip;
    m_Far = farClip;

    m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_Aspect, m_Near, m_Far);
}

