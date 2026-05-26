#include "engine/SimulationHost.hpp"
#include "engine/threading/ThreadManagementService.hpp"
#include "telemetry/TelemetryService.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <variant>

namespace {

using namespace ndde;
using namespace std::chrono_literals;

[[nodiscard]] bool wait_for_state(ThreadManagementService& threads,
                                  ThreadJobId id,
                                  ThreadJobState state,
                                  std::chrono::milliseconds timeout = 2s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto status = threads.status(id);
        if (status && status->state == state) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

TEST(ThreadManagementService, InitShutdownWithZeroWorkersIsSafe) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{.worker_count = u32(0), .enable_background_workers = false});

    const ThreadJobId id = threads.submit(ThreadJobDescriptor{}, [](ThreadJobContext&) {});
    ASSERT_NE(id.value, u64(0));
    EXPECT_EQ(threads.status(id)->state, ThreadJobState::Queued);

    threads.shutdown();
    EXPECT_FALSE(threads.initialised());
    EXPECT_EQ(threads.status(id)->state, ThreadJobState::Cancelled);
}

TEST(ThreadManagementService, SubmittedJobCompletesAndReturnsResult) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{.worker_count = u32(1)});

    const ThreadJobId id = threads.submit(ThreadJobDescriptor{}, [](ThreadJobContext& context) {
        context.complete(ResourceId{u64(902)});
    });

    ASSERT_TRUE(wait_for_state(threads, id, ThreadJobState::Completed));
    const auto results = threads.consume_completed_results();
    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<ResourceId>(results.front()));
    EXPECT_EQ(std::get<ResourceId>(results.front()).value, u64(902));
}

TEST(ThreadManagementService, CancellationIsObservableThroughContext) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{.worker_count = u32(1)});
    std::atomic<bool> started = false;

    const ThreadJobId id = threads.submit(ThreadJobDescriptor{}, [&started](ThreadJobContext& context) {
        started.store(true);
        while (!context.stop_requested()) {
            std::this_thread::sleep_for(1ms);
        }
    });

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!started.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(started.load());

    threads.request_cancel(id);
    ASSERT_TRUE(wait_for_state(threads, id, ThreadJobState::Cancelled));
}

TEST(ThreadManagementService, RequireThreadRolePassesOnMainThread) {
    DiagnosticsService diagnostics;
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{.worker_count = u32(0), .enable_background_workers = false},
                 ThreadServiceBindings{.diagnostics = &diagnostics});

    EXPECT_TRUE(threads.require_thread_role(ThreadRole::Main, "main-only api"));
    threads.drain_service_mailboxes();
    EXPECT_TRUE(diagnostics.active().empty());
}

TEST(ThreadManagementService, RequireThreadRoleReportsWorkerViolation) {
    DiagnosticsService diagnostics;
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{.worker_count = u32(1)},
                 ThreadServiceBindings{.diagnostics = &diagnostics});

    std::atomic<bool> allowed = true;
    const ThreadJobId id = threads.submit(ThreadJobDescriptor{}, [&threads, &allowed](ThreadJobContext&) {
        allowed.store(threads.require_thread_role(ThreadRole::Main, "main-only api"));
    });

    ASSERT_TRUE(wait_for_state(threads, id, ThreadJobState::Completed));
    threads.drain_service_mailboxes();

    EXPECT_FALSE(allowed.load());
    ASSERT_EQ(diagnostics.active().size(), 1u);
    EXPECT_EQ(diagnostics.active().front().code, ErrorCode::ThreadRoleViolation);
    EXPECT_NE(diagnostics.active().front().message.find("expected Main thread"), std::string::npos);
    EXPECT_NE(diagnostics.active().front().message.find("Worker thread"), std::string::npos);
}

