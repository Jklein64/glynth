#version 330 core

#define M_PI 3.1415926535897932384626433832795

// Zero means not focused
uniform float u_time;
uniform sampler1D u_outline;
uniform int u_num_outline_samples;

in vec2 texcoord;
out vec4 frag_color;

void main() {
    int texel = int(texcoord.x * u_num_outline_samples);
    texel = min(texel, u_num_outline_samples - 1);
    vec2 sample = texelFetch(u_outline, texel, 0).rg;
    float value = texcoord.y > 0.5 ? sample.y : sample.x;
    frag_color = vec4(0.3 + value * 0.7,0, 0, 1);

    // if (u_time == 0) {
    //     frag_color = vec4(1, texcoord, 1);
    // } else {
    //     // MacOS defaults to a period of 2 seconds, which looks good
    //     float alpha = (atan(3 *sin(M_PI * 2 * u_time)) + 1) / 2;
    //     frag_color = vec4(texcoord, 1, alpha);
    // }
}
