#pragma once
// app/GaussianRipple.hpp
// GaussianRipple: Gaussian height-field base + decaying radial wave.
// Inherits IDeformableSurface -- is_time_varying() returns true.
//
// GaussianSurface.hpp was archived to src/old/. The eval_static() function
// and domain constants it provided have been inlined here directly.
// Nothing outside this file needs to include GaussianSurface.hpp.

#include "math/Surfaces.hpp"
#include "numeric/ops.hpp"
#include <cmath>

namespace ndde {

class GaussianRipple final : public ndde::math::IDeformableSurface {
public:
    // Domain matches the old GaussianSurface constants
    static constexpr f32 XMIN = f32(-6);
    static constexpr f32 XMAX = f32( 6);
    static constexpr f32 YMIN = f32(-6);
    static constexpr f32 YMAX = f32( 6);

    struct Params {
        f32 amplitude   = 0.5f;
        f32 damping     = 0.35f;
        f32 wavelength  = 1.8f;
        f32 speed       = 2.2f;
        f32 sigma       = 2.8f;
        f32 epicentre_u = 0.f;
        f32 epicentre_v = 0.f;
    };

    GaussianRipple() = default;
    explicit GaussianRipple(Params p) : m_p(p) {}

    [[nodiscard]] f32 u_min(f32 = 0.f) const override { return XMIN; }
    [[nodiscard]] f32 u_max(f32 = 0.f) const override { return XMAX; }
    [[nodiscard]] f32 v_min(f32 = 0.f) const override { return YMIN; }
    [[nodiscard]] f32 v_max(f32 = 0.f) const override { return YMAX; }

    [[nodiscard]] bool is_periodic_u() const override { return false; }
    [[nodiscard]] bool is_periodic_v() const override { return false; }

    [[nodiscard]] ndde::math::SurfaceMetadata metadata(f32 t = 0.f) const override {
        ndde::math::SurfaceMetadata data = ndde::math::IDeformableSurface::metadata(t);
        data.name    = "Gaussian Ripple";
        data.formula = "base Gaussian field + decaying radial perturbation";
        data.has_analytic_derivatives = false;
        data.parameters = {{
            {.name = "amplitude", .value = m_p.amplitude, .description = "initial ripple height"},
            {.name = "damping", .value = m_p.damping, .description = "exponential decay rate"},
            {.name = "wavelength", .value = m_p.wavelength, .description = "radial wave spacing"},
            {.name = "speed", .value = m_p.speed, .description = "ripple phase speed"},
            {.name = "sigma", .value = m_p.sigma, .description = "Gaussian envelope width"},
            {.name = "epicentre_u", .value = m_p.epicentre_u, .description = "ripple center u coordinate"},
            {.name = "epicentre_v", .value = m_p.epicentre_v, .description = "ripple center v coordinate"}
        }};
        data.parameter_count = 7u;
        return data;
    }

    [[nodiscard]] Vec3 evaluate(f32 u, f32 v, f32 t = 0.f) const override {
        return Vec3{ u, v, height(u, v, effective_time(t)) };
    }
    [[nodiscard]] Vec3 du(f32 u, f32 v, f32 t = 0.f) const override {
        constexpr f32 h = 1e-3f;
        const f32 te = effective_time(t);
        return Vec3{ 1.f, 0.f, (height(u+h,v,te) - height(u-h,v,te)) / (2.f*h) };
    }
    [[nodiscard]] Vec3 dv(f32 u, f32 v, f32 t = 0.f) const override {
        constexpr f32 h = 1e-3f;
        const f32 te = effective_time(t);
        return Vec3{ 0.f, 1.f, (height(u,v+h,te) - height(u,v-h,te)) / (2.f*h) };
    }

    void set_epicentre(f32 u0, f32 v0) noexcept {
        m_p.epicentre_u = u0; m_p.epicentre_v = v0; m_time = 0.f;
    }
    void reset() override { m_time = 0.f; }

    Params&       params()       noexcept { return m_p; }
    const Params& params() const noexcept { return m_p; }

private:
    Params m_p;

    [[nodiscard]] f32 effective_time(f32 t) const noexcept {
        return t > 0.f ? t : m_time;
    }

    // 6-Gaussian asymmetric + double sinusoidal ripple (was GaussianSurface::eval_static)
    [[nodiscard]] static f32 base_height(f32 x, f32 y) noexcept {
        const f32 g0 =  1.6f * ops::exp(-((x-1.5f)*(x-1.5f)/0.6f  + (y-0.4f)*(y-0.4f)/0.9f));
        const f32 g1 = -1.3f * ops::exp(-((x+1.3f)*(x+1.3f)/0.8f  + (y+1.1f)*(y+1.1f)/0.5f));
        const f32 g2 =  1.1f * ops::exp(-((x+0.2f)*(x+0.2f)/1.2f  + (y-2.0f)*(y-2.0f)/0.4f));
        const f32 g3 = -0.8f * ops::exp(-((x-2.5f)*(x-2.5f)/0.5f  + (y+0.7f)*(y+0.7f)/1.0f));
        const f32 g4 =  0.7f * ops::exp(-((x-0.8f)*(x-0.8f)/0.7f  + (y+2.3f)*(y+2.3f)/0.6f));
        const f32 g5 = -0.5f * ops::exp(-((x+2.8f)*(x+2.8f)/0.3f  + (y-1.4f)*(y-1.4f)/0.8f));
        const f32 s0 =  0.15f * ops::sin(2.0f*x) * ops::sin(3.0f*y);
        const f32 s1 =  0.12f * ops::cos(1.5f*x - 1.0f) * ops::sin(2.5f*y + 0.7f);
        return g0+g1+g2+g3+g4+g5+s0+s1;
    }

    [[nodiscard]] f32 height(f32 u, f32 v, f32 t) const noexcept {
        const f32 z_base = base_height(u, v);
        const f32 du_  = u - m_p.epicentre_u;
        const f32 dv_  = v - m_p.epicentre_v;
        const f32 r    = ops::sqrt(du_*du_ + dv_*dv_);
        const f32 wave = m_p.amplitude
            * ops::exp(-m_p.damping * t)
            * ops::sin(ops::two_pi_v<f32> * r / m_p.wavelength - m_p.speed * t)
            * ops::exp(-0.5f * r * r / (m_p.sigma * m_p.sigma));
        return z_base + wave;
    }
};

} // namespace ndde
