#pragma once
// engine/threading/ThreadManagementService.hpp
// Engine-owned worker lifecycle and cross-thread service mailbox coordinator.

#include "engine/diagnostics/DiagnosticsService.hpp"
#include "engine/events/EventBusService.hpp"
#include "engine/logging/LoggerService.hpp"
#include "engine/threading/ThreadTypes.hpp"

#include <condition_variable>
#include <deque>
#include <future>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <thread>
#include <vector>

namespace ndde {

class ThreadManagementService;
class MetricsService;

class ThreadJobContext {
public:
    ThreadJobContext() = default;

    [[nodiscard]] ThreadJobId job_id() const noexcept { return m_job_id; }
    [[nodiscard]] std::stop_token stop_token() const noexcept { return m_stop_token; }
    [[nodiscard]] bool stop_requested() const noexcept { return m_stop_token.stop_requested(); }

    void publish_worker_record(events::EventRecord record) const;
    void report_diagnostic(DiagnosticReport report) const;
    void log(LogSeverity severity, LogCategory category, std::string_view message) const;
    void complete(ThreadJobResult result) const;

    template <class Result>
    void complete(Result result) const {
        complete(ThreadJobResult{std::move(result)});
    }

private:
    friend class ThreadManagementService;

    ThreadJobContext(ThreadManagementService& service,
                     ThreadJobId job_id,
                     std::stop_token stop_token) noexcept;

    ThreadManagementService* m_service = nullptr;
    ThreadJobId m_job_id = {};
    std::stop_token m_stop_token;
};

using ThreadJobFn = std::function<void(ThreadJobContext&)>;
using SimulationThreadFn = std::function<void(std::stop_token, std::span<const SimulationThreadCommand>)>;
using RenderThreadFn = std::function<void(std::stop_token, std::span<const RenderThreadCommand>)>;

struct ThreadServiceBindings {
    DiagnosticsService* diagnostics = nullptr;
    EventBusService* events = nullptr;
    LoggerService* logger = nullptr;
    MetricsService* metrics = nullptr;
};

class ThreadManagementService {
public:
    ThreadManagementService() = default;
    ~ThreadManagementService();

    ThreadManagementService(const ThreadManagementService&) = delete;
    ThreadManagementService& operator=(const ThreadManagementService&) = delete;
    ThreadManagementService(ThreadManagementService&&) = delete;
    ThreadManagementService& operator=(ThreadManagementService&&) = delete;

    void init(ThreadPoolConfig config = {}, ThreadServiceBindings bindings = {});
    void shutdown() noexcept;

    [[nodiscard]] ThreadJobId submit(ThreadJobDescriptor descriptor, ThreadJobFn job);

    void request_cancel(ThreadJobId id) noexcept;
    void request_cancel_all() noexcept;

    [[nodiscard]] std::optional<ThreadJobStatus> status(ThreadJobId id) const;
    [[nodiscard]] std::span<const ThreadJobStatus> jobs() const;
    [[nodiscard]] std::vector<ThreadJobResult> consume_completed_results();

    [[nodiscard]] bool enqueue_simulation_command(SimulationThreadCommand command);
    [[nodiscard]] std::vector<SimulationThreadCommand> consume_simulation_commands();
    void publish_event_record(EventChannelId channel, events::EventRecord record);
    [[nodiscard]] bool enqueue_logger_task(std::function<void()> task);
    [[nodiscard]] bool run_logger_task_sync(std::function<void()> task);
    [[nodiscard]] bool start_simulation_thread(SimulationThreadFn step);
    void stop_simulation_thread() noexcept;
    [[nodiscard]] bool enqueue_render_command(RenderThreadCommand command);
    [[nodiscard]] std::vector<RenderThreadCommand> consume_render_commands();
    [[nodiscard]] bool enqueue_render_task(std::function<void()> task);
    [[nodiscard]] bool run_render_task_sync(std::function<void()> task);
    [[nodiscard]] bool start_render_thread(RenderThreadFn step);
    void stop_render_thread() noexcept;
    void publish_simulation_snapshot(SimulationRenderSnapshot snapshot);
    [[nodiscard]] std::optional<SimulationRenderSnapshot> latest_simulation_snapshot() const;

    void drain_service_mailboxes();

