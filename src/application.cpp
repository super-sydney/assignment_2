//#include "Image.h"
#include "mesh.h"
#include "texture.h"
// Always include window first (because it includes glfw, which includes GL which needs to be included AFTER glew).
// Can't wait for modules to fix this stuff...
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
// Include glad before glfw3
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui/imgui.h>
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <framework/window.h>
#include <framework/camera.h>
#include <framework/file_picker.h>
#include <functional>
#include <iostream>
#include <vector>

class Application {
public:
    Application()
        : m_window("Final Project", glm::ivec2(1024, 1024), OpenGLVersion::GL41), m_texture(nullptr)
    {
        // Create default texture here so we can change it later
        m_texture = std::make_unique<Texture>(RESOURCE_ROOT "resources/checkerboard.png");
        // Default camera to point at origin
        const glm::vec3 camPos = glm::vec3(-1.0f, 1.0f, -1.0f);
        const glm::vec3 target = glm::vec3(0.0f);
        const glm::vec3 dir = glm::normalize(target - camPos);

        const float pitch = glm::degrees(glm::asin(dir.y));
        const float yaw = glm::degrees(glm::atan(dir.z, dir.x));
        m_camera = Camera(camPos, glm::vec3(0.0f, 1.0f, 0.0f), yaw, pitch);
        m_window.registerKeyCallback([this](int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                onKeyPressed(key, mods);
            else if (action == GLFW_RELEASE)
                onKeyReleased(key, mods);
        });
        m_window.registerMouseMoveCallback(std::bind(&Application::onMouseMove, this, std::placeholders::_1));
        m_window.registerMouseButtonCallback([this](int button, int action, int mods) {
            if (action == GLFW_PRESS)
                onMouseClicked(button, mods);
            else if (action == GLFW_RELEASE)
                onMouseReleased(button, mods);
        });

        // Toggle mouse capture by clicking left mouse button
        m_window.registerMouseButtonCallback([this](int button, int action, int mods)
                                             {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
                m_mouseCaptured = !m_mouseCaptured;
                m_window.setMouseCapture(m_mouseCaptured);
                // reset lastMouse to current to avoid jump
                m_lastMousePos = m_window.getCursorPos();
            } });