TEST(ThreadManagementService, WorkerMailboxesDrainIntoBoundServices) {
    DiagnosticsService diagnostics;
    EventBusService events;
    LoggerService logger;
    events.init();
    logger.init();

    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{.worker_count = u32(1)},
                 ThreadServiceBindings{
                     .diagnostics = &diagnostics,
                     .events = &events,
                     .logger = &logger
                 });

    const ThreadJobId id = threads.submit(ThreadJobDescriptor{}, [](ThreadJobContext& context) {
        context.log(LogSeverity::Info, LogCategory::Worker, "worker says hello");

        DiagnosticReport report;
        report.severity = DiagnosticSeverity::Warning;
        report.code = ErrorCode::ThreadFault;
        report.source.subsystem = DiagnosticSubsystem::Threading;
        report.title = "thread warning";
        report.message = "worker diagnostic";
        context.report_diagnostic(std::move(report));

        events::EventRecord record;
        record.kind = events::EventKind::AlertCustom;
        record.severity = events::EventSeverity::Notice;
        record.set_label("WorkerJob");
        context.publish_worker_record(record);
    });

    ASSERT_TRUE(wait_for_state(threads, id, ThreadJobState::Completed));
    threads.drain_service_mailboxes();
    events.drain(EventChannelId::Worker, f32(0), u64(0));

    ASSERT_EQ(logger.records().size(), 1u);
    EXPECT_EQ(logger.message(logger.records().front().id), "worker says hello");
    ASSERT_EQ(diagnostics.active().size(), 1u);
    EXPECT_EQ(diagnostics.active().front().message, "worker diagnostic");
    ASSERT_EQ(events.log(EventChannelId::Worker).entries().size(), 1u);
}

TEST(ThreadManagementService, EventMailboxDrainsPublishedChannel) {
    EventBusService events;
    events.init();

    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{.worker_count = u32(0)},
                 ThreadServiceBindings{.events = &events});

    events::EventRecord record;
    record.kind = events::EventKind::AppStarted;
    record.severity = events::EventSeverity::Info;
    record.set_label("AppChannelRecord");

    threads.publish_event_record(EventChannelId::App, record);
    threads.drain_service_mailboxes();
    events.drain(EventChannelId::App, f32(0), u64(0));

    ASSERT_EQ(events.log(EventChannelId::App).entries().size(), 1u);
    EXPECT_EQ(events.log(EventChannelId::App).entries().front().kind, events::EventKind::AppStarted);
}

TEST(ThreadManagementService, LoggerThreadDrainsWorkerLogsAsynchronously) {
    LoggerService logger;
    logger.init();

    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
                     .worker_count = u32(1),
                     .enable_logger_thread = true
                 },
                 ThreadServiceBindings{.logger = &logger});

    const ThreadJobId id = threads.submit(ThreadJobDescriptor{}, [](ThreadJobContext& context) {
        context.log(LogSeverity::Info, LogCategory::Worker, "async worker log");
    });

    ASSERT_TRUE(wait_for_state(threads, id, ThreadJobState::Completed));

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (logger.snapshot().empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }

    const auto snapshot = logger.snapshot();
    ASSERT_EQ(snapshot.size(), 1u);
    EXPECT_EQ(snapshot.front().message, "async worker log");
}

TEST(ThreadManagementService, WorkerExceptionBecomesDiagnostic) {
    DiagnosticsService diagnostics;
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{.worker_count = u32(1)},
                 ThreadServiceBindings{.diagnostics = &diagnostics});

    const ThreadJobId id = threads.submit(ThreadJobDescriptor{}, [](ThreadJobContext&) {
        throw std::runtime_error{"kaboom"};
    });

    ASSERT_TRUE(wait_for_state(threads, id, ThreadJobState::Failed));
    threads.drain_service_mailboxes();

    ASSERT_EQ(diagnostics.active().size(), 1u);
    EXPECT_EQ(diagnostics.active().front().code, ErrorCode::ThreadFault);
}

TEST(ThreadManagementService, QueueOverflowRejectsNewJobs) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .max_queued_jobs = u64(1),
        .enable_background_workers = false
    });

    const ThreadJobId first = threads.submit(ThreadJobDescriptor{}, [](ThreadJobContext&) {});
    const ThreadJobId second = threads.submit(ThreadJobDescriptor{}, [](ThreadJobContext&) {});

    EXPECT_NE(first.value, u64(0));
    EXPECT_EQ(second.value, u64(0));
    EXPECT_EQ(threads.dropped_events(), u64(1));
}

