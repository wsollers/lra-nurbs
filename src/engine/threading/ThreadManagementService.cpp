#include "engine/threading/ThreadManagementService.hpp"
#include "engine/metricservice/MetricsService.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <format>
#include <memory>

namespace ndde {

namespace {
thread_local ThreadRole t_thread_role = ThreadRole::Unknown;

bool has_capacity(u64 size, u64 capacity) noexcept {
    return capacity == u64(0) || size < capacity;
}

std::string_view role_name(ThreadRole role) noexcept {
    switch (role) {
        case ThreadRole::Main: return "Main";
        case ThreadRole::Gui: return "Gui";
        case ThreadRole::Simulation: return "Simulation";
        case ThreadRole::Renderer: return "Renderer";
        case ThreadRole::Logger: return "Logger";
        case ThreadRole::Worker: return "Worker";
        case ThreadRole::Io: return "Io";
        case ThreadRole::Telemetry: return "Telemetry";
        case ThreadRole::Unknown: return "Unknown";
        default: return "Unknown";
    }
}
} // namespace

ThreadJobContext::ThreadJobContext(ThreadManagementService& service,
                                   ThreadJobId job_id,
                                   std::stop_token stop_token) noexcept
    : m_service(&service)
    , m_job_id(job_id)
    , m_stop_token(stop_token)
{}

void ThreadJobContext::publish_worker_record(events::EventRecord record) const {
    if (!m_service) return;
    record.id_a = record.id_a == u64(0) ? m_job_id.value : record.id_a;
    m_service->enqueue_event(EventChannelId::Worker, record);
}

void ThreadJobContext::report_diagnostic(DiagnosticReport report) const {
    if (!m_service) return;
    if (!report.source.component.has_value()) {
        report.source.component = ids::unknown_component;
    }
    m_service->enqueue_diagnostic(std::move(report));
}

void ThreadJobContext::log(LogSeverity severity,
                           LogCategory category,
                           std::string_view message) const {
    if (!m_service) return;
    m_service->enqueue_log(ThreadManagementService::QueuedLog{
        .severity = severity,
        .category = category,
        .source = {},
        .message = std::string{message}
    });
}

void ThreadJobContext::complete(ThreadJobResult result) const {
    if (!m_service) return;
    m_service->push_result(std::move(result));
}

ThreadManagementService::~ThreadManagementService() {
    shutdown();
}

void ThreadManagementService::init(ThreadPoolConfig config, ThreadServiceBindings bindings) {
    shutdown();

    m_config = config;
    m_bindings = bindings;
    m_main_thread_id = std::this_thread::get_id();
    t_thread_role = ThreadRole::Main;
    m_accepting_jobs = true;
    m_shutting_down = false;
    m_initialised = true;

    u32 worker_count = u32(0);
    if (m_config.enable_background_workers) {
        worker_count = m_config.worker_count == u32(0)
            ? default_worker_count()
            : m_config.worker_count;
    }

    m_workers.reserve(worker_count);
    for (u32 index = u32(0); index < worker_count; ++index) {
        m_workers.emplace_back([this, worker_index = u64(index + u32(1))]
                               (std::stop_token stop) noexcept {
            worker_loop(stop, worker_index);
        });
    }

    if (m_config.enable_logger_thread) {
        m_logger_thread.emplace([this](std::stop_token stop) noexcept {
            logger_loop(stop);
        });
    }
}

void ThreadManagementService::shutdown() noexcept {
    {
        std::scoped_lock lock(m_mutex);
        if (!m_initialised && m_workers.empty()) return;
        m_accepting_jobs = false;
        m_shutting_down = true;
        for (JobRecord& job : m_jobs) {
            if (job.status.cancellable &&
                (job.status.state == ThreadJobState::Queued ||
                 job.status.state == ThreadJobState::Running ||
                 job.status.state == ThreadJobState::CancelRequested)) {
                job.stop_source.request_stop();
                job.status.state = ThreadJobState::CancelRequested;
            }
        }
    }

    m_work_available.notify_all();
    m_log_available.notify_all();
    m_simulation_available.notify_all();
    m_render_available.notify_all();
    if (m_simulation_thread) {
        m_simulation_thread->request_stop();
        m_simulation_available.notify_all();
        m_simulation_thread.reset();
    }
    if (m_render_thread) {
        m_render_thread->request_stop();
        m_render_available.notify_all();
        m_render_thread.reset();
    }
    if (m_logger_thread) {
        m_logger_thread->request_stop();
        m_log_available.notify_all();
        m_logger_thread.reset();
    }
    m_workers.clear();

    {
        std::scoped_lock lock(m_mutex);
        for (JobRecord& job : m_jobs) {
            if (job.status.state == ThreadJobState::Queued ||
                job.status.state == ThreadJobState::CancelRequested) {
                job.status.state = ThreadJobState::Cancelled;
            }
        }
        m_pending.clear();
        m_simulation_commands.clear();
        m_render_commands.clear();
        m_render_tasks.clear();
        m_logger_tasks.clear();
        m_simulation_step = {};
        m_render_step = {};
        m_shutting_down = false;
        m_initialised = false;
    }
}

ThreadJobId ThreadManagementService::submit(ThreadJobDescriptor descriptor, ThreadJobFn job) {
    if (!job) return {};

    std::scoped_lock lock(m_mutex);
    if (!m_initialised || !m_accepting_jobs) return {};
    if (!has_capacity(static_cast<u64>(m_pending.size()), m_config.max_queued_jobs)) {
        ++m_dropped_events;
        return {};
    }

    const ThreadJobId id = descriptor.id.value == u64(0)
        ? ThreadJobId{m_next_job_id++}
        : descriptor.id;

    JobRecord record;
    record.status = ThreadJobStatus{
        .id = id,
        .owner = descriptor.owner,
        .node = descriptor.node,
        .priority = descriptor.priority,
        .state = ThreadJobState::Queued,
        .cancellable = descriptor.cancellable,
        .queued_order = m_next_queue_order++
    };

    m_jobs.push_back(std::move(record));
    m_pending.push_back(PendingJob{.id = id, .fn = std::move(job)});
    if (m_bindings.metrics) {
        m_bindings.metrics->increment(MetricId::JobsSubmitted);
    }
    m_work_available.notify_one();
    return id;
}

void ThreadManagementService::request_cancel(ThreadJobId id) noexcept {
    std::scoped_lock lock(m_mutex);
    JobRecord* job = find_job_locked(id);
    if (!job || !job->status.cancellable) return;
    if (job->status.state == ThreadJobState::Completed ||
        job->status.state == ThreadJobState::Cancelled ||
        job->status.state == ThreadJobState::Failed) {
        return;
    }
    job->stop_source.request_stop();
    job->status.state = ThreadJobState::CancelRequested;
}

void ThreadManagementService::request_cancel_all() noexcept {
    std::scoped_lock lock(m_mutex);
    for (JobRecord& job : m_jobs) {
        if (!job.status.cancellable) continue;
        if (job.status.state == ThreadJobState::Completed ||
            job.status.state == ThreadJobState::Cancelled ||
            job.status.state == ThreadJobState::Failed) {
            continue;
        }
        job.stop_source.request_stop();
        job.status.state = ThreadJobState::CancelRequested;
    }
}

std::optional<ThreadJobStatus> ThreadManagementService::status(ThreadJobId id) const {
    std::scoped_lock lock(m_mutex);
    const JobRecord* job = find_job_locked(id);
    if (!job) return std::nullopt;
    return job->status;
}

std::span<const ThreadJobStatus> ThreadManagementService::jobs() const {
    std::scoped_lock lock(m_mutex);
    m_job_status_view.clear();
    m_job_status_view.reserve(m_jobs.size());
    for (const JobRecord& job : m_jobs) {
        m_job_status_view.push_back(job.status);
    }
    return m_job_status_view;
}

std::vector<ThreadJobResult> ThreadManagementService::consume_completed_results() {
    std::scoped_lock lock(m_mutex);
    std::vector<ThreadJobResult> out;
    out.swap(m_completed_results);
    return out;
}

bool ThreadManagementService::enqueue_simulation_command(SimulationThreadCommand command) {
    std::scoped_lock lock(m_mutex);
    if (!has_capacity(static_cast<u64>(m_simulation_commands.size()), m_config.max_queued_jobs)) {
        ++m_dropped_events;
        return false;
    }
    m_simulation_commands.push_back(command);
    m_simulation_available.notify_one();
    return true;
}

std::vector<SimulationThreadCommand> ThreadManagementService::consume_simulation_commands() {
    std::scoped_lock lock(m_mutex);
    std::vector<SimulationThreadCommand> out;
    out.swap(m_simulation_commands);
    return out;
}

void ThreadManagementService::publish_event_record(EventChannelId channel, events::EventRecord record) {
    enqueue_event(channel, record);
}

bool ThreadManagementService::enqueue_logger_task(std::function<void()> task) {
    if (!task) return false;
    std::scoped_lock lock(m_mutex);
    if (!m_initialised ||
        !has_capacity(static_cast<u64>(m_logger_tasks.size()), m_config.max_mailbox_records)) {
        ++m_dropped_logs;
        return false;
    }
    m_logger_tasks.push_back(std::move(task));
    m_log_available.notify_one();
    return true;
}

bool ThreadManagementService::run_logger_task_sync(std::function<void()> task) {
    if (!task) return false;
    if (is_thread_role(ThreadRole::Logger) || !m_logger_thread) {
        task();
        return true;
    }

    auto done = std::make_shared<std::promise<void>>();
    std::future<void> future = done->get_future();
    const bool queued = enqueue_logger_task([task = std::move(task), done] {
        try {
            task();
            done->set_value();
        } catch (...) {
            done->set_exception(std::current_exception());
        }
    });
    if (!queued) return false;
    future.get();
    return true;
}

bool ThreadManagementService::start_simulation_thread(SimulationThreadFn step) {
    if (!step) return false;

    std::scoped_lock lock(m_mutex);
    if (!m_initialised || m_simulation_thread.has_value()) {
        return false;
    }

    m_simulation_step = std::move(step);
    m_simulation_thread.emplace([this](std::stop_token stop) noexcept {
        simulation_loop(stop);
    });
    return true;
}

void ThreadManagementService::stop_simulation_thread() noexcept {
    std::optional<std::jthread> thread;
    {
        std::scoped_lock lock(m_mutex);
        thread.swap(m_simulation_thread);
        m_simulation_step = {};
    }
    if (thread) {
        thread->request_stop();
        m_simulation_available.notify_all();
        thread.reset();
    }
}

bool ThreadManagementService::enqueue_render_command(RenderThreadCommand command) {
    std::scoped_lock lock(m_mutex);
    if (!has_capacity(static_cast<u64>(m_render_commands.size()), m_config.max_queued_jobs)) {
        ++m_dropped_events;
        return false;
    }
    m_render_commands.push_back(std::move(command));
    m_render_available.notify_one();
    return true;
}

std::vector<RenderThreadCommand> ThreadManagementService::consume_render_commands() {
    std::scoped_lock lock(m_mutex);
    std::vector<RenderThreadCommand> out;
    out.swap(m_render_commands);
    return out;
}

bool ThreadManagementService::enqueue_render_task(std::function<void()> task) {
    if (!task) return false;
    std::scoped_lock lock(m_mutex);
    if (!m_initialised ||
        !has_capacity(static_cast<u64>(m_render_tasks.size()), m_config.max_mailbox_records)) {
        ++m_dropped_events;
        return false;
    }
    m_render_tasks.push_back(std::move(task));
    m_render_available.notify_one();
    return true;
}

bool ThreadManagementService::run_render_task_sync(std::function<void()> task) {
    if (!task) return false;
    if (is_thread_role(ThreadRole::Renderer) || !m_render_thread) {
        task();
        return true;
    }

    auto done = std::make_shared<std::promise<void>>();
    std::future<void> future = done->get_future();
    const bool queued = enqueue_render_task([task = std::move(task), done] {
        try {
            task();
            done->set_value();
        } catch (...) {
            done->set_exception(std::current_exception());
        }
    });
    if (!queued) return false;
    future.get();
    return true;
}

bool ThreadManagementService::start_render_thread(RenderThreadFn step) {
    if (!step) return false;

    std::scoped_lock lock(m_mutex);
    if (!m_initialised || m_render_thread.has_value()) {
        return false;
    }

    m_render_step = std::move(step);
    m_render_thread.emplace([this](std::stop_token stop) noexcept {
        render_loop(stop);
    });
    return true;
}

void ThreadManagementService::stop_render_thread() noexcept {
    std::optional<std::jthread> thread;
    {
        std::scoped_lock lock(m_mutex);
        thread.swap(m_render_thread);
        m_render_step = {};
    }
    if (thread) {
        thread->request_stop();
        m_render_available.notify_all();
        thread.reset();
    }
}

void ThreadManagementService::publish_simulation_snapshot(SimulationRenderSnapshot snapshot) {
    std::scoped_lock lock(m_mutex);
    m_latest_simulation_snapshot = std::move(snapshot);
}

std::optional<SimulationRenderSnapshot> ThreadManagementService::latest_simulation_snapshot() const {
    std::scoped_lock lock(m_mutex);
    return m_latest_simulation_snapshot;
}

void ThreadManagementService::drain_service_mailboxes() {
    std::vector<QueuedLog> logs;
    std::vector<DiagnosticReport> diagnostics;
    std::vector<QueuedEvent> events;
    {
        std::scoped_lock lock(m_mutex);
        logs.swap(m_log_mailbox);
        diagnostics.swap(m_diagnostic_mailbox);
        events.swap(m_event_mailbox);
    }

    if (m_bindings.logger && !m_logger_thread) {
        for (const QueuedLog& log : logs) {
            (void)m_bindings.logger->write(log.severity, log.category, log.source, log.message);
        }
    }

    if (m_bindings.diagnostics) {
        for (DiagnosticReport& report : diagnostics) {
            (void)m_bindings.diagnostics->report(std::move(report));
        }
    }

    if (m_bindings.events) {
        std::array<bool, static_cast<std::size_t>(EventChannelId::Count)> touched_channels{};
        for (QueuedEvent& event : events) {
            (void)m_bindings.events->enqueue_record(event.channel, event.record);
            touched_channels[static_cast<std::size_t>(event.channel)] = true;
        }
        for (std::size_t i = 0; i < touched_channels.size(); ++i) {
            if (touched_channels[i]) {
                (void)m_bindings.events->drain_mailbox(static_cast<EventChannelId>(i));
            }
        }
    }
}

ThreadStats ThreadManagementService::stats() const {
    std::scoped_lock lock(m_mutex);
    return ThreadStats{
        .worker_count = static_cast<u32>(m_workers.size()),
        .queued_jobs = static_cast<u64>(m_pending.size()),
        .completed_results = static_cast<u64>(m_completed_results.size()),
        .dropped_results = m_dropped_results,
        .dropped_logs = m_dropped_logs,
        .dropped_diagnostics = m_dropped_diagnostics,
        .dropped_events = m_dropped_events
    };
}

bool ThreadManagementService::is_main_thread() const noexcept {
    return std::this_thread::get_id() == m_main_thread_id;
}

ThreadRole ThreadManagementService::current_thread_role() const noexcept {
    return t_thread_role;
}

bool ThreadManagementService::is_thread_role(ThreadRole expected) const noexcept {
    return current_thread_role() == expected;
}

bool ThreadManagementService::require_thread_role(ThreadRole expected, std::string_view api_name) {
    const ThreadRole actual = current_thread_role();
    if (actual == expected) {
        return true;
    }

    DiagnosticReport report;
    report.severity = DiagnosticSeverity::Error;
    report.lifetime = DiagnosticLifetime::Frame;
    report.code = ErrorCode::ThreadRoleViolation;
    report.source.subsystem = DiagnosticSubsystem::Threading;
    report.title = "Thread role violation";
    report.message = std::format(
        "{} expected {} thread but was called on {} thread",
        api_name.empty() ? std::string_view{"Unnamed API"} : api_name,
        role_name(expected),
        role_name(actual));
    report.suggested_fix = "Route the call through the owning thread's command queue, mailbox, or immutable snapshot.";
    report.facts.push_back(DiagnosticFact{.key = "expected_role", .value = std::string{role_name(expected)}});
    report.facts.push_back(DiagnosticFact{.key = "actual_role", .value = std::string{role_name(actual)}});
    enqueue_diagnostic(std::move(report));
    return false;
}

bool ThreadManagementService::simulation_thread_running() const noexcept {
    std::scoped_lock lock(m_mutex);
    return m_simulation_thread.has_value();
}

bool ThreadManagementService::render_thread_running() const noexcept {
    std::scoped_lock lock(m_mutex);
    return m_render_thread.has_value();
}

void ThreadManagementService::worker_loop(std::stop_token service_stop, u64 worker_index) noexcept {
    t_thread_role = ThreadRole::Worker;
    std::optional<MetricsThreadScope> metrics_scope;
    if (m_bindings.metrics) {
        metrics_scope.emplace(*m_bindings.metrics, ThreadRole::Worker);
    }
    while (!service_stop.stop_requested()) {
        PendingJob pending = wait_for_job(service_stop);
        if (!pending.fn) return;

        JobRecord* job_record = nullptr;
        std::stop_token job_stop;
        {
            std::scoped_lock lock(m_mutex);
            job_record = find_job_locked(pending.id);
            if (!job_record) continue;
            if (job_record->status.state == ThreadJobState::CancelRequested ||
                job_record->stop_source.stop_requested()) {
                job_record->status.state = ThreadJobState::Cancelled;
                continue;
            }
            job_record->status.state = ThreadJobState::Running;
            job_record->status.worker_index = worker_index;
            job_stop = job_record->stop_source.get_token();
        }

        ThreadJobContext context{*this, pending.id, job_stop};
        try {
            pending.fn(context);
            std::scoped_lock lock(m_mutex);
            JobRecord* finished = find_job_locked(pending.id);
            if (finished) {
                finished->status.state = finished->stop_source.stop_requested()
                    ? ThreadJobState::Cancelled
                    : ThreadJobState::Completed;
                if (m_bindings.metrics) {
                    m_bindings.metrics->increment(finished->status.state == ThreadJobState::Cancelled
                        ? MetricId::JobsCancelled
                        : MetricId::JobsCompleted);
                }
            }
        } catch (const std::exception& ex) {
            set_state(pending.id, ThreadJobState::Failed, worker_index);
            if (m_bindings.metrics) {
                m_bindings.metrics->increment(MetricId::JobsFailed);
            }
            report_thread_fault(pending.id, ex.what());
        } catch (...) {
            set_state(pending.id, ThreadJobState::Failed, worker_index);
            if (m_bindings.metrics) {
                m_bindings.metrics->increment(MetricId::JobsFailed);
            }
            report_thread_fault(pending.id, "worker job threw an unknown exception");
        }
    }
}

void ThreadManagementService::logger_loop(std::stop_token service_stop) noexcept {
    t_thread_role = ThreadRole::Logger;
    std::optional<MetricsThreadScope> metrics_scope;
    if (m_bindings.metrics) {
        metrics_scope.emplace(*m_bindings.metrics, ThreadRole::Logger);
    }
    while (!service_stop.stop_requested()) {
        LoggerWork work = wait_for_logger_work(service_stop);
        if (work.logs.empty() && work.tasks.empty()) {
            continue;
        }
        if (m_bindings.logger) {
            for (const QueuedLog& log : work.logs) {
                (void)m_bindings.logger->write(log.severity, log.category, log.source, log.message);
            }
            m_bindings.logger->drain_sinks();
        }
        for (auto& task : work.tasks) {
            try {
                task();
            } catch (const std::exception& ex) {
                report_thread_fault(ThreadJobId{}, ex.what());
            } catch (...) {
                report_thread_fault(ThreadJobId{}, "logger task threw an unknown exception");
            }
        }
    }

    for (;;) {
        LoggerWork work;
        {
            std::scoped_lock lock(m_mutex);
            if (m_log_mailbox.empty() && m_logger_tasks.empty()) {
                break;
            }
            work.logs.swap(m_log_mailbox);
            work.tasks.swap(m_logger_tasks);
        }
        if (m_bindings.logger) {
            for (const QueuedLog& log : work.logs) {
                (void)m_bindings.logger->write(log.severity, log.category, log.source, log.message);
            }
            m_bindings.logger->drain_sinks();
        }
        for (auto& task : work.tasks) {
            try {
                task();
            } catch (const std::exception& ex) {
                report_thread_fault(ThreadJobId{}, ex.what());
            } catch (...) {
                report_thread_fault(ThreadJobId{}, "logger task threw an unknown exception");
            }
        }
    }
}

void ThreadManagementService::simulation_loop(std::stop_token service_stop) noexcept {
    t_thread_role = ThreadRole::Simulation;
    std::optional<MetricsThreadScope> metrics_scope;
    if (m_bindings.metrics) {
        metrics_scope.emplace(*m_bindings.metrics, ThreadRole::Simulation);
    }
    while (!service_stop.stop_requested()) {
        std::vector<SimulationThreadCommand> commands = wait_for_simulation_commands(service_stop);
        if (commands.empty()) {
            continue;
        }

        SimulationThreadFn step;
        {
            std::scoped_lock lock(m_mutex);
            step = m_simulation_step;
        }
        if (!step) {
            continue;
        }

        try {
            step(service_stop, commands);
        } catch (const std::exception& ex) {
            report_thread_fault(ThreadJobId{}, ex.what());
        } catch (...) {
            report_thread_fault(ThreadJobId{}, "simulation thread callback threw an unknown exception");
        }
    }
}

void ThreadManagementService::render_loop(std::stop_token service_stop) noexcept {
    t_thread_role = ThreadRole::Renderer;
    std::optional<MetricsThreadScope> metrics_scope;
    if (m_bindings.metrics) {
        metrics_scope.emplace(*m_bindings.metrics, ThreadRole::Renderer);
    }
    while (!service_stop.stop_requested()) {
        RenderWork work = wait_for_render_work(service_stop);
        if (work.commands.empty() && work.tasks.empty()) {
            continue;
        }

        for (auto& task : work.tasks) {
            try {
                task();
            } catch (const std::exception& ex) {
                report_thread_fault(ThreadJobId{}, ex.what());
            } catch (...) {
                report_thread_fault(ThreadJobId{}, "render thread task threw an unknown exception");
            }
        }

        RenderThreadFn step;
        {
            std::scoped_lock lock(m_mutex);
            step = m_render_step;
        }
        if (!step || work.commands.empty()) {
            continue;
        }

        try {
            step(service_stop, work.commands);
        } catch (const std::exception& ex) {
            report_thread_fault(ThreadJobId{}, ex.what());
        } catch (...) {
            report_thread_fault(ThreadJobId{}, "render thread callback threw an unknown exception");
        }
    }

    for (;;) {
        RenderWork work;
        {
            std::scoped_lock lock(m_mutex);
            if (m_render_commands.empty() && m_render_tasks.empty()) {
                break;
            }
            work.commands.swap(m_render_commands);
            work.tasks.swap(m_render_tasks);
        }
        for (auto& task : work.tasks) {
            try {
                task();
            } catch (const std::exception& ex) {
                report_thread_fault(ThreadJobId{}, ex.what());
            } catch (...) {
                report_thread_fault(ThreadJobId{}, "render thread task threw an unknown exception during shutdown");
            }
        }
    }
}

ThreadManagementService::PendingJob
ThreadManagementService::wait_for_job(std::stop_token service_stop) {
    std::unique_lock lock(m_mutex);
    m_work_available.wait(lock, [this, service_stop] {
        return service_stop.stop_requested() || m_shutting_down || !m_pending.empty();
    });
    if (service_stop.stop_requested() || m_shutting_down || m_pending.empty()) {
        return {};
    }

    auto best = m_pending.begin();
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        const JobRecord* lhs = find_job_locked(it->id);
        const JobRecord* rhs = find_job_locked(best->id);
        if (!lhs || !rhs) continue;
        if (static_cast<u8>(lhs->status.priority) > static_cast<u8>(rhs->status.priority)) {
            best = it;
        }
    }

