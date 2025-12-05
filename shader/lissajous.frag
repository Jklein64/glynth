#version 330 core

in vec2 texcoord;
out vec4 frag_color;

void main() {
    frag_color = vec4(1, texcoord, 1);
}
