#pragma once
// sim/IIntegrator.hpp
// Integrator interface: advances a particle's state by one time step.
//
// Design
// ──────
// An integrator is the numerical algorithm that converts a velocity field
// (IEquation) into a trajectory.  It owns the update rule for ParticleState.
//
// The integrator is separate from the equation for two reasons:
//
//   1. The same equation can be integrated with different algorithms.
//      GradientWalker is the equation; Euler, RK4, and Milstein are the
//      integrators.  Swapping the integrator changes accuracy and stability
//      without touching the physics.
//
//   2. The integrator is the only place that needs a *mutable* ParticleState.
//      IEquation::velocity() takes const& -- it is a pure mathematical
//      function.  The integrator applies the result and owns the mutation.
//      This removes the const_cast that lived in Step 4's GradientWalker.
//
// The step() method:
//   Advances state by dt using the equation's velocity field at time t.
//   Boundary handling is NOT the integrator's responsibility -- it is done
//   by AnimatedCurve::step() after calling the integrator.  This keeps the
//   integrator surface-agnostic (it knows nothing about periodicity or walls).
//
// The simulation loop calls:
//   integrator.step(state, equation, surface, t, dt)
// and then applies boundary correction to state.uv.

#include "sim/IEquation.hpp"   // ParticleState, IEquation
#include "math/Surfaces.hpp"   // ISurface

namespace ndde::sim {

class IIntegrator {
public:
    virtual ~IIntegrator() = default;

    // Advance state by dt seconds using the equation's velocity field.
    // Mutates state in place -- the caller owns the ParticleState.
    // t: current simulation time (passed through to equation.update)
    // dt: time step (seconds)
    virtual void step(ParticleState&              state,
                      IEquation&                  equation,
                      const ndde::math::ISurface& surface,
                      float                       t,
                      float                       dt) const = 0;

    [[nodiscard]] virtual std::string name() const = 0;

protected:
    IIntegrator() = default;
    IIntegrator(const IIntegrator&) = default;
    IIntegrator& operator=(const IIntegrator&) = default;
    IIntegrator(IIntegrator&&) = default;
    IIntegrator& operator=(IIntegrator&&) = default;
};

} // namespace ndde::sim
