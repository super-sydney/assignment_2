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
#include <stb/stb_image.h>


class Application {
public:
    Application()
        : m_window("Final Project", glm::ivec2(1024, 1024), OpenGLVersion::GL41), m_texture(nullptr)
    {
        // Create default texture here so we can change it later
        // Load default ground PBR maps from resources/ground
        try
        {
            m_texture = std::make_unique<Texture>(RESOURCE_ROOT "resources/ground/ground.jpg");
            m_normalMap = std::make_unique<Texture>(RESOURCE_ROOT "resources/ground/ground_normals.png");
            m_roughnessMap = std::make_unique<Texture>(RESOURCE_ROOT "resources/ground/ground_roughness.jpg");
            m_aoMap = std::make_unique<Texture>(RESOURCE_ROOT "resources/ground/ground_ao.jpg");
            m_heightMap = std::make_unique<Texture>(RESOURCE_ROOT "resources/ground/ground_height.png");
            m_useTexture = true;
            m_useNormalMap = false;
            m_useRoughnessMap = false;
            m_useAOMap = false;
            m_useHeightMap = false;
        }
        catch (...)
        {
            // Fall back to checkerboard if any default asset fails to load
            m_texture = std::make_unique<Texture>(RESOURCE_ROOT "resources/checkerboard.png");
        }
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

        initDragonPath();

        m_pathPoints = sampleBezierPath(m_dragonPath, 50);

        glGenVertexArrays(1, &m_pathVAO);
        glGenBuffers(1, &m_pathVBO);

        glBindVertexArray(m_pathVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_pathVBO);
        glBufferData(GL_ARRAY_BUFFER, m_pathPoints.size() * sizeof(glm::vec3), m_pathPoints.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glBindVertexArray(0);


        std::vector<std::string> faces = {
            std::string(RESOURCE_ROOT) + "resources/cubemap/px.png",
            std::string(RESOURCE_ROOT) + "resources/cubemap/nx.png",
            std::string(RESOURCE_ROOT) + "resources/cubemap/py.png",
            std::string(RESOURCE_ROOT) + "resources/cubemap/ny.png",
            std::string(RESOURCE_ROOT) + "resources/cubemap/pz.png",
            std::string(RESOURCE_ROOT) + "resources/cubemap/nz.png",
        };
        m_cubemapTexture = loadCubemap(faces);

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "Shaders/shadow_frag.glsl");
            m_shadowShader = shadowBuilder.build();

            m_skyboxShader = ShaderBuilder()
                .addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/skybox_vert.glsl")
                .addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/skybox_frag.glsl")
                .build();

            ShaderBuilder lineBuilder;
            lineBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/line_vert.glsl");
            lineBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/line_frag.glsl");
            m_lineShader = lineBuilder.build();


            // Any new shaders can be added below in similar fashion.
            // ==> Don't forget to reconfigure CMake when you do!
            //     Visual Studio: PROJECT => Generate Cache for ComputerGraphics
            //     VS Code: ctrl + shift + p => CMake: Configure => enter
            // ....
        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }

		// setup skybox VAO/VBO
        setupSkybox();


        // Blinn-phong shader for comparison
        try
        {
            ShaderBuilder basicBuilder;
            basicBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            basicBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/blinnphong_frag.glsl");
            m_basicShader = basicBuilder.build();
        }
        catch (ShaderLoadingException &e)
        {
            // It's fine if this shader fails to load; we'll just keep using the default shader
            std::cerr << "Warning: failed to load basic shader: " << e.what() << std::endl;
        }

        // Initialize simple material params (these will be uploaded as uniforms)
        m_kd = glm::vec3(0.5f);
        m_ks = glm::vec3(0.5f);
        m_shininess = 3.0f;
        m_transparency = 1.0f;

        // Initialize a default light
        m_lights.push_back({glm::vec3(2.0f, 4.0f, 2.0f), glm::vec3(1.0f, 1.0f, 1.0f)});

