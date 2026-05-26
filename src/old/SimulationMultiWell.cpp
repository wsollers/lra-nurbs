#include "app/SimulationMultiWell.hpp"

#include "app/AlternateViewPanel.hpp"
#include "app/SimulationRenderPackets.hpp"
#include "memory/Containers.hpp"

#include <imgui.h>
#include <utility>

namespace ndde {

SimulationMultiWell::SimulationMultiWell(memory::MemoryService* memory)
    : m_surface(SurfaceRegistry::make_multi_well(memory, 4.f))
    , m_particles(m_surface.get(), 3103u)
    , m_spawner(m_particles, m_spawn_count, m_sim_time, m_goal_status)
{
    sync_context();
}

void SimulationMultiWell::on_register(SimulationHost& host) {
    m_host = &host;
    sync_context();
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - Controls",
        .category = "Simulation",
        .scope = PanelScope::Simulation,
        .draw = [this] { draw_control_panel(); }
    }));
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - Swarms",
        .category = "Simulation",
        .scope = PanelScope::Simulation,
        .draw = [this] { draw_swarm_panel(); }
    }));
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - Particles",
        .category = "Simulation",
        .scope = PanelScope::Simulation,
        .draw = [this] { draw_particle_panel(); }
    }));
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - Goals",
        .category = "Simulation",
        .scope = PanelScope::Simulation,
        .draw = [this] { draw_goal_panel(); }
    }));
    m_avoider_hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = 'A', .mods = 2},
        .label = "Spawn avoider",
        .callback = [this] { spawn_avoider(); }
    });
    m_centroid_hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = 'C', .mods = 2},
        .label = "Spawn centroid seeker",
        .callback = [this] { spawn_centroid_seeker(); }
    });
    m_reset_hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = 'R', .mods = 2},
        .label = "Reset multi-well",
        .callback = [this] { reset_showcase(); }
    });
    m_main_handle = host.render().register_view(RenderViewDescriptor{
        .title = "Multi-Well 3D",
        .kind = RenderViewKind::Main,
        .camera_profile = CameraViewProfile::PerspectiveSurface3D,
        .overlays = {.show_axes = true, .show_grid = true, .show_hover_frenet = true, .show_osculating_circle = true}
    }, &m_main_view);
    m_alt_handle = host.render().register_view(RenderViewDescriptor{
        .title = "Multi-Well Alternate",
        .kind = RenderViewKind::Alternate,
        .alternate_mode = AlternateViewMode::VectorField,
        .projection = CameraProjection::Orthographic,
        .camera_profile = CameraViewProfile::Orthographic2D,
        .overlays = {.show_axes = true},
        .alternate = {
            .isocline_direction_angle = -0.35f,
            .isocline_target_slope = 0.f,
            .isocline_tolerance = 0.6f,
            .isocline_bands = 7u,
            .vector_mode = VectorFieldMode::NegativeGradient,
            .vector_samples = 22u,
            .vector_scale = 1.15f,
            .flow_seed_count = 10u,
            .flow_steps = 42u,
            .flow_step_size = 0.08f
        }
    }, &m_alternate_view);
}

void SimulationMultiWell::on_start() {
    reset_showcase();
}

void SimulationMultiWell::on_tick(const TickInfo& tick) {
    m_context.set_tick(tick);
    if (!tick.paused && !m_paused) {
        m_sim_time = tick.time;
        m_particles.update(tick.dt, m_sim_speed, m_sim_time);
        m_context.dirty().mark_particles_changed();
    }
    submit_geometry();
}

void SimulationMultiWell::on_stop() {
    m_panel_handles.clear();
    m_avoider_hotkey.reset();
    m_centroid_hotkey.reset();
    m_reset_hotkey.reset();
    m_main_handle.reset();
    m_alt_handle.reset();
    m_host = nullptr;
}

SceneSnapshot SimulationMultiWell::snapshot() const {
    return SceneSnapshot{
        .name = std::string(name()),
        .paused = m_paused,
        .sim_time = m_sim_time,
        .sim_speed = m_sim_speed,
        .particle_count = m_particles.size(),
        .status = m_goal_status == GoalStatus::Succeeded ? "Succeeded" : "Running",
        .particles = m_particles.snapshot_particles()
    };
}