TEST(ThreadManagementService, SimulationCommandQueuePreservesOrderAndDrains) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .max_queued_jobs = u64(3),
        .enable_background_workers = false
    });

    EXPECT_TRUE(threads.enqueue_simulation_command(SimulationThreadCommand{
        .kind = SimulationThreadCommandKind::Pause
    }));
    EXPECT_TRUE(threads.enqueue_simulation_command(SimulationThreadCommand{
        .kind = SimulationThreadCommandKind::Tick,
        .tick = TickInfo{.tick_index = u64(7), .dt = f32(0.25f), .time = f32(1.5f)}
    }));
    EXPECT_TRUE(threads.enqueue_simulation_command(SimulationThreadCommand{
        .kind = SimulationThreadCommandKind::SwitchSimulation,
        .simulation_index = u64(2)
    }));
    EXPECT_FALSE(threads.enqueue_simulation_command(SimulationThreadCommand{
        .kind = SimulationThreadCommandKind::Resume
    }));

    const auto commands = threads.consume_simulation_commands();
    ASSERT_EQ(commands.size(), 3u);
    EXPECT_EQ(commands[0].kind, SimulationThreadCommandKind::Pause);
    EXPECT_EQ(commands[1].kind, SimulationThreadCommandKind::Tick);
    EXPECT_EQ(commands[1].tick.tick_index, u64(7));
    EXPECT_EQ(commands[2].kind, SimulationThreadCommandKind::SwitchSimulation);
    EXPECT_EQ(commands[2].simulation_index, u64(2));
    EXPECT_TRUE(threads.consume_simulation_commands().empty());
}

TEST(ThreadManagementService, RenderCommandQueueReportsOverflow) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .max_queued_jobs = u64(1),
        .enable_background_workers = false
    });

    EXPECT_TRUE(threads.enqueue_render_command(RenderThreadCommand{
        .kind = RenderThreadCommandKind::Frame
    }));
    EXPECT_FALSE(threads.enqueue_render_command(RenderThreadCommand{
        .kind = RenderThreadCommandKind::Resize,
        .width = u32(800),
        .height = u32(600)
    }));
    EXPECT_EQ(threads.dropped_events(), u64(1));
}

TEST(ThreadManagementService, SimulationSnapshotMailboxKeepsLatestImmutableCopy) {
    ndde::memory::MemoryService memory;
    memory.begin_frame();

    SceneSnapshot scene;
    scene.name = "Sim A";
    scene.paused = true;
    scene.sim_time = 3.25f;
    scene.sim_speed = 2.0f;
    scene.status = "Running";
    scene.particles.push_back(ParticleSnapshot{.id = 11u, .label = "first", .x = 1.f});
    scene.particle_count = scene.particles.size();

    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{.worker_count = u32(0), .enable_background_workers = false});
    threads.publish_simulation_snapshot(make_simulation_render_snapshot(scene));

    scene.name = "mutated";
    scene.particles.front().label = "mutated";

    const auto snapshot = threads.latest_simulation_snapshot();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->name, "Sim A");
    EXPECT_TRUE(snapshot->paused);
    EXPECT_FLOAT_EQ(snapshot->sim_time, f32(3.25f));
    ASSERT_EQ(snapshot->particles.size(), 1u);
    EXPECT_EQ(snapshot->particles.front().label, "first");

    threads.publish_simulation_snapshot(SimulationRenderSnapshot{.name = "Sim B"});
    const auto latest = threads.latest_simulation_snapshot();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->name, "Sim B");
}

TEST(ThreadManagementService, SimulationThreadConsumesCommandBatches) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .enable_background_workers = false
    });

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<SimulationThreadCommandKind> seen;
    ThreadRole callback_role = ThreadRole::Unknown;

    ASSERT_TRUE(threads.start_simulation_thread(
        [&threads, &mutex, &cv, &seen, &callback_role]
        (std::stop_token, std::span<const SimulationThreadCommand> commands) {
            {
                std::scoped_lock lock(mutex);
                callback_role = threads.current_thread_role();
                for (const SimulationThreadCommand& command : commands) {
                    seen.push_back(command.kind);
                }
            }
            cv.notify_one();
        }));
    EXPECT_TRUE(threads.simulation_thread_running());
    EXPECT_FALSE(threads.start_simulation_thread([](std::stop_token, std::span<const SimulationThreadCommand>) {}));

    ASSERT_TRUE(threads.enqueue_simulation_command(SimulationThreadCommand{
        .kind = SimulationThreadCommandKind::Pause
    }));
    ASSERT_TRUE(threads.enqueue_simulation_command(SimulationThreadCommand{
        .kind = SimulationThreadCommandKind::Resume
    }));

    std::unique_lock lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 2s, [&seen] { return seen.size() >= 2u; }));
    EXPECT_EQ(callback_role, ThreadRole::Simulation);
    EXPECT_EQ(seen[0], SimulationThreadCommandKind::Pause);
    EXPECT_EQ(seen[1], SimulationThreadCommandKind::Resume);
    lock.unlock();

    threads.stop_simulation_thread();
    EXPECT_FALSE(threads.simulation_thread_running());
}

