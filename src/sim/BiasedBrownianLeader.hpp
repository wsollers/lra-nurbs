#pragma once
// sim/BiasedBrownianLeader.hpp
// BiasedBrownianLeader: stochastic SDE leader with goal-directed drift.
//
// Itô SDE:  dX_t = mu(X_t) dt + sigma dW_t
//
// where:
//   mu(X)  = drift_strength * (goal_uv - X.uv) / |goal_uv - X.uv|
//              (straight-line drift in parameter space toward the goal)
//   sigma   = isotropic diffusion coefficient  (noise_coefficient returns this)
//   dW_t   = 2D Wiener increment (handled by MilsteinIntegrator)
//
// Optional gradient_drift adds a component along the surface gradient
// (same as BrownianMotion::drift_strength), creating competition between
// local slope information and global goal knowledge.
//
// Goal-switching (identical to LeaderSeekerEquation):
//   - Arrival neighbourhood: |state.uv - goal_uv| < arrival_radius
//   - Gradient flatness:     |grad_mag - target_grad_magnitude| < epsilon
//
// Use MilsteinIntegrator (not EulerIntegrator).  For constant sigma the
// Milstein correction is zero (reduces to Euler-Maruyama), but using
// Milstein from the start avoids an integrator swap when curvature-adaptive
// noise (sigma ~ 1/sqrt(K)) is added later.
//
// m_goal is mutable -- const-correct at the IEquation interface level, but
// goal-switching state must persist across velocity() calls.

#include "sim/IEquation.hpp"
#include "numeric/ops.hpp"
#include "math/ExtremumTable.hpp"
#include "sim/LeaderSeekerEquation.hpp"   // Goal enum
#include <glm/glm.hpp>
#include <cmath>
#include <string>

namespace ndde::sim {

class BiasedBrownianLeader final : public IEquation {
public:
    struct Params {
        float sigma                 = 0.3f;  ///< diffusion coefficient
        float drift_strength        = 0.6f;  ///< goal-directed drift magnitude (param-units/s)
        float gradient_drift        = 0.0f;  ///< optional gradient-following drift (+uphill/-downhill)
        float target_grad_magnitude = 0.f;   ///< gradient magnitude that triggers goal flip
        float epsilon               = 0.1f;  ///< half-width of the flatness band
        float arrival_radius        = 0.4f;  ///< neighbourhood radius for goal flip
    };

    explicit BiasedBrownianLeader(const ndde::math::ExtremumTable* table)
        : BiasedBrownianLeader(table, Params{})
    {}
    BiasedBrownianLeader(const ndde::math::ExtremumTable* table,
                         Params p)
        : m_table(table), m_p(p)
    {}

    // update() implementation from design doc (ctrl_a_leader_seeker.md),
    // copied verbatim.
    [[nodiscard]] glm::vec2 update(
        ParticleState&              state,
        const ndde::math::ISurface& surface,
        float                       /*t*/) const override
    {
        if (!m_table || !m_table->valid)
            return {0.f, 0.f};

        const glm::vec2 goal_uv = (m_goal == Goal::SeekMax)
            ? m_table->max_uv : m_table->min_uv;

        glm::vec2 delta = goal_uv - state.uv;

        // Shortest-path wrap for periodic surfaces (torus)
        if (surface.is_periodic_u()) {
            const float span = surface.u_max() - surface.u_min();
            if (delta.x >  span * 0.5f) delta.x -= span;
            if (delta.x < -span * 0.5f) delta.x += span;
        }
        if (surface.is_periodic_v()) {
            const float span = surface.v_max() - surface.v_min();
            if (delta.y >  span * 0.5f) delta.y -= span;
            if (delta.y < -span * 0.5f) delta.y += span;
        }

        const float dist = ops::length(delta);

        // Goal flip: arrival neighbourhood
        if (dist < m_p.arrival_radius) {
            m_goal = (m_goal == Goal::SeekMax) ? Goal::SeekMin : Goal::SeekMax;
            return {0.f, 0.f};  // pause for one step at the goal
        }

        // Goal flip: gradient flatness
        const Vec3 du_v = surface.du(state.uv.x, state.uv.y);
        const Vec3 dv_v = surface.dv(state.uv.x, state.uv.y);
        const float grad_mag = ops::sqrt(du_v.z*du_v.z + dv_v.z*dv_v.z);
        if (ops::abs(grad_mag - m_p.target_grad_magnitude) < m_p.epsilon)
            m_goal = (m_goal == Goal::SeekMax) ? Goal::SeekMin : Goal::SeekMax;

        // Goal-directed drift: unit vector toward goal, scaled by drift_strength
        if (dist < 1e-7f) return {0.f, 0.f};
        glm::vec2 mu = (delta / dist) * m_p.drift_strength;

        // Optional gradient drift (same as BrownianMotion::drift_strength)
        if (ops::abs(m_p.gradient_drift) > 1e-7f) {
            const float fu = du_v.z;
            const float fv = dv_v.z;
            const float gn = ops::sqrt(fu*fu + fv*fv) + 1e-7f;
            mu += glm::vec2{ m_p.gradient_drift * fu / gn,
                             m_p.gradient_drift * fv / gn };
        }

        return mu;
    }

    [[nodiscard]] glm::vec2 noise_coefficient(
        const ParticleState&        /*state*/,
        const ndde::math::ISurface& /*surface*/,
        float                       /*t*/) const override
    {
        return { m_p.sigma, m_p.sigma };
    }

    [[nodiscard]] float phase_rate() const override { return 0.f; }
    [[nodiscard]] std::string name() const override { return "BiasedBrownianLeader"; }

    Params&       params()       noexcept { return m_p; }
    const Params& params() const noexcept { return m_p; }
    Goal          goal()   const noexcept { return m_goal; }

private:
    const ndde::math::ExtremumTable* m_table;  // non-owning, stable scene address
    Params                           m_p;
    mutable Goal                     m_goal = Goal::SeekMax;
};

} // namespace ndde::sim
