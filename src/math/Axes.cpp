// math/Axes.cpp
#include "math/Axes.hpp"
#include "numeric/ops.hpp"
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace ndde::math {

// ── Helpers ───────────────────────────────────────────────────────────────────

static u32 count_lines(float extent, float step) noexcept {
    const auto n = static_cast<u32>(ops::floor(extent / step));
    return 2u * n;
}

// ── Legacy fixed-extent grid ──────────────────────────────────────────────────

u32 grid_vertex_count(const AxesConfig& cfg) noexcept {
    const u32 lines_per_axis = count_lines(cfg.extent, cfg.minor_step);
    const u32 z_extra = cfg.is_3d ? lines_per_axis * 2u : 0u;
    return (lines_per_axis * 2u + z_extra) * 2u;
}

u32 axes_vertex_count(const AxesConfig& cfg) noexcept {
    return cfg.is_3d ? 6u : 4u;
}

void build_grid(std::span<Vertex> out, const AxesConfig& cfg) {
    const u32 needed = grid_vertex_count(cfg);
    if (out.size() < static_cast<std::size_t>(needed))
        throw std::invalid_argument("[build_grid] output span too small");

    const float eps = cfg.minor_step * 0.01f;
    u32 idx = 0;

    auto push = [&](Vec3 a, Vec3 b, Vec4 col) {
        out[idx++] = Vertex{ a, col };
        out[idx++] = Vertex{ b, col };
    };

    auto grid_color = [&](float coord) -> Vec4 {
        const float rem = ops::fmod(ops::abs(coord) + eps * 0.5f, cfg.major_step);
        return (rem < eps) ? colors::GRID_MAJOR : colors::GRID_MINOR;
    };

    for (float x = cfg.minor_step; x <= cfg.extent + eps; x += cfg.minor_step) {
        const Vec4 col = grid_color(x);
        push({ x,  -cfg.extent, 0.f }, {  x, cfg.extent, 0.f }, col);
        push({ -x, -cfg.extent, 0.f }, { -x, cfg.extent, 0.f }, col);
    }
    for (float y = cfg.minor_step; y <= cfg.extent + eps; y += cfg.minor_step) {
        const Vec4 col = grid_color(y);
        push({ -cfg.extent,  y, 0.f }, { cfg.extent,  y, 0.f }, col);
        push({ -cfg.extent, -y, 0.f }, { cfg.extent, -y, 0.f }, col);
    }

    if (cfg.is_3d) {
        for (float x = cfg.minor_step; x <= cfg.extent + eps; x += cfg.minor_step) {
            const Vec4 col = grid_color(x);
            push({  x, 0.f, -cfg.extent }, {  x, 0.f, cfg.extent }, col);
            push({ -x, 0.f, -cfg.extent }, { -x, 0.f, cfg.extent }, col);
        }
        for (float y = cfg.minor_step; y <= cfg.extent + eps; y += cfg.minor_step) {
            const Vec4 col = grid_color(y);
            push({ 0.f,  y, -cfg.extent }, { 0.f,  y, cfg.extent }, col);
            push({ 0.f, -y, -cfg.extent }, { 0.f, -y, cfg.extent }, col);
        }
    }
}

void build_axes(std::span<Vertex> out, const AxesConfig& cfg) {
    const u32 needed = axes_vertex_count(cfg);
    if (out.size() < static_cast<std::size_t>(needed))
        throw std::invalid_argument("[build_axes] output span too small");

    const float e = cfg.extent;
    u32 idx = 0;

    out[idx++] = Vertex{ { -e, 0.f, 0.f }, colors::X_AXIS };
    out[idx++] = Vertex{ {  e, 0.f, 0.f }, colors::X_AXIS };
    out[idx++] = Vertex{ { 0.f, -e, 0.f }, colors::Y_AXIS };
    out[idx++] = Vertex{ { 0.f,  e, 0.f }, colors::Y_AXIS };

    if (cfg.is_3d) {
        out[idx++] = Vertex{ { 0.f, 0.f, -e }, colors::Z_AXIS };
        out[idx++] = Vertex{ { 0.f, 0.f,  e }, colors::Z_AXIS };
    }
}

// ── Viewport-aware infinite grid ─────────────────────────────────────────────
// Generates exactly the grid lines visible in [vl,vr] x [vb,vt].
// For each visible X = k*minor_step, draws a vertical line spanning [vb,vt].
// For each visible Y = k*minor_step, draws a horizontal line spanning [vl,vr].
// Skips X=0 and Y=0 (those are the axes, drawn separately).

u32 grid_vp_max_vertices(float vl, float vr, float vb, float vt,
                           float minor_step) noexcept {
    if (minor_step <= 0.f) return 0u;
    // Count of unique integer multiples of minor_step in each axis range,
    // excluding 0 (the axis itself). Double for ±, double for both axes, 2 verts each.
    const float safe_step = std::max(minor_step, 1e-6f);
    const u32 nx = static_cast<u32>(ops::ceil((vr - vl) / safe_step)) + 2u;
    const u32 ny = static_cast<u32>(ops::ceil((vt - vb) / safe_step)) + 2u;
    return (nx + ny) * 2u;  // 2 vertices per line
}

u32 build_grid_viewport(std::span<Vertex> out,
                         float vl, float vr, float vb, float vt,
                         float minor_step, float major_step)
{
    if (minor_step <= 0.f || major_step <= 0.f) return 0u;

    const u32 max_v = grid_vp_max_vertices(vl, vr, vb, vt, minor_step);
    if (out.size() < static_cast<std::size_t>(max_v))
        throw std::invalid_argument("[build_grid_viewport] output span too small");

    const float eps     = minor_step * 0.001f;
    u32         idx     = 0;

    // Round down to nearest grid line below/left of view
    const float x_start = ops::floor(vl / minor_step) * minor_step;
    const float y_start = ops::floor(vb / minor_step) * minor_step;

    auto is_major = [&](float coord) -> bool {
        const float rem = ops::fmod(ops::abs(coord) + eps * 0.5f, major_step);
        return rem < eps;
    };

    // Vertical lines (constant X)
    for (float x = x_start; x <= vr + eps; x += minor_step) {
        if (ops::abs(x) < eps) continue;  // skip the Y-axis line (drawn separately)
        const Vec4 col = is_major(x) ? colors::GRID_MAJOR : colors::GRID_MINOR;
        out[idx++] = Vertex{ Vec3{ x, vb, 0.f }, col };
        out[idx++] = Vertex{ Vec3{ x, vt, 0.f }, col };
    }

    // Horizontal lines (constant Y)
    for (float y = y_start; y <= vt + eps; y += minor_step) {
        if (ops::abs(y) < eps) continue;  // skip the X-axis line
        const Vec4 col = is_major(y) ? colors::GRID_MAJOR : colors::GRID_MINOR;
        out[idx++] = Vertex{ Vec3{ vl, y, 0.f }, col };
        out[idx++] = Vertex{ Vec3{ vr, y, 0.f }, col };
    }

    return idx;
}

} // namespace ndde::math