TEST(ThreadManagementService, RenderCommandQueuePreservesImmutableFrameSnapshot) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .max_queued_jobs = u64(2),
        .enable_background_workers = false
    });

    RenderThreadCommand command;
    command.kind = RenderThreadCommandKind::Frame;
    command.frame.frame_index = u64(42);
    command.frame.frame_ms = f32(16.6f);
    command.frame.simulation.name = "Render Snapshot";
    command.frame.simulation.particles.push_back(ParticleSnapshot{.id = 17u, .label = "p"});

    EXPECT_TRUE(threads.enqueue_render_command(command));
    command.frame.simulation.name = "mutated";
    command.frame.simulation.particles.front().label = "mutated";

    const auto commands = threads.consume_render_commands();
    ASSERT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands.front().kind, RenderThreadCommandKind::Frame);
    EXPECT_EQ(commands.front().frame.frame_index, u64(42));
    EXPECT_EQ(commands.front().frame.simulation.name, "Render Snapshot");
    ASSERT_EQ(commands.front().frame.simulation.particles.size(), 1u);
    EXPECT_EQ(commands.front().frame.simulation.particles.front().label, "p");
}

TEST(ThreadManagementService, RenderThreadConsumesCommandBatchesWithRendererRole) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .enable_background_workers = false
    });

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<RenderThreadCommandKind> seen;
    ThreadRole callback_role = ThreadRole::Unknown;

    ASSERT_TRUE(threads.start_render_thread(
        [&threads, &mutex, &cv, &seen, &callback_role](
            std::stop_token,
            std::span<const RenderThreadCommand> commands) {
            std::scoped_lock lock(mutex);
            callback_role = threads.current_thread_role();
            for (const RenderThreadCommand& command : commands) {
                seen.push_back(command.kind);
            }
            cv.notify_one();
        }));

    ASSERT_TRUE(threads.enqueue_render_command(RenderThreadCommand{
        .kind = RenderThreadCommandKind::Frame
    }));

    std::unique_lock lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 2s, [&seen] { return !seen.empty(); }));
    EXPECT_EQ(callback_role, ThreadRole::Renderer);
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen.front(), RenderThreadCommandKind::Frame);

    lock.unlock();
    threads.stop_render_thread();
}

TEST(ThreadManagementService, RenderTaskRunsOnRendererThreadSynchronously) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .enable_background_workers = false
    });

    ASSERT_TRUE(threads.start_render_thread(
        [](std::stop_token, std::span<const RenderThreadCommand>) {}));

    std::atomic<bool> ran = false;
    std::atomic<bool> renderer_role = false;
    ASSERT_TRUE(threads.run_render_task_sync([&threads, &ran, &renderer_role] {
        ran.store(true);
        renderer_role.store(threads.is_thread_role(ThreadRole::Renderer));
    }));

    EXPECT_TRUE(ran.load());
    EXPECT_TRUE(renderer_role.load());
    threads.stop_render_thread();
}

TEST(ThreadManagementService, RenderThreadDrainsQueuedTaskDuringShutdown) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .enable_background_workers = false
    });

    ASSERT_TRUE(threads.start_render_thread(
        [](std::stop_token, std::span<const RenderThreadCommand>) {}));

    std::atomic<bool> ran = false;
    ASSERT_TRUE(threads.enqueue_render_task([&ran] {
        ran.store(true);
    }));

    threads.shutdown();
    EXPECT_TRUE(ran.load());
}