    PendingJob job = std::move(*best);
    m_pending.erase(best);
    return job;
}

ThreadManagementService::LoggerWork
ThreadManagementService::wait_for_logger_work(std::stop_token service_stop) {
    std::unique_lock lock(m_mutex);
    m_log_available.wait(lock, [this, service_stop] {
        return service_stop.stop_requested() || !m_log_mailbox.empty() || !m_logger_tasks.empty();
    });

    LoggerWork work;
    work.logs.swap(m_log_mailbox);
    work.tasks.swap(m_logger_tasks);
    return work;
}

std::vector<SimulationThreadCommand>
ThreadManagementService::wait_for_simulation_commands(std::stop_token service_stop) {
    std::unique_lock lock(m_mutex);
    m_simulation_available.wait(lock, [this, service_stop] {
        return service_stop.stop_requested() || m_shutting_down || !m_simulation_commands.empty();
    });

    std::vector<SimulationThreadCommand> commands;
    commands.swap(m_simulation_commands);
    return commands;
}

ThreadManagementService::RenderWork
ThreadManagementService::wait_for_render_work(std::stop_token service_stop) {
    std::unique_lock lock(m_mutex);
    m_render_available.wait(lock, [this, service_stop] {
        return service_stop.stop_requested() || m_shutting_down ||
               !m_render_commands.empty() || !m_render_tasks.empty();
    });

    RenderWork work;
    work.commands.swap(m_render_commands);
    work.tasks.swap(m_render_tasks);
    return work;
}

