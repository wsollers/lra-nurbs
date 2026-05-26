#pragma once
// sim/DirectPursuitEquation.hpp
// DirectPursuitEquation: Strategy A pursuit.
//
// The pursuer always knows the leader's CURRENT parameter-space position.
// velocity() = (leader_uv - state.uv) / |delta| * pursuit_speed
// Periodic shortest-path wrapping applied on periodic axes.
//
// The leader position is obtained via a std::function<glm::vec2()> callback,
// so the pursuer stays decoupled from the AnimatedCurve API.
// In practice: [this]{ return m_curves[0].head_uv(); }

#include "sim/IEquation.hpp"
#include "numeric/ops.hpp"
#include <functional>
#include <glm/glm.hpp>
#include <cmath>
#include <string>

namespace ndde::sim {

class DirectPursuitEquation final : public IEquation {
public:
    struct Params {
        float pursuit_speed = 0.8f;
        float noise_sigma   = 0.0f;
    };

    // leader_uv_fn: invoked each step to get the leader's current uv.
    explicit DirectPursuitEquation(std::function<glm::vec2()> leader_uv_fn)
        : DirectPursuitEquation(std::move(leader_uv_fn), Params{})
    {}
    DirectPursuitEquation(std::function<glm::vec2()> leader_uv_fn,
                          Params p)
        : m_leader_uv_fn(std::move(leader_uv_fn)), m_p(p)
    {}

    [[nodiscard]] glm::vec2 update(
        ParticleState&              state,
        const ndde::math::ISurface& surface,
        float                       /*t*/) const override
    {
        const glm::vec2 target = m_leader_uv_fn();
        glm::vec2 delta = target - state.uv;

        // Periodic shortest-path wrapping
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
    [[nodiscard]] std::string name() const override { return "DirectPursuit"; }

    Params& params() noexcept { return m_p; }

private:
    std::function<glm::vec2()> m_leader_uv_fn;
    Params m_p;
};

} // namespace ndde::sim
