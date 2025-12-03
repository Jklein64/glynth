#version 330 core

#define M_PI 3.1415926535897932384626433832795

// design parameters
const float WEDGE_WIDTH = 0.5;
const float RING_IN = 10; // for diameter 20px
const float RING_OUT = 20; // for diameter 40px
const vec3 ACCENT = vec3(0.9607843137, 0.7529411765, 0.137254902);
const vec3 ACCENT_FADED = vec3(0.435, 0.353, 0.149);

uniform vec2 u_resolution;
uniform float u_value;

in vec2 texcoord;
out vec4 frag_color;

float map(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

mat2 rotate_cw(float angle) {
    return mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
}

// query point p, center c, radius r
float sd_circle(vec2 p, vec2 c, float r) {
    return distance(p, c) - r;
}

// query point p, center c, inner radius r0, outer radius r1
float sd_circle_ring(vec2 p, vec2 c, float r0, float r1) {
    float r = (r0 + r1) / 2;
    float t = r1 - r0;
    return abs(sd_circle(p, c, r)) - t / 2;
}

// distance to pie wedge spanning t0 to t1 (radians) CW from +y axis
// query point p, center c, start t0, stop t0, radius r
float sd_pie(vec2 p, vec2 c, float t0, float t1, float r) {
    p -= c;
    // t in iq's formula is half of range
    float t = (t1 - t0) / 2;
    // rotate to respect start and stop positions
    p = rotate_cw(t + t0) * p;
    // package sine and cosine into vector for iq's formula
    vec2 cs = vec2(sin(t), cos(t));
    p.x = abs(p.x);
    float l = length(p) - r;
    float m = length(p - cs * clamp(dot(p, cs), 0.0, r)); // c=sin/cos of aperture
    return max(l, m * sign(cs.y * p.x - cs.x * p.y));
}

float sd_colorbar(vec2 p, vec2 c, float t0, float t1) {
    // remap from [0, 1] to colorbar range
    t0 = map(t0, 0, 1, -M_PI + WEDGE_WIDTH, M_PI - WEDGE_WIDTH);
    t1 = map(t1, 0, 1, -M_PI + WEDGE_WIDTH, M_PI - WEDGE_WIDTH);
    // colorbar is intersection here for some reason...
    float d1 = sd_circle_ring(p, c, RING_IN, RING_OUT);
    float d2 = sd_pie(p, c, t0, t1, RING_OUT);
    return max(d1, d2);
}

void main() {
    vec2 p = texcoord * u_resolution;
    vec2 c = vec2(0.5, 0.5) * u_resolution;
    frag_color = vec4(0, 0, 0, 1);
    // -0.5 to 0.5 causes perfect alignment with no overlap but still 1px smoothing
    float in_bar_light = 1 - smoothstep(-0.5, 0.5, sd_colorbar(p, c, 0, u_value));
    float in_bar_dark = 1 - smoothstep(-0.5, 0.5, sd_colorbar(p, c, u_value, 1));
    frag_color.xyz += in_bar_light * ACCENT;
    frag_color.xyz += in_bar_dark * ACCENT_FADED;
}
