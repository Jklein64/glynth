#version 330 core

#define M_PI 3.1415926535897932384626433832795
const vec3 ACCENT = vec3(0.9607843137, 0.7529411765, 0.137254902);

uniform vec2 u_resolution;
// zero means not focused
uniform float u_time;
// samples of the outline, rg -> xy
uniform sampler1D u_samples;
// whether the outline is valid
uniform bool u_has_outline;
// bounds of cursor block
uniform vec2 u_outline_glyph_corner;
uniform vec2 u_outline_glyph_size;

in vec2 texcoord;
out vec4 frag_color;

// from https://www.shadertoy.com/view/Wlfyzl
float sd_line_segment(in vec2 p, in vec2 a, in vec2 b) {
    vec2 ba = b - a;
    vec2 pa = p - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - h * ba);
}

// modified from https://iquilezles.org/articles/distfunctions2d/
// bottom left corner p, total dimensions b
float sd_box(in vec2 p, in vec2 b) {
    p -= b / 2;
    b = b / 2;
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
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
            // offset start and end to avoid excessive overdraw
            vec2 v = 0.8 * normalize(b - a);
            float d = sd_line_segment(p, a + v, b - v) - 0.5;
            float s = 1 - smoothstep(0.0, 1.0, d);
            frag_color.xyz += s * ACCENT;
            a = b;
        }

        if(u_time != 0) {
            // blink to show interactivity
            vec2 corner = u_outline_glyph_corner;
            vec2 size = u_outline_glyph_size;
            // s.x += x;
            // c.x -= x / 2;
              // MacOS defaults to a period of 2 seconds, which looks good
            float alpha = (atan(3 * sin(M_PI * 2 * u_time)) + 1) / 2;
            float s = 1 - smoothstep(0.0, 2.0, sd_box(p - corner, size) - 1);
            frag_color.xyz += alpha * 0.4 * s * ACCENT;
        }
    }
}
