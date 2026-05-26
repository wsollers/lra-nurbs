#pragma once
// simulation/fields/IField.hpp
// A Field is a composable environmental effect on the manifold.
// Every effect type is a section of a bundle over the surface.
//
//   Drift field      → section of T(surface)  — adds to (du/dt, dv/dt)
//   Damping field    → endomorphism of T       — scales existing velocity
//   Potential field  → section of R (scalar)  — gradient gives drift
//   Metric ripple    → conformal factor (1+εh) — warps geodesic distances
//
// FieldCompositor accumulates all active fields for a tick.

#include "math/Scalars.hpp"
#include "sim/IEquation.hpp"
#include "math/Surfaces.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ndde::simulation {

class IField {
public:
    virtual ~IField() = default;

    // Drift contribution in parameter space — added to agent's own drift.
    // Default: zero (pure metric/diffusion fields override only metric_factor).
    [[nodiscard]] virtual glm::vec2 drift_contribution(
        const sim::ParticleState& state,
        const math::ISurface&     surface,
        f32                       t) const { (void)state; (void)surface; (void)t; return {}; }

    // Diffusion scaling contribution — multiplied into agent's sigma.
    [[nodiscard]] virtual glm::vec2 diffusion_contribution(
        const sim::ParticleState& state,
        const math::ISurface&     surface,
        f32                       t) const { (void)state; (void)surface; (void)t; return {f32(1), f32(1)}; }

    // Conformal metric factor at (u, v, t).
    // g(x,t) = metric_factor(u,v,t) * g0.
    // Returns 1.f for fields that don't deform the metric.
    [[nodiscard]] virtual f32 metric_factor(f32 u, f32 v, f32 t) const {
        (void)u; (void)v; (void)t; return f32(1);
    }

    // Visual surface displacement along the local rendered height axis.
    // Default: zero for fields that do not deform rendered geometry.
    [[nodiscard]] virtual f32 surface_displacement(f32 u, f32 v, f32 t) const {
        (void)u; (void)v; (void)t; return f32(0);
    }

    // Return false when this field has fully decayed and should be removed.
    [[nodiscard]] virtual bool active(f32 t) const { (void)t; return true; }

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

protected:
    IField() = default;
    IField(const IField&) = default;
    IField& operator=(const IField&) = default;
    IField(IField&&) = default;
    IField& operator=(IField&&) = default;
};

// ── FieldCompositor ───────────────────────────────────────────────────────────

class FieldCompositor {
public:
    void add(std::shared_ptr<IField> field);
    void remove(std::string_view name);
    void clear();

    // Accumulated drift from all active fields.
    [[nodiscard]] glm::vec2 total_drift(
        const sim::ParticleState& state,
        const math::ISurface&     surface,
        f32                       t) const;

    // Net conformal factor: product of all active metric_factor() values.
    [[nodiscard]] f32 metric_factor(f32 u, f32 v, f32 t) const;

    // Net diffusion scale: componentwise product of all active diffusion factors.
    [[nodiscard]] glm::vec2 diffusion_factor(
        const sim::ParticleState& state,
        const math::ISurface&     surface,
        f32                       t) const;

    // Sum of active fields' rendered surface displacement.
    [[nodiscard]] f32 surface_displacement(f32 u, f32 v, f32 t) const;

    // Remove fields where active(t) == false. Returns names of removed fields.
    std::vector<std::string> sweep_decayed(f32 t);

    [[nodiscard]] std::size_t size()   const noexcept { return m_fields.size(); }
    [[nodiscard]] bool        empty()  const noexcept { return m_fields.empty(); }
    [[nodiscard]] const std::vector<std::shared_ptr<IField>>& fields() const noexcept { return m_fields; }

private:
    std::vector<std::shared_ptr<IField>> m_fields;
};

} // namespace ndde::simulation
