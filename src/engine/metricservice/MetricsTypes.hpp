#pragma once
// engine/metricservice/MetricsTypes.hpp
// Compile-time metric identities, summaries, and lightweight staging types.

#include "engine/RuntimeIds.hpp"

#include <array>
#include <chrono>
#include <string_view>

namespace ndde {

enum class MetricId : u32 {
    FrameMs,
    FrameFps,
    FrameAcquireMs,
    FrameSubmitMs,
    FramePresentMs,
    RenderTaskWaitMs,
    ImGuiBuildMs,
    SimulationTickMs,
    SimulationRenderSubmitMs,
    TelemetryTickMs,
    EventDrainMs,
    LoggerDrainMs,
    CaptureReadbackMs,
    BackgroundJobWaitMs,
    BackgroundJobRunMs,
    RuntimeLockWaitNs,
    RuntimeLockHoldNs,
    FramesSubmitted,
    FramesPresented,
    FramesSkipped,
    JobsSubmitted,
    JobsCompleted,
    JobsFailed,
    JobsCancelled,
    DroppedMetricSamples,
    Count
};

enum class MetricKind : u8 {
    Counter,
    Gauge,
    RollingSample,
    Duration,
    Rate
};

enum class MetricUnit : u8 {
    Count,
    Frames,
    Ticks,
    Jobs,
    Bytes,
    Milliseconds,
    Nanoseconds,
    PerSecond,
    Percent
};

enum class LockId : u32 {
    SimulationRuntime,
    LoggerService,
    ThreadManagementService,
    EventMailbox,
    ResourceManagerService,
    CaptureService,
    Count
};

struct MetricDescriptor {
    MetricId id = MetricId::FrameMs;
    ComponentId component = ids::unknown_component;
    MetricKind kind = MetricKind::RollingSample;
    MetricUnit unit = MetricUnit::Count;
    std::string_view name;
    std::string_view group;
};

struct MetricSummary {
    u64 count = u64(0);
    f64 latest = 0.0;
    f64 min = 0.0;
    f64 max = 0.0;
    f64 mean = 0.0;
    f64 median = 0.0;
    f64 p95 = 0.0;
};

struct MetricSnapshot {
    MetricId id = MetricId::FrameMs;
    MetricSummary short_window;
    MetricSummary long_window;
};

struct SimulationWorkMetrics {
    u64 particles_updated = u64(0);
    u64 field_samples = u64(0);
    u64 surface_evaluations = u64(0);
    u64 derivative_evaluations = u64(0);
    u64 metric_evaluations = u64(0);
    u64 curvature_evaluations = u64(0);
    u64 integrator_steps = u64(0);
};

struct LockWorkMetrics {
    u64 acquisitions = u64(0);
    u64 contentions = u64(0);
    u64 total_wait_ns = u64(0);
    u64 total_hold_ns = u64(0);
};

struct MetricsConfig {
    u32 short_window_samples = u32(120);
    u32 long_window_samples = u32(600);
};

inline constexpr u32 metric_count = static_cast<u32>(MetricId::Count);
inline constexpr u32 lock_count = static_cast<u32>(LockId::Count);

inline constexpr ComponentId metric_component_engine_frame{"engine.frame"};
inline constexpr ComponentId metric_component_engine_render{"engine.render"};
inline constexpr ComponentId metric_component_engine_simulation{"engine.simulation"};
inline constexpr ComponentId metric_component_engine_jobs{"engine.jobs"};
inline constexpr ComponentId metric_component_engine_locks{"engine.locks"};
inline constexpr ComponentId metric_component_engine_metrics{"engine.metrics"};

inline constexpr std::array<MetricDescriptor, metric_count> metric_descriptors{{
    {MetricId::FrameMs, metric_component_engine_frame, MetricKind::Duration, MetricUnit::Milliseconds, "Frame", "Frame"},
    {MetricId::FrameFps, metric_component_engine_frame, MetricKind::Rate, MetricUnit::PerSecond, "FPS", "Frame"},
    {MetricId::FrameAcquireMs, metric_component_engine_render, MetricKind::Duration, MetricUnit::Milliseconds, "Acquire", "Render"},
    {MetricId::FrameSubmitMs, metric_component_engine_render, MetricKind::Duration, MetricUnit::Milliseconds, "Submit", "Render"},
    {MetricId::FramePresentMs, metric_component_engine_render, MetricKind::Duration, MetricUnit::Milliseconds, "Present", "Render"},
    {MetricId::RenderTaskWaitMs, metric_component_engine_render, MetricKind::Duration, MetricUnit::Milliseconds, "Render Task Wait", "Render"},
    {MetricId::ImGuiBuildMs, metric_component_engine_frame, MetricKind::Duration, MetricUnit::Milliseconds, "ImGui Build", "Frame"},
    {MetricId::SimulationTickMs, metric_component_engine_simulation, MetricKind::Duration, MetricUnit::Milliseconds, "Simulation Tick", "Simulation"},
    {MetricId::SimulationRenderSubmitMs, metric_component_engine_simulation, MetricKind::Duration, MetricUnit::Milliseconds, "Simulation Render Submit", "Simulation"},
    {MetricId::TelemetryTickMs, metric_component_engine_simulation, MetricKind::Duration, MetricUnit::Milliseconds, "Telemetry Tick", "Simulation"},
    {MetricId::EventDrainMs, metric_component_engine_frame, MetricKind::Duration, MetricUnit::Milliseconds, "Event Drain", "Frame"},
    {MetricId::LoggerDrainMs, metric_component_engine_frame, MetricKind::Duration, MetricUnit::Milliseconds, "Logger Drain", "Frame"},
    {MetricId::CaptureReadbackMs, metric_component_engine_render, MetricKind::Duration, MetricUnit::Milliseconds, "Capture Readback", "Render"},
    {MetricId::BackgroundJobWaitMs, metric_component_engine_jobs, MetricKind::Duration, MetricUnit::Milliseconds, "Job Wait", "Jobs"},
    {MetricId::BackgroundJobRunMs, metric_component_engine_jobs, MetricKind::Duration, MetricUnit::Milliseconds, "Job Run", "Jobs"},
    {MetricId::RuntimeLockWaitNs, metric_component_engine_locks, MetricKind::Duration, MetricUnit::Nanoseconds, "Runtime Lock Wait", "Locks"},
    {MetricId::RuntimeLockHoldNs, metric_component_engine_locks, MetricKind::Duration, MetricUnit::Nanoseconds, "Runtime Lock Hold", "Locks"},
    {MetricId::FramesSubmitted, metric_component_engine_render, MetricKind::Counter, MetricUnit::Frames, "Frames Submitted", "Render"},
    {MetricId::FramesPresented, metric_component_engine_render, MetricKind::Counter, MetricUnit::Frames, "Frames Presented", "Render"},
    {MetricId::FramesSkipped, metric_component_engine_render, MetricKind::Counter, MetricUnit::Frames, "Frames Skipped", "Render"},
    {MetricId::JobsSubmitted, metric_component_engine_jobs, MetricKind::Counter, MetricUnit::Jobs, "Jobs Submitted", "Jobs"},
    {MetricId::JobsCompleted, metric_component_engine_jobs, MetricKind::Counter, MetricUnit::Jobs, "Jobs Completed", "Jobs"},
    {MetricId::JobsFailed, metric_component_engine_jobs, MetricKind::Counter, MetricUnit::Jobs, "Jobs Failed", "Jobs"},
    {MetricId::JobsCancelled, metric_component_engine_jobs, MetricKind::Counter, MetricUnit::Jobs, "Jobs Cancelled", "Jobs"},
    {MetricId::DroppedMetricSamples, metric_component_engine_metrics, MetricKind::Counter, MetricUnit::Count, "Dropped Metric Samples", "Metrics"},
}};

} // namespace ndde
