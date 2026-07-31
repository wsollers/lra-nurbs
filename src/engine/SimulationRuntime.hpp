#pragma once
// engine/SimulationRuntime.hpp
// ISimulation-only runtime.  The old IScene adapter path is intentionally gone.

#include "engine/ISimulation.hpp"
#include "engine/SimulationHost.hpp"
#include "engine/threading/ThreadTypes.hpp"
#include "memory/Containers.hpp"
#include "memory/MemoryService.hpp"
#include "memory/Unique.hpp"

#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <string_view>

namespace ndde {

using SimulationSnapshot = SceneSnapshot;
class ThreadManagementService;

class SimulationSnapshotStore {
public:
    void publish(SimulationSnapshot snapshot) {
        std::scoped_lock lock(m_mutex);
        m_snapshot = std::move(snapshot);
    }

    [[nodiscard]] SimulationSnapshot snapshot() const {
        std::scoped_lock lock(m_mutex);
        return m_snapshot;
    }

private:
    mutable std::mutex m_mutex;
    SimulationSnapshot m_snapshot;
};

class SimulationRuntime final {
public:
    using SimulationFactory = std::function<memory::Unique<ISimulation>(memory::MemoryService&)>;

    SimulationRuntime(std::string name, SimulationFactory factory);
    ~SimulationRuntime();

    SimulationRuntime(const SimulationRuntime&) = delete;
    SimulationRuntime& operator=(const SimulationRuntime&) = delete;
    SimulationRuntime(SimulationRuntime&&) = delete;
    SimulationRuntime& operator=(SimulationRuntime&&) = delete;

    void instantiate(SimulationHost& host);
    void start();
    void stop();
    void pause();
    void resume();
    void tick(TickInfo tick);
    void tick_simulation(TickInfo tick);
    void submit_render();
    void process_thread_commands(std::span<const SimulationThreadCommand> commands,
                                 ThreadManagementService* threads = nullptr);
    void record_telemetry_tick(u64 tick_index, const TickInfo& tick, EngineAPI& api);
    void publish();

    [[nodiscard]] std::string_view name() const noexcept { return m_name; }
    [[nodiscard]] bool paused() const noexcept { return m_paused; }
    [[nodiscard]] bool instantiated() const noexcept { return static_cast<bool>(m_simulation); }
    [[nodiscard]] ISimulation& simulation();
    [[nodiscard]] const ISimulation& simulation() const;
    [[nodiscard]] SimulationSnapshot snapshot() const;
    [[nodiscard]] SimulationMetadata metadata() const;

private:
    mutable std::recursive_mutex m_mutex;
    std::string m_name;
    SimulationFactory m_factory;
    memory::Unique<ISimulation> m_simulation;
    SimulationSnapshotStore m_snapshots;
    bool m_started = false;
    bool m_paused = true;
};

} // namespace ndde
