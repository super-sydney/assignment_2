#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Camera movement options.
// We prefer a strongly-typed enum to make usage explicit and to match the
// project's conventions (see `OpenGLVersion` in `window.h`).
enum class CameraMovement
{
    Forward,
    Backward,
    Left,
    Right,
    Up,
    Down
};

// Simple free-fly camera supporting basic FPS-style controls.
// - Position and orientation (stored as Euler angles yaw/pitch)
// - Produces a view matrix via getViewMatrix()
// - Movement is frame-rate independent when using processKeyboard(..., deltaTime)
class Camera
{
public:
    // Construct a camera with sensible defaults.
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);

    // Returns the view matrix calculated from the current position and orientation.
    glm::mat4 getViewMatrix() const;

    // Process input from keyboard. Direction describes movement in camera space.
    void processKeyboard(CameraMovement direction, float deltaTime);

    // Process raw mouse movement (offsets in pixels). Constrain pitch prevents flipping.
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

    // Process scroll-wheel input to change the zoom / FOV.
    void processMouseScroll(float yoffset);

    // Accessors
    glm::vec3 getPosition() const { return m_position; }
    float getZoom() const { return m_zoom; }

private:
    // Camera attributes
    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;

    // Euler angles
    float m_yaw;
    float m_pitch;

    // Options
    float m_movementSpeed;
    float m_mouseSensitivity;
    float m_zoom;

    // Recalculate the front/right/up vectors from the current Euler angles.
    void updateCameraVectors();
};
