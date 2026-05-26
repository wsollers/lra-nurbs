#pragma once
// math/SineRationalSurface.hpp
// SineRationalSurface: height-field  p(u,v) = (u, v, f(u,v))
//
// Mathematical definition
// ───────────────────────
//
//   f(x,y) = ──────────────────── sin(2x) cos(2y)  +  0.1 sin(5x) sin(5y)
//                1 + (x+y+1)²
//
// The first term is a Lorentzian envelope (amplitude 3, ridge along x+y=-1)
// modulating a sinusoidal corrugation. The second term adds fine-scale
// texture. Together they produce a landscape with:
//
//   - A strong diagonal ridge (x+y ≈ -1) draped in sinusoidal folds
//   - Multiple local maxima and minima with non-trivial level curves
//   - Smooth gradients everywhere (C^∞ on R²)
//
// Geometric significance
// ──────────────────────
// The level curves f(x,y) = c are closed or open curves that wind around
// local extrema. Near the ridge they are approximately parallel to x+y = const.
// Away from the ridge the sinusoidal texture dominates and the level curves
// become oscillatory.
//
// Analytic gradients
// ──────────────────
// Let  s = x+y+1,  A = 1+s²,  P = sin(2x)cos(2y).
//
//   ∂f/∂x = (-6s/A²) P  +  (3/A)(2 cos(2x) cos(2y))  +  0.5 cos(5x) sin(5y)
//   ∂f/∂y = (-6s/A²) P  +  (3/A)(-2 sin(2x) sin(2y)) +  0.5 sin(5x) cos(5y)
//
// Both are computed exactly — no finite differences.

#include "math/Surfaces.hpp"
#include "numeric/ops.hpp"
#include <cmath>

namespace ndde::math {

class SineRationalSurface final : public ISurface {
public:
    // Domain: square [-extent, extent]²  (non-periodic, finite)
    explicit SineRationalSurface(float extent = 4.f) : m_ext(extent) {}

    // ── ISurface ──────────────────────────────────────────────────────────────

    [[nodiscard]] Vec3 evaluate(float u, float v, float /*t*/ = 0.f) const override {
        return {u, v, f(u, v)};
    }

    [[nodiscard]] Vec3 du(float u, float v, float /*t*/ = 0.f) const override {
        return {1.f, 0.f, df_du(u, v)};
    }

    [[nodiscard]] Vec3 dv(float u, float v, float /*t*/ = 0.f) const override {
        return {0.f, 1.f, df_dv(u, v)};
    }

    [[nodiscard]] float u_min(float = 0.f) const override { return -m_ext; }
    [[nodiscard]] float u_max(float = 0.f) const override { return  m_ext; }
    [[nodiscard]] float v_min(float = 0.f) const override { return -m_ext; }
    [[nodiscard]] float v_max(float = 0.f) const override { return  m_ext; }

    [[nodiscard]] bool is_periodic_u() const override { return false; }
    [[nodiscard]] bool is_periodic_v() const override { return false; }
    [[nodiscard]] SurfaceMetadata metadata(float t = 0.f) const override {
        SurfaceMetadata data = ISurface::metadata(t);
        data.name = "Sine-Rational Surface";
        data.formula = "z=(3/(1+(x+y+1)^2)) sin(2x) cos(2y)+0.1 sin(5x) sin(5y)";
        data.has_analytic_derivatives = true;
        data.parameters = {{
            {.name = "extent", .value = m_ext, .description = "square domain half-width"}
        }};
        data.parameter_count = 1u;
        return data;
    }

    // ── Public scalar helpers used by analysis simulations ───────────────────

    [[nodiscard]] float height(float u, float v) const noexcept { return f(u, v); }

    // Gradient magnitude squared — cheap approximation of local slope.
    [[nodiscard]] float grad_mag_sq(float u, float v) const noexcept {
        const float fx = df_du(u, v);
        const float fy = df_dv(u, v);
        return fx*fx + fy*fy;
    }

    float extent() const noexcept { return m_ext; }

private:
    float m_ext;

    // ── Height function ───────────────────────────────────────────────────────

    [[nodiscard]] static float f(float x, float y) noexcept {
        const float s = x + y + 1.f;
        const float A = 1.f + s * s;
        return (3.f / A) * ops::sin(2.f * x) * ops::cos(2.f * y)
             + 0.1f * ops::sin(5.f * x) * ops::sin(5.f * y);
    }

    // ── Analytic ∂f/∂x ───────────────────────────────────────────────────────
    //
    //  ∂f/∂x = (-6s / A²) · sin(2x)cos(2y)
    //         +  (3 / A)  · 2cos(2x)cos(2y)
    //         +  0.5 · cos(5x)sin(5y)

    [[nodiscard]] static float df_du(float x, float y) noexcept {
        const float s   = x + y + 1.f;
        const float A   = 1.f + s * s;
        const float A2  = A * A;
        const float s2x = ops::sin(2.f * x);
        const float c2x = ops::cos(2.f * x);
        const float c2y = ops::cos(2.f * y);
        return (-6.f * s / A2) * s2x * c2y
             + ( 6.f / A)      * c2x * c2y
             +   0.5f * ops::cos(5.f * x) * ops::sin(5.f * y);
    }

    // ── Analytic ∂f/∂y ───────────────────────────────────────────────────────
    //
    //  ∂f/∂y = (-6s / A²) · sin(2x)cos(2y)
    //         +  (3 / A)  · (-2sin(2x)sin(2y))
    //         +  0.5 · sin(5x)cos(5y)

    [[nodiscard]] static float df_dv(float x, float y) noexcept {
        const float s   = x + y + 1.f;
        const float A   = 1.f + s * s;
        const float A2  = A * A;
        const float s2x = ops::sin(2.f * x);
        const float s2y = ops::sin(2.f * y);
        const float c2y = ops::cos(2.f * y);
        return (-6.f * s / A2) * s2x * c2y
             + (-6.f / A)      * s2x * s2y
             +   0.5f * ops::sin(5.f * x) * ops::cos(5.f * y);
    }
};

} // namespace ndde::math
