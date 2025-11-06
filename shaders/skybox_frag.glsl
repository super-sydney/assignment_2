#version 410 core
in vec3 TexCoords;
out vec4 FragColor;
uniform samplerCube skybox;
uniform float daylight;

void main()
{
    vec3 color = texture(skybox, TexCoords).rgb;

    // Fade to night by darkening
    color *= daylight;

    FragColor = vec4(color, 1.0);
}