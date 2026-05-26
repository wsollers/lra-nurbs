#pragma once
// math/ExtremumSurface.hpp
// ExtremumSurface: bimodal Gaussian height field with one peak and one pit.
//
// f(u,v) = A1 * exp(-r1^2 / (2*s1^2))   <- positive peak
//         - A2 * exp(-r2^2 / (2*s2^2))   <- negative pit
// where r1 = ||(u,v) - peak_centre||, r2 = ||(u,v) - pit_centre||
//
// Domain: [-extent, extent]^2 (non-periodic by default).
// Peak at (+2, +2), pit at (-2, -2) by default.
//
// Both du() and dv() are exact analytic formulas -- no finite differences.
// The global max and min are well-separated and unambiguous, making this
// surface ideal for the ExtremumTable grid search.

#include "math/Surfaces.hpp"
#include "numeric/ops.hpp"
#include <cmath>
#include <glm/glm.hpp>

namespace ndde::math {

class ExtremumSurface final : public ISurface {
public:
    struct Params {
        glm::vec2 peak_centre = { 2.f,  2.f};
        float     peak_amp    = 2.f;
        float     peak_sigma  = 1.8f;
        glm::vec2 pit_centre  = {-2.f, -2.f};
        float     pit_amp     = 1.8f;   // applied as negative
        float     pit_sigma   = 1.6f;
        float     extent      = 5.f;    // domain = [-extent, extent]^2
    };

    ExtremumSurface() = default;
    explicit ExtremumSurface(Params p) : m_p(p) {}

    [[nodiscard]] Vec3 evaluate(float u, float v, float /*t*/ = 0.f) const override {
        return Vec3{ u, v, height(u, v) };
    }

    [[nodiscard]] Vec3 du(float u, float v, float /*t*/ = 0.f) const override {
        return Vec3{ 1.f, 0.f, dh_du(u, v) };
    }

    [[nodiscard]] Vec3 dv(float u, float v, float /*t*/ = 0.f) const override {
        return Vec3{ 0.f, 1.f, dh_dv(u, v) };
    }

    [[nodiscard]] float u_min(float = 0.f) const override { return -m_p.extent; }
    [[nodiscard]] float u_max(float = 0.f) const override { return  m_p.extent; }
    [[nodiscard]] float v_min(float = 0.f) const override { return -m_p.extent; }
    [[nodiscard]] float v_max(float = 0.f) const override { return  m_p.extent; }

    [[nodiscard]] bool is_periodic_u() const override { return false; }
    [[nodiscard]] bool is_periodic_v() const override { return false; }

    Params&       params()       noexcept { return m_p; }
    const Params& params() const noexcept { return m_p; }

private:
    Params m_p;

    [[nodiscard]] float height(float u, float v) const noexcept {
        const float du1  = u - m_p.peak_centre.x;
        const float dv1  = v - m_p.peak_centre.y;
        const float du2  = u - m_p.pit_centre.x;
        const float dv2  = v - m_p.pit_centre.y;
        const float r1sq = du1*du1 + dv1*dv1;
        const float r2sq = du2*du2 + dv2*dv2;
        const float s1   = 2.f * m_p.peak_sigma * m_p.peak_sigma;
        const float s2   = 2.f * m_p.pit_sigma  * m_p.pit_sigma;
        return  m_p.peak_amp * ops::exp(-r1sq / s1)
              - m_p.pit_amp  * ops::exp(-r2sq / s2);
    }

    // Analytic df/du
    [[nodiscard]] float dh_du(float u, float v) const noexcept {
        const float du1  = u - m_p.peak_centre.x;
        const float dv1  = v - m_p.peak_centre.y;
        const float du2  = u - m_p.pit_centre.x;
        const float dv2  = v - m_p.pit_centre.y;
        const float s1   = 2.f * m_p.peak_sigma * m_p.peak_sigma;
        const float s2   = 2.f * m_p.pit_sigma  * m_p.pit_sigma;
        return (-2.f * du1 / s1) * m_p.peak_amp * ops::exp(-(du1*du1+dv1*dv1)/s1)
             + ( 2.f * du2 / s2) * m_p.pit_amp  * ops::exp(-(du2*du2+dv2*dv2)/s2);
    }

    // Analytic df/dv
    [[nodiscard]] float dh_dv(float u, float v) const noexcept {
        const float du1  = u - m_p.peak_centre.x;
        const float dv1  = v - m_p.peak_centre.y;
        const float du2  = u - m_p.pit_centre.x;
        const float dv2  = v - m_p.pit_centre.y;
        const float s1   = 2.f * m_p.peak_sigma * m_p.peak_sigma;
        const float s2   = 2.f * m_p.pit_sigma  * m_p.pit_sigma;
        return (-2.f * dv1 / s1) * m_p.peak_amp * ops::exp(-(du1*du1+dv1*dv1)/s1)
             + ( 2.f * dv2 / s2) * m_p.pit_amp  * ops::exp(-(du2*du2+dv2*dv2)/s2);
    }
};

} // namespace ndde::math
