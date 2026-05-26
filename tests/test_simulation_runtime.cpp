#include "engine/SimulationRuntime.hpp"
#include "engine/SimulationHost.hpp"
#include "engine/threading/ThreadManagementService.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <thread>

namespace {

using namespace ndde;
using namespace std::chrono_literals;

class RuntimeDummySimulation final : public ISimulation {
public:
    std::string_view name() const override { return "Dummy Simulation"; }

    void on_register(SimulationHost& host) override {
        panel = host.panels().register_panel(PanelDescriptor{.title = "Runtime Panel", .draw = [] {}});
        hotkey = host.hotkeys().register_action(HotkeyDescriptor{
            .chord = {.key = 88, .mods = 2},
            .label = "Runtime Hotkey",
            .callback = [this] { ++hotkey_calls; }
        });
        view = host.render().register_view(RenderViewDescriptor{
            .title = "Runtime View",
            .kind = RenderViewKind::Main
        }, &view_id);
    }

    void on_start() override { ++starts; }
    void on_tick(const TickInfo& tick) override {
        last_tick = tick;
        ++ticks;
    }
    void on_simulation_tick(const TickInfo& tick) override {
        last_tick = tick;
        ++simulation_ticks;
        if (block_simulation_tick.load()) {
            simulation_tick_entered.store(true);
            while (!allow_simulation_tick_exit.load()) {
                std::this_thread::sleep_for(1ms);
            }
        }
    }
    void on_submit_render() override {
        ++render_submits;
    }
    void on_telemetry_tick(u64 tick_index, const TickInfo& tick, EngineAPI&) override {
        last_telemetry_tick_index = tick_index;
        last_telemetry_tick = tick;
        ++telemetry_ticks;
    }
    void on_stop() override {
        ++stops;
        panel.reset();
        hotkey.reset();
        view.reset();
    }

    [[nodiscard]] SceneSnapshot snapshot() const override {
        return SceneSnapshot{
            .name = "Dummy Simulation",
            .paused = last_tick.paused,
            .sim_time = last_tick.time,
            .status = "Simulation Snapshot"
        };
    }

    PanelHandle panel;
    HotkeyHandle hotkey;
    RenderViewHandle view;
    RenderViewId view_id = 0;
    TickInfo last_tick{};
    int starts = 0;
    int stops = 0;
    int ticks = 0;
    int simulation_ticks = 0;
    int render_submits = 0;
    int telemetry_ticks = 0;
    int hotkey_calls = 0;
    u64 last_telemetry_tick_index = u64(0);
    TickInfo last_telemetry_tick{};
    std::atomic<bool> block_simulation_tick = false;
    std::atomic<bool> simulation_tick_entered = false;
    std::atomic<bool> allow_simulation_tick_exit = false;
};

TEST(SimulationRuntime, RuntimeOwnsISimulationLifecycle) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    RuntimeDummySimulation* raw = nullptr;
    SimulationRuntime runtime("Simulation Runtime",
        [&raw](memory::MemoryService& memory) {
            auto sim = memory.simulation().make_unique_as<ISimulation, RuntimeDummySimulation>();
            raw = static_cast<RuntimeDummySimulation*>(sim.get());
            return sim;
        });

    runtime.instantiate(host);
    ASSERT_NE(raw, nullptr);
    EXPECT_TRUE(runtime.paused());
    EXPECT_EQ(services.panels().active_count(), 1u);
    EXPECT_EQ(services.hotkeys().active_count(), 1u);
    EXPECT_EQ(services.render().active_view_count(), 1u);

    runtime.start();
    EXPECT_FALSE(runtime.paused());
    runtime.tick(TickInfo{.tick_index = 1u, .dt = 0.5f, .time = 0.5f, .paused = false});
    runtime.publish();

    EXPECT_EQ(raw->starts, 1);
    EXPECT_EQ(raw->ticks, 1);
    EXPECT_EQ(raw->simulation_ticks, 0);
    EXPECT_FLOAT_EQ(raw->last_tick.time, 0.5f);
    EXPECT_EQ(runtime.snapshot().name, "Dummy Simulation");
    EXPECT_EQ(runtime.snapshot().status, "Simulation Snapshot");

    EXPECT_TRUE(services.hotkeys().dispatch({.key = 88, .mods = 2}));
    EXPECT_EQ(raw->hotkey_calls, 1);

    runtime.pause();
    EXPECT_TRUE(runtime.paused());
    runtime.tick(TickInfo{.tick_index = 2u, .dt = 0.5f, .time = 1.0f, .paused = false});
    EXPECT_TRUE(raw->last_tick.paused);
    EXPECT_FLOAT_EQ(raw->last_tick.dt, 0.f);

