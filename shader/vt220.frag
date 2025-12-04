#version 330 core

#define MAIN_BLOOM_ITERATIONS 10
#define MAIN_BLOOM_SIZE 0.01

#define REFLECTION_BLUR_ITERATIONS 10
#define REFLECTION_BLUR_SIZE 0.05

#define WIDTH 0.8
#define HEIGHT 0.35
#define CURVE 5.0

#define BEZEL_COL vec4(0.8, 0.8, 0.8, 0.0)
#define PHOSPHOR_COL vec4(0.2, 1.0, 0.2, 0.0)
#define AMBIENT 0.2

// #define NO_OF_LINES iResolution.y*HEIGHT
#define SMOOTH 0.002

precision highp float;

in vec2 texcoord;
out vec4 frag_color;

// using normal vectors of a sphere with radius r
vec2 crt_warp(vec2 uv, float r) {
    uv = (uv - 0.5) * 2.0;// uv is now -1 to 1
    uv = r * uv / sqrt(r * r - dot(uv, uv));
    uv = (uv / 2.0) + 0.5;// back to 0-1 coords
    return uv;
}

float squircle(vec2 p, vec2 b, float r) {
    return length(max(abs(p) - b, 0.0)) - r;
}

float squircle_border(vec2 uv, vec2 dims_inner, vec2 dims_outer) {
    float dist_inner = squircle(uv - vec2(0.5, 0.5), dims_inner, 0.05);
    float dist_outer = squircle(uv - vec2(0.5, 0.5), dims_outer, 0.05);
    return smoothstep(-SMOOTH * 2.0, SMOOTH * 10.0, dist_inner) *
        smoothstep(SMOOTH * 3.0, -SMOOTH * 2.0, dist_outer);
}

// smoothsteps inner from -s.x to s.y and outer from s.
float squircle_border(vec2 uv, vec2 dims) {
    return squircle_border(uv, dims, dims);
}

void main() {
    frag_color = vec4(0.0, 0.0, 0.0, 1.0);

    vec4 c = vec4(0.0, 0.0, 0.0, 0.0);
    vec3 iResolution = vec3(840.0, 473.0, 1.0);
    vec2 uv = texcoord.xy;
    vec2 aspect = vec2(1., iResolution.y / iResolution.x);
    uv = 0.5 + (uv - 0.5) / aspect.yx;
    float r = CURVE;

    // Screen Layer
    vec2 uvS = crt_warp(uv, r);

    // Enclosure Layer
    vec2 uvE = crt_warp(uv, r + 0.25);

    c += BEZEL_COL * AMBIENT * 0.7 *
        smoothstep(-SMOOTH, SMOOTH, squircle(uvS - vec2(0.5, 0.5), vec2(WIDTH, HEIGHT), 0.05)) *
        smoothstep(SMOOTH, -SMOOTH, squircle(uvE - vec2(0.5, 0.5), vec2(WIDTH, HEIGHT) + 0.05, 0.05));

    // Corner
    c += (BEZEL_COL) * 0.8 * squircle_border(uvE, vec2(WIDTH, HEIGHT) + 0.05);

    // Outer Border
    c += BEZEL_COL * AMBIENT *
        smoothstep(-SMOOTH, SMOOTH, squircle(uvE - vec2(0.5, 0.5), vec2(WIDTH, HEIGHT) + 0.05, 0.05)) *
        smoothstep(SMOOTH, -SMOOTH, squircle(uvE - vec2(0.5, 0.5), vec2(WIDTH, HEIGHT) + 0.15, 0.05));

    frag_color += c;
}
