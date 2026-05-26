#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;

layout(push_constant) uniform PushConstants {
    mat4  mvp;
    vec4  uniform_color;
    int   mode;
} pc;

layout(location = 0) out vec4 frag_color;

void main() {
    gl_Position = pc.mvp * vec4(in_position, 1.0);

    if (pc.mode == 0) {
        frag_color = in_color;          // per-vertex colour from buffer
    } else {
        frag_color = pc.uniform_color;  // uniform colour from push constant
    }
}
