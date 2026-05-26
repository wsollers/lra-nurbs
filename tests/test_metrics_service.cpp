#include "engine/metricservice/MetricsService.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace {

using namespace ndde;
using namespace std::chrono_literals;

TEST(MetricsService, RollingWindowComputesMedianP95AndMean) {
    MetricsService metrics;
    metrics.init(MetricsConfig{.short_window_samples = u32(5), .long_window_samples = u32(5)});

    metrics.record_sample(MetricId::FrameMs, 5.0);
    metrics.record_sample(MetricId::FrameMs, 1.0);
    metrics.record_sample(MetricId::FrameMs, 3.0);
    metrics.record_sample(MetricId::FrameMs, 2.0);
    metrics.record_sample(MetricId::FrameMs, 4.0);

    const MetricSummary summary = metrics.short_summary(MetricId::FrameMs);
    EXPECT_EQ(summary.count, 5u);
    EXPECT_DOUBLE_EQ(summary.latest, 4.0);
    EXPECT_DOUBLE_EQ(summary.min, 1.0);
    EXPECT_DOUBLE_EQ(summary.max, 5.0);
    EXPECT_DOUBLE_EQ(summary.mean, 3.0);
    EXPECT_DOUBLE_EQ(summary.median, 3.0);
    EXPECT_DOUBLE_EQ(summary.p95, 5.0);
}

TEST(MetricsService, RollingWindowDropsOldestSamplesByCapacity) {
    MetricsService metrics;
    metrics.init(MetricsConfig{.short_window_samples = u32(3), .long_window_samples = u32(3)});

    metrics.record_sample(MetricId::FrameMs, 1.0);
    metrics.record_sample(MetricId::FrameMs, 2.0);
    metrics.record_sample(MetricId::FrameMs, 3.0);
    metrics.record_sample(MetricId::FrameMs, 100.0);

    const MetricSummary summary = metrics.short_summary(MetricId::FrameMs);
    EXPECT_EQ(summary.count, 3u);
    EXPECT_DOUBLE_EQ(summary.latest, 100.0);
    EXPECT_DOUBLE_EQ(summary.min, 2.0);
    EXPECT_DOUBLE_EQ(summary.max, 100.0);
    EXPECT_DOUBLE_EQ(summary.median, 3.0);
}

TEST(MetricsService, MedianFpsDerivesFromMedianFrameMilliseconds) {
    MetricsService metrics;
    metrics.init(MetricsConfig{.short_window_samples = u32(3), .long_window_samples = u32(3)});

    metrics.record_frame_time(f32(10.0f));
    metrics.record_frame_time(f32(20.0f));
    metrics.record_frame_time(f32(30.0f));

    EXPECT_DOUBLE_EQ(metrics.short_summary(MetricId::FrameMs).median, 20.0);
    EXPECT_DOUBLE_EQ(metrics.median_fps(), 50.0);
}

TEST(MetricsService, CountersAccumulateAndReset) {
    MetricsService metrics;

    metrics.increment(MetricId::FramesSubmitted);
    metrics.increment(MetricId::FramesSubmitted, u64(4));
    EXPECT_EQ(metrics.counter_value(MetricId::FramesSubmitted), 5u);

    metrics.reset();
    EXPECT_EQ(metrics.counter_value(MetricId::FramesSubmitted), 0u);
}

TEST(MetricsService, ThreadScopeInstallsContextAndDrainsStaging) {
    MetricsService metrics;
    metrics.init(MetricsConfig{.short_window_samples = u32(8), .long_window_samples = u32(8)});

    {
        MetricsThreadScope scope(metrics, ThreadRole::Worker);
        MetricsThreadContext* context = MetricsThreadHandle::current();
        ASSERT_NE(context, nullptr);
        EXPECT_EQ(context->role(), ThreadRole::Worker);

        context->increment(MetricId::FramesSubmitted, u64(2));
        context->record_duration(MetricId::BackgroundJobRunMs, 2ms);
        context->accumulate_simulation_work(SimulationWorkMetrics{
            .particles_updated = u64(7),
            .field_samples = u64(11),
            .metric_evaluations = u64(13)
        });

        metrics.drain_thread_contexts();
        EXPECT_EQ(metrics.counter_value(MetricId::FramesSubmitted), 2u);
        EXPECT_DOUBLE_EQ(metrics.short_summary(MetricId::BackgroundJobRunMs).latest, 2.0);
        const SimulationWorkMetrics totals = metrics.simulation_work_totals();
        EXPECT_EQ(totals.particles_updated, 7u);
        EXPECT_EQ(totals.field_samples, 11u);
        EXPECT_EQ(totals.metric_evaluations, 13u);
    }

    EXPECT_EQ(MetricsThreadHandle::current(), nullptr);
}

TEST(MetricsService, ScopedMetricTimerRecordsDuration) {
    MetricsService metrics;
    metrics.init(MetricsConfig{.short_window_samples = u32(4), .long_window_samples = u32(4)});

    {
        ScopedMetricTimer timer(metrics, MetricId::EventDrainMs);
    }

    const MetricSummary summary = metrics.short_summary(MetricId::EventDrainMs);
    EXPECT_EQ(summary.count, 1u);
    EXPECT_GE(summary.latest, 0.0);
}

} // namespace