    [[nodiscard]] ThreadStats stats() const;
    [[nodiscard]] bool initialised() const noexcept { return m_initialised; }
    [[nodiscard]] bool is_main_thread() const noexcept;
    [[nodiscard]] ThreadRole current_thread_role() const noexcept;
    [[nodiscard]] bool is_thread_role(ThreadRole expected) const noexcept;
    [[nodiscard]] bool require_thread_role(ThreadRole expected, std::string_view api_name);
    [[nodiscard]] bool simulation_thread_running() const noexcept;
    [[nodiscard]] bool render_thread_running() const noexcept;
    [[nodiscard]] u64 dropped_results() const noexcept { return m_dropped_results; }
    [[nodiscard]] u64 dropped_logs() const noexcept { return m_dropped_logs; }
    [[nodiscard]] u64 dropped_diagnostics() const noexcept { return m_dropped_diagnostics; }
    [[nodiscard]] u64 dropped_events() const noexcept { return m_dropped_events; }

private:
    struct PendingJob {
        ThreadJobId id = {};
        ThreadJobFn fn;
    };

    struct JobRecord {
        ThreadJobStatus status;
        std::stop_source stop_source;
    };

    struct QueuedLog {
        LogSeverity severity = LogSeverity::Info;
        LogCategory category = LogCategory::Worker;
        LogSourceRef source = {};
        std::string message;
    };

    struct QueuedEvent {
        EventChannelId channel = EventChannelId::Worker;
        events::EventRecord record;
    };

    struct LoggerWork {
        std::vector<QueuedLog> logs;
        std::vector<std::function<void()>> tasks;
    };

    struct RenderWork {
        std::vector<RenderThreadCommand> commands;
        std::vector<std::function<void()>> tasks;
    };

    mutable std::mutex m_mutex;
    mutable std::vector<ThreadJobStatus> m_job_status_view;
    std::condition_variable m_work_available;
    std::condition_variable m_log_available;
    std::condition_variable m_simulation_available;
    std::condition_variable m_render_available;
    std::deque<PendingJob> m_pending;
    std::vector<JobRecord> m_jobs;
    std::vector<std::jthread> m_workers;
    std::optional<std::jthread> m_logger_thread;
    std::optional<std::jthread> m_simulation_thread;
    std::optional<std::jthread> m_render_thread;
    SimulationThreadFn m_simulation_step;
    RenderThreadFn m_render_step;
    std::vector<ThreadJobResult> m_completed_results;
    std::vector<SimulationThreadCommand> m_simulation_commands;
    std::vector<RenderThreadCommand> m_render_commands;
    std::vector<std::function<void()>> m_render_tasks;
    std::optional<SimulationRenderSnapshot> m_latest_simulation_snapshot;
    std::vector<QueuedLog> m_log_mailbox;
    std::vector<std::function<void()>> m_logger_tasks;
    std::vector<DiagnosticReport> m_diagnostic_mailbox;
    std::vector<QueuedEvent> m_event_mailbox;
    ThreadPoolConfig m_config;
    ThreadServiceBindings m_bindings;
    std::thread::id m_main_thread_id;
    u64 m_next_job_id = u64(1);
    u64 m_next_queue_order = u64(1);
    u64 m_dropped_results = u64(0);
    u64 m_dropped_logs = u64(0);
    u64 m_dropped_diagnostics = u64(0);
    u64 m_dropped_events = u64(0);
    bool m_accepting_jobs = false;
    bool m_shutting_down = false;
    bool m_initialised = false;

    void worker_loop(std::stop_token service_stop, u64 worker_index) noexcept;
    void logger_loop(std::stop_token service_stop) noexcept;
    void simulation_loop(std::stop_token service_stop) noexcept;
    void render_loop(std::stop_token service_stop) noexcept;

    [[nodiscard]] PendingJob wait_for_job(std::stop_token service_stop);
    [[nodiscard]] LoggerWork wait_for_logger_work(std::stop_token service_stop);
    [[nodiscard]] std::vector<SimulationThreadCommand> wait_for_simulation_commands(std::stop_token service_stop);
    [[nodiscard]] RenderWork wait_for_render_work(std::stop_token service_stop);
    [[nodiscard]] JobRecord* find_job_locked(ThreadJobId id) noexcept;
    [[nodiscard]] const JobRecord* find_job_locked(ThreadJobId id) const noexcept;
    [[nodiscard]] static u32 default_worker_count() noexcept;

    void set_state(ThreadJobId id, ThreadJobState state, u64 worker_index = u64(0)) noexcept;
    void push_result(ThreadJobResult result);
    void enqueue_log(QueuedLog log);
    void enqueue_diagnostic(DiagnosticReport report);
    void enqueue_event(EventChannelId channel, events::EventRecord record);
    void report_thread_fault(ThreadJobId id, std::string message) noexcept;

    friend class ThreadJobContext;
};

} // namespace ndde
