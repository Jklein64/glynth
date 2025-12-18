#version 330 core

uniform sampler1D u_samples;
uniform vec2 u_resolution;

in vec2 texcoord;
out vec4 frag_color;

void main() {
    vec2 p = u_resolution * texcoord;
    int num_samples = textureSize(u_samples, 0);
    float v = texelFetch(u_samples, int(num_samples * texcoord.x), 0).r;
    frag_color = vec4((v + 1) / 2, 0, 0, 1);
}
