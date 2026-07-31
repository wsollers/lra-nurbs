#include "app/simulations/LearningSimulation.hpp"

#include <memory_resource>
#include <utility>

namespace ndde {

LearningSimulation::LearningSimulation(memory::MemoryService* memory)
    : m_memory(memory)
    , m_workbenches(memory ? memory->persistent().resource() : std::pmr::get_default_resource())
{
}

LearningSimulation::LearningSimulation(memory::MemoryService* memory, std::string workbench_id)
    : m_memory(memory)
    , m_workbenches(memory ? memory->persistent().resource() : std::pmr::get_default_resource())
    , m_workbench_id(std::move(workbench_id))
{
}

void LearningSimulation::on_register(SimulationHost& host) {
    m_host = &host;
    m_memory = &host.memory();
    if (m_workbenches.size() == 0) {
        register_app_workbenches(m_workbenches);
    }

    m_main_handle = host.render().register_view(RenderViewDescriptor{
        .title = "Learning XY",
        .kind = RenderViewKind::Main,
        .projection = CameraProjection::Orthographic,
        .camera_profile = CameraViewProfile::Orthographic2D,
        .camera = CameraState{.target = Vec3{0.f, 0.f, 0.f}, .yaw = 0.f, .pitch = 0.f, .zoom = 1.f},
        .overlays = {.show_axes = true, .show_grid = true, .show_labels = true}
    }, &m_main_view);
}

void LearningSimulation::on_start() {
    if (!m_memory || m_workbenches.size() == 0) {
        m_status = "No learning workbenches registered";
        return;
    }
    m_active_workbench = m_workbench_id.empty()
        ? m_workbenches.create(*m_memory, 0)
        : m_workbenches.create(*m_memory, m_workbench_id);
    if (!m_active_workbench) {
        m_status = "Failed to create learning workbench";
        return;
    }
    m_build_context.clear();
    m_active_workbench->build(m_build_context);
    auto context = render_context();
    m_active_workbench->on_start(context);
    m_status = m_active_workbench->metadata().title;
}

void LearningSimulation::on_tick(const TickInfo& tick) {
    on_simulation_tick(tick);
    on_submit_render();
}

void LearningSimulation::on_simulation_tick(const TickInfo& tick) {
    m_time = tick.time;
    if (m_active_workbench) {
        m_active_workbench->on_tick(tick);
    }
}

void LearningSimulation::on_submit_render() {
    if (!m_active_workbench || !m_host) return;
    auto context = render_context();
    m_active_workbench->on_submit_render(context);
}

void LearningSimulation::on_stop() {
    if (m_active_workbench) {
        m_active_workbench->on_stop();
        m_active_workbench.reset();
    }
    m_build_context.clear();
    m_main_handle.reset();
    m_main_view = RenderViewId(0);
    m_host = nullptr;
}

SceneSnapshot LearningSimulation::snapshot() const {
    return SceneSnapshot{
        .name = std::string(name()),
        .paused = false,
        .sim_time = m_time,
        .sim_speed = f32(1),
        .particle_count = 0u,
        .status = m_status
    };
}

SimulationMetadata LearningSimulation::metadata() const {
    return SimulationMetadata{
        .name = std::string(name()),
        .surface_name = m_active_workbench ? m_active_workbench->metadata().title : "Learning workbench",
        .surface_formula = "f(x) = sin(x), f'(x) = cos(x)",
        .status = m_status,
        .sim_time = m_time,
        .sim_speed = f32(1),
        .particle_count = 0u,
        .surface_has_analytic_derivatives = true
    };
}

WorkbenchRenderContext LearningSimulation::render_context() {
    return WorkbenchRenderContext{
        .host = *m_host,
        .main_view = m_main_view
    };
}

} // namespace ndde
