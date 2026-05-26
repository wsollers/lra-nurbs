#pragma once
// sim/IConstraint.hpp
// IConstraint: post-integration correction applied to a single ParticleState.
// IPairConstraint: correction applied to a pair of ParticleStates.
//
// Design
// ──────
// Constraints are the boundary policy of the simulation.  They are applied by
// AnimatedCurve::step() AFTER the integrator has proposed a new uv, BEFORE
// the trail point is pushed.
//
// IConstraint (per-particle):
//   Called once per integration sub-step for each particle.
//   Stored as vector<unique_ptr<IConstraint>> in AnimatedCurve.
//   Applied in insertion order.
//
// IPairConstraint (pairwise):
//   Called once per ordered pair (i, j), i < j, after all per-particle
//   IConstraint::apply() calls.
//   Stored by ParticleSystem as pair constraints.
//
// Contract for IConstraint::apply():
//   - MAY  modify state.uv freely.
//   - MUST NOT modify state.phase or state.angle
//     (those are integrator / equation responsibilities).
//
// Contract for IPairConstraint::apply():
//   - MAY modify a.uv and b.uv.
//   - MUST NOT modify phase or angle of either particle.

#include "sim/IEquation.hpp"   // ParticleState
#include "math/Surfaces.hpp"   // ISurface
#include <string>

namespace ndde::sim {

// ── IConstraint ───────────────────────────────────────────────────────────────
class IConstraint {
public:
    virtual ~IConstraint() = default;

    // Apply the constraint: modify state.uv so the particle satisfies the
    // constraint after this call.
    virtual void apply(ParticleState&              state,
                       const ndde::math::ISurface& surface) const = 0;

    [[nodiscard]] virtual std::string name() const = 0;

protected:
    IConstraint() = default;
    IConstraint(const IConstraint&) = default;
    IConstraint& operator=(const IConstraint&) = default;
    IConstraint(IConstraint&&) = default;
    IConstraint& operator=(IConstraint&&) = default;
};

// ── IPairConstraint ───────────────────────────────────────────────────────────
// Post-integration correction applied to a pair of particles.
//
// Called by ParticleSystem AFTER all per-particle
// IConstraint::apply() calls, once per distinct ordered pair (i, j) with i < j.
// Both states may be modified.
class IPairConstraint {
public:
    virtual ~IPairConstraint() = default;

    virtual void apply(ParticleState&              a,
                       ParticleState&              b,
                       const ndde::math::ISurface& surface) const = 0;

    [[nodiscard]] virtual std::string name() const = 0;

protected:
    IPairConstraint() = default;
    IPairConstraint(const IPairConstraint&) = default;
    IPairConstraint& operator=(const IPairConstraint&) = default;
    IPairConstraint(IPairConstraint&&) = default;
    IPairConstraint& operator=(IPairConstraint&&) = default;
};

} // namespace ndde::sim
