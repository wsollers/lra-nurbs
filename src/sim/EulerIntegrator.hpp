#pragma once
// sim/EulerIntegrator.hpp
// Explicit (forward) Euler integrator for ODEs and SDE drift terms.
//
// Algorithm
// ─────────
// Given state x_n and drift f(x,t):
//   x_{n+1} = x_n + f(x_n, t) * dt
//
// For the phase accumulator:
//   phase_{n+1} = phase_n + equation.phase_rate() * |f(x_n,t)| * dt
//
// This is first-order accurate in dt.  The error per step is O(dt^2); the
// global error over a fixed interval [0,T] is O(dt).  For the particle walker
// -- which is a qualitative visualisation tool rather than a high-precision
// simulation -- first-order accuracy is sufficient.  Higher-order methods
// (RK4, Milstein for SDEs) are planned for Step 9.
//
// Note on angle mutation
// ──────────────────────
// GradientWalker needs to persist state.angle between steps (its turn-rate
// limiter is stateful).  Because step() now takes a mutable ParticleState&,
// the equation can freely update state.angle without a const_cast.
// The NOTE in GradientWalker.hpp is therefore resolved here -- the const_cast
// is eliminated in this step.

#include "sim/IIntegrator.hpp"
#include "numeric/ops.hpp"
#include <glm/glm.hpp>

namespace ndde::sim {

class EulerIntegrator final : public IIntegrator {
public:
    // step(): explicit Euler advance.
    // Calls equation.update(state, surface, t) with mutable state so
    // the equation can update state.angle freely (no const_cast needed).
    void step(ParticleState&              state,
              IEquation&                  equation,
              const ndde::math::ISurface& surface,
              float                       t,
              float                       dt) const override
    {
        // Velocity in parameter space: (du/dt, dv/dt)
        const glm::vec2 vel = equation.update(state, surface, t);

        // Euler position update
        state.uv += vel * dt;

        // Phase accumulates proportional to arc-length speed.
        // phase_rate() is 0 for equations that don't use a steering phase.
        state.phase += equation.phase_rate() * ops::length(vel) * dt;
    }

    [[nodiscard]] std::string name() const override { return "EulerIntegrator"; }
};

} // namespace ndde::sim