TEST(ThreadManagementService, RequireThreadRolePassesOnSimulationThread) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
        .worker_count = u32(0),
        .enable_background_workers = false
    });

    std::mutex mutex;
    std::condition_variable cv;
    bool checked = false;
    bool allowed = false;

    ASSERT_TRUE(threads.start_simulation_thread(
        [&threads, &mutex, &cv, &checked, &allowed]
        (std::stop_token, std::span<const SimulationThreadCommand>) {
            {
                std::scoped_lock lock(mutex);
                allowed = threads.require_thread_role(ThreadRole::Simulation, "simulation-only api");
                checked = true;
            }
            cv.notify_one();
        }));

    ASSERT_TRUE(threads.enqueue_simulation_command(SimulationThreadCommand{
        .kind = SimulationThreadCommandKind::Tick
    }));

    std::unique_lock lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 2s, [&checked] { return checked; }));
    EXPECT_TRUE(allowed);
    lock.unlock();

    threads.stop_simulation_thread();
}

TEST(ThreadManagementService, SimulationThreadExceptionBecomesDiagnostic) {
    DiagnosticsService diagnostics;
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
                     .worker_count = u32(0),
                     .enable_background_workers = false
                 },
                 ThreadServiceBindings{.diagnostics = &diagnostics});

    ASSERT_TRUE(threads.start_simulation_thread(
        [](std::stop_token, std::span<const SimulationThreadCommand>) {
            throw std::runtime_error{"simulation boom"};
        }));
    ASSERT_TRUE(threads.enqueue_simulation_command(SimulationThreadCommand{
        .kind = SimulationThreadCommandKind::Tick
    }));

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (diagnostics.active().empty() && std::chrono::steady_clock::now() < deadline) {
        threads.drain_service_mailboxes();
        std::this_thread::sleep_for(5ms);
    }

    threads.stop_simulation_thread();
    threads.drain_service_mailboxes();
    ASSERT_FALSE(diagnostics.active().empty());
    EXPECT_EQ(diagnostics.active().front().code, ErrorCode::ThreadFault);
}

TEST(EventBusService, WorkerThreadTypedPublishReportsThreadRoleViolation) {
    struct WorkerTypedEvent {
        u64 value = u64(1);
    };

    EngineServices services;
    std::atomic<u64> sequence = u64(999);

    const ThreadJobId id = services.threads().submit(
        ThreadJobDescriptor{},
        [&services, &sequence](ThreadJobContext&) {
            events::EventRecord record;
            record.kind = events::EventKind::AppStarted;
            sequence.store(services.events().publish(EventChannelId::Simulation,
                                                     WorkerTypedEvent{},
                                                     record));
        });

    ASSERT_TRUE(wait_for_state(services.threads(), id, ThreadJobState::Completed));
    services.threads().drain_service_mailboxes();

    EXPECT_EQ(sequence.load(), u64(0));

    const auto role_violations =
        services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_NE(role_violations.front().message.find("EventBusService::publish"), std::string::npos);
}

TEST(LoggerService, WorkerThreadDirectWriteReportsThreadRoleViolation) {
    EngineServices services;
    std::atomic<u64> log_id = u64(999);

    const ThreadJobId id = services.threads().submit(
        ThreadJobDescriptor{},
        [&services, &log_id](ThreadJobContext&) {
            log_id.store(services.logger().write(LogSeverity::Info,
                                                 LogCategory::Worker,
                                                 LogSourceRef{},
                                                 "direct worker write").value);
        });

    ASSERT_TRUE(wait_for_state(services.threads(), id, ThreadJobState::Completed));
    services.threads().drain_service_mailboxes();

    EXPECT_EQ(log_id.load(), u64(0));
    EXPECT_TRUE(services.logger().snapshot().empty());

    const auto role_violations =
        services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_NE(role_violations.front().message.find("LoggerService::write"), std::string::npos);
}

TEST(LoggerService, MainThreadDirectWriteReportsThreadRoleViolationWhenLoggerThreadOwnsWrites) {
    EngineServices services;

    const LogRecordId id = services.logger().write(LogSeverity::Info,
                                                   LogCategory::Engine,
                                                   LogSourceRef{},
                                                   "main direct write");
    services.threads().drain_service_mailboxes();

    EXPECT_EQ(id.value, u64(0));
    EXPECT_TRUE(services.logger().snapshot().empty());

    const auto role_violations =
        services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_NE(role_violations.front().message.find("LoggerService::write"), std::string::npos);
    EXPECT_NE(role_violations.front().message.find("expected Logger thread"), std::string::npos);
}

