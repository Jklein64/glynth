#version 330 core

#define M_PI 3.1415926535897932384626433832795

// Zero means not focused
uniform float u_time;
// Samples of the outline, rg -> xy
uniform sampler1D u_samples;
// Whether the outline is valid
uniform bool u_has_outline;

in vec2 texcoord;
out vec4 frag_color;

void main() {
    int num_samples = textureSize(u_samples, 0);
    int texel = int(texcoord.x * num_samples);
    texel = min(texel, num_samples - 1);
    vec2 sample = texelFetch(u_samples, texel, 0).rg;
    float value = texcoord.y > 0.5 ? sample.y : sample.x;
    frag_color = vec4(value, 0, 0, 1);

    // if (u_time == 0) {
    //     frag_color = vec4(1, texcoord, 1);
    // } else {
    //     // MacOS defaults to a period of 2 seconds, which looks good
    //     float alpha = (atan(3 *sin(M_PI * 2 * u_time)) + 1) / 2;
    //     frag_color = vec4(texcoord, 1, alpha);
    // }
}