ThreadManagementService::JobRecord*
ThreadManagementService::find_job_locked(ThreadJobId id) noexcept {
    auto it = std::find_if(m_jobs.begin(), m_jobs.end(), [id](const JobRecord& job) {
        return job.status.id == id;
    });
    return it == m_jobs.end() ? nullptr : &*it;
}

const ThreadManagementService::JobRecord*
ThreadManagementService::find_job_locked(ThreadJobId id) const noexcept {
    auto it = std::find_if(m_jobs.begin(), m_jobs.end(), [id](const JobRecord& job) {
        return job.status.id == id;
    });
    return it == m_jobs.end() ? nullptr : &*it;
}

u32 ThreadManagementService::default_worker_count() noexcept {
    const u32 hardware = static_cast<u32>(std::max(1u, std::thread::hardware_concurrency()));
    if (hardware <= u32(2)) return u32(1);
    return std::min<u32>(hardware - u32(1), u32(8));
}

void ThreadManagementService::set_state(ThreadJobId id,
                                        ThreadJobState state,
                                        u64 worker_index) noexcept {
    std::scoped_lock lock(m_mutex);
    JobRecord* job = find_job_locked(id);
    if (!job) return;
    job->status.state = state;
    if (worker_index != u64(0)) {
        job->status.worker_index = worker_index;
    }
}

