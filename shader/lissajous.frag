#version 330 core

#define M_PI 3.1415926535897932384626433832795

// Zero means not focused
uniform float u_time;

in vec2 texcoord;
out vec4 frag_color;

void main() {
    if (u_time == 0) {
        frag_color = vec4(1, texcoord, 1);
    } else {
        // MacOS defaults to a period of 2 seconds, which looks good
        float alpha = (atan(3 *sin(M_PI * 2 * u_time)) + 1) / 2;
        frag_color = vec4(texcoord, 1, alpha);
    }
}
