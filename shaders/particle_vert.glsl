#version 410
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;

out vec4 vertexColor;

uniform mat4 view;
uniform mat4 projection;
uniform float pointSize; 

void main() {
    gl_Position = projection * view * vec4(aPos, 1.0);
    vertexColor = aColor;
    gl_PointSize = pointSize; // make it bigger
}
