#version 330 core

in vec2 texcoord;
out vec4 frag_color;

void main() {
    frag_color = vec4(texcoord.xy, 0, 1);
}
