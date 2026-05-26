#include "engine/metricservice/MetricsService.hpp"

#include <algorithm>

namespace ndde {

namespace {
thread_local MetricsThreadContext* t_metrics_context = nullptr;

[[nodiscard]] f64 ns_to_ms(std::chrono::nanoseconds duration) noexcept {
    return static_cast<f64>(duration.count()) / 1'000'000.0;
}

[[nodiscard]] MetricId lock_wait_metric(LockId id) noexcept {
    switch (id) {
        case LockId::SimulationRuntime: return MetricId::RuntimeLockWaitNs;
        default: return MetricId::RuntimeLockWaitNs;
    }
}

[[nodiscard]] MetricId lock_hold_metric(LockId id) noexcept {
    switch (id) {
        case LockId::SimulationRuntime: return MetricId::RuntimeLockHoldNs;
        default: return MetricId::RuntimeLockHoldNs;
    }
}
} // namespace

void MetricsThreadContext::increment(MetricId id, u64 amount) noexcept {
    const u32 slot = static_cast<u32>(id);
    if (slot >= metric_count) return;
    std::scoped_lock lock(m_mutex);
    m_counters[slot] += amount;
}

void MetricsThreadContext::record_duration(MetricId id, std::chrono::nanoseconds duration) noexcept {
    const u32 slot = static_cast<u32>(id);
    if (slot >= metric_count) return;
    const f64 value = metric_descriptors[slot].unit == MetricUnit::Nanoseconds
        ? static_cast<f64>(duration.count())
        : ns_to_ms(duration);
    std::scoped_lock lock(m_mutex);
    SampleAccumulator& sample = m_samples[slot];
    ++sample.count;
    sample.latest = value;
    sample.total += value;
}

void MetricsThreadContext::record_lock_wait(LockId id, std::chrono::nanoseconds duration) noexcept {
    const u32 slot = static_cast<u32>(id);
    if (slot >= lock_count) return;
    {
        std::scoped_lock lock(m_mutex);
        LockWorkMetrics& work = m_locks[slot];
        ++work.acquisitions;
        if (duration.count() > 0) {
            ++work.contentions;
        }
        work.total_wait_ns += static_cast<u64>(duration.count());
    }
    record_duration(lock_wait_metric(id), duration);
}

void MetricsThreadContext::record_lock_hold(LockId id, std::chrono::nanoseconds duration) noexcept {
    const u32 slot = static_cast<u32>(id);
    if (slot >= lock_count) return;
    {
        std::scoped_lock lock(m_mutex);
        m_locks[slot].total_hold_ns += static_cast<u64>(duration.count());
    }
    record_duration(lock_hold_metric(id), duration);
}

void MetricsThreadContext::accumulate_simulation_work(const SimulationWorkMetrics& work) noexcept {
    std::scoped_lock lock(m_mutex);
    m_simulation_work.particles_updated += work.particles_updated;
    m_simulation_work.field_samples += work.field_samples;
    m_simulation_work.surface_evaluations += work.surface_evaluations;
    m_simulation_work.derivative_evaluations += work.derivative_evaluations;
    m_simulation_work.metric_evaluations += work.metric_evaluations;
    m_simulation_work.curvature_evaluations += work.curvature_evaluations;
    m_simulation_work.integrator_steps += work.integrator_steps;
}

MetricsThreadScope::MetricsThreadScope(MetricsService& service, ThreadRole role) noexcept
    : m_service(&service)
    , m_previous(t_metrics_context)
    , m_context(role) {
    t_metrics_context = &m_context;
    m_service->register_context(m_context);
}

MetricsThreadScope::~MetricsThreadScope() {
    if (m_service) {
        m_service->unregister_context(m_context);
    }
    t_metrics_context = m_previous;
}

MetricsThreadContext* MetricsThreadHandle::current() noexcept {
    return t_metrics_context;
}

ScopedMetricTimer::ScopedMetricTimer(MetricsService& metrics, MetricId id) noexcept
    : m_metrics(&metrics)
    , m_id(id)
    , m_started(std::chrono::steady_clock::now()) {}

ScopedMetricTimer::~ScopedMetricTimer() {
    if (!m_metrics) return;
    m_metrics->record_duration(m_id, std::chrono::steady_clock::now() - m_started);
}

MetricsService::MetricsService() {
    init();
}

MetricsService::~MetricsService() = default;

void MetricsService::init(MetricsConfig config) {
    std::scoped_lock lock(m_mutex);
    m_config = config;
    for (MetricStore& metric : m_metrics) {
        metric.counter = u64(0);
        metric.gauge = 0.0;
        metric.short_window.configure(m_config.short_window_samples);
        metric.long_window.configure(m_config.long_window_samples);
    }
    m_contexts.clear();
    m_simulation_work_totals = {};
    m_frame_index = u64(0);
    m_frame_wall_time_seconds = 0.0;
}

void MetricsService::reset() {
    init(m_config);
}

void MetricsService::begin_frame(u64 frame_index, f64 wall_time_seconds) {
    std::scoped_lock lock(m_mutex);
    m_frame_index = frame_index;
    m_frame_wall_time_seconds = wall_time_seconds;
}

void MetricsService::end_frame() {
    drain_thread_contexts();
}

void MetricsService::increment(MetricId id, u64 amount) {
    std::scoped_lock lock(m_mutex);
    m_metrics[index(id)].counter += amount;
}

void MetricsService::set_gauge(MetricId id, f64 value) {
    std::scoped_lock lock(m_mutex);
    MetricStore& metric = m_metrics[index(id)];
    metric.gauge = value;
    metric.short_window.push(value);
    metric.long_window.push(value);
}

void MetricsService::record_sample(MetricId id, f64 value) {
    std::scoped_lock lock(m_mutex);
    MetricStore& metric = m_metrics[index(id)];
    metric.gauge = value;
    metric.short_window.push(value);
    metric.long_window.push(value);
}

void MetricsService::record_duration(MetricId id, std::chrono::nanoseconds duration) {
    const u32 slot = index(id);
    const f64 value = metric_descriptors[slot].unit == MetricUnit::Nanoseconds
        ? static_cast<f64>(duration.count())
        : ns_to_ms(duration);
    record_sample(id, value);
}

void MetricsService::record_frame_time(f32 frame_ms) {
    record_sample(MetricId::FrameMs, static_cast<f64>(frame_ms));
    if (frame_ms > f32(0)) {
        record_sample(MetricId::FrameFps, 1000.0 / static_cast<f64>(frame_ms));
    }
}

void MetricsService::record_simulation_work(const SimulationWorkMetrics& work) {
    std::scoped_lock lock(m_mutex);
    add_simulation_work(work);
}

void MetricsService::register_context(MetricsThreadContext& context) {
    std::scoped_lock lock(m_mutex);
    const auto found = std::find(m_contexts.begin(), m_contexts.end(), &context);
    if (found == m_contexts.end()) {
        m_contexts.push_back(&context);
    }
}

void MetricsService::unregister_context(MetricsThreadContext& context) {
    std::scoped_lock lock(m_mutex);
    const auto found = std::find(m_contexts.begin(), m_contexts.end(), &context);
    if (found != m_contexts.end()) {
        m_contexts.erase(found);
    }

    std::array<u64, metric_count> counters{};
    std::array<MetricsThreadContext::SampleAccumulator, metric_count> samples{};
    SimulationWorkMetrics simulation_work{};
    {
        std::scoped_lock context_lock(context.m_mutex);
        counters = context.m_counters;
        samples = context.m_samples;
        simulation_work = context.m_simulation_work;
        context.m_counters = {};
        context.m_samples = {};
        context.m_locks = {};
        context.m_simulation_work = {};
    }

    for (u32 slot = u32(0); slot < metric_count; ++slot) {
        m_metrics[slot].counter += counters[slot];
        if (samples[slot].count > u64(0)) {
            const f64 value = samples[slot].total / static_cast<f64>(samples[slot].count);
            m_metrics[slot].gauge = samples[slot].latest;
            m_metrics[slot].short_window.push(value);
            m_metrics[slot].long_window.push(value);
        }
    }
    add_simulation_work(simulation_work);
}

void MetricsService::drain_thread_contexts() {
    std::scoped_lock service_lock(m_mutex);
    for (MetricsThreadContext* context : m_contexts) {
        if (!context) continue;

        std::array<u64, metric_count> counters{};
        std::array<MetricsThreadContext::SampleAccumulator, metric_count> samples{};
        SimulationWorkMetrics simulation_work{};

        {
            std::scoped_lock lock(context->m_mutex);
            counters = context->m_counters;
            samples = context->m_samples;
            simulation_work = context->m_simulation_work;
            context->m_counters = {};
            context->m_samples = {};
            context->m_locks = {};
            context->m_simulation_work = {};
        }

        for (u32 slot = u32(0); slot < metric_count; ++slot) {
            m_metrics[slot].counter += counters[slot];
            if (samples[slot].count > u64(0)) {
                const f64 value = samples[slot].total / static_cast<f64>(samples[slot].count);
                m_metrics[slot].gauge = samples[slot].latest;
                m_metrics[slot].short_window.push(value);
                m_metrics[slot].long_window.push(value);
            }
        }
        add_simulation_work(simulation_work);
    }
}

std::vector<MetricSnapshot> MetricsService::snapshot() const {
    std::scoped_lock lock(m_mutex);
    std::vector<MetricSnapshot> result;
    result.reserve(metric_count);
    for (u32 slot = u32(0); slot < metric_count; ++slot) {
        result.push_back(MetricSnapshot{
            .id = static_cast<MetricId>(slot),
            .short_window = m_metrics[slot].short_window.summary(),
            .long_window = m_metrics[slot].long_window.summary()
        });
    }
    return result;
}

MetricSnapshot MetricsService::snapshot(MetricId id) const {
    std::scoped_lock lock(m_mutex);
    const MetricStore& metric = m_metrics[index(id)];
    return MetricSnapshot{
        .id = id,
        .short_window = metric.short_window.summary(),
        .long_window = metric.long_window.summary()
    };
}

MetricSummary MetricsService::short_summary(MetricId id) const {
    return snapshot(id).short_window;
}

f64 MetricsService::median_fps() const {
    const MetricSummary frame_ms = short_summary(MetricId::FrameMs);
    return frame_ms.median > 0.0 ? 1000.0 / frame_ms.median : 0.0;
}

u64 MetricsService::counter_value(MetricId id) const {
    std::scoped_lock lock(m_mutex);
    return m_metrics[index(id)].counter;
}

SimulationWorkMetrics MetricsService::simulation_work_totals() const {
    std::scoped_lock lock(m_mutex);
    return m_simulation_work_totals;
}

void MetricsService::RollingWindow::configure(u32 capacity) {
    m_capacity = capacity;
    m_values.assign(capacity, 0.0);
    clear();
}

void MetricsService::RollingWindow::clear() noexcept {
    m_next = u32(0);
    m_size = u32(0);
}

void MetricsService::RollingWindow::push(f64 value) {
    if (m_capacity == u32(0) || m_values.empty()) return;
    m_values[m_next] = value;
    m_next = (m_next + u32(1)) % m_capacity;
    if (m_size < m_capacity) {
        ++m_size;
    }
}

MetricSummary MetricsService::RollingWindow::summary() const {
    if (m_size == u32(0)) {
        return {};
    }

    std::vector<f64> values;
    values.reserve(m_size);
    for (u32 i = u32(0); i < m_size; ++i) {
        values.push_back(m_values[i]);
    }

    MetricSummary summary;
    summary.count = static_cast<u64>(m_size);
    summary.latest = m_values[(m_next + m_capacity - u32(1)) % m_capacity];
    const auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
    summary.min = *min_it;
    summary.max = *max_it;
    f64 total = 0.0;
    for (const f64 value : values) {
        total += value;
    }
    summary.mean = total / static_cast<f64>(values.size());

    auto median_values = values;
    const auto median_index = median_values.begin() + static_cast<std::ptrdiff_t>(median_values.size() / 2u);
    std::nth_element(median_values.begin(), median_index, median_values.end());
    summary.median = *median_index;
    if ((values.size() % 2u) == 0u) {
        const auto lower_index = median_values.begin() + static_cast<std::ptrdiff_t>((values.size() / 2u) - 1u);
        std::nth_element(median_values.begin(), lower_index, median_values.end());
        summary.median = (*lower_index + summary.median) * 0.5;
    }

    auto p95_values = values;
    const std::size_t p95_slot = static_cast<std::size_t>(
        std::min<u64>(static_cast<u64>(p95_values.size() - 1u),
                      (static_cast<u64>(p95_values.size()) * u64(95)) / u64(100)));
    const auto p95_index = p95_values.begin() + static_cast<std::ptrdiff_t>(p95_slot);
    std::nth_element(p95_values.begin(), p95_index, p95_values.end());
    summary.p95 = *p95_index;
    return summary;
}

u32 MetricsService::index(MetricId id) noexcept {
    const u32 slot = static_cast<u32>(id);
    return slot < metric_count ? slot : u32(0);
}

void MetricsService::add_simulation_work(const SimulationWorkMetrics& work) noexcept {
    m_simulation_work_totals.particles_updated += work.particles_updated;
    m_simulation_work_totals.field_samples += work.field_samples;
    m_simulation_work_totals.surface_evaluations += work.surface_evaluations;
    m_simulation_work_totals.derivative_evaluations += work.derivative_evaluations;
    m_simulation_work_totals.metric_evaluations += work.metric_evaluations;
    m_simulation_work_totals.curvature_evaluations += work.curvature_evaluations;
    m_simulation_work_totals.integrator_steps += work.integrator_steps;
}

} // namespace ndde
