// math/ExtremumTable.cpp
#include "math/ExtremumTable.hpp"
#include "numeric/ops.hpp"
#include "math/Surfaces.hpp"
#include "numeric/ops.hpp"
#include <limits>
#include <algorithm>
#include <cmath>

namespace ndde::math {

void ExtremumTable::build(const ISurface& surface, float t, u32 grid_n) {
    valid = false;

    const float u0 = surface.u_min(t), u1 = surface.u_max(t);
    const float v0 = surface.v_min(t), v1 = surface.v_max(t);
    const float du = (u1 - u0) / static_cast<float>(grid_n);
    const float dv = (v1 - v0) / static_cast<float>(grid_n);

    float best_max = -std::numeric_limits<float>::max();
    float best_min =  std::numeric_limits<float>::max();

    // Grid search -- O(grid_n^2) surface evaluations
    for (u32 i = 0; i <= grid_n; ++i) {
        for (u32 j = 0; j <= grid_n; ++j) {
            const float u = u0 + static_cast<float>(i) * du;
            const float v = v0 + static_cast<float>(j) * dv;
            const float z = surface.evaluate(u, v, t).z;
            if (z > best_max) { best_max = z; max_uv = {u, v}; max_z = z; }
            if (z < best_min) { best_min = z; min_uv = {u, v}; min_z = z; }
        }
    }

    // Gradient refinement (20 steps of gradient ascent/descent from candidate)
    constexpr float step  = 0.02f;
    constexpr int   iters = 20;

    // Refine max: gradient ascent on z-component
    {
        glm::vec2 p = max_uv;
        for (int k = 0; k < iters; ++k) {
            const Vec3 du_v = surface.du(p.x, p.y, t);
            const Vec3 dv_v = surface.dv(p.x, p.y, t);
            p.x = ops::clamp(p.x + step * du_v.z, u0, u1);
            p.y = ops::clamp(p.y + step * dv_v.z, v0, v1);
        }
        const float z = surface.evaluate(p.x, p.y, t).z;
        if (z > max_z) { max_uv = p; max_z = z; }
    }

    // Refine min: gradient descent on z-component
    {
        glm::vec2 p = min_uv;
        for (int k = 0; k < iters; ++k) {
            const Vec3 du_v = surface.du(p.x, p.y, t);
            const Vec3 dv_v = surface.dv(p.x, p.y, t);
            p.x = ops::clamp(p.x - step * du_v.z, u0, u1);
            p.y = ops::clamp(p.y - step * dv_v.z, v0, v1);
        }
        const float z = surface.evaluate(p.x, p.y, t).z;
        if (z < min_z) { min_uv = p; min_z = z; }
    }

    valid = true;
}

} // namespace ndde::math
