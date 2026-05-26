#pragma once
// sim/DelayPursuitEquation.hpp
// DelayPursuitEquation: a DDE-driven pursuit law on a parametric surface.
//
// Mathematical formulation
// ────────────────────────
// Let X_c(t) in R^2 be the chaser's parameter-space position.
// Let X_l(s) in R^2, s in R, be the leader's position history.
//
// The DELAY DIFFERENTIAL EQUATION governing the chaser is:
//
//   dX_c/dt = v_s * (X_l(t - tau) - X_c(t)) / |X_l(t - tau) - X_c(t)|
//            + sigma * dW/dt          [optional noise term]
//
// where:
//   tau      = delay (seconds): the chaser targets where the leader WAS
//   v_s      = pursuit speed (param-units / second)
//   sigma    = noise diffusion coefficient (0 = pure deterministic pursuit)
//   X_l(t - tau) is recovered from a HistoryBuffer via linear interpolation
//
// Geometric handling of periodicity
// ──────────────────────────────────
// On a periodic surface (torus), the shortest path from X_c to X_l is NOT
// simply X_l - X_c -- we must choose the shortest wrapped difference in
// each coordinate:
//
//   delta_u = X_l.u - X_c.u
//   if |delta_u| > span_u / 2: delta_u -= sign(delta_u) * span_u
//   (similarly for v)
//
// This ensures the chaser always takes the shortest path around the tube,
// rather than the long way around.
//
// For non-periodic surfaces (Gaussian, Paraboloid) the standard difference
// is used -- the sign of the boundary wrapping check handles reflection.
//
// DDE vs ODE: the key difference
// ────────────────────────────────
// An ODE velocity field depends on the CURRENT state: f(X(t), t).
// A DDE velocity field depends on a PAST state: f(X(t), X(t-tau), t).
// This creates a functional differential equation -- the initial condition
// is a FUNCTION on [-tau, 0] (the initial history), not just a point.
// In our discrete simulation, the HistoryBuffer provides this initial history
// once enough leader steps have been recorded.
//
// Before the buffer has enough history (newest_t - oldest_t < tau), the
// chaser uses the oldest available record as a target (cold-start clamping).
//
// Phase: unused (phase_rate = 0).
// The integrator handles noise via noise_coefficient() if sigma > 0.
// Recommended integrator: MilsteinIntegrator (reduces to Euler-Maruyama for
// constant sigma, which is the default here).

#include "sim/IEquation.hpp"
#include "numeric/ops.hpp"
#include "sim/HistoryBuffer.hpp"
#include <glm/glm.hpp>
#include <cmath>

namespace ndde::sim {

class DelayPursuitEquation final : public IEquation {
public:
    struct Params {
        float tau           = 1.5f;  ///< delay (seconds)
        float pursuit_speed = 0.8f;  ///< max approach speed (param-units/s)
        float noise_sigma   = 0.0f;  ///< isotropic noise (0 = deterministic)
    };

    // history: non-owning pointer to the leader's HistoryBuffer.
    //          Must remain valid for the lifetime of this equation.
    // surface: used for periodicity queries (is_periodic_u/v, u_min/max etc.)
    //          NOTE: the surface pointer is stored at construction time.
    //          If the surface changes (swap_surface), this equation becomes stale.
    //          The scene is responsible for respawning the chaser after a swap.
    explicit DelayPursuitEquation(const HistoryBuffer*         history,
                                  const ndde::math::ISurface*  surface,
                                  Params                        p = {})
        : m_history(history), m_surface(surface), m_p(p)
    {}

    // ── IEquation::update ───────────────────────────────────────────────────
    // Deterministic pursuit toward X_l(t - tau).
    [[nodiscard]] glm::vec2 update(
        ParticleState&              state,
        const ndde::math::ISurface& /*surface*/,   // surface context unused here
        float                       t) const override
    {
        if (!m_history || m_history->empty())
            return {0.f, 0.f};

        // Query the leader's position at (t - tau)
        const float t_past = t - m_p.tau;
        const glm::vec2 target = m_history->query(t_past);

        // Direction from chaser to target, with periodic shortest-path correction
        glm::vec2 delta = target - state.uv;

        if (m_surface) {
            // Wrap delta into the shortest path for each periodic axis
            if (m_surface->is_periodic_u()) {
                const float span_u = m_surface->u_max() - m_surface->u_min();
                if (delta.x >  span_u * 0.5f) delta.x -= span_u;
                if (delta.x < -span_u * 0.5f) delta.x += span_u;
            }
            if (m_surface->is_periodic_v()) {
                const float span_v = m_surface->v_max() - m_surface->v_min();
                if (delta.y >  span_v * 0.5f) delta.y -= span_v;
                if (delta.y < -span_v * 0.5f) delta.y += span_v;
            }
        }

        const float dist = ops::length(delta);
        if (dist < 1e-7f) return {0.f, 0.f};

        // Pure pursuit: unit vector toward target, scaled by pursuit speed
        return (delta / dist) * m_p.pursuit_speed;
    }

    // Optional isotropic noise on top of the deterministic pursuit
    [[nodiscard]] glm::vec2 noise_coefficient(
        const ParticleState&        /*state*/,
        const ndde::math::ISurface& /*surface*/,
        float                       /*t*/) const override
    {
        return { m_p.noise_sigma, m_p.noise_sigma };
    }

    [[nodiscard]] float phase_rate() const override { return 0.f; }

    [[nodiscard]] std::string name() const override { return "DelayPursuit"; }

    Params&       params()       noexcept { return m_p; }
    const Params& params() const noexcept { return m_p; }

private:
    const HistoryBuffer*        m_history;  // non-owning, points to leader's buffer
    const ndde::math::ISurface* m_surface;  // non-owning, for periodicity queries
    Params m_p;
};

} // namespace ndde::sim
