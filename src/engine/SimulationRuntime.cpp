#include "engine/SimulationRuntime.hpp"
#include "engine/metricservice/MetricsService.hpp"
#include "engine/threading/ThreadManagementService.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace ndde {

SimulationRuntime::SimulationRuntime(std::string name, SimulationFactory factory)
    : m_name(std::move(name))
    , m_factory(std::move(factory))
{
    publish();
}

SimulationRuntime::~SimulationRuntime() {
    stop();
}

void SimulationRuntime::instantiate(SimulationHost& host) {
    std::scoped_lock lock(m_mutex);
    stop();
    m_simulation = m_factory(host.memory());
    m_simulation->on_register(host);
    m_started = false;
    m_paused = true;
    publish();
}

void SimulationRuntime::start() {
    std::scoped_lock lock(m_mutex);
    if (!m_simulation)
        throw std::runtime_error("[SimulationRuntime] start called before instantiate");
    if (!m_started) {
        m_simulation->on_start();
        m_started = true;
    }
    resume();
}

void SimulationRuntime::stop() {
    std::scoped_lock lock(m_mutex);
    if (m_simulation) {
        m_simulation->on_stop();
        m_simulation.reset();
    }
    m_started = false;
    m_paused = true;
    publish();
}

void SimulationRuntime::pause() {
    std::scoped_lock lock(m_mutex);
    m_paused = true;
    publish();
}

void SimulationRuntime::resume() {
    std::scoped_lock lock(m_mutex);
    if (!m_simulation)
        throw std::runtime_error("[SimulationRuntime] resume called before instantiate");
    m_paused = false;
    publish();
}

void SimulationRuntime::tick(TickInfo tick) {
    const auto started = std::chrono::steady_clock::now();
    std::scoped_lock lock(m_mutex);
    if (!m_simulation) return;
    TickInfo effective = tick;
    effective.paused = m_paused || tick.paused;
    if (effective.paused) effective.dt = 0.f;
    m_simulation->on_tick(effective);
    publish();
    if (MetricsThreadContext* metrics = MetricsThreadHandle::current()) {
        metrics->record_duration(MetricId::SimulationTickMs,
                                 std::chrono::steady_clock::now() - started);
    }
}

void SimulationRuntime::tick_simulation(TickInfo tick) {
    const auto started = std::chrono::steady_clock::now();
    std::scoped_lock lock(m_mutex);
    if (!m_simulation) return;
    TickInfo effective = tick;
    effective.paused = m_paused || tick.paused;
    if (effective.paused) effective.dt = 0.f;
    m_simulation->on_simulation_tick(effective);
    publish();
    if (MetricsThreadContext* metrics = MetricsThreadHandle::current()) {
        metrics->record_duration(MetricId::SimulationTickMs,
                                 std::chrono::steady_clock::now() - started);
    }
}

void SimulationRuntime::submit_render() {
    const auto started = std::chrono::steady_clock::now();
    std::scoped_lock lock(m_mutex);
    if (!m_simulation) return;
    m_simulation->on_submit_render();
    if (MetricsThreadContext* metrics = MetricsThreadHandle::current()) {
        metrics->record_duration(MetricId::SimulationRenderSubmitMs,
                                 std::chrono::steady_clock::now() - started);
    }
}

void SimulationRuntime::process_thread_commands(std::span<const SimulationThreadCommand> commands,
                                                ThreadManagementService* threads) {
    if (threads &&
        !threads->require_thread_role(ThreadRole::Simulation,
                                      "SimulationRuntime::process_thread_commands")) {
        return;
    }

    for (const SimulationThreadCommand& command : commands) {
        switch (command.kind) {
            case SimulationThreadCommandKind::Pause:
                pause();
                break;
            case SimulationThreadCommandKind::Resume:
                resume();
                break;
            case SimulationThreadCommandKind::Stop:
            case SimulationThreadCommandKind::Shutdown:
                // Full teardown calls simulation on_stop(), which unregisters
                // owner-thread services such as panels, hotkeys, and views.
                // The simulation thread may only quiesce runtime state; the
                // engine owner thread performs lifecycle teardown.
                pause();
                break;
            case SimulationThreadCommandKind::Tick:
                tick_simulation(command.tick);
                break;
            case SimulationThreadCommandKind::SurfacePoke: {
                    std::scoped_lock lock(m_mutex);
                    if (m_simulation) {
                        m_simulation->on_simulation_command(command);
                    }
                }
                publish();
                break;
            case SimulationThreadCommandKind::SwitchSimulation:
            case SimulationThreadCommandKind::ResetClock:
                publish();
                break;
        }
    }

    if (threads) {
        threads->publish_simulation_snapshot(make_simulation_render_snapshot(snapshot()));
    }
}

void SimulationRuntime::record_telemetry_tick(u64 tick_index, const TickInfo& tick, EngineAPI& api) {
    const auto started = std::chrono::steady_clock::now();
    std::scoped_lock lock(m_mutex);
    if (!m_simulation) return;
    m_simulation->on_telemetry_tick(tick_index, tick, api);
    if (MetricsThreadContext* metrics = MetricsThreadHandle::current()) {
        metrics->record_duration(MetricId::TelemetryTickMs,
                                 std::chrono::steady_clock::now() - started);
    }
}

void SimulationRuntime::publish() {
    std::scoped_lock lock(m_mutex);
    if (m_simulation) {
        SimulationSnapshot snapshot = m_simulation->snapshot();
        snapshot.paused = m_paused;
        m_snapshots.publish(std::move(snapshot));
        return;
    }
    m_snapshots.publish(SimulationSnapshot{
        .name = m_name,
        .paused = true,
        .status = "Not instantiated"
    });
}

ISimulation& SimulationRuntime::simulation() {
    std::scoped_lock lock(m_mutex);
    if (!m_simulation)
        throw std::runtime_error("[SimulationRuntime] simulation requested before instantiate");
    return *m_simulation;
}

const ISimulation& SimulationRuntime::simulation() const {
    std::scoped_lock lock(m_mutex);
    if (!m_simulation)
        throw std::runtime_error("[SimulationRuntime] simulation requested before instantiate");
    return *m_simulation;
}

SimulationSnapshot SimulationRuntime::snapshot() const {
    return m_snapshots.snapshot();
}

SimulationMetadata SimulationRuntime::metadata() const {
    std::scoped_lock lock(m_mutex);
    if (!m_simulation) {
        return SimulationMetadata{
            .name = m_name,
            .status = "Not instantiated",
            .paused = true
        };
    }
    SimulationMetadata data = m_simulation->metadata();
    data.paused = m_paused;
    return data;
}

SimulationRegistry::SimulationRegistry(memory::MemoryService& memory) noexcept
    : m_memory(memory)
    , m_runtimes(memory.persistent().resource())
{}

void SimulationRegistry::add(memory::Unique<SimulationRuntime> runtime) {
    m_runtimes.push_back(std::move(runtime));
}

SimulationRuntime* SimulationRegistry::get(std::size_t index) noexcept {
    if (index >= m_runtimes.size()) return nullptr;
    return m_runtimes[index].get();
}

const SimulationRuntime* SimulationRegistry::get(std::size_t index) const noexcept {
    if (index >= m_runtimes.size()) return nullptr;
    return m_runtimes[index].get();
}

} // namespace ndde
