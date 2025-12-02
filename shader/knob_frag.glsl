#version 330 core

uniform vec2 u_resolution;

in vec2 texcoord;
out vec4 frag_color;

void main() {
    vec2 pixel = texcoord * u_resolution;
    vec2 center_pixel = vec2(0.5, 0.5) * u_resolution;
    // Nice smoothed circle
    float s = smoothstep(0, 2, distance(pixel, center_pixel) - 40);
    frag_color = (1 - s) * vec4(0.82, 0.38, 0.38, 1.0);
}