TEST(LoggerService, WorkerMailboxWriteIsAcceptedByLoggerThread) {
    EngineServices services;

    const ThreadJobId id = services.threads().submit(
        ThreadJobDescriptor{},
        [](ThreadJobContext& context) {
            context.log(LogSeverity::Info, LogCategory::Worker, "mailbox worker write");
        });

    ASSERT_TRUE(wait_for_state(services.threads(), id, ThreadJobState::Completed));

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (services.logger().snapshot().empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }

    const auto snapshot = services.logger().snapshot();
    ASSERT_EQ(snapshot.size(), 1u);
    EXPECT_EQ(snapshot.front().message, "mailbox worker write");
    EXPECT_TRUE(services.diagnostics().active_with(ErrorCode::ThreadRoleViolation).empty());
}

TEST(ThreadManagementService, LoggerTaskRunsOnLoggerThreadSynchronously) {
    LoggerService logger;
    logger.init();

    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
                     .worker_count = u32(0),
                     .enable_logger_thread = true,
                     .enable_background_workers = false
                 },
                 ThreadServiceBindings{.logger = &logger});

    std::atomic<bool> ran = false;
    std::atomic<bool> logger_role = false;
    ASSERT_TRUE(threads.run_logger_task_sync([&threads, &ran, &logger_role] {
        ran.store(true);
        logger_role.store(threads.is_thread_role(ThreadRole::Logger));
    }));

    EXPECT_TRUE(ran.load());
    EXPECT_TRUE(logger_role.load());
}

TEST(ThreadManagementService, LoggerThreadDrainsQueuedTaskDuringShutdown) {
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
                     .worker_count = u32(0),
                     .enable_logger_thread = true,
                     .enable_background_workers = false
                 });

    std::atomic<bool> ran = false;
    ASSERT_TRUE(threads.enqueue_logger_task([&ran] {
        ran.store(true);
    }));

    threads.shutdown();
    EXPECT_TRUE(ran.load());
}

TEST(TelemetryService, WorkerThreadExtensionRecordReportsThreadRoleViolation) {
    DiagnosticsService diagnostics;
    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{.worker_count = u32(1)},
                 ThreadServiceBindings{.diagnostics = &diagnostics});

    telemetry::TelemetryService telemetry;
    telemetry.set_owner_guard([&threads](std::string_view api_name) {
        return threads.require_thread_role(ThreadRole::Main, api_name);
    });

    std::atomic<bool> recorded = true;
    const ThreadJobId id = threads.submit(
        ThreadJobDescriptor{},
        [&telemetry, &recorded](ThreadJobContext&) {
            recorded.store(telemetry.record_ext(telemetry::TelemetryExtRecord{}));
        });

    ASSERT_TRUE(wait_for_state(threads, id, ThreadJobState::Completed));
    threads.drain_service_mailboxes();

    EXPECT_FALSE(recorded.load());
    const auto role_violations =
        diagnostics.active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_NE(role_violations.front().message.find("TelemetryService::record_ext"), std::string::npos);
}

TEST(TelemetryService, FlushCanBeHandedToLoggerThread) {
    DiagnosticsService diagnostics;
    LoggerService logger;
    logger.init();

    ThreadManagementService threads;
    threads.init(ThreadPoolConfig{
                     .worker_count = u32(0),
                     .enable_logger_thread = true,
                     .enable_background_workers = false
                 },
                 ThreadServiceBindings{
                     .diagnostics = &diagnostics,
                     .logger = &logger
                 });

    telemetry::TelemetryService telemetry;
    telemetry.set_owner_guard([&threads](std::string_view api_name) {
        if (api_name == "TelemetryService::flush" &&
            threads.is_thread_role(ThreadRole::Logger)) {
            return true;
        }
        return threads.require_thread_role(ThreadRole::Main, api_name);
    });

    std::atomic<u64> flushed = u64(999);
    ASSERT_TRUE(threads.run_logger_task_sync([&telemetry, &flushed] {
        flushed.store(telemetry.flush());
    }));
    threads.drain_service_mailboxes();
    EXPECT_EQ(flushed.load(), u64(0));
    EXPECT_TRUE(diagnostics.active_with(ErrorCode::ThreadRoleViolation).empty());
}

