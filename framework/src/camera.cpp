#include <framework/camera.h>
#include <iostream>

// Construct with defaults similar to common OpenGL examples.
Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : m_position(position)
    , m_worldUp(up)
    , m_yaw(yaw)
    , m_pitch(pitch)
    , m_front(glm::vec3(1.0f, -1.0f, 1.0f))
    , m_movementSpeed(2.5f)
    , m_mouseSensitivity(0.1f)
    , m_zoom(45.0f)
{
    // Initialize derived vectors from Euler angles
    updateCameraVectors();
}

// Return view matrix from current camera parameters.
glm::mat4 Camera::getViewMatrix() const
{
    std::cout << "Camera Front:" << m_front.x << ", " << m_front.y << ", " << m_front.z << std::endl;
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

// Move the camera in local camera-space. deltaTime makes movement frame-rate independent
void Camera::processKeyboard(CameraMovement direction, float deltaTime)
{
    float velocity = m_movementSpeed * deltaTime;
    switch (direction) {
    case CameraMovement::Forward:
        m_position += m_front * velocity;
        break;
    case CameraMovement::Backward:
        m_position -= m_front * velocity;
        break;
    case CameraMovement::Left:
        m_position -= m_right * velocity;
        break;
    case CameraMovement::Right:
        m_position += m_right * velocity;
        break;
    case CameraMovement::Up:
        m_position += m_worldUp * velocity;
        break;
    case CameraMovement::Down:
        m_position -= m_worldUp * velocity;
        break;
    }
}

// Update yaw/pitch from mouse input and update derived vectors
void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= m_mouseSensitivity;
    yoffset *= m_mouseSensitivity;

    m_yaw += xoffset;
    m_pitch += yoffset;

    if (constrainPitch) {
        if (m_pitch > 89.0f)
            m_pitch = 89.0f;
        if (m_pitch < -89.0f)
            m_pitch = -89.0f;
    }

    updateCameraVectors();
}

// Zoom control and clamp to reasonable values
void Camera::processMouseScroll(float yoffset)
{
    m_zoom -= yoffset;
    if (m_zoom < 1.0f)
        m_zoom = 1.0f;
    if (m_zoom > 90.0f)
        m_zoom = 90.0f;
}

// Recalculate front/right/up from yaw and pitch
void Camera::updateCameraVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}
