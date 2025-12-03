#version 330 core

in vec3 vertex_color;
in vec2 texcoord;
out vec3 frag_color;

void main() {
    // frag_color = vec3(texcoord, 0.0);
    frag_color = vertex_color;
}
