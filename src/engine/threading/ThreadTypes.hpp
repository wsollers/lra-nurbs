#pragma once
// engine/threading/ThreadTypes.hpp
// Engine-owned thread roles, worker job descriptors, status records, and results.

#include "engine/RuntimeIds.hpp"
#include "engine/diagnostics/DiagnosticsTypes.hpp"
#include "engine/IScene.hpp"
#include "engine/SimulationClock.hpp"
#include "simulation/events/EventRecord.hpp"

#include <chrono>
#include <string>
#include <variant>
#include <vector>

namespace ndde {

struct ThreadJobId {
    u64 value = u64(0);

    friend constexpr bool operator==(ThreadJobId, ThreadJobId) noexcept = default;
};

enum class ThreadRole : u8 {
    Main,
    Gui,
    Simulation,
    Renderer,
    Logger,
    Worker,
    Io,
    Telemetry,
    Unknown
};

enum class ThreadJobPriority : u8 {
    Low,
    Normal,
    High
};

enum class ThreadJobState : u8 {
    Queued,
    Running,
    Completed,
    CancelRequested,
    Cancelled,
    Failed
};

struct ThreadJobDescriptor {
    ThreadJobId id = {};
    ComponentId owner = ids::unknown_component;
    RuntimeNodeId node = {};
    ThreadJobPriority priority = ThreadJobPriority::Normal;
    bool cancellable = true;
};

struct ThreadJobStatus {
    ThreadJobId id = {};
    ComponentId owner = ids::unknown_component;
    RuntimeNodeId node = {};
    ThreadJobPriority priority = ThreadJobPriority::Normal;
    ThreadJobState state = ThreadJobState::Queued;
    bool cancellable = true;
    u64 queued_order = u64(0);
    u64 worker_index = u64(0);
};

struct GeometryBuildResult {
    ThreadJobId job = {};
    ResourceId resource = {};
    u64 vertex_count = u64(0);
    u64 index_count = u64(0);
};

struct ValidationJobResult {
    ThreadJobId job = {};
    RuntimeNodeId scenario = {};
    u64 warning_count = u64(0);
    u64 error_count = u64(0);
};

struct ThreadTextResult {
    ThreadJobId job = {};
    std::string message;
};

using ThreadJobResult = std::variant<
    GeometryBuildResult,
    ValidationJobResult,
    DiagnosticId,
    ResourceId,
    ThreadTextResult
>;

enum class SimulationThreadCommandKind : u8 {
    Pause,
    Resume,
    Stop,
    Tick,
    SurfacePoke,
    SwitchSimulation,
    ResetClock,
    Shutdown
};

struct SimulationSurfacePoke {
    u64 view = u64(0);
    Vec2 uv{};
    Vec2 fallback_uv{};
    Vec2 screen_ndc{};
    f32 amplitude = f32(0.35);
    f32 radius = f32(0.9);
    f32 falloff = f32(1);
    bool ray_hit = false;
    u32 seed = u32(0);
};

struct SimulationThreadCommand {
    SimulationThreadCommandKind kind = SimulationThreadCommandKind::Tick;
    TickInfo tick = {};
    SimulationSurfacePoke surface_poke = {};
    u64 simulation_index = u64(0);
};

struct SimulationRenderSnapshot {
    std::string name;
    bool paused = false;
    f32 sim_time = f32(0);
    f32 sim_speed = f32(0);
    u64 particle_count = u64(0);
    std::string status;
    std::vector<ParticleSnapshot> particles;
};

struct RenderFrameSnapshot {
    u64 frame_index = u64(0);
    f32 frame_ms = f32(0);
    SimulationRenderSnapshot simulation;
    bool ui_draw_data_ready = false;
};

enum class RenderThreadCommandKind : u8 {
    Frame,
    Resize,
    Shutdown
};

struct RenderThreadCommand {
    RenderThreadCommandKind kind = RenderThreadCommandKind::Frame;
    RenderFrameSnapshot frame;
    u32 width = u32(0);
    u32 height = u32(0);
};

[[nodiscard]] inline SimulationRenderSnapshot make_simulation_render_snapshot(const SceneSnapshot& source) {
    SimulationRenderSnapshot snapshot{
        .name = source.name,
        .paused = source.paused,
        .sim_time = static_cast<f32>(source.sim_time),
        .sim_speed = static_cast<f32>(source.sim_speed),
        .particle_count = static_cast<u64>(source.particle_count),
        .status = source.status
    };
    snapshot.particles.assign(source.particles.begin(), source.particles.end());
    return snapshot;
}

struct ThreadPoolConfig {
    u32 worker_count = u32(0);
    u64 max_queued_jobs = u64(256);
    u64 max_completed_results = u64(256);
    u64 max_mailbox_records = u64(512);
    bool enable_simulation_thread = false;
    bool enable_render_thread = false;
    bool enable_logger_thread = false;
    bool enable_background_workers = true;
};

struct ThreadStats {
    u32 worker_count = u32(0);
    u64 queued_jobs = u64(0);
    u64 completed_results = u64(0);
    u64 dropped_results = u64(0);
    u64 dropped_logs = u64(0);
    u64 dropped_diagnostics = u64(0);
    u64 dropped_events = u64(0);
};

} // namespace ndde
