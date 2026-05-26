#pragma once
// math/Axes.hpp
// Generates vertex data for coordinate axes and grid lines.
// Two grid functions:
//   build_grid()          — legacy, bounded by AxesConfig::extent
//   build_grid_viewport() — covers the visible view exactly; use this for 2D

#include "math/Scalars.hpp"
#include "math/GeometryTypes.hpp"
#include <span>
#include <cmath>

namespace ndde::math {

namespace colors {
    inline constexpr Vec4 X_AXIS    { 1.00f, 0.22f, 0.22f, 1.00f };
    inline constexpr Vec4 Y_AXIS    { 0.22f, 1.00f, 0.22f, 1.00f };
    inline constexpr Vec4 Z_AXIS    { 0.22f, 0.22f, 1.00f, 1.00f };
    inline constexpr Vec4 GRID_MAJOR{ 0.28f, 0.28f, 0.28f, 1.00f };
    inline constexpr Vec4 GRID_MINOR{ 0.11f, 0.11f, 0.11f, 1.00f };
} // namespace colors

struct AxesConfig {
    float extent     = 2.0f;
    float major_step = 1.0f;
    float minor_step = 0.2f;
    bool  is_3d      = false;
};

// ── Legacy fixed-extent grid ──────────────────────────────────────────────────

[[nodiscard]] u32 grid_vertex_count(const AxesConfig& cfg) noexcept;
[[nodiscard]] u32 axes_vertex_count(const AxesConfig& cfg) noexcept;

void build_grid(std::span<Vertex> out, const AxesConfig& cfg);
void build_axes(std::span<Vertex> out, const AxesConfig& cfg);

// ── Viewport-aware infinite grid ─────────────────────────────────────────────
// Generates only the grid lines that are actually visible in the view.
// Grid extends from [vl, vr] x [vb, vt] (world-space view bounds).
// Lines at multiples of minor_step; brighter at multiples of major_step.
// Returns the number of vertices written (always even; max = grid_vp_max_vertices).

[[nodiscard]] u32 grid_vp_max_vertices(float vl, float vr, float vb, float vt,
                                        float minor_step) noexcept;

u32 build_grid_viewport(std::span<Vertex> out,
                         float vl, float vr, float vb, float vt,
                         float minor_step, float major_step);

} // namespace ndde::math
