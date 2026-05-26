#pragma once
// app/SpawnStrategy.hpp
// Lightweight legacy spawn helpers kept for older AnimatedCurve-based paths.
//
// All functions are header-only (short, inline).  No .cpp needed.
//
// Usage pattern:
//   const spawn::SpawnContext ctx{ m_surface.get(), &m_equation,
//                                  &m_integrator, &m_milstein, m_sim_speed };
//   const glm::vec2 ref = spawn::reference_uv(m_curves, *m_surface);
//   const glm::vec2 uv  = spawn::offset_spawn(ref, 1.5f, angle, *m_surface);
//   m_curves.push_back(spawn::spawn_shared(uv, Role::Leader, slot, ctx));
//
// Pre-warm convention:
//   Leader / Chaser:       prewarm = true  (60 frames default)
//   Delay-pursuit chaser:  prewarm = false (must wait for leader history)

#include "app/AnimatedCurve.hpp"
#include "numeric/ops.hpp"
#include "math/Surfaces.hpp"
#include "memory/Containers.hpp"
#include "sim/IEquation.hpp"
#include "sim/IIntegrator.hpp"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

namespace ndde::spawn {

// ── SpawnContext ──────────────────────────────────────────────────────────────
// Aggregates the scene-owned pointers that every spawn function needs.
// All pointers are non-owning; the scene outlives any spawned particle.

struct SpawnContext {
    const ndde::math::ISurface*    surface;            ///< active surface
    ndde::sim::IEquation*           shared_equation;    ///< for Leader/Chaser
    const ndde::sim::IIntegrator*   shared_integrator;  ///< Euler (deterministic)
    const ndde::sim::IIntegrator*   milstein;           ///< for SDE equations
    f32 sim_speed    = 1.f;
    int   prewarm_frames = 60;
};

// ── reference_uv ─────────────────────────────────────────────────────────────
// Return the parameter-space position of curve 0, or the domain centre if
// there are no curves yet.  Used as the anchor for offset_spawn().

[[nodiscard]] inline glm::vec2 reference_uv(
    const memory::SimVector<AnimatedCurve>& curves,
    const ndde::math::ISurface&       surface) noexcept
{
    if (curves.empty())
        return { (surface.u_min() + surface.u_max()) * 0.5f,
                 (surface.v_min() + surface.v_max()) * 0.5f };
    return curves[0].head_uv();
}

// ── offset_spawn ─────────────────────────────────────────────────────────────
// Return a spawn position offset from ref_uv by (radius, angle), clamped to
// the surface domain with a 0.5-unit margin.

[[nodiscard]] inline glm::vec2 offset_spawn(
    glm::vec2                   ref_uv,
    f32                       offset_radius,
    f32                       angle,
    const ndde::math::ISurface& surface) noexcept
{
    constexpr f32 margin = 0.5f;
    return {
        std::clamp(ref_uv.x + offset_radius * ops::cos(angle),
                   surface.u_min() + margin, surface.u_max() - margin),
        std::clamp(ref_uv.y + offset_radius * ops::sin(angle),
                   surface.v_min() + margin, surface.v_max() - margin)
    };
}

// ── spawn_shared ─────────────────────────────────────────────────────────────
// Construct a particle using the scene's shared equation + integrator.
// Used for Leader and plain Chaser particles.

[[nodiscard]] inline AnimatedCurve spawn_shared(
    glm::vec2              uv,
    AnimatedCurve::Role    role,
    u32                    slot,
    const SpawnContext&    ctx)
{
    AnimatedCurve c(uv.x, uv.y, role, slot,
                    ctx.surface, ctx.shared_equation, ctx.shared_integrator);
    for (int i = 0; i < ctx.prewarm_frames; ++i)
        c.advance(1.f/60.f, ctx.sim_speed);
    return c;
}

// ── spawn_owned ──────────────────────────────────────────────────────────────
// Construct a particle that OWNS its equation (BrownianMotion, DelayPursuit,
// etc.).  Uses ctx.milstein as the integrator.
//
// prewarm: set false for delay-pursuit chasers that must wait for leader history.

[[nodiscard]] inline AnimatedCurve spawn_owned(
    glm::vec2                                uv,
    AnimatedCurve::Role                      role,
    u32                                      slot,
    memory::Unique<ndde::sim::IEquation>     eq,
    const SpawnContext&                      ctx,
    bool                                     prewarm = true)
{
    AnimatedCurve c = AnimatedCurve::with_equation(
        uv.x, uv.y, role, slot,
        ctx.surface, std::move(eq), ctx.milstein);
    if (prewarm)
        for (int i = 0; i < ctx.prewarm_frames; ++i)
            c.advance(1.f/60.f, ctx.sim_speed);
    return c;
}

} // namespace ndde::spawn
