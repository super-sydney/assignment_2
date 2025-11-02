#version 410

uniform mat4 mvpMatrix;
uniform mat4 modelMatrix;
// Normals should be transformed differently than positions:
// https://paroj.github.io/gltut/Illumination/Tut09%20Normal%20Transformation.html
uniform mat3 normalModelMatrix;

// Sum-of-sines water parameters (must match application)
uniform int numWaves;
uniform float omega;
uniform float time;
uniform float phi;
uniform float alpha;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 tangent;

out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragTexCoord;
out vec4 fragTangent;
void main()
{
    // Wave displacement in model space
    vec3 pos = position;
    float height = 0.0;
    vec2 dpdx = vec2(0.0);
    float angle = 0.1;
    float alpha_current = alpha;
    float omega_current = omega;

    for (int i = 0; i < numWaves; ++i)
    {
        vec2 dir = vec2(cos(angle), sin(angle));
        height += alpha_current * sin(dot(dir, pos.xz) * omega_current + time * phi);

        dpdx += alpha_current * cos(dot(dir,pos.xz) * omega_current + time * phi) * omega_current * dir;

        angle += 0.73 * 3.14159;
        alpha_current *= 0.75;
        omega_current *= 1.5;
    }
    pos.y += height;

    gl_Position = mvpMatrix * vec4(pos, 1);

    fragPosition = (modelMatrix * vec4(pos, 1)).xyz;
    fragNormal = normalModelMatrix * normalize(normal - vec3(dpdx.x, 0.0, dpdx.y));
    fragTexCoord    = texCoord;
    // Transform tangent to stay in the same space as the normal
    vec3 t_transformed = normalize(normalModelMatrix * tangent.xyz);
    fragTangent     = vec4(t_transformed, tangent.w);
}
