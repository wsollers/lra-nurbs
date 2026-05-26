#pragma once
// math/GeometryTypes.hpp
// Renderer-neutral geometry payloads shared by math, app, memory, and renderer.

#include "math/Scalars.hpp"

namespace ndde {

struct Vertex {
    Vec3 pos;
    Vec4 color;
};

} // namespace ndde
