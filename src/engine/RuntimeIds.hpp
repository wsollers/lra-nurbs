#pragma once
// engine/RuntimeIds.hpp
// Shared stable identifiers used by metadata, diagnostics, scenarios, and telemetry.

#include "math/Scalars.hpp"

#include <string_view>

namespace ndde {

struct ComponentId {
    std::string_view value{};

    friend constexpr bool operator==(ComponentId, ComponentId) noexcept = default;
};

struct RuntimeNodeId {
    u64 value = u64(0);

    friend constexpr bool operator==(RuntimeNodeId, RuntimeNodeId) noexcept = default;
};

struct StreamId {
    std::string_view value{};

    friend constexpr bool operator==(StreamId, StreamId) noexcept = default;
};

struct EventTypeId {
    std::string_view value{};

    friend constexpr bool operator==(EventTypeId, EventTypeId) noexcept = default;
};

enum class EventScope : u8 {
    App,
    Scenario,
    Simulation,
    View,
    Capture,
    Worker
};

struct ResourceId {
    u64 value = u64(0);

    friend constexpr bool operator==(ResourceId, ResourceId) noexcept = default;
};

struct ResourceHandle {
    u64 value = u64(0);

    friend constexpr bool operator==(ResourceHandle, ResourceHandle) noexcept = default;
};

struct ResourceGeneration {
    u64 value = u64(0);

    friend constexpr bool operator==(ResourceGeneration, ResourceGeneration) noexcept = default;
};

struct CaptureArtifactId {
    u64 value = u64(0);

    friend constexpr bool operator==(CaptureArtifactId, CaptureArtifactId) noexcept = default;
};

[[nodiscard]] inline constexpr RuntimeNodeId runtime_node_id_from_text(std::string_view text) noexcept {
    u64 hash = u64(14695981039346656037ull);
    for (const char ch : text) {
        hash ^= static_cast<u64>(static_cast<unsigned char>(ch));
        hash *= u64(1099511628211ull);
    }
    return RuntimeNodeId{hash};
}

namespace ids {

inline constexpr ComponentId unknown_component{"unknown"};
inline constexpr ComponentId simulation_wave_predator_prey{"simulation.wave_predator_prey"};
inline constexpr ComponentId surface_torus{"surface.torus"};
inline constexpr ComponentId surface_sine_rational{"surface.sine_rational"};
inline constexpr ComponentId surface_wave_predator_prey{"surface.wave_predator_prey"};
inline constexpr ComponentId field_metric_ripple{"field.metric_ripple"};
inline constexpr ComponentId field_damping{"field.damping"};
inline constexpr ComponentId behavior_brownian{"behavior.brownian"};
inline constexpr ComponentId behavior_seek{"behavior.seek"};
inline constexpr ComponentId behavior_avoid{"behavior.avoid"};
inline constexpr ComponentId solver_ode_euler{"solver.ode.euler"};
inline constexpr ComponentId solver_ode_rk4{"solver.ode.rk4"};
inline constexpr ComponentId system_gravity_pendulum{"system.gravity.pendulum"};
inline constexpr ComponentId system_gravity_planar_n_body{"system.gravity.planar_n_body"};
inline constexpr ComponentId integration_zoo_function_one{"integration.zoo.function_one"};
inline constexpr ComponentId integration_zoo_function_x{"integration.zoo.function_x"};
inline constexpr ComponentId integration_zoo_function_exp{"integration.zoo.function_exp"};
inline constexpr ComponentId integration_zoo_function_ln{"integration.zoo.function_ln"};
inline constexpr ComponentId integration_zoo_function_reciprocal{"integration.zoo.function_reciprocal"};
inline constexpr ComponentId integration_zoo_gaussian{"integration.zoo.gaussian"};
inline constexpr ComponentId integration_zoo_wave{"integration.zoo.wave"};
inline constexpr ComponentId integration_zoo_polynomial{"integration.zoo.polynomial"};
inline constexpr ComponentId integration_zoo_step_x{"integration.zoo.step_x"};
inline constexpr ComponentId integration_zoo_plane_patch{"integration.zoo.plane_patch"};
inline constexpr ComponentId integration_zoo_torus_chart{"integration.zoo.torus_chart"};

} // namespace ids

namespace resource_handles {

inline constexpr ResourceHandle invalid{u64(0)};
inline constexpr ResourceHandle renderer_line_shader{u64(0x0001'0000'0000'0010)};
inline constexpr ResourceHandle renderer_triangle_shader{u64(0x0001'0000'0000'0011)};
inline constexpr ResourceHandle colormap_viridis{u64(0x0001'0000'0000'0020)};

} // namespace resource_handles

} // namespace ndde
