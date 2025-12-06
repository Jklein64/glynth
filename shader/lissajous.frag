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

// from https://iquilezles.org/articles/distfunctions2d/
// float sd_box(in vec2 p, in vec2 b) {
//     b = b / 2;
//     vec2 d = abs(p) - b;
//     return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
// }
// point p, bottom left corner c, size s
bool in_box(vec2 p, vec2 c, vec2 s) {
    bool in_x = c.x <= p.x && p.x <= c.x + s.x;
    bool in_y = c.y <= p.y && p.y <= c.y + s.y;
    return in_x && in_y;
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

        // if(p.x - u_outline_glyph_corner.x < u_outline_glyph_size.x && p.y - u_outline_glyph_corner.y < u_outline_glyph_size.y) {
        //     frag_color = vec4(1, 0, 0, 1);
        // }

        if(in_box(p, u_outline_glyph_corner, u_outline_glyph_size)) {
            frag_color.r = 1;
        }
        // float d = sd_box(p - u_outline_glyph_corner, u_outline_glyph_size);
        // float s = 1 - step(0.0, d);
        // frag_color.xyz += s * vec3(1, 0, 0);
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
