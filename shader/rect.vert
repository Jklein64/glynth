#version 330 core
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;
out vec2 texcoord;

uniform mat4 u_projection;

void main() {
    u_projection;
    texcoord = uv;
    gl_Position = u_projection * vec4(pos.x, pos.y, 0.0, 1.0);
}
