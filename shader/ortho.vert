#version 330 core
layout(location = 0) in vec2 pos;
out vec2 texcoord;

void main() {
    texcoord = 0.5 * pos.xy + vec2(0.5);
    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
