#pragma once
// engine/ISimulation.hpp
// First-class simulation contract for the next architecture.

#include "engine/IScene.hpp"
#include "engine/SimulationMetadata.hpp"
#include "engine/SimulationClock.hpp"
#include "engine/SimulationHost.hpp"
#include "engine/EngineAPI.hpp"
#include "engine/threading/ThreadTypes.hpp"

#include <string_view>

namespace ndde {

class ISimulation {
public:
    virtual ~ISimulation() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;

    virtual void on_register(SimulationHost& host) = 0;
    virtual void on_start() = 0;
    virtual void on_tick(const TickInfo& tick) = 0;
    virtual void on_simulation_tick(const TickInfo& tick) { on_tick(tick); }
    virtual void on_simulation_command(const SimulationThreadCommand&) {}
    virtual void on_submit_render() {}
    virtual void on_stop() = 0;

    // Optional telemetry hook — called by Engine::run_frame() each tick when
    // telemetry is enabled, AFTER on_tick().
    // Override to push TelemetryRecord rows with richer per-particle data
    // (noise_sigma, speed, angle, geodesic_k) than the base snapshot provides.
    // The default no-op means the engine falls back to snapshot-based recording.
    // api.record_telemetry() is a producer push. api.record_telemetry_ext()
    // appends to the owner-thread extension scratch buffer and should only be
    // used from the engine telemetry hook path.
    virtual void on_telemetry_tick(
        [[maybe_unused]] u64              tick_index,
        [[maybe_unused]] const TickInfo&  tick,
        [[maybe_unused]] EngineAPI&       api) {}

    [[nodiscard]] virtual SceneSnapshot snapshot() const {
        return SceneSnapshot{ .name = std::string(name()) };
    }

    [[nodiscard]] virtual SimulationMetadata metadata() const {
        const SceneSnapshot s = snapshot();
        return SimulationMetadata{
            .name = s.name,
            .status = s.status,
            .sim_time = s.sim_time,
            .sim_speed = s.sim_speed,
            .particle_count = s.particle_count,
            .paused = s.paused,
            .goal_succeeded = s.status == "Succeeded"
        };
    }

protected:
    ISimulation() = default;
    ISimulation(const ISimulation&) = default;
    ISimulation& operator=(const ISimulation&) = default;
    ISimulation(ISimulation&&) = default;
    ISimulation& operator=(ISimulation&&) = default;
};

} // namespace ndde
