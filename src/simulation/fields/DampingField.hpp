#pragma once
// simulation/fields/DampingField.hpp
// Velocity damping: drift contribution = -gamma * current_velocity.
// This is a simple linear drag that slows all agents uniformly.
// gamma = 0 → no damping. gamma = 1 → full stop per second.

#include "simulation/fields/IField.hpp"

namespace ndde::simulation {

class DampingField final : public IField {
public:
    explicit DampingField(f32 gamma = f32(0.05)) : m_gamma(gamma) {}

    [[nodiscard]] glm::vec2 drift_contribution(
        const sim::ParticleState& state,
        const math::ISurface&,
        f32) const override {
        return -m_gamma * state.uv;   // proportional drag in param space
    }

    [[nodiscard]] std::string_view name() const noexcept override { return "DampingField"; }

    void set_gamma(f32 g) noexcept { m_gamma = g; }
    [[nodiscard]] f32 gamma() const noexcept { return m_gamma; }

private:
    f32 m_gamma;
};

} // namespace ndde::simulation