    runtime.stop();
    EXPECT_EQ(services.panels().active_count(), 0u);
    EXPECT_EQ(services.hotkeys().active_count(), 0u);
    EXPECT_EQ(services.render().active_view_count(), 0u);
}

TEST(SimulationRuntime, ReinstantiateRollsBackPreviousRegistrations) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    int created = 0;
    SimulationRuntime runtime("Simulation Runtime",
        [&created](memory::MemoryService& memory) {
            ++created;
            return memory.simulation().make_unique_as<ISimulation, RuntimeDummySimulation>();
        });

    runtime.instantiate(host);
    EXPECT_EQ(created, 1);
    EXPECT_EQ(services.panels().active_count(), 1u);

    runtime.instantiate(host);
    EXPECT_EQ(created, 2);
    EXPECT_EQ(services.panels().active_count(), 1u);
    EXPECT_EQ(services.hotkeys().active_count(), 1u);
    EXPECT_EQ(services.render().active_view_count(), 1u);
}

TEST(SimulationRuntime, SeparatesSimulationTickFromRenderSubmit) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    RuntimeDummySimulation* raw = nullptr;
    SimulationRuntime runtime("Simulation Runtime",
        [&raw](memory::MemoryService& memory) {
            auto sim = memory.simulation().make_unique_as<ISimulation, RuntimeDummySimulation>();
            raw = static_cast<RuntimeDummySimulation*>(sim.get());
            return sim;
        });

    runtime.instantiate(host);
    runtime.start();
    runtime.tick_simulation(TickInfo{.tick_index = u64(1), .dt = f32(0.1f), .time = f32(0.1f)});

    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->ticks, 0);
    EXPECT_EQ(raw->simulation_ticks, 1);
    EXPECT_EQ(raw->render_submits, 0);

    runtime.submit_render();
    EXPECT_EQ(raw->render_submits, 1);
}

TEST(SimulationRuntime, SerializesSimulationTickAndRenderSubmit) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    RuntimeDummySimulation* raw = nullptr;
    SimulationRuntime runtime("Simulation Runtime",
        [&raw](memory::MemoryService& memory) {
            auto sim = memory.simulation().make_unique_as<ISimulation, RuntimeDummySimulation>();
            raw = static_cast<RuntimeDummySimulation*>(sim.get());
            return sim;
        });

    runtime.instantiate(host);
    runtime.start();
    ASSERT_NE(raw, nullptr);
    raw->block_simulation_tick.store(true);

    std::jthread sim_thread([&runtime] {
        runtime.tick_simulation(TickInfo{.tick_index = u64(1), .dt = f32(0.1f), .time = f32(0.1f)});
    });

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!raw->simulation_tick_entered.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(raw->simulation_tick_entered.load());

    std::atomic<bool> render_returned = false;
    std::jthread render_thread([&runtime, &render_returned] {
        runtime.submit_render();
        render_returned.store(true);
    });

    std::this_thread::sleep_for(30ms);
    EXPECT_FALSE(render_returned.load());
    raw->allow_simulation_tick_exit.store(true);

    sim_thread.join();
    render_thread.join();
    EXPECT_TRUE(render_returned.load());
    EXPECT_EQ(raw->render_submits, 1);
}

TEST(SimulationRuntime, SerializesSimulationTickAndTelemetryHook) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    RuntimeDummySimulation* raw = nullptr;
    SimulationRuntime runtime("Simulation Runtime",
        [&raw](memory::MemoryService& memory) {
            auto sim = memory.simulation().make_unique_as<ISimulation, RuntimeDummySimulation>();
            raw = static_cast<RuntimeDummySimulation*>(sim.get());
            return sim;
        });

    runtime.instantiate(host);
    runtime.start();
    ASSERT_NE(raw, nullptr);
    raw->block_simulation_tick.store(true);

    std::jthread sim_thread([&runtime] {
        runtime.tick_simulation(TickInfo{.tick_index = u64(1), .dt = f32(0.1f), .time = f32(0.1f)});
    });

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!raw->simulation_tick_entered.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(raw->simulation_tick_entered.load());

    EngineAPI api;
    std::atomic<bool> telemetry_returned = false;
    std::jthread telemetry_thread([&runtime, &api, &telemetry_returned] {
        runtime.record_telemetry_tick(
            u64(7),
            TickInfo{.tick_index = u64(7), .dt = f32(0.25f), .time = f32(0.25f)},
            api);
        telemetry_returned.store(true);
    });

    std::this_thread::sleep_for(30ms);
    EXPECT_FALSE(telemetry_returned.load());
    raw->allow_simulation_tick_exit.store(true);

    sim_thread.join();
    telemetry_thread.join();
    EXPECT_TRUE(telemetry_returned.load());
    EXPECT_EQ(raw->telemetry_ticks, 1);
    EXPECT_EQ(raw->last_telemetry_tick_index, u64(7));
    EXPECT_FLOAT_EQ(raw->last_telemetry_tick.time, f32(0.25f));
}

