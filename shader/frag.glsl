#version 330 core

in vec3 vertex_color;
out vec3 frag_color;

void main() {
    frag_color = vertex_color;
}
