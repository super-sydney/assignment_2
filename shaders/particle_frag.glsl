#version 410
in vec4 vertexColor;
out vec4 FragColor;

void main() {
	vec2 coord = gl_PointCoord - vec2(0.5);
	if (length(coord) > 0.5) discard;
	FragColor = vec4(vertexColor.rgb, vertexColor.a);
}
