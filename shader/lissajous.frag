#version 330 core

#define M_PI 3.1415926535897932384626433832795
const vec3 ACCENT = vec3(0.9607843137, 0.7529411765, 0.137254902);

// zero means not focused
uniform float u_time;
// samples of the outline, rg -> xy
uniform sampler1D u_samples;
// whether the outline is valid
uniform bool u_has_outline;
uniform vec2 u_resolution;

in vec2 texcoord;
out vec4 frag_color;

// from https://www.shadertoy.com/view/Wlfyzl
float sd_line_segment(in vec2 p, in vec2 a, in vec2 b) {
    vec2 ba = b - a;
    vec2 pa = p - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - h * ba);
}

void main() {
    frag_color = vec4(0, 0, 0, 1);
    if(u_has_outline) {
        vec2 p = u_resolution * texcoord;
        int num_samples = textureSize(u_samples, 0);
        // draw lines from a -> b
        vec2 a = texelFetch(u_samples, 0, 0).rg;
        for(int i = 1; i < num_samples; i++) {
            vec2 b = texelFetch(u_samples, i, 0).rg;
            float s = 1 - smoothstep(0.0, 1.0, sd_line_segment(p, a, b) - 0.5);
            frag_color.xyz += s * ACCENT;
            a = b;
        }
    }

    // int texel = int(texcoord.x * num_samples);
    // texel = min(texel, num_samples - 1);
    // vec2 sample = texelFetch(u_samples, texel, 0).rg;
    // float value = texcoord.y > 0.5 ? sample.y : sample.x;
    // frag_color = vec4(value, 0, 0, 1);

    // if (u_time == 0) {
    //     frag_color = vec4(1, texcoord, 1);
    // } else {
    //     // MacOS defaults to a period of 2 seconds, which looks good
    //     float alpha = (atan(3 *sin(M_PI * 2 * u_time)) + 1) / 2;
    //     frag_color = vec4(texcoord, 1, alpha);
    // }
}
