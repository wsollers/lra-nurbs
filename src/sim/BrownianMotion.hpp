#pragma once
// sim/BrownianMotion.hpp
// BrownianMotion: Itô SDE equation for a diffusing particle on a surface.
//
// Mathematical formulation
// ────────────────────────
// In parameter space (u,v), the Itô SDE is:
//
//   dX_t = mu(X_t, t) dt + sigma(X_t, t) dW_t
//
// where:
//   X_t in R^2  -- position in parameter space
//   mu          -- drift vector field (deterministic part)
//   sigma       -- diffusion coefficient (scalar or 2x2 matrix)
//   dW_t        -- 2D Wiener increment: dW ~ N(0, dt * I)
//
// For PURE Brownian motion:  mu = 0,  sigma = constant.
//
// The noise_coefficient() method returns sigma per axis.
// The velocity() method returns the drift mu (zero for pure Brownian motion).
//
// The Milstein integrator (MilsteinIntegrator) handles the stochastic term.
//
// Geometric note
// ──────────────
// Brownian motion on a Riemannian manifold is defined via the Laplace-Beltrami
// operator.  In local coordinates (u,v) with metric g_{ij}, the diffusion
// tensor is g^{ij} (the inverse metric).
//
// For a height-field surface p(u,v) = (u,v,f(u,v)):
//   g = [ 1+f_u^2   f_u*f_v ]   g^{-1} = (1/det) [ 1+f_v^2  -f_u*f_v ]
//       [ f_u*f_v   1+f_v^2 ]                     [ -f_u*f_v  1+f_u^2 ]
//   det(g) = 1 + f_u^2 + f_v^2
//
// For the FLAT (isotropic constant sigma) approximation implemented here:
//   sigma_u = sigma_v = sigma_base
// This is exact on a flat surface and approximate on curved surfaces.
// The curvature-corrected version (sigma ~ 1/sqrt(det g)) is implemented
// via the CurvatureAdaptiveBrownian variant (future step).
//
// Drift term: optional gradient drift that biases diffusion uphill or downhill.
//   mu = drift_strength * grad(f) / (|grad(f)| + eps)
// Set drift_strength = 0 for pure isotropic Brownian motion.

#include "sim/IEquation.hpp"
#include "numeric/ops.hpp"
#include <glm/glm.hpp>

namespace ndde::sim {

class BrownianMotion final : public IEquation {
public:
    struct Params {
        float sigma          = 0.4f;   ///< isotropic diffusion coefficient
        float drift_strength = 0.f;    ///< signed gradient drift (+ = uphill, - = downhill)
    };

    BrownianMotion() = default;
    explicit BrownianMotion(Params p) : m_p(p) {}

    // Drift: zero for pure Brownian motion, or gradient-aligned bias.
    [[nodiscard]] glm::vec2 update(
        ParticleState&              state,
        const ndde::math::ISurface& surface,
        float                       t) const override
    {
        (void)state; (void)t;
        if (ops::abs(m_p.drift_strength) < 1e-7f)
            return {0.f, 0.f};

        // Gradient direction in parameter space (z-components of tangent vectors)
        const glm::vec3 du_vec = surface.du(state.uv.x, state.uv.y);
        const glm::vec3 dv_vec = surface.dv(state.uv.x, state.uv.y);
        const float fu = du_vec.z;
        const float fv = dv_vec.z;
        const float gn = ops::sqrt(fu*fu + fv*fv) + 1e-7f;
        return { m_p.drift_strength * fu / gn,
                 m_p.drift_strength * fv / gn };
    }

    // Diffusion coefficient: per-axis sigma for dX = mu*dt + sigma*dW.
    [[nodiscard]] glm::vec2 noise_coefficient(
        const ParticleState&        /*state*/,
        const ndde::math::ISurface& /*surface*/,
        float                       /*t*/) const override
    {
        return { m_p.sigma, m_p.sigma };
    }

    [[nodiscard]] bool has_constant_noise() const override { return true; }

    // No phase accumulation for Brownian motion.
    [[nodiscard]] float phase_rate() const override { return 0.f; }

    [[nodiscard]] std::string name() const override { return "BrownianMotion"; }

    Params&       params()       noexcept { return m_p; }
    const Params& params() const noexcept { return m_p; }

private:
    Params m_p;
};

} // namespace ndde::sim
