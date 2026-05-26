#pragma once
// app/GaussianSurface.hpp
// GaussianSurface: 6-Gaussian asymmetric + double sinusoidal ripple height field.
// Implements ndde::math::ISurface -- p(u,v) = (u, v, f(u,v)).
//
// Static helpers (grad, unit_normal, curvature, tessellate_*, height_color)
// remain for graph-surface rendering helpers such as heatmaps and contours.
//
// FrenetFrame, SurfaceFrame, make_surface_frame, AnimatedCurve are defined
// here temporarily.  They will move to their own headers in later steps.

#include "math/Scalars.hpp"
#include "math/Surfaces.hpp"    // ISurface base class
#include "math/GeometryTypes.hpp"
#include "app/FrenetFrame.hpp"   // FrenetFrame, SurfaceFrame, make_surface_frame
#include "app/AnimatedCurve.hpp" // AnimatedCurve
#include <glm/glm.hpp>
#include <span>

namespace ndde {

// == GaussianSurface ==========================================================
// Implements ISurface as a height-field graph:  p(u,v) = (u, v, f(u,v))
// where f is the 6-Gaussian asymmetric + double sinusoidal ripple function.

class GaussianSurface : public ndde::math::ISurface {
public:
    static constexpr f32 XMIN = -6.f;
    static constexpr f32 XMAX =  6.f;
    static constexpr f32 YMIN = -6.f;
    static constexpr f32 YMAX =  6.f;
    static constexpr f32 Z_MIN = -2.0f;
    static constexpr f32 Z_MAX =  2.2f;

    // ── ISurface overrides ────────────────────────────────────────────────────
    // These are the new polymorphic entry points.  Internally they delegate
    // to the static methods below so all existing call sites keep working.

    [[nodiscard]] Vec3  evaluate(float u, float v, float t = 0.f) const override {
        (void)t;
        return Vec3{ u, v, eval_static(u, v) };
    }
    [[nodiscard]] Vec3  du(float u, float v, float t = 0.f) const override;
    [[nodiscard]] Vec3  dv(float u, float v, float t = 0.f) const override;

    [[nodiscard]] float u_min(float = 0.f) const override { return XMIN; }
    [[nodiscard]] float u_max(float = 0.f) const override { return XMAX; }
    [[nodiscard]] float v_min(float = 0.f) const override { return YMIN; }
    [[nodiscard]] float v_max(float = 0.f) const override { return YMAX; }
    [[nodiscard]] ndde::math::SurfaceMetadata metadata(float t = 0.f) const override {
        ndde::math::SurfaceMetadata data = ndde::math::ISurface::metadata(t);
        data.name = "Gaussian Surface";
        data.formula = "six Gaussian height field + sinusoidal ripple texture";
        data.has_analytic_derivatives = true;
        return data;
    }
    [[nodiscard]] float extent() const noexcept { return XMAX; }

    // ── Static helpers (unchanged -- existing call sites keep working) ─────────
    // eval_static() is the renamed form of the old eval().
    // The old name eval() now resolves to the ISurface::evaluate() override,
    // but static callers used GaussianSurface::eval(x,y) which is kept below
    // as a forwarding alias so nothing breaks before Step 3.
    [[nodiscard]] static f32  eval_static(f32 x, f32 y) noexcept;

    // Backward-compat alias: GaussianSurface::eval(x,y) still works.
    // Will be removed in Step 3 once all call sites migrate to ISurface.
    [[nodiscard]] static f32  eval(f32 x, f32 y) noexcept { return eval_static(x, y); }

    struct Grad { f32 fx, fy; };
    [[nodiscard]] static Grad grad(f32 x, f32 y) noexcept;
    [[nodiscard]] static Vec3 unit_normal(f32 x, f32 y) noexcept;
    [[nodiscard]] static f32  gaussian_curvature(f32 x, f32 y) noexcept;
    [[nodiscard]] static f32  mean_curvature(f32 x, f32 y) noexcept;

    [[nodiscard]] static u32 wireframe_vertex_count(u32 u_lines, u32 v_lines) noexcept;
    static void tessellate_wireframe(std::span<Vertex> out, u32 u_lines, u32 v_lines);

    [[nodiscard]] static u32 contour_max_vertices(u32 grid_n, u32 n_levels) noexcept;
    static u32 tessellate_contours(std::span<Vertex> out,
                                   u32 grid_n,
                                   const float* levels, u32 n_levels,
                                   Vec4 color = {1.f,1.f,1.f,0.7f});

    [[nodiscard]] static Vec4 height_color(f32 z) noexcept;
};

} // namespace ndde
