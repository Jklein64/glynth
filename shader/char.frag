#version 330 core

// TODO make this a uniform
const vec3 ACCENT = vec3(0.9607843137, 0.7529411765, 0.137254902);

uniform sampler2D u_bitmap;

in vec2 texcoord;
out vec4 frag_color;

void main() {
    // need to flip vertically due to texcoord/freetype mismatch
    vec2 uv = vec2(texcoord.x, 1 - texcoord.y);
    // red channel of bitmap contains the alpha
    float alpha = texture(u_bitmap, uv).r;
    frag_color = vec4(ACCENT, alpha);
}