TEST(SimulationRuntime, RegistryStoresSimulationRuntimesOnly) {
    EngineServices services;
    SimulationRegistry registry(services.memory());
    registry.add_runtime<RuntimeDummySimulation>("Simulation A");
    registry.add_runtime<RuntimeDummySimulation>("Simulation B");

    EXPECT_EQ(registry.size(), 2u);
    ASSERT_NE(registry.get(0), nullptr);
    ASSERT_NE(registry.get(1), nullptr);
    EXPECT_EQ(registry.get(0)->name(), "Simulation A");
    EXPECT_EQ(registry.get(1)->name(), "Simulation B");
}

TEST(SimulationRuntime, ProcessesThreadCommandsAndPublishesSnapshotMailbox) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    RuntimeDummySimulation* raw = nullptr;
    SimulationRuntime runtime("Simulation Runtime",
        [&raw](memory::MemoryService& memory) {
            auto sim = memory.simulation().make_unique_as<ISimulation, RuntimeDummySimulation>();
            raw = static_cast<RuntimeDummySimulation*>(sim.get());
            return sim;
        });

    runtime.instantiate(host);
    runtime.start();

    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .enable_background_workers = false
    });

    ASSERT_TRUE(threads.start_simulation_thread(
        [&runtime, &threads](std::stop_token, std::span<const SimulationThreadCommand> commands) {
            runtime.process_thread_commands(commands, &threads);
        }));

    ASSERT_TRUE(threads.enqueue_simulation_command(SimulationThreadCommand{
        .kind = SimulationThreadCommandKind::Tick,
        .tick = TickInfo{.tick_index = u64(4), .dt = f32(0.25f), .time = f32(0.25f)}
    }));

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while ((!threads.latest_simulation_snapshot().has_value() ||
            threads.latest_simulation_snapshot()->sim_time < f32(0.25f)) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }

    threads.stop_simulation_thread();
    const auto snapshot = threads.latest_simulation_snapshot();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->name, "Dummy Simulation");
    EXPECT_FLOAT_EQ(snapshot->sim_time, f32(0.25f));
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->simulation_ticks, 1);
    EXPECT_EQ(raw->ticks, 0);

    runtime.stop();
}

TEST(SimulationRuntime, ThreadStopCommandDoesNotRunOwnerThreadTeardown) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    RuntimeDummySimulation* raw = nullptr;
    SimulationRuntime runtime("Simulation Runtime",
        [&raw](memory::MemoryService& memory) {
            auto sim = memory.simulation().make_unique_as<ISimulation, RuntimeDummySimulation>();
            raw = static_cast<RuntimeDummySimulation*>(sim.get());
            return sim;
        });

    runtime.instantiate(host);
    runtime.start();
    ASSERT_NE(raw, nullptr);

    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .enable_background_workers = false
    });

    const std::array commands{
        SimulationThreadCommand{.kind = SimulationThreadCommandKind::Stop}
    };
    runtime.process_thread_commands(commands, nullptr);

    EXPECT_TRUE(runtime.paused());
    EXPECT_EQ(raw->stops, 0);
    EXPECT_EQ(services.panels().active_count(), 1u);
    EXPECT_EQ(services.hotkeys().active_count(), 1u);
    EXPECT_EQ(services.render().active_view_count(), 1u);

    runtime.stop();
    EXPECT_EQ(services.panels().active_count(), 0u);
    EXPECT_EQ(services.hotkeys().active_count(), 0u);
    EXPECT_EQ(services.render().active_view_count(), 0u);
}

TEST(SimulationRuntime, ProcessThreadCommandsRequiresSimulationRoleWhenThreadServiceIsProvided) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    RuntimeDummySimulation* raw = nullptr;
    SimulationRuntime runtime("Simulation Runtime",
        [&raw](memory::MemoryService& memory) {
            auto sim = memory.simulation().make_unique_as<ISimulation, RuntimeDummySimulation>();
            raw = static_cast<RuntimeDummySimulation*>(sim.get());
            return sim;
        });

    runtime.instantiate(host);
    runtime.start();
    ASSERT_NE(raw, nullptr);

    const std::array commands{
        SimulationThreadCommand{
            .kind = SimulationThreadCommandKind::Tick,
            .tick = TickInfo{.tick_index = u64(9), .dt = f32(0.1f), .time = f32(0.1f)}
        }
    };
    runtime.process_thread_commands(commands, &services.threads());
    services.threads().drain_service_mailboxes();

    EXPECT_EQ(raw->simulation_ticks, 0);
    const auto role_violations =
        services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_NE(role_violations.front().message.find("SimulationRuntime::process_thread_commands"),
              std::string::npos);
}

} // namespace
