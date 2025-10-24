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
uniform sampler2D roughnessMap;
uniform bool hasRoughnessMap;
uniform sampler2D aoMap;
uniform bool hasAOMap;
uniform sampler2D heightMap;
uniform bool hasHeightMap;
uniform float heightScale;

uniform vec3 cameraPosition;
uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform float ka;
uniform float metallic;
uniform float roughnessValue;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in vec4 fragTangent;

layout(location = 0) out vec4 fragColor;

// Schlick-GGX
float G1(float Ndot, float k) {
    return Ndot / (Ndot * (1.0 - k) + k + 1e-7);
}

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

    // Basic parallax mapping
    vec2 uv = fragTexCoord;
    if (hasHeightMap) {
        float height = texture(heightMap, uv).r;

        vec3 viewDir = normalize(cameraPosition - fragPosition);
        vec3 viewDirT = normalize(TBN * viewDir);
        vec2 p = viewDirT.xy / viewDirT.z * (height * heightScale);
        uv -= p;
    }

    // Determine diffuse color, either kd or sampled texture
    vec3 kdColor = kd;
    if (hasTexCoords && useTexture) {
        // Sampled color textures are usually sRGB, convert to linear before use
        vec3 sampled = texture(colorMap, uv).rgb;
        kdColor = pow(sampled, vec3(2.2));
    }
    if (useMaterial || hasTexCoords) {
        // Light vector from fragment to light
        vec3 L = normalize(lightPosition - fragPosition);
        // Use normal mapping if available
        vec3 Nsample = N;
        if (hasNormalMap) {
            vec3 mapN = texture(normalMap, uv).rgb;
            mapN = mapN * 2.0 - 1.0; // expand to [-1,1]
            Nsample = normalize(TBN * mapN); // transform to world space
        }

        vec3 V = normalize(cameraPosition - fragPosition);
        vec3 H = normalize(V + L);

        float NdotL = max(dot(Nsample, L), 0.0);
        float NdotV = max(dot(Nsample, V), 0.0);
        float NdotH = max(dot(Nsample, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);

        float rough = roughnessValue;
        if (hasRoughnessMap) {
            rough = texture(roughnessMap, uv).r;
        }

        float alpha = rough * rough;

        const float PI = 3.14159265359;

        // GGX NDF
        float alpha2 = alpha * alpha;
        float denom = (NdotH * NdotH) * (alpha2 - 1.0) + 1.0;
        float D = alpha2 / (PI * denom * denom + 1e-7);

        // Smith Visibility
        float k = alpha/2.0;
        float G = G1(NdotV, k) * G1(NdotL, k);

        // Schlick Fresnel
        vec3 albedo = kdColor;
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = F0 + (1 - F0) * pow(1.0 - VdotH, 5.0);

        // Specular term
        vec3 numerator = D * G * F;
        float denominator = 4.0 * NdotV * NdotL + 1e-7;
        vec3 specular = numerator / denominator;

        // Burley Diffuse
        float F90 = 0.5 + 2.0*rough*VdotH*VdotH;
        vec3 diffuse = (1.0 - F) * (1.0 - metallic) * (albedo / PI) * (1.0 + (F90 - 1.0)*pow(1-NdotL,5.0)) * (1.0 + (F90 - 1.0)*pow(1-NdotV,5.0));

        // Apply AO to diffuse and ambient
        float ao = 1.0;
        if (hasAOMap) {
            ao = texture(aoMap, uv).r;
        }

        vec3 ambient = ka * lightColor * ao * albedo * 0.1;
        vec3 Lo = (diffuse * ao + specular) * lightColor * NdotL;
        vec3 color = ambient + Lo;

        if (useEnvironmentMap) {
            vec3 I = normalize(fragPosition - cameraPosition);
            vec3 R = reflect(I, Nsample);                     
            vec3 envColor = texture(environmentMap, R).rgb;   
            float reflectionStrength = 0.2;                   
            color = mix(color, envColor, reflectionStrength);
        }

        // Linear back to sRGB
        fragColor = vec4(pow(color, vec3(1.0/2.2)), transparency);
    }
    else
    {
        fragColor = vec4(normalize(N), 1.0);
    }
}
