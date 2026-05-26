#pragma once
// sim/MomentumBearingEquation.hpp
// MomentumBearingEquation: Strategy C pursuit.
//
// The pursuer infers the leader's goal direction from a moving average of the
// leader's recent velocity (position difference over a time window).
//
// velocity() = normalise(leader_pos(t) - leader_pos(t - window_sec)) * speed
//
// Periodic shortest-path wrapping is applied when computing the displacement
// vector, so the strategy is correct on the torus.
//
// When window_sec of history is not yet available, the pursuer uses whatever
// displacement is recorded (oldest available record).
//
// window_sec acts as a low-pass filter on the leader's velocity:
//   - Longer window  -> smoother direction estimate, slower response to goal flips
//   - Shorter window -> noisier but more responsive
//
// This is Strategy C from docs/ctrl_a_leader_seeker.md.

#include "sim/IEquation.hpp"
#include "numeric/ops.hpp"
#include "sim/HistoryBuffer.hpp"
#include <glm/glm.hpp>
#include <cmath>
#include <string>

namespace ndde::sim {

class MomentumBearingEquation final : public IEquation {
public:
    struct Params {
        float pursuit_speed = 0.8f;
        float window_sec    = 1.5f;  ///< averaging window in seconds
        float noise_sigma   = 0.f;
    };

    // history: non-owning pointer to the leader's HistoryBuffer.
    //          Must remain valid for the lifetime of this equation.
    explicit MomentumBearingEquation(const HistoryBuffer* history)
        : MomentumBearingEquation(history, Params{})
    {}
    MomentumBearingEquation(const HistoryBuffer* history, Params p)
        : m_history(history), m_p(p)
    {}

    [[nodiscard]] glm::vec2 update(
        ParticleState&              /*state*/,
        const ndde::math::ISurface& surface,
        float                       t) const override
    {
        if (!m_history || m_history->empty())
            return {0.f, 0.f};

        // Query leader position at t and at (t - window_sec)
        const glm::vec2 pos_now  = m_history->query(t);
        const glm::vec2 pos_past = m_history->query(t - m_p.window_sec);

        glm::vec2 displacement = pos_now - pos_past;

        // Periodic shortest-path wrapping on the displacement vector
        if (surface.is_periodic_u()) {
            const float span = surface.u_max() - surface.u_min();
            if (displacement.x >  span * 0.5f) displacement.x -= span;
            if (displacement.x < -span * 0.5f) displacement.x += span;
        }
        if (surface.is_periodic_v()) {
            const float span = surface.v_max() - surface.v_min();
            if (displacement.y >  span * 0.5f) displacement.y -= span;
            if (displacement.y < -span * 0.5f) displacement.y += span;
        }

        const float len = ops::length(displacement);
        if (len < 1e-7f) return {0.f, 0.f};
        return (displacement / len) * m_p.pursuit_speed;
    }

    [[nodiscard]] glm::vec2 noise_coefficient(
        const ParticleState&        /*state*/,
        const ndde::math::ISurface& /*surface*/,
        float                       /*t*/) const override
    {
        return { m_p.noise_sigma, m_p.noise_sigma };
    }

    [[nodiscard]] float phase_rate() const override { return 0.f; }
    [[nodiscard]] std::string name() const override { return "MomentumBearing"; }

    Params& params() noexcept { return m_p; }

private:
    const HistoryBuffer* m_history;  // non-owning, points to leader's buffer
    Params               m_p;
};

} // namespace ndde::sim
