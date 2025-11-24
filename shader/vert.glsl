#version 330 core
layout(location = 0) in vec2 aPos; // the position variable has attribute position 0
layout(location = 1) in vec4 source_color;

out vec4 vertexColor; // specify a color output to the fragment shader

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0); // see how we directly give a vec2 to vec4's constructor
    vertexColor = source_color; // vec4(0.5, 0.0, 0.0, 1.0); // set the output variable to a dark-red color
}
