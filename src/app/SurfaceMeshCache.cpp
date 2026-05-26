#include "app/SurfaceMeshCache.hpp"

#include <algorithm>
#include <memory>

namespace ndde {

void SurfaceMeshCache::clear() {
    m_fill.clear();
    m_wire.clear();
    m_contour.clear();
    m_fill_count = 0;
    m_wire_count = 0;
    m_contour_count = 0;
    m_cached_grid = 0;
    m_cached_build_fill = true;
    m_cached_build_contour = true;
    m_dirty = true;
}

void SurfaceMeshCache::bind_memory(memory::MemoryService* memory) {
    std::pmr::memory_resource* resource = memory ? memory->cache().resource() : std::pmr::get_default_resource();
    if (resource == m_cache_resource)
        return;

    std::destroy_at(&m_fill);
    std::destroy_at(&m_wire);
    std::destroy_at(&m_contour);
    std::construct_at(&m_fill, resource);
    std::construct_at(&m_wire, resource);
    std::construct_at(&m_contour, resource);
    m_cache_resource = resource;
    m_fill_count = 0;
    m_wire_count = 0;
    m_contour_count = 0;
    m_cached_grid = 0;
    m_cached_build_fill = true;
    m_cached_build_contour = true;
    m_dirty = true;
}

Vec4 SurfaceMeshCache::height_color(f32 z, f32 scale) noexcept {
    const f32 t = std::clamp(z / (scale + 1e-9f), -1.f, 1.f);
    if (t >= 0.f)
        return {0.50f + t * 0.35f, 0.50f - t * 0.38f, 0.50f - t * 0.42f, 0.82f};
    const f32 s = -t;
    return {0.50f - s * 0.40f, 0.50f + s * 0.18f - s * s * 0.46f, 0.50f + s * 0.35f, 0.82f};
}

Vec4 SurfaceMeshCache::curvature_color(f32 k, f32 scale) noexcept {
    return height_color(k, scale);
}

void SurfaceMeshCache::rebuild_if_needed(const ndde::math::ISurface& surface,
                                         const SurfaceMeshOptions& options) {
    const u32 n = std::max(options.grid_lines, 2u);
    const bool grid_changed = n != m_cached_grid;
    const bool fill_mode_changed = options.build_fill != m_cached_build_fill;
    const bool contour_mode_changed = options.build_contour != m_cached_build_contour;
    if (!m_dirty && !grid_changed && !fill_mode_changed && !contour_mode_changed && !surface.is_time_varying()) return;

    const f32 time = options.time;
    const u32 fill_capacity = n * n * 6u;

    const f32 u0 = surface.u_min(time);
    const f32 u1 = surface.u_max(time);
    const f32 v0 = surface.v_min(time);
    const f32 v1 = surface.v_max(time);
    const f32 du = (u1 - u0) / static_cast<f32>(n);
    const f32 dv = (v1 - v0) / static_cast<f32>(n);

    u32 idx = 0;
    if (options.build_fill) {
        m_fill.resize(fill_capacity);
        for (u32 i = 0; i < n; ++i) {
            const f32 ua = u0 + static_cast<f32>(i) * du;
            const f32 ub = ua + du;
            for (u32 j = 0; j < n; ++j) {
                const f32 va = v0 + static_cast<f32>(j) * dv;
                const f32 vb = va + dv;

                const Vec3 p00 = surface.evaluate(ua, va, time);
                const Vec3 p10 = surface.evaluate(ub, va, time);
                const Vec3 p01 = surface.evaluate(ua, vb, time);
                const Vec3 p11 = surface.evaluate(ub, vb, time);

                const f32 uc = (ua + ub) * 0.5f;
                const f32 vc = (va + vb) * 0.5f;
                const Vec4 cell_color =
                    options.fill_color_mode == SurfaceFillColorMode::GaussianCurvatureCell
                        ? curvature_color(surface.gaussian_curvature(uc, vc, time), options.color_scale)
                        : height_color(surface.evaluate(uc, vc, time).z, options.color_scale);

                const auto vertex_color = [&](f32 u, f32 v) {
                    if (options.fill_color_mode == SurfaceFillColorMode::HeightVertex)
                        return height_color(surface.evaluate(u, v, time).z, options.color_scale);
                    return cell_color;
                };

                const Vec4 c00 = vertex_color(ua, va);
                const Vec4 c10 = vertex_color(ub, va);
                const Vec4 c01 = vertex_color(ua, vb);
                const Vec4 c11 = vertex_color(ub, vb);

                m_fill[idx++] = {p00, c00}; m_fill[idx++] = {p10, c10};
                m_fill[idx++] = {p11, c11}; m_fill[idx++] = {p00, c00};
                m_fill[idx++] = {p11, c11}; m_fill[idx++] = {p01, c01};
            }
        }
        m_fill_count = idx;
    } else {
        m_fill.clear();
        m_fill_count = 0;
    }

    const u32 wire_capacity = surface.wireframe_vertex_count(n, n);
    m_wire.resize(wire_capacity);
    surface.tessellate_wireframe({m_wire.data(), wire_capacity},
                                 n, n, time, options.wire_color);
    m_wire_count = wire_capacity;

    if (options.build_contour) {
        m_contour.resize(fill_capacity);
        idx = 0;
        for (u32 i = 0; i < n; ++i) {
            const f32 ua = u0 + static_cast<f32>(i) * du;
            const f32 ub = ua + du;
            for (u32 j = 0; j < n; ++j) {
                const f32 va = v0 + static_cast<f32>(j) * dv;
                const f32 vb = va + dv;
                const Vec3 p00{ua, va, 0.f};
                const Vec3 p10{ub, va, 0.f};
                const Vec3 p01{ua, vb, 0.f};
                const Vec3 p11{ub, vb, 0.f};
                const Vec4 c00 = height_color(surface.evaluate(ua, va, time).z, options.color_scale);
                const Vec4 c10 = height_color(surface.evaluate(ub, va, time).z, options.color_scale);
                const Vec4 c01 = height_color(surface.evaluate(ua, vb, time).z, options.color_scale);
                const Vec4 c11 = height_color(surface.evaluate(ub, vb, time).z, options.color_scale);
                m_contour[idx++] = {p00, c00}; m_contour[idx++] = {p10, c10};
                m_contour[idx++] = {p11, c11}; m_contour[idx++] = {p00, c00};
                m_contour[idx++] = {p11, c11}; m_contour[idx++] = {p01, c01};
            }
        }
        m_contour_count = idx;
    } else {
        m_contour.clear();
        m_contour_count = 0;
    }

    m_cached_grid = n;
    m_cached_build_fill = options.build_fill;
    m_cached_build_contour = options.build_contour;
    m_dirty = false;
}

} // namespace ndde
