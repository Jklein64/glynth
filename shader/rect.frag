#version 330 core

uniform vec2 u_resolution;

in vec2 texcoord;
out vec4 frag_color;

void main() {
    float dist = distance(texcoord, vec2(0.0, 0.0));
    float scale = exp(-(dist * dist));
    frag_color = scale * vec4(1.0, 0.0, 0.0, 1.0);
}
