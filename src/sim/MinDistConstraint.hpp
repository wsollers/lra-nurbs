#pragma once
// sim/MinDistConstraint.hpp
// MinDistConstraint: push two particles apart when they are closer than
// min_dist in parameter space.
//
// Collision resolution: elastic midpoint push.  Each particle is displaced
// half the overlap distance along the separation vector, conserving the
// centre of mass and maintaining stability for small min_dist values.
//
// Periodic wrapping: the shortest-path delta is used on periodic axes so
// the constraint is correct on the torus (no seam-crossing artefacts).
//
// min_dist units: parameter-space units (same as uv coordinates).
//   GaussianSurface (u,v) = (x,y) in world metres  -> min_dist in metres
//   Torus (u,v) in radians                          -> min_dist in radians

#include "sim/IConstraint.hpp"   // IPairConstraint
#include <glm/glm.hpp>
#include <cmath>

namespace ndde::sim {

class MinDistConstraint final : public IPairConstraint {
public:
    explicit MinDistConstraint(float min_dist = 0.3f) : m_min_dist(min_dist) {}

    void apply(ParticleState&              a,
               ParticleState&              b,
               const ndde::math::ISurface& surface) const override
    {
        glm::vec2 delta = b.uv - a.uv;

        // Shortest-path wrap for periodic surfaces
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
        if (dist >= m_min_dist || dist < 1e-7f) return;

        // Push each particle half the overlap distance along the separation axis
        const float     overlap = m_min_dist - dist;
        const glm::vec2 push    = (delta / dist) * (overlap * 0.5f);
        b.uv += push;
        a.uv -= push;
    }

    [[nodiscard]] std::string name() const override { return "MinDist"; }

    float min_dist() const noexcept { return m_min_dist; }
    void  set_min_dist(float d) noexcept { m_min_dist = d; }

private:
    float m_min_dist;
};

} // namespace ndde::sim
