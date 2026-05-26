#pragma once
// engine/metricservice/MetricsService.hpp
// Lightweight live metrics aggregation and thread-local staging.

#include "engine/metricservice/MetricsTypes.hpp"
#include "engine/threading/ThreadTypes.hpp"

#include <array>
#include <chrono>
#include <mutex>
#include <vector>

namespace ndde {

class MetricsService;

class MetricsThreadContext {
public:
    MetricsThreadContext() = default;
    explicit MetricsThreadContext(ThreadRole role) noexcept : m_role(role) {}

    [[nodiscard]] ThreadRole role() const noexcept { return m_role; }

    void increment(MetricId id, u64 amount = u64(1)) noexcept;
    void record_duration(MetricId id, std::chrono::nanoseconds duration) noexcept;
    void record_lock_wait(LockId id, std::chrono::nanoseconds duration) noexcept;
    void record_lock_hold(LockId id, std::chrono::nanoseconds duration) noexcept;
    void accumulate_simulation_work(const SimulationWorkMetrics& work) noexcept;

private:
    friend class MetricsService;

    struct SampleAccumulator {
        u64 count = u64(0);
        f64 latest = 0.0;
        f64 total = 0.0;
    };

    ThreadRole m_role = ThreadRole::Unknown;
    std::array<u64, metric_count> m_counters{};
    std::array<SampleAccumulator, metric_count> m_samples{};
    std::array<LockWorkMetrics, lock_count> m_locks{};
    SimulationWorkMetrics m_simulation_work{};
    mutable std::mutex m_mutex;
};

class MetricsThreadScope {
public:
    MetricsThreadScope(MetricsService& service, ThreadRole role) noexcept;
    ~MetricsThreadScope();

    MetricsThreadScope(const MetricsThreadScope&) = delete;
    MetricsThreadScope& operator=(const MetricsThreadScope&) = delete;
    MetricsThreadScope(MetricsThreadScope&&) = delete;
    MetricsThreadScope& operator=(MetricsThreadScope&&) = delete;

    [[nodiscard]] MetricsThreadContext& context() noexcept { return m_context; }

private:
    MetricsService* m_service = nullptr;
    MetricsThreadContext* m_previous = nullptr;
    MetricsThreadContext m_context;
};

class MetricsThreadHandle {
public:
    [[nodiscard]] static MetricsThreadContext* current() noexcept;
};

class ScopedMetricTimer {
public:
    ScopedMetricTimer(MetricsService& metrics, MetricId id) noexcept;
    ~ScopedMetricTimer();

    ScopedMetricTimer(const ScopedMetricTimer&) = delete;
    ScopedMetricTimer& operator=(const ScopedMetricTimer&) = delete;
    ScopedMetricTimer(ScopedMetricTimer&&) = delete;
    ScopedMetricTimer& operator=(ScopedMetricTimer&&) = delete;

private:
    MetricsService* m_metrics = nullptr;
    MetricId m_id = MetricId::FrameMs;
    std::chrono::steady_clock::time_point m_started{};
};

class MetricsService {
public:
    MetricsService();
    ~MetricsService();

    MetricsService(const MetricsService&) = delete;
    MetricsService& operator=(const MetricsService&) = delete;
    MetricsService(MetricsService&&) = delete;
    MetricsService& operator=(MetricsService&&) = delete;

    void init(MetricsConfig config = {});
    void reset();

    void begin_frame(u64 frame_index, f64 wall_time_seconds);
    void end_frame();

    void increment(MetricId id, u64 amount = u64(1));
    void set_gauge(MetricId id, f64 value);
    void record_sample(MetricId id, f64 value);
    void record_duration(MetricId id, std::chrono::nanoseconds duration);
    void record_frame_time(f32 frame_ms);
    void record_simulation_work(const SimulationWorkMetrics& work);

    void register_context(MetricsThreadContext& context);
    void unregister_context(MetricsThreadContext& context);
    void drain_thread_contexts();

    [[nodiscard]] std::vector<MetricSnapshot> snapshot() const;
    [[nodiscard]] MetricSnapshot snapshot(MetricId id) const;
    [[nodiscard]] MetricSummary short_summary(MetricId id) const;
    [[nodiscard]] f64 median_fps() const;
    [[nodiscard]] u64 counter_value(MetricId id) const;
    [[nodiscard]] SimulationWorkMetrics simulation_work_totals() const;

private:
    class RollingWindow {
    public:
        void configure(u32 capacity);
        void clear() noexcept;
        void push(f64 value);
        [[nodiscard]] MetricSummary summary() const;

    private:
        std::vector<f64> m_values;
        u32 m_capacity = u32(0);
        u32 m_next = u32(0);
        u32 m_size = u32(0);
    };

    struct MetricStore {
        u64 counter = u64(0);
        f64 gauge = 0.0;
        RollingWindow short_window;
        RollingWindow long_window;
    };

    [[nodiscard]] static u32 index(MetricId id) noexcept;
    void add_simulation_work(const SimulationWorkMetrics& work) noexcept;

    mutable std::mutex m_mutex;
    MetricsConfig m_config;
    std::array<MetricStore, metric_count> m_metrics;
    std::vector<MetricsThreadContext*> m_contexts;
    SimulationWorkMetrics m_simulation_work_totals{};
    u64 m_frame_index = u64(0);
    f64 m_frame_wall_time_seconds = 0.0;
};

} // namespace ndde
