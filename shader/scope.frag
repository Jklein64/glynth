#version 330 core

const vec3 ACCENT = vec3(0.9607843137, 0.7529411765, 0.137254902);

uniform sampler1D u_samples;
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
    vec2 p = u_resolution * texcoord;
    int num_samples = textureSize(u_samples, 0);
    // draw lines from a -> b. see lissajous.frag
    vec2 a = vec2(0, texelFetch(u_samples, 0, 0).r);
    for(int i = 1; i < num_samples; i++) {
        vec2 b = vec2(i, texelFetch(u_samples, i, 0).r);
        float d = sd_line_segment(p, a, b) - 0.5;
        float s = 1 - smoothstep(0.0, 1.0, d);
        frag_color.xyz = max(frag_color.xyz, s * ACCENT);
        a = b;
    }
}