SimulationMetadata SimulationMultiWell::metadata() const {
    const ndde::math::SurfaceMetadata surface = m_surface->metadata(m_sim_time);
    return SimulationMetadata{
        .name = std::string(name()),
        .surface_name = std::string(surface.name),
        .surface_formula = std::string(surface.formula),
        .status = m_goal_status == GoalStatus::Succeeded ? "Succeeded" : "Running",
        .sim_time = m_sim_time,
        .sim_speed = m_sim_speed,
        .particle_count = m_particles.size(),
        .paused = m_paused,
        .goal_succeeded = m_goal_status == GoalStatus::Succeeded,
        .surface_has_analytic_derivatives = surface.has_analytic_derivatives,
        .surface_deformable = surface.deformable,
        .surface_time_varying = surface.time_varying
    };
}

void SimulationMultiWell::sync_context() {
    if (m_host) m_particles.bind_memory(&m_host->memory());
    m_particles.set_surface(m_surface.get());
    m_context.set_surface(m_surface.get());
    m_context.set_particles(&m_particles.particles());
    m_context.set_rng(&m_particles.rng());
    m_context.set_time(m_sim_time);
}

void SimulationMultiWell::spawn_avoider() {
    m_last_swarm = m_spawner.spawn_avoider();
    sync_context();
}

void SimulationMultiWell::spawn_centroid_seeker() {
    m_last_swarm = m_spawner.spawn_centroid_seeker();
    sync_context();
}

void SimulationMultiWell::reset_showcase() {
    m_last_swarm = m_spawner.spawn_showcase_service();
    sync_context();
}

void SimulationMultiWell::draw_control_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 72.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f, 200.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Controls")) { ImGui::End(); return; }
    SimulationControlPanel::draw(metadata(), SimulationControls{
        .paused = &m_paused,
        .sim_speed = &m_sim_speed,
        .reset = [this] { reset_showcase(); }
    });
    if (m_host) {
        AlternateViewPanel::draw_main_overlays(m_host->render(), m_main_view);
        AlternateViewPanel::draw(m_host->render(), m_alternate_view);
    }
    ImGui::End();
}

void SimulationMultiWell::draw_swarm_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 300.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f, 200.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Swarms")) { ImGui::End(); return; }
    memory::FrameVector<SwarmRecipeAction> actions{
        {.label = "Spawn avoider", .hotkey = "Ctrl+A", .spawn = [this] { spawn_avoider(); return m_last_swarm; }},
        {.label = "Spawn centroid", .hotkey = "Ctrl+C", .spawn = [this] { spawn_centroid_seeker(); return m_last_swarm; }},
        {.label = "Reset multi-well", .hotkey = "Ctrl+R", .spawn = [this] { reset_showcase(); return m_last_swarm; }}
    };
    SwarmRecipePanel::draw(SwarmRecipePanelState{}, actions, m_particles, m_goal_status, &m_last_swarm,
        SwarmRecipePanelOptions{.show_sim_controls = false, .show_particle_inspector = false});
    ImGui::End();
}

void SimulationMultiWell::draw_particle_panel() {
    ImGui::SetNextWindowPos(ImVec2(342.f, 72.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.f, 480.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Particles")) { ImGui::End(); return; }
    ParticleInspectorPanel::draw(m_particles.particles());
    ImGui::End();
}

void SimulationMultiWell::draw_goal_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 548.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f, 130.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Goals")) { ImGui::End(); return; }
    GoalStatusPanel::draw(metadata());
    ImGui::End();
}

void SimulationMultiWell::submit_geometry() {
    if (!m_host) return;
    submit_surface_sim_packets(m_host->render(), m_main_view, m_alternate_view,
        *m_surface, m_mesh, m_particles, SurfaceMeshOptions{
            .grid_lines = 70u,
            .time = 0.f,
            .color_scale = 1.25f,
            .wire_color = {0.92f, 0.96f, 1.f, 0.32f},
            .fill_color_mode = SurfaceFillColorMode::HeightCell,
            .build_contour = true
        }, &m_host->interaction(), &m_host->memory(), &m_host->camera());
}

} // namespace ndde
