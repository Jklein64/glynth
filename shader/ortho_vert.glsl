#version 330 core
layout(location = 0) in vec2 pos;
layout(location = 1) in vec3 color;

out vec3 vertex_color;
out vec2 texcoord;

void main() {
    vertex_color = color;

    texcoord = 0.5 * pos.xy + vec2(0.5);
    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
