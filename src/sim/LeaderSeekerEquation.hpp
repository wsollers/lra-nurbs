#pragma once
// sim/LeaderSeekerEquation.hpp
// LeaderSeekerEquation: deterministic state-machine equation for a leader
// particle that navigates between the global maximum and minimum of a surface.
//
// State machine
// ─────────────
// Goal is a two-state enum: SeekMax <-> SeekMin.
// Starts in SeekMax.  Transitions occur when:
//   (a) the particle enters the arrival neighbourhood of the current goal, OR
//   (b) the local gradient magnitude is within epsilon of target_grad_magnitude
//       (default 0.0 = flat, meaning the particle has reached the extremum).
//
// velocity() logic
// ─────────────────
// 1. Query ExtremumTable -> goal_uv (max_uv if SeekMax, min_uv if SeekMin).
// 2. delta = goal_uv - state.uv  (with periodic shortest-path wrapping).
// 3. If dist < arrival_radius: flip goal, return {0,0}.
// 4. Compute gradient magnitude at state.uv.
// 5. If |grad_mag - target_grad_magnitude| < epsilon: flip goal.
// 6. Return (delta / dist) * pursuit_speed.
//
// noise_coefficient() returns {noise_sigma, noise_sigma} so the
// MilsteinIntegrator adds Brownian kicks that prevent ridge-lock.
//
// m_goal is mutable -- the equation is const-correct at the IEquation
// interface level, but goal-switching state must persist across velocity() calls.

#include "sim/IEquation.hpp"
#include "numeric/ops.hpp"
#include "math/ExtremumTable.hpp"
#include "numeric/ops.hpp"
#include <glm/glm.hpp>
#include <cmath>
#include <string>

namespace ndde::sim {

enum class Goal : u8 { SeekMax = 0, SeekMin = 1 };

class LeaderSeekerEquation final : public IEquation {
public:
    struct Params {
        float target_grad_magnitude = 0.f;  ///< gradient magnitude that triggers flip (0 = flat)
        float epsilon               = 0.1f; ///< half-width of the flatness band
        float pursuit_speed         = 0.8f; ///< approach speed (param-units/s)
        float noise_sigma           = 0.15f;///< Brownian perturbation coefficient
        float arrival_radius        = 0.4f; ///< neighbourhood radius for goal flip
    };

    explicit LeaderSeekerEquation(const ndde::math::ExtremumTable* table)
        : LeaderSeekerEquation(table, Params{})
    {}
    LeaderSeekerEquation(const ndde::math::ExtremumTable* table,
                         Params p)
        : m_table(table), m_p(p)
    {}

    [[nodiscard]] glm::vec2 update(
        ParticleState&              state,
        const ndde::math::ISurface& surface,
        float                       /*t*/) const override
    {
        if (!m_table || !m_table->valid)
            return {0.f, 0.f};

        // 1. Goal UV
        const glm::vec2 goal_uv = (m_goal == Goal::SeekMax)
            ? m_table->max_uv : m_table->min_uv;

        // 2. Delta with periodic shortest-path wrapping
        glm::vec2 delta = goal_uv - state.uv;
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

        // 3. Arrival neighbourhood: flip goal
        if (dist < m_p.arrival_radius) {
            m_goal = (m_goal == Goal::SeekMax) ? Goal::SeekMin : Goal::SeekMax;
            return {0.f, 0.f};
        }

        // 4-5. Gradient flatness check: flip goal
        const Vec3 du_v = surface.du(state.uv.x, state.uv.y);
        const Vec3 dv_v = surface.dv(state.uv.x, state.uv.y);
        const float grad_mag = ops::sqrt(du_v.z*du_v.z + dv_v.z*dv_v.z);
        if (ops::abs(grad_mag - m_p.target_grad_magnitude) < m_p.epsilon)
            m_goal = (m_goal == Goal::SeekMax) ? Goal::SeekMin : Goal::SeekMax;

        // 6. Bearing: unit vector toward goal
        if (dist < 1e-7f) return {0.f, 0.f};
        return (delta / dist) * m_p.pursuit_speed;
    }

    [[nodiscard]] glm::vec2 noise_coefficient(
        const ParticleState&        /*state*/,
        const ndde::math::ISurface& /*surface*/,
        float                       /*t*/) const override
    {
        return { m_p.noise_sigma, m_p.noise_sigma };
    }

    [[nodiscard]] float phase_rate() const override { return 0.f; }
    [[nodiscard]] std::string name() const override { return "LeaderSeeker"; }

    Params&       params()       noexcept { return m_p; }
    const Params& params() const noexcept { return m_p; }
    Goal          goal()   const noexcept { return m_goal; }

private:
    const ndde::math::ExtremumTable* m_table;  // non-owning, stable scene address
    Params                           m_p;
    mutable Goal                     m_goal = Goal::SeekMax;
};

} // namespace ndde::sim
