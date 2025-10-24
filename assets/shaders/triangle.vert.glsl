#version 460 core

const vec4 positions[] = {
    vec4(-0.5, -0.5, 0.0, 1.0),
    vec4( 0.0,  0.5, 0.0, 1.0),
    vec4( 0.5, -0.5, 0.0, 1.0)
};

void main() {
    gl_Position = positions[gl_VertexIndex];
}