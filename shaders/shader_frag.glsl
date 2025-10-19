#version 410

layout(std140) uniform Material // Must match the GPUMaterial defined in src/mesh.h
{
    vec3 kd;
	vec3 ks;
	float shininess;
	float transparency;
};

uniform sampler2D colorMap;
uniform bool hasTexCoords;
uniform bool useMaterial;

uniform vec3 cameraPosition;
uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform float ka;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 normal = normalize(fragNormal);

    if (hasTexCoords)
    {
        fragColor = vec4(texture(colorMap, fragTexCoord).rgb, 1.0);
    }
    else if (useMaterial)
    {
        // Light vector from fragment to light
        vec3 L = normalize(lightPosition - fragPosition);
        vec3 N = normalize(fragNormal);
    // Reflect expects the incident vector; reflect(-L, N) reflects the light direction around the normal
    vec3 R = reflect(-L, N);
        vec3 V = normalize(cameraPosition - fragPosition);

        float diff = max(dot(N, L), 0.0);
        float spec = 0.0;
        if (diff > 0.0)
        {
            spec = pow(max(dot(R, V), 0.0), shininess);
        }

    vec3 ambient = ka * lightColor;

    vec3 color = ambient + (kd * diff + ks * spec) * lightColor;
    fragColor = vec4(color, transparency);
    }
    else
    {
        fragColor = vec4(normalize(normal), 1.0);
    }
}
