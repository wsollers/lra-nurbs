#pragma once
// sim/DomainConfinement.hpp
// DomainConfinement: keep a particle within the surface domain.
//
// Periodic axes: wrap (modular arithmetic, open interval [u_min, u_max)).
// Non-periodic axes: clamp to an inner margin and flip state.angle.
//   This exactly replicates the hard-coded boundary logic that previously
//   lived in AnimatedCurve::step() (GaussianSurface.cpp).
//
// Margin: 0.3 parameter-space units on non-periodic axes.  The particle
// is stopped at (boundary + margin) and the heading angle is reflected.
//   - x-walls (u): angle -> pi - angle   (horizontal reflection)
//   - y-walls (v): angle -> -angle       (vertical reflection)
//
// This is correct for the GradientWalker equation whose m_walk.angle drives
// velocity.  For equations that do not use state.angle (BrownianMotion,
// DelayPursuitEquation) the angle modification is a no-op in practice
// because those equations ignore state.angle entirely.

#include "sim/IConstraint.hpp"
#include "numeric/ops.hpp"
#include <algorithm>

namespace ndde::sim {

class DomainConfinement final : public IConstraint {
public:
    void apply(ParticleState&              state,
               const ndde::math::ISurface& surface) const override
    {
        const float u0 = surface.u_min();
        const float u1 = surface.u_max();
        const float v0 = surface.v_min();
        const float v1 = surface.v_max();

        if (surface.is_periodic_u()) {
            const float span = u1 - u0;
            while (state.uv.x <  u0) state.uv.x += span;
            while (state.uv.x >= u1) state.uv.x -= span;
        } else {
            constexpr float margin = 0.3f;
            if (state.uv.x < u0 + margin) {
                state.uv.x   = u0 + margin;
                state.angle  = ops::pi_v<float> - state.angle;
            }
            if (state.uv.x > u1 - margin) {
                state.uv.x   = u1 - margin;
                state.angle  = ops::pi_v<float> - state.angle;
            }
        }

        if (surface.is_periodic_v()) {
            const float span = v1 - v0;
            while (state.uv.y <  v0) state.uv.y += span;
            while (state.uv.y >= v1) state.uv.y -= span;
        } else {
            constexpr float margin = 0.3f;
            if (state.uv.y < v0 + margin) {
                state.uv.y  = v0 + margin;
                state.angle = -state.angle;
            }
            if (state.uv.y > v1 - margin) {
                state.uv.y  = v1 - margin;
                state.angle = -state.angle;
            }
        }
    }

    [[nodiscard]] std::string name() const override { return "DomainConfinement"; }
};

} // namespace ndde::sim
