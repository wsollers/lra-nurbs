#pragma once
// sim/GradientWalker.hpp
// GradientWalker: gradient-perpendicular steering law, implementing IEquation.
//
// Geometric intuition
// ───────────────────
// The particle steers perpendicular to the surface gradient in parameter space.
// On a height-field z = f(u,v), the gradient (df/du, df/dv) points toward the
// steepest ascent.  The perpendicular direction (-df/dv, df/du) / |grad f| runs
// along the level curves of f -- the particle follows the contours of the surface.
//
// A sinusoidal steering perturbation prevents the particle from locking onto a
// single contour.  Without it the trajectory degenerates into a closed orbit
// around a fixed level curve.  With it the particle drifts across contours,
// producing the complex space-filling paths visible in the UI.
//
// The heading is smoothed toward the desired direction by a turn-rate limiter
// (the clamp on da) which prevents abrupt direction reversals at walls and
// high-curvature gradient regions.
//
// On a general surface (not a height-field), du.z and dv.z are the z-components
// of the surface tangent vectors in the embedding -- still a geometrically
// meaningful slope indicator.  On the torus (du.z == dv.z == 0 everywhere),
// steering becomes purely perturbation-driven: the particle wanders freely.
//
// Step 5 change: velocity() now takes ParticleState& (mutable).
// The const_cast from Step 4 is eliminated.  The integrator (EulerIntegrator)
// owns the mutation contract -- it passes mutable state to the equation.

#include "sim/IEquation.hpp"
#include "numeric/ops.hpp"
#include <algorithm>

namespace ndde::sim {

class GradientWalker final : public IEquation {
public:
    // walk_speed  [param-units/s]: base speed magnitude of the velocity vector
    // steer_amp   [radians]:       amplitude of the sinusoidal heading offset
    // steer_freq  [rad/phase]:     frequency of the steering oscillation
    // turn_rate   [radians/step]:  maximum heading change per integrator call
    explicit GradientWalker(float walk_speed = 0.9f,
                            float steer_amp  = 0.55f,
                            float steer_freq = 0.4f,
                            float turn_rate  = 2.f)
        : m_walk_speed(walk_speed)
        , m_steer_amp(steer_amp)
        , m_steer_freq(steer_freq)
        , m_turn_rate(turn_rate)
    {}

    // ── IEquation::update (mutable state) ───────────────────────────────────
    // Computes (du/dt, dv/dt) and updates state.angle in place.
    // The angle update is intentionally stateful: it implements a first-order
    // low-pass filter (rate limiter) on the heading direction.  This requires
    // reading and writing state.angle across calls.
    // Step 5: state is now mutable -- no const_cast needed.
    [[nodiscard]] glm::vec2 update(
        ParticleState&              state,
        const ndde::math::ISurface& surface,
        float                       /*t*/) const override
    {
        // z-components of the surface tangent vectors as gradient proxies.
        // For p(u,v) = (u,v,f(u,v)): du.z = df/du, dv.z = df/dv exactly.
        // For the torus: du.z = dv.z = 0, so gn = eps and the particle
        // wanders under the perturbation alone.
        const glm::vec3 du_vec = surface.du(state.uv.x, state.uv.y);
        const glm::vec3 dv_vec = surface.dv(state.uv.x, state.uv.y);
        const float fx = du_vec.z;
        const float fy = dv_vec.z;
        const float gn = ops::sqrt(fx*fx + fy*fy) + 1e-5f;

        // Perpendicular-to-gradient in the (u,v) plane
        const float perp_x = -fy / gn;
        const float perp_y =  fx / gn;

        // Desired heading: gradient-perpendicular + sinusoidal perturbation
        const float steer   = m_steer_amp * ops::sin(state.phase * m_steer_freq);
        const float desired = ops::atan2(perp_y, perp_x) + steer;

        // Rate-limited heading update -- mutable, no const_cast (Step 5 fix)
        float da = desired - state.angle;
        while (da >  ops::pi_v<float>) da -= ops::two_pi_v<float>;
        while (da < -ops::pi_v<float>) da += ops::two_pi_v<float>;
        state.angle += ops::clamp(da, -m_turn_rate, m_turn_rate);

        return { m_walk_speed * ops::cos(state.angle),
                 m_walk_speed * ops::sin(state.angle) };
    }

    // Phase accumulates at phase_rate() * |vel| * dt per integrator step.
    [[nodiscard]] float phase_rate() const override { return 1.5f; }

    [[nodiscard]] std::string name() const override { return "GradientWalker"; }

    float walk_speed() const noexcept { return m_walk_speed; }
    float steer_amp()  const noexcept { return m_steer_amp;  }
    float steer_freq() const noexcept { return m_steer_freq; }
    float turn_rate()  const noexcept { return m_turn_rate;  }

private:
    float m_walk_speed;
    float m_steer_amp;
    float m_steer_freq;
    float m_turn_rate;
};

} // namespace ndde::sim