TEST(RenderService, WorkerThreadMutationReportsThreadRoleViolation) {
    EngineServices services;
    std::atomic<bool> registered = true;

    const ThreadJobId id = services.threads().submit(
        ThreadJobDescriptor{},
        [&services, &registered](ThreadJobContext&) {
            RenderViewId view_id = u64(0);
            auto handle = services.render().register_view(
                RenderViewDescriptor{.title = "Worker View"},
                &view_id);
            registered.store(static_cast<bool>(handle));
        });

    ASSERT_TRUE(wait_for_state(services.threads(), id, ThreadJobState::Completed));
    services.threads().drain_service_mailboxes();

    EXPECT_FALSE(registered.load());
    EXPECT_EQ(services.render().active_view_count(), 0u);
    ASSERT_FALSE(services.diagnostics().active().empty());
    EXPECT_EQ(services.diagnostics().active().back().code, ErrorCode::ThreadRoleViolation);
}

TEST(EngineGuiServices, WorkerThreadMutationReportsThreadRoleViolations) {
    EngineServices services;
    std::atomic<bool> panel_registered = true;
    std::atomic<bool> hotkey_registered = true;

    const ThreadJobId id = services.threads().submit(
        ThreadJobDescriptor{},
        [&services, &panel_registered, &hotkey_registered](ThreadJobContext&) {
            auto panel = services.panels().register_panel(PanelDescriptor{
                .title = "Worker Panel",
                .draw = [] {}
            });
            auto hotkey = services.hotkeys().register_action(HotkeyDescriptor{
                .chord = {.key = 77, .mods = 0},
                .label = "Worker Hotkey",
                .callback = [] {}
            });
            services.interaction().set_mouse(u64(42), Vec2{10.f, 20.f}, Vec2{}, true);

            panel_registered.store(static_cast<bool>(panel));
            hotkey_registered.store(static_cast<bool>(hotkey));
        });

    ASSERT_TRUE(wait_for_state(services.threads(), id, ThreadJobState::Completed));
    services.threads().drain_service_mailboxes();

    EXPECT_FALSE(panel_registered.load());
    EXPECT_FALSE(hotkey_registered.load());
    EXPECT_EQ(services.panels().active_count(), 0u);
    EXPECT_EQ(services.hotkeys().active_count(), 0u);
    EXPECT_FALSE(services.interaction().mouse_state(u64(42)).enabled);

    const auto role_violations =
        services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_GE(role_violations.front().occurrence_count, u64(3));
}

TEST(CameraService, WorkerThreadMutationReportsThreadRoleViolation) {
    EngineServices services;
    RenderViewId view_id = u64(0);
    auto view = services.render().register_view(RenderViewDescriptor{
        .title = "Camera View",
        .kind = RenderViewKind::Main
    }, &view_id);
    ASSERT_NE(view_id, u64(0));

    const CameraState before = services.camera().camera(view_id);

    const ThreadJobId id = services.threads().submit(
        ThreadJobDescriptor{},
        [&services](ThreadJobContext&) {
            services.camera().orbit_main(f32(120), f32(10));
        });

    ASSERT_TRUE(wait_for_state(services.threads(), id, ThreadJobState::Completed));
    services.threads().drain_service_mailboxes();

    const CameraState after = services.camera().camera(view_id);
    EXPECT_FLOAT_EQ(after.yaw, before.yaw);
    EXPECT_FLOAT_EQ(after.pitch, before.pitch);

    const auto role_violations =
        services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_NE(role_violations.front().message.find("CameraService::orbit_main"), std::string::npos);
}

TEST(DiagnosticsService, WorkerThreadDirectReportUsesThreadRoleViolation) {
    EngineServices services;
    std::atomic<u64> reported_id = u64(999);

    const ThreadJobId id = services.threads().submit(
        ThreadJobDescriptor{},
        [&services, &reported_id](ThreadJobContext&) {
            const DiagnosticId diagnostic = services.diagnostics().report(DiagnosticReport{
                .severity = DiagnosticSeverity::Error,
                .code = ErrorCode::InvalidParameter,
                .title = "Worker direct diagnostic"
            });
            reported_id.store(diagnostic.value);
        });

    ASSERT_TRUE(wait_for_state(services.threads(), id, ThreadJobState::Completed));
    services.threads().drain_service_mailboxes();

    EXPECT_EQ(reported_id.load(), u64(0));
    EXPECT_TRUE(services.diagnostics().active_with(ErrorCode::InvalidParameter).empty());

    const auto role_violations =
        services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_NE(role_violations.front().message.find("DiagnosticsService::report"), std::string::npos);
}