        m_meshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/dragon.obj");

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "Shaders/shadow_frag.glsl");
            m_shadowShader = shadowBuilder.build();

            // Any new shaders can be added below in similar fashion.
            // ==> Don't forget to reconfigure CMake when you do!
            //     Visual Studio: PROJECT => Generate Cache for ComputerGraphics
            //     VS Code: ctrl + shift + p => CMake: Configure => enter
            // ....
        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }

        // Initialize simple material params (these will be uploaded as uniforms)
        m_kd = glm::vec3(0.5f);
        m_ks = glm::vec3(0.5f);
        m_shininess = 3.0f;
        m_transparency = 1.0f;

        // Initialize a default light
        m_lights.push_back({glm::vec3(2.0f, 4.0f, 2.0f), glm::vec3(1.0f, 1.0f, 1.0f)});
    }

    void update()
    {
        int dummyInteger = 0; // Initialized to 0
        double lastTime = glfwGetTime();
        while (!m_window.shouldClose()) {
            // This is your game loop
            // Put your real-time logic and rendering in here
            m_window.updateInput();

            // Time step
            double currentTime = glfwGetTime();
            float deltaTime = static_cast<float>(currentTime - lastTime);
            lastTime = currentTime;

            // Handle continuous key presses (WASD)
            if (m_window.isKeyPressed(GLFW_KEY_W))
                m_camera.processKeyboard(CameraMovement::Forward, deltaTime);
            if (m_window.isKeyPressed(GLFW_KEY_S))
                m_camera.processKeyboard(CameraMovement::Backward, deltaTime);
            if (m_window.isKeyPressed(GLFW_KEY_A))
                m_camera.processKeyboard(CameraMovement::Left, deltaTime);
            if (m_window.isKeyPressed(GLFW_KEY_D))
                m_camera.processKeyboard(CameraMovement::Right, deltaTime);
            if (m_window.isKeyPressed(GLFW_KEY_SPACE))
                m_camera.processKeyboard(CameraMovement::Up, deltaTime);
            if (m_window.isKeyPressed(GLFW_KEY_LEFT_CONTROL) || m_window.isKeyPressed(GLFW_KEY_LEFT_ALT))
                m_camera.processKeyboard(CameraMovement::Down, deltaTime);

            // Mouse look when captured
            if (m_mouseCaptured)
            {
                glm::vec2 cursor = m_window.getCursorPos();
                glm::vec2 delta = cursor - m_lastMousePos;
                m_lastMousePos = cursor;
                m_camera.processMouseMovement(delta.x, delta.y);
            }

            // Use ImGui for easy input/output of ints, floats, strings, etc...
            ImGui::Begin("Window");
            // Material parameters
            ImGui::Text("Material parameters");
            ImGui::SliderFloat("Shininess", &m_shininess, 0.0f, 80.0f);
            ImGui::ColorEdit3("Kd", &m_kd[0]);
            ImGui::ColorEdit3("Ks", &m_ks[0]);
            ImGui::SliderFloat("Ambient ka", &m_ka, 0.0f, 1.0f);
            ImGui::Separator();

            // Lights
            ImGui::Text("Lights");
            ImGui::Text("Active Light: %zu/%zu", m_selectedLight + 1, m_lights.size() > 0 ? m_lights.size() : 1);

            // Build listbox strings
            std::vector<std::string> itemStrings;
            for (size_t i = 0; i < m_lights.size(); ++i)
            {
                std::string active = (i == m_selectedLight) ? " [ACTIVE]" : "";
                itemStrings.push_back("Light " + std::to_string(i) + active);
            }
            std::vector<const char *> itemCStrings;
            for (const auto &s : itemStrings)
                itemCStrings.push_back(s.c_str());

            int tempSelected = static_cast<int>(m_selectedLight);
            if (!itemCStrings.empty())
            {
                if (ImGui::ListBox("Lights", &tempSelected, itemCStrings.data(), (int)itemCStrings.size(), 4))
                {
                    m_selectedLight = static_cast<size_t>(tempSelected);
                }
            }

            if (ImGui::Button("Reset Lights"))
            {
                m_lights.clear();
                m_lights.push_back({glm::vec3(2.0f, 4.0f, 2.0f), glm::vec3(1.0f)});
                m_selectedLight = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Light"))
            {
                // Place new light at camera position
                m_lights.push_back({m_camera.getPosition(), glm::vec3(1.0f, 1.0f, 1.0f)});
                m_selectedLight = m_lights.size() - 1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove Light") && !m_lights.empty())
            {
                m_lights.erase(m_lights.begin() + m_selectedLight);
                if (m_selectedLight >= m_lights.size() && !m_lights.empty())
                    m_selectedLight = m_lights.size() - 1;
            }

            if (!m_lights.empty() && m_selectedLight < m_lights.size())
            {
                ImGui::Separator();
                ImGui::Text("Edit Current Light:");
                ImGui::ColorEdit3("Light Color", &m_lights[m_selectedLight].color[0]);
            }

            ImGui::Separator();
            ImGui::Checkbox("Use Texture", &m_useTexture);
            ImGui::SameLine();
            if (ImGui::Button("Choose Texture..."))
            {
                if (auto path = pickOpenFile("png,jpg"))
                {
                    try
                    {
                        m_texture = std::make_unique<Texture>(path->string());
                        m_useTexture = true;
                    }
                    catch (...)
                    {
                        std::cerr << "Failed to load normal map" << std::endl;
                    }
                }
            }

            ImGui::Separator();
            ImGui::Checkbox("Use Normal Map", &m_useNormalMap);
            ImGui::SameLine();
            if (ImGui::Button("Choose Normal Map..."))
            {
                if (auto path = pickOpenFile("png,jpg"))
                {
                    try
                    {
                        m_normalMap = std::make_unique<Texture>(path->string());
                        m_useNormalMap = true;
                    }
                    catch (...)
                    {
                        std::cerr << "Failed to load normal map" << std::endl;
                    }
                }
            }

            ImGui::Separator();
            ImGui::Checkbox("Use material if no texture", &m_useMaterial);
            ImGui::End();

            // Clear the screen
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // ...
            glEnable(GL_DEPTH_TEST);

            // Update view matrix from camera
            m_viewMatrix = m_camera.getViewMatrix();
            const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * m_modelMatrix;
            // Normals should be transformed differently than positions (ignoring translations + dealing with scaling):
            // https://paroj.github.io/gltut/Illumination/Tut09%20Normal%20Transformation.html
            const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));

            for (GPUMesh& mesh : m_meshes) {
                m_defaultShader.bind();
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                //Uncomment this line when you use the modelMatrix (or fragmentPosition)
                //glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalModelMatrix));
                if (mesh.hasTextureCoords()) {
                    // If user wants to use textures, bind and tell shader to sample; otherwise treat as no texcoords for shading
                    if (m_useTexture)
                    {
                        if (m_texture)
                            m_texture->bind(GL_TEXTURE0);
                        glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_TRUE);
                        glUniform1i(m_defaultShader.getUniformLocation("useTexture"), GL_TRUE);
                        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_FALSE);
                    }
                    else
                    {
                        // Mesh has texcoords, but user disabled texture usage: tell shader it has texcoords=false
                        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                        glUniform1i(m_defaultShader.getUniformLocation("useTexture"), GL_FALSE);
                        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial);
                    }
                }
                else
                {
                    glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                    glUniform1i(m_defaultShader.getUniformLocation("useTexture"), GL_FALSE);
                    glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial);
                }
                // Upload camera and material uniforms
                glUniform3fv(m_defaultShader.getUniformLocation("cameraPosition"), 1, glm::value_ptr(m_camera.getPosition()));

                // Update material UBO for this mesh (std140 block 'Material')
                GPUMaterial mat;
                mat.kd = m_kd;
                mat.ks = m_ks;
                mat.shininess = m_shininess;
                mat.transparency = m_transparency;
                // Update the mesh's material UBO
                mesh.updateMaterialBuffer(mat);

                // Active light (place at camera position if requested by add action)
                glm::vec3 lightPos = m_lights.empty() ? glm::vec3(2.0f, 4.0f, 2.0f) : m_lights[m_selectedLight].position;
                glm::vec3 lightCol = m_lights.empty() ? glm::vec3(1.0f) : m_lights[m_selectedLight].color;
                glUniform3fv(m_defaultShader.getUniformLocation("lightPosition"), 1, glm::value_ptr(lightPos));
                // If shader supports a light color uniform, upload it (optional)
                int locColor = m_defaultShader.getUniformLocation("lightColor");
                if (locColor >= 0)
                    glUniform3fv(locColor, 1, glm::value_ptr(lightCol));
                int locKa = m_defaultShader.getUniformLocation("ka");
                if (locKa >= 0)
                    glUniform1f(locKa, m_ka);

                int locHasNM = m_defaultShader.getUniformLocation("hasNormalMap");
                if (locHasNM >= 0)
                    glUniform1i(locHasNM, (m_useNormalMap && m_normalMap) ? GL_TRUE : GL_FALSE);

                if (m_useNormalMap && m_normalMap)
                {
                    m_normalMap->bind(GL_TEXTURE1);
                    int locNM = m_defaultShader.getUniformLocation("normalMap");
                    if (locNM >= 0)
                        glUniform1i(locNM, 1);
                }
                mesh.draw(m_defaultShader);
            }

            // Processes input and swaps the window buffer
            m_window.swapBuffers();
        }
    }

    // In here you can handle key presses
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyPressed(int key, int mods)
    {
        std::cout << "Key pressed: " << key << std::endl;
    }

    // In here you can handle key releases
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyReleased(int key, int mods)
    {
        std::cout << "Key released: " << key << std::endl;
    }

    // If the mouse is moved this function will be called with the x, y screen-coordinates of the mouse
    void onMouseMove(const glm::dvec2& cursorPos)
    {
        // std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
    }

    // If one of the mouse buttons is pressed this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseClicked(int button, int mods)
    {
        std::cout << "Pressed mouse button: " << button << std::endl;
    }

    // If one of the mouse buttons is released this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseReleased(int button, int mods)
    {
        std::cout << "Released mouse button: " << button << std::endl;
    }

private:
    Window m_window;

    // Shader for default rendering and for depth rendering
    Shader m_defaultShader;
    Shader m_shadowShader;

    std::vector<GPUMesh> m_meshes;
    std::unique_ptr<Texture> m_texture;
    bool m_useMaterial { true };

    // Simple material parameters exposed to ImGui
    glm::vec3 m_kd;
    glm::vec3 m_ks;
    float m_shininess;
    float m_transparency;
    float m_ka{0.0f};

    // Simple light structure for UI
    struct LightSimple
    {
        glm::vec3 position;
        glm::vec3 color;
    };

    std::vector<LightSimple> m_lights;
    size_t m_selectedLight{0};
    // Normal mapping
    std::unique_ptr<Texture> m_normalMap;
    bool m_useNormalMap{false};
    bool m_useTexture{true};

    // Projection and view matrices for you to fill in and use
    glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 m_modelMatrix { 1.0f };
    Camera m_camera;
    bool m_mouseCaptured{false};
    glm::vec2 m_lastMousePos{0.0f};
};

int main()
{
    Application app;
    app.update();

    return 0;
}