void ThreadManagementService::push_result(ThreadJobResult result) {
    std::scoped_lock lock(m_mutex);
    if (!has_capacity(static_cast<u64>(m_completed_results.size()), m_config.max_completed_results)) {
        ++m_dropped_results;
        return;
    }
    m_completed_results.push_back(std::move(result));
}

void ThreadManagementService::enqueue_log(QueuedLog log) {
    std::scoped_lock lock(m_mutex);
    if (!has_capacity(static_cast<u64>(m_log_mailbox.size()), m_config.max_mailbox_records)) {
        ++m_dropped_logs;
        return;
    }
    m_log_mailbox.push_back(std::move(log));
    m_log_available.notify_one();
}

void ThreadManagementService::enqueue_diagnostic(DiagnosticReport report) {
    std::scoped_lock lock(m_mutex);
    if (!has_capacity(static_cast<u64>(m_diagnostic_mailbox.size()), m_config.max_mailbox_records)) {
        ++m_dropped_diagnostics;
        return;
    }
    m_diagnostic_mailbox.push_back(std::move(report));
}

void ThreadManagementService::enqueue_event(EventChannelId channel, events::EventRecord record) {
    std::scoped_lock lock(m_mutex);
    if (!has_capacity(static_cast<u64>(m_event_mailbox.size()), m_config.max_mailbox_records)) {
        ++m_dropped_events;
        return;
    }
    m_event_mailbox.push_back(QueuedEvent{.channel = channel, .record = record});
}

void ThreadManagementService::report_thread_fault(ThreadJobId id, std::string message) noexcept {
    DiagnosticReport report;
    report.severity = DiagnosticSeverity::Error;
    report.lifetime = DiagnosticLifetime::UntilResolved;
    report.code = ErrorCode::ThreadFault;
    report.source.subsystem = DiagnosticSubsystem::Threading;
    report.title = "Worker job failed";
    report.message = std::format("Worker job {} failed: {}", id.value, message);
    report.suggested_fix = "Inspect the job owner and make the worker path cancellable and exception-safe.";
    enqueue_diagnostic(std::move(report));
}

} // namespace ndde