        const int segmentCount = 6;        
        const float segmentLength = 0.3f;  
        auto head = std::make_unique<SnakeSegment>();
        head->localPosition = glm::vec3(0.0f);
        SnakeSegment* prev = head.get();
        m_snakeSegments.push_back(prev);

        for (int i = 1; i < segmentCount; ++i) {
            auto seg = std::make_unique<SnakeSegment>();
            seg->localPosition = glm::vec3(0.0f, 0.0f, segmentLength);
            prev->child = std::move(seg);
            prev = prev->child.get();
            m_snakeSegments.push_back(prev);
        }

        m_snakeRoot = std::move(head);


    }

    GLuint loadCubemap(const std::vector<std::string>& faces)
    {
        if (faces.size() != 6) {
            std::cerr << "loadCubemap: expected 6 faces, got " << faces.size() << std::endl;
            return 0;
        }

        // Important: ensure stb doesn't flip cubemap faces unexpectedly
        stbi_set_flip_vertically_on_load(false);

        GLuint textureID = 0;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

        // In case row alignment is not 4 (defensive)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (unsigned int i = 0; i < faces.size(); ++i) {
            int width = 0, height = 0, nrChannels = 0;
            // Force 0 to get number of channels; we will handle both 3 and 4 channels
            stbi_uc* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);

            if (!data) {
                std::cerr << "Cubemap failed to load at path: " << faces[i] << "\n";
                std::cerr << " stbi failure: " << stbi_failure_reason() << std::endl;
                // leave the face empty (still valid) or bail out:
                // return 0;
                continue;
            }

            if (width != height) {
                std::cerr << "Warning: cubemap face not square: " << faces[i] << " (" << width << "x" << height << ")\n";
            }

            GLenum format = GL_RGB;
            if (nrChannels == 1) format = GL_RED;
            else if (nrChannels == 3) format = GL_RGB;
            else if (nrChannels == 4) format = GL_RGBA;
            else {
                std::cerr << "Unexpected channel count (" << nrChannels << ") for " << faces[i] << std::endl;
            }

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

            stbi_image_free(data);

            std::cout << "Loaded cubemap face " << i << ": " << faces[i]
                << " (" << width << "x" << height << ", ch=" << nrChannels << ")\n";
        }

        // Filters + wrapping
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // Generate mipmaps if you set MIN_FILTER to a mipmap mode
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        // restore default alignment
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        return textureID;
    }

    void setupSkybox()
    {
        float skyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        glGenVertexArrays(1, &m_skyboxVAO);
        glGenBuffers(1, &m_skyboxVBO);
        glBindVertexArray(m_skyboxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_skyboxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
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
            ImGui::SliderFloat("Metallic", &m_metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &m_roughness, 0.04f, 1.0f);
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
            ImGui::Checkbox("Use PBR shader", &m_usePBR);
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
            ImGui::Checkbox("Use Roughness Map", &m_useRoughnessMap);
            ImGui::SameLine();
            if (ImGui::Button("Choose Roughness Map..."))
            {
                if (auto path = pickOpenFile("png,jpg"))
                {
                    try
                    {
                        m_roughnessMap = std::make_unique<Texture>(path->string());
                        m_useRoughnessMap = true;
                    }
                    catch (...)
                    {
                        std::cerr << "Failed to load roughness map" << std::endl;
                    }
                }
            }

            ImGui::Separator();
            ImGui::Checkbox("Use AO Map", &m_useAOMap);
            ImGui::SameLine();
            if (ImGui::Button("Choose AO Map..."))
            {
                if (auto path = pickOpenFile("png,jpg"))
                {
                    try
                    {
                        m_aoMap = std::make_unique<Texture>(path->string());
                        m_useAOMap = true;
                    }
                    catch (...)
                    {
                        std::cerr << "Failed to load AO map" << std::endl;
                    }
                }
            }

            ImGui::Separator();
            ImGui::Checkbox("Use Height Map", &m_useHeightMap);
            ImGui::SameLine();
            if (ImGui::Button("Choose Height Map..."))
            {
                if (auto path = pickOpenFile("png,jpg"))
                {
                    try
                    {
                        m_heightMap = std::make_unique<Texture>(path->string());
                        m_useHeightMap = true;
                    }
                    catch (...)
                    {
                        std::cerr << "Failed to load height map" << std::endl;
                    }
                }
            }

            if (m_useHeightMap)
            {
                ImGui::SliderFloat("Height scale", &m_heightScale, 0.0f, 0.2f);
            }

            ImGui::Separator();
            ImGui::Checkbox("Use Environment Map", &m_useEnvironmentMapping);

            ImGui::Separator();
            ImGui::Checkbox("Render the Bezier curves", &m_showPath);

            ImGui::Separator();
            ImGui::Checkbox("Use material if no texture", &m_useMaterial);

            ImGui::Separator();
            ImGui::Text("Snake animation controls:");
            ImGui::SliderFloat("Wave Speed", &m_snakeWaveSpeed, 0.0f, 10.0f);
            ImGui::SliderFloat("Wave Amplitude (deg)", &m_snakeWaveAmplitude, 0.0f, glm::radians(90.0f));
            ImGui::SliderFloat("Wavelength", &m_snakeWavelength, 0.1f, 2.0f);
            ImGui::Checkbox("Pause Snake", &m_snakePaused);

            ImGui::End();

            // Clear the screen
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // ...
            glEnable(GL_DEPTH_TEST);


            // Draw Skybox
            glDepthFunc(GL_LEQUAL); // change depth function so skybox passes when depth is 1.0
            m_skyboxShader.bind();

            // Remove translation from view matrix
            glm::mat4 viewNoTranslate = glm::mat4(glm::mat3(m_camera.getViewMatrix()));
            glUniformMatrix4fv(m_skyboxShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(viewNoTranslate));
            glUniformMatrix4fv(m_skyboxShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projectionMatrix));

            glBindVertexArray(m_skyboxVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTexture);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
            glDepthFunc(GL_LESS); // reset to default

            // Update view matrix from camera
            m_viewMatrix = m_camera.getViewMatrix();


            if (m_showPath) {
                renderBezierPath();
            }

            updateSnakeMotion(deltaTime);
            updateSnake(deltaTime);
            drawSnake();


            // Dragon motion along bezier path
            updateBezierMotion(deltaTime);

            // Mode matrix for dragon that translates and rotates it along the bezier path
            m_modelMatrix = glm::translate(glm::mat4(1.0f), m_dragonPosition) * m_dragonRotation ;

            const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * m_modelMatrix;

            // Normals should be transformed differently than positions (ignoring translations + dealing with scaling):
            // https://paroj.github.io/gltut/Illumination/Tut09%20Normal%20Transformation.html
            const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));


            for (GPUMesh& mesh : m_meshes) {
                // Choose active shader based on UI toggle
                Shader &activeShader = m_usePBR ? m_defaultShader : m_basicShader;
                activeShader.bind();
                glUniformMatrix4fv(activeShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                //Uncomment this line when you use the modelMatrix (or fragmentPosition)
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalModelMatrix));
                if (mesh.hasTextureCoords()) {
                    // If user wants to use textures, bind and tell shader to sample; otherwise treat as no texcoords for shading
                    if (m_useTexture)
                    {
                        if (m_texture)
                            m_texture->bind(GL_TEXTURE0);
                        glUniform1i(activeShader.getUniformLocation("colorMap"), 0);
                        glUniform1i(activeShader.getUniformLocation("hasTexCoords"), GL_TRUE);
                        glUniform1i(activeShader.getUniformLocation("useTexture"), GL_TRUE);
                        glUniform1i(activeShader.getUniformLocation("useMaterial"), GL_FALSE);
                    }
                    else
                    {
                        // Mesh has texcoords, but user disabled texture usage: tell shader it has texcoords=false
                        glUniform1i(activeShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                        glUniform1i(activeShader.getUniformLocation("useTexture"), GL_FALSE);
                        glUniform1i(activeShader.getUniformLocation("useMaterial"), m_useMaterial);
                    }
                }
                else
                {
                    glUniform1i(activeShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                    glUniform1i(activeShader.getUniformLocation("useTexture"), GL_FALSE);
                    glUniform1i(activeShader.getUniformLocation("useMaterial"), m_useMaterial);
                }
                // Upload camera and material uniforms
                glUniform3fv(activeShader.getUniformLocation("cameraPosition"), 1, glm::value_ptr(m_camera.getPosition()));


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
                glUniform3fv(activeShader.getUniformLocation("lightPosition"), 1, glm::value_ptr(lightPos));
                // If shader supports a light color uniform, upload it (optional)
                int locColor = activeShader.getUniformLocation("lightColor");
                if (locColor >= 0)
                    glUniform3fv(locColor, 1, glm::value_ptr(lightCol));
                int locKa = activeShader.getUniformLocation("ka");
                if (locKa >= 0)
                    glUniform1f(locKa, m_ka);

                int locHasNM = activeShader.getUniformLocation("hasNormalMap");
                if (locHasNM >= 0)
                    glUniform1i(locHasNM, (m_useNormalMap && m_normalMap) ? GL_TRUE : GL_FALSE);

                if (m_useNormalMap && m_normalMap)
                {
                    m_normalMap->bind(GL_TEXTURE1);
                    int locNM = activeShader.getUniformLocation("normalMap");
                    if (locNM >= 0)
                        glUniform1i(locNM, 1);
                }
                // Roughness map in texture 2
                int locHasRough = activeShader.getUniformLocation("hasRoughnessMap");
                if (locHasRough >= 0)
                    glUniform1i(locHasRough, (m_useRoughnessMap && m_roughnessMap) ? GL_TRUE : GL_FALSE);
                if (m_useRoughnessMap && m_roughnessMap)
                {
                    m_roughnessMap->bind(GL_TEXTURE2);
                    int locR = activeShader.getUniformLocation("roughnessMap");
                    if (locR >= 0)
                        glUniform1i(locR, 2);
                }

                // AO map in texture 3
                int locHasAO = activeShader.getUniformLocation("hasAOMap");
                if (locHasAO >= 0)
                    glUniform1i(locHasAO, (m_useAOMap && m_aoMap) ? GL_TRUE : GL_FALSE);
                if (m_useAOMap && m_aoMap)
                {
                    m_aoMap->bind(GL_TEXTURE3);
                    int locAO = activeShader.getUniformLocation("aoMap");
                    if (locAO >= 0)
                        glUniform1i(locAO, 3);
                }

                // Height map in texture 4
                int locHasHeight = activeShader.getUniformLocation("hasHeightMap");
                if (locHasHeight >= 0)
                    glUniform1i(locHasHeight, (m_useHeightMap && m_heightMap) ? GL_TRUE : GL_FALSE);
                if (m_useHeightMap && m_heightMap)
                {
                    m_heightMap->bind(GL_TEXTURE4);
                    int locH = activeShader.getUniformLocation("heightMap");
                    if (locH >= 0)
                        glUniform1i(locH, 4);
                }

                int locHeightScale = activeShader.getUniformLocation("heightScale");
                if (locHeightScale >= 0)
                    glUniform1f(locHeightScale, m_heightScale);

                // Upload PBR parameters
                int locMetallic = activeShader.getUniformLocation("metallic");
                if (locMetallic >= 0)
                    glUniform1f(locMetallic, m_metallic);
                int locRoughVal = activeShader.getUniformLocation("roughnessValue");
                if (locRoughVal >= 0)
                    glUniform1f(locRoughVal, m_roughness);

				// Environment mapping on texture 5
                glUniform1i(activeShader.getUniformLocation("useEnvironmentMap"), m_useEnvironmentMapping ? GL_TRUE : GL_FALSE);
                if (m_useEnvironmentMapping) {
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTexture);
                    glUniform1i(activeShader.getUniformLocation("environmentMap"), 5);
                }

                mesh.draw(activeShader);
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

    struct CubicBezier {
        glm::vec3 p0, p1, p2, p3;

        glm::vec3 evaluate(float t) const {
            float u = 1.0f - t;
            return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
        }
    };

    void initDragonPath() {
        m_dragonPath = {
            { {0,1,3}, {1,2,3}, {2,2,3}, {3,1,3} }, // curve 1
            { {3,1,3}, {4,0,2}, {2,-1,1}, {0,0,0} }, // curve 2
            { {0,0,0}, {-1,1,1}, {-2,2,2}, {0,1,3} } // curve 3
        };
    }

    void updateBezierMotion(float deltaTime) {
        if (m_dragonPath.empty()) return;

        m_curveT += m_curveSpeed * deltaTime;
        if (m_curveT > 1.0f) {
            m_curveT = 0.0f;
            m_currentCurve = (m_currentCurve + 1) % m_dragonPath.size();
        }

        glm::vec3 newPos = m_dragonPath[m_currentCurve].evaluate(m_curveT);

        float nextT = m_curveT + 0.01f;
        if (nextT > 1.0f) nextT = 1.0f;
        glm::vec3 nextPos = m_dragonPath[m_currentCurve].evaluate(nextT);
        glm::vec3 direction = glm::normalize(newPos - nextPos);

        // Build rotation from direction
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(up, direction));
        glm::vec3 correctedUp = glm::normalize(glm::cross(direction, right));

        m_dragonRotation = glm::mat4(1.0f);
        m_dragonRotation[0] = glm::vec4(right, 0.0f);
        m_dragonRotation[1] = glm::vec4(correctedUp, 0.0f);
        m_dragonRotation[2] = glm::vec4(direction, 0.0f);

        m_dragonPosition = newPos;
    }

    void renderBezierPath() {
        if (m_pathPoints.empty()) return;

        m_lineShader.bind();
        glUniformMatrix4fv(m_lineShader.getUniformLocation("view"),1, GL_FALSE, glm::value_ptr(m_viewMatrix));
        glUniformMatrix4fv(m_lineShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projectionMatrix));
        // Set the line color to red
        glUniform3f(m_lineShader.getUniformLocation("color"), 1.0f, 0.0f, 0.0f);

        glBindVertexArray(m_pathVAO);
        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(m_pathPoints.size()));
        glBindVertexArray(0);
    }


    std::vector<glm::vec3> sampleBezierPath(const std::vector<Application::CubicBezier>& curves, int samplesPerCurve = 20) {
        std::vector<glm::vec3> points;
        for (const auto& curve : curves) {
            for (int i = 0; i <= samplesPerCurve; ++i) {
                float t = static_cast<float>(i) / samplesPerCurve;
                points.push_back(curve.evaluate(t));
            }
        }
        return points;
    }

    struct SnakeSegment {
        glm::vec3 localPosition;
        glm::mat4 localRotation = glm::mat4(1.0f);
        std::unique_ptr<SnakeSegment> child = nullptr;
    };

    void updateSnake(float dt) {
        if (m_snakeSegments.empty() || m_snakePaused) return;

        m_snakeTime += dt;

        for (size_t i = 0; i < m_snakeSegments.size(); ++i) {
            float phase = m_snakeTime * m_snakeWaveSpeed - static_cast<float>(i) * m_snakeWavelength;
            float angle = sin(phase) * m_snakeWaveAmplitude;

            m_snakeSegments[i]->localRotation = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0));
        }
    }


    void drawMeshWithShader(GPUMesh& mesh, const glm::mat4& modelMatrix)
    {

        Shader& shader = m_usePBR ? m_defaultShader : m_basicShader;
        shader.bind();

        glm::mat4 mvp = m_projectionMatrix * m_viewMatrix * modelMatrix;
        glUniformMatrix4fv(shader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(shader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(modelMatrix));

        glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(modelMatrix));
        glUniformMatrix3fv(shader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));

        mesh.draw(shader);
    }


    void drawSnakeSegment(SnakeSegment* segment, const glm::mat4& parentTransform) {
        if (!segment) return;

        glm::mat4 model =
            parentTransform *
            glm::translate(glm::mat4(1.0f), segment->localPosition) *
            segment->localRotation;

        
        if (!m_meshes.empty())
            drawMeshWithShader(m_meshes[0], model); 

        drawSnakeSegment(segment->child.get(), model);
    }

    void drawSnake() {
        if (!m_snakeRoot) return;

        glm::mat4 base =
            glm::translate(glm::mat4(1.0f), m_snakePosition) *
            m_snakeRotation;

        drawSnakeSegment(m_snakeRoot.get(), base);
    }

    void updateSnakeMotion(float deltaTime) {
        if (m_dragonPath.empty()) return;

        m_snakeT += m_snakeSpeed * deltaTime;
        if (m_snakeT > 1.0f) {
            m_snakeT = 0.0f;
            m_snakeCurve = (m_snakeCurve + 1) % m_dragonPath.size();
        }

        glm::vec3 newPos = m_dragonPath[m_snakeCurve].evaluate(m_snakeT);

        float nextT = m_snakeT + 0.01f;
        if (nextT > 1.0f) nextT = 1.0f;
        glm::vec3 nextPos = m_dragonPath[m_snakeCurve].evaluate(nextT);
        glm::vec3 direction = glm::normalize(newPos - nextPos);

        // Build rotation so the snake faces along the path
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(up, direction));
        glm::vec3 correctedUp = glm::normalize(glm::cross(direction, right));

        m_snakeRotation = glm::mat4(1.0f);
        m_snakeRotation[0] = glm::vec4(right, 0.0f);
        m_snakeRotation[1] = glm::vec4(correctedUp, 0.0f);
        m_snakeRotation[2] = glm::vec4(direction, 0.0f);

        m_snakePosition = newPos;
    }



