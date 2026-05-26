#version 450

layout(location = 0) in  vec4 frag_color;
layout(location = 0) out vec4 out_color;

void main() {
    // Discard degenerate sentinel vertices (alpha == 0).
    // The Hyperbola two-branch buffer inserts a NaN sentinel between branches
    // with alpha = 0 to visually separate them on a LINE_STRIP.
    if (frag_color.a < 0.01) discard;
    out_color = frag_color;
}