TEST(EngineRegistryAndInputServices, WorkerThreadMutationReportsThreadRoleViolations) {
    EngineServices services;
    std::atomic<bool> metadata_registered = true;
    std::atomic<bool> view_input_enabled = true;

    const ThreadJobId id = services.threads().submit(
        ThreadJobDescriptor{},
        [&services, &metadata_registered, &view_input_enabled](ThreadJobContext&) {
            metadata_registered.store(services.metadata().register_component(ComponentDescriptor{
                .id = ComponentId{"worker.component"},
                .display_name = "Worker Component",
                .category = ObjectCategory::FieldObject
            }));

            const ViewInputSample sample = services.view_input().update(ViewInputUpdate{
                .view = u64(88),
                .rect = {.origin = Vec2{}, .size = Vec2{100.f, 100.f}},
                .cursor = Vec2{25.f, 25.f}
            });
            view_input_enabled.store(sample.enabled);
        });

    ASSERT_TRUE(wait_for_state(services.threads(), id, ThreadJobState::Completed));
    services.threads().drain_service_mailboxes();

    EXPECT_FALSE(metadata_registered.load());
    EXPECT_EQ(services.metadata().get_descriptor(ComponentId{"worker.component"}), nullptr);
    EXPECT_FALSE(view_input_enabled.load());
    EXPECT_FALSE(services.view_input().sample(u64(88)).enabled);

    const auto role_violations =
        services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_GE(role_violations.front().occurrence_count, u64(2));
}

TEST(ResourceManagerService, WorkerThreadMutationReportsThreadRoleViolation) {
    EngineServices services;
    std::atomic<u64> reserved_id = u64(999);

    const ThreadJobId id = services.threads().submit(
        ThreadJobDescriptor{},
        [&services, &reserved_id](ThreadJobContext&) {
            const ResourceId resource = services.resources().reserve(
                ResourceKind::CpuMesh,
                ResourceOwner::Worker,
                ResourceLifetime::Cache);
            reserved_id.store(resource.value);
        });

    ASSERT_TRUE(wait_for_state(services.threads(), id, ThreadJobState::Completed));
    services.threads().drain_service_mailboxes();

    EXPECT_EQ(reserved_id.load(), u64(0));
    EXPECT_TRUE(services.resources().resources_by_kind(ResourceKind::CpuMesh).empty());

    const auto role_violations =
        services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_NE(role_violations.front().message.find("ResourceManagerService::reserve"), std::string::npos);
}

TEST(CaptureService, WorkerThreadMutationReportsThreadRoleViolation) {
    EngineServices services;

    const ThreadJobId id = services.threads().submit(
        ThreadJobDescriptor{},
        [&services](ThreadJobContext&) {
            services.capture().request_still(CaptureRequest{
                .target = CaptureTarget::MainWindow,
                .include_manifest = false
            }, CaptureRunMetadata{
                .simulation_name = "Worker Capture",
                .run_id = "worker_capture"
            });
        });

    ASSERT_TRUE(wait_for_state(services.threads(), id, ThreadJobState::Completed));
    services.threads().drain_service_mailboxes();

    EXPECT_TRUE(services.capture().completed_artifacts().empty());
    EXPECT_TRUE(services.capture().consume_pending_stills().empty());

    const auto role_violations =
        services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ASSERT_EQ(role_violations.size(), 1u);
    EXPECT_NE(role_violations.front().message.find("CaptureService::request_still"), std::string::npos);
}

TEST(EngineServices, OwnsThreadManagementServiceAndPassesItToSimulationHost) {
    EngineServices services;
    SimulationHost host = services.simulation_host();

    EXPECT_EQ(&host.threads(), &services.threads());
    EXPECT_TRUE(services.threads().initialised());
    EXPECT_TRUE(services.threads().is_main_thread());
}

} // namespace
