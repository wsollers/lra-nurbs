#pragma once
// sim/IEquation.hpp
// Equation interface: the deterministic velocity field driving a particle.
//
// Design
// ──────
// An equation provides (du/dt, dv/dt) in parameter space given the current
// particle state and the surface it lives on.  The integrator (Step 5) calls
// update() once per sub-step and uses the result to advance the state.
//
// Three equation families are planned:
//   ODE  -- update() is a deterministic function of state.
//            noise_coefficient() returns {0,0} (the default).
//
//   SDE  -- update() is the drift term f(x,t).
//            noise_coefficient() is the diffusion coefficient g(x).
//            The integrator handles the Wiener increment dW.
//            Example: BrownianMotion (Step 9).
//
//   DDE  -- update() reads a history buffer at time t - tau.
//            Example: DelayPursuit (Step 10).
//
// Geometric note
// ──────────────
// velocity() returns a 2-vector in PARAMETER space, not world space.
// The unit is (parameter-units) per second.  For the Gaussian height-field
// where (u,v) = (x,y), this is metres/second in the XY plane.  For the
// torus (u,v) are angles: velocity (1,0) is 1 radian/second in longitude.
//
// The ISurface& is provided so equations that depend on local geometry
// (gradient-following, normal-curvature steering, geodesic pursuit) can
// query du/dv/unit_normal/gaussian_curvature at the current position.

#include "math/Surfaces.hpp"   // ISurface
#include "math/Scalars.hpp"    // f32
#include <glm/glm.hpp>
#include <string>

namespace ndde::sim {

// ── ParticleState ─────────────────────────────────────────────────────────────
// The complete integration state of one particle, in parameter space.
// This is the minimal state needed by any IEquation implementation.
// AnimatedCurve::WalkState maps directly onto this struct (same layout).

struct ParticleState {
    glm::vec2 uv    = {0.f, 0.f};  ///< (u,v) position in parameter space
    float     phase = 0.f;          ///< steering oscillation accumulator
    float     angle = 0.f;          ///< current heading angle (radians)
};

// ── IEquation ─────────────────────────────────────────────────────────────────
// Velocity field for a particle on a parametric surface.
// The integrator calls update() each sub-step and Euler-advances the state.
// For SDEs the integrator also calls noise_coefficient() and generates dW.

class IEquation {
public:
    virtual ~IEquation() = default;

    // Deterministic velocity field: returns (du/dt, dv/dt) in parameter space.
    // For SDEs this is the drift term; for ODEs it is the complete velocity.
    //
    // state is mutable so stateful equations (e.g. GradientWalker's turn-rate
    // limiter) can update state.angle without a const_cast.  The integrator
    // is the only caller and it owns the ParticleState.
    [[nodiscard]] virtual glm::vec2 update(
        ParticleState&              state,
        const ndde::math::ISurface& surface,
        float                       t) const = 0;

    // Diffusion coefficient for SDEs: returns per-axis g(x) for dX = update()*dt + g*dW.
    // Default: {0,0} -- no noise, pure ODE.
    [[nodiscard]] virtual glm::vec2 noise_coefficient(
        const ParticleState&        /*state*/,
        const ndde::math::ISurface& /*surface*/,
        float                       /*t*/) const
    {
        return {0.f, 0.f};
    }

    // True when noise_coefficient is independent of state and time. Integrators
    // can use this to skip finite-difference diffusion-gradient work.
    [[nodiscard]] virtual bool has_constant_noise() const { return false; }

    // Rate at which the particle's phase accumulates per unit parameter-space
    // distance travelled.  Used by equations with a steering oscillation.
    // Default: 0.f -- no phase.
    [[nodiscard]] virtual float phase_rate() const { return 0.f; }

    [[nodiscard]] virtual std::string name() const = 0;

protected:
    IEquation() = default;
    IEquation(const IEquation&) = default;
    IEquation& operator=(const IEquation&) = default;
    IEquation(IEquation&&) = default;
    IEquation& operator=(IEquation&&) = default;
};

} // namespace ndde::sim
