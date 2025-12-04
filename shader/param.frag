#version 330 core

// design parameters
const vec4 ACCENT = vec4(0.9607843137, 0.7529411765, 0.137254902, 0.0);

uniform vec2 u_resolution;

in vec2 texcoord;
out vec4 frag_color;

// see https://www.shadertoy.com/view/Wlfyzl
float line_segment(in vec2 p, in vec2 a, in vec2 b) {
	vec2 ba = b - a;
	vec2 pa = p - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0., 1.);
	return length(pa - h * ba);
}

void main() {
    frag_color = vec4(0, 0, 0, 0);
    vec2 uv = fract(texcoord * 40) - 0.5;
    // create stippled pattern by thresholding a square wave
    float in_stipple = step(-0.05, uv.x);
    frag_color += in_stipple * ACCENT;
    vec2 p = texcoord * u_resolution;
    // draw line and make stippled by using as alpha for pattern
    float dist_line = line_segment(p, vec2(61, 31), vec2(172, 31));
    frag_color.a = 1 - smoothstep(0.0, 1.0, dist_line);
}
