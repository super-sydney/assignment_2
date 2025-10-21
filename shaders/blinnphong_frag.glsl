#version 410

layout(std140) uniform Material // Must match the GPUMaterial defined in src/mesh.h
{
    vec3 kd;
	vec3 ks;
	float shininess;
	float transparency;
};

uniform samplerCube environmentMap;   
uniform bool useEnvironmentMap;  


uniform sampler2D colorMap;
uniform bool hasTexCoords;
uniform bool useTexture;
uniform bool useMaterial;
uniform sampler2D normalMap;
uniform bool hasNormalMap;

uniform vec3 cameraPosition;
uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform float ka;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in vec4 fragTangent;

layout(location = 0) out vec4 fragColor;

void main()
{
    // Base normal (transformed by normalModelMatrix in vertex shader already)
    vec3 N = normalize(fragNormal);
    // Build TBN from interpolated tangent (w = handedness)
    vec3 T = normalize(fragTangent.xyz);
    // Renormalize tangent against the normal to correct after interpolation
    T = normalize(T - N * dot(N, T));
    vec3 B = normalize(cross(N, T)) * fragTangent.w;
    mat3 TBN = mat3(T, B, N);

    // Determine diffuse color: either the material kd or sampled texture (if available and requested)
    vec3 kdColor = kd;
    if (hasTexCoords && useTexture) {
        kdColor = texture(colorMap, fragTexCoord).rgb;
    }
    if (useMaterial || hasTexCoords) {
        // Light vector from fragment to light
        vec3 L = normalize(lightPosition - fragPosition);
        // Use normal mapping if available
        vec3 Nsample = N;
        if (hasNormalMap) {
            vec3 mapN = texture(normalMap, fragTexCoord).rgb;
            mapN = mapN * 2.0 - 1.0; // expand to [-1,1]
            // Optionally flip green channel depending on normal map convention
            Nsample = normalize(TBN * mapN);
        }
        // Reflect expects the incident vector; reflect(-L, Nsample) reflects the light direction around the normal
        vec3 R = reflect(-L, Nsample);
        vec3 V = normalize(cameraPosition - fragPosition);

        float diff = max(dot(Nsample, L), 0.0);
        float spec = 0.0;
        if (diff > 0.0)
        {
            spec = pow(max(dot(R, V), 0.0), shininess);
        }

        vec3 ambient = ka * lightColor;

        vec3 color = ambient + (kdColor * diff + ks * spec) * lightColor;

        if (useEnvironmentMap) {
            vec3 I = normalize(fragPosition - cameraPosition);
            vec3 R = reflect(I, Nsample);                     
            vec3 envColor = texture(environmentMap, R).rgb;   
            float reflectionStrength = 0.2;                   
            color = mix(color, envColor, reflectionStrength);
        }

        fragColor = vec4(color, transparency);
    }
    else
    {
        fragColor = vec4(normalize(N), 1.0);
    }
}