private:
    Window m_window;

    // Shader for default rendering and for depth rendering
    Shader m_defaultShader;
    Shader m_shadowShader;
    // Basic blinn phong shader to compare against PBR
    Shader m_basicShader;
    Shader m_skyboxShader;
    Shader m_lineShader;

    bool m_usePBR{true};

	bool m_useEnvironmentMapping { false };
    GLuint m_cubemapTexture;

    GLuint m_skyboxVAO = 0;
    GLuint m_skyboxVBO = 0;

    glm::vec3 m_dragonPosition = glm::vec3(0.0f);
	glm::mat4 m_dragonRotation = glm::mat4(1.0f);
    std::vector<CubicBezier> m_dragonPath;
    size_t m_currentCurve = 0;
    float m_curveT = 0.0f;
    float m_curveSpeed = 0.25f; 

	bool m_showPath = true;
    GLuint m_pathVAO = 0;
    GLuint m_pathVBO = 0;
    std::vector<glm::vec3> m_pathPoints;

    std::unique_ptr<SnakeSegment> m_snakeRoot;
    std::vector<SnakeSegment*> m_snakeSegments;
    float m_snakeTime = 0.0f;
    glm::vec3 m_snakePosition = glm::vec3(0.0f);
    glm::mat4 m_snakeRotation = glm::mat4(1.0f);
    float m_snakeT = 0.0f;
    float m_snakeSpeed = 0.2f; // can be same or different from dragon
    size_t m_snakeCurve = 2;

    // Snake animation parameters
    float m_snakeWaveSpeed = 3.0f;
    float m_snakeWaveAmplitude = glm::radians(20.0f); // in radians
    float m_snakeWavelength = 0.6f;
    bool m_snakePaused = false;

    std::vector<GPUMesh> m_meshes;
    std::unique_ptr<Texture> m_texture;
    bool m_useMaterial { true };

    // Simple material parameters exposed to ImGui
    glm::vec3 m_kd;
    glm::vec3 m_ks;
    float m_shininess;
    float m_transparency;
    float m_ka{0.0f};
    float m_metallic{0.0f};
    float m_roughness{0.5f};

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
    std::unique_ptr<Texture> m_roughnessMap;
    bool m_useRoughnessMap{false};
    std::unique_ptr<Texture> m_aoMap;
    bool m_useAOMap{false};
    std::unique_ptr<Texture> m_heightMap;
    bool m_useHeightMap{false};
    float m_heightScale{0.03f};
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
