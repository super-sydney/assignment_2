#version 410

layout(std140) uniform Material // Must match the GPUMaterial defined in src/mesh.h
{
    vec3 kd;
	vec3 ks;
	float shininess;
	float transparency;
};

uniform bool useMaterial;

uniform vec3 cameraPosition;
uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform float ka;

uniform int numWaves;
uniform float time;
uniform samplerCube environmentMap;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in vec4 fragTangent;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 N = normalize(fragNormal);
	vec3 V = normalize(cameraPosition - fragPosition);
	vec3 L = normalize(lightPosition - fragPosition);
	vec3 H = normalize(L + V);

	// Blue water
	vec3 kd = vec3(0.0, 0.35, 0.7);

	float NdotL = max(dot(N, L), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	vec3 diffuse = kd * NdotL * lightColor;
	vec3 spec = ks * pow(NdotH, shininess) * lightColor;
	vec3 ambient = ka * lightColor;

	vec3 color = ambient + diffuse + spec;

	// Fresnel for reflections
	float cosTheta = max(dot(N, V), 0.0);
	float F = pow(1.0 - cosTheta, 5.0);
	spec *= F;

	// Sample environment map for reflections
	vec3 R = reflect(-V, N);
	vec3 envColor = texture(environmentMap, R).rgb;

	// Mix base shading with environment reflection using Fresnel
	vec3 final = mix(color, envColor, F);

	fragColor = vec4(final, 1.0);
}
