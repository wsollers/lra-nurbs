#include "app/SimulationSurfaceGaussian.hpp"
#include "app/AlternateViewPanel.hpp"
#include "app/SimulationRenderPackets.hpp"
#include "memory/Containers.hpp"

#include <imgui.h>
#include <algorithm>
#include <span>
#include <utility>

namespace ndde {

SimulationSurfaceGaussian::SimulationSurfaceGaussian(memory::MemoryService* mem)
    : m_surface(mem ? mem->simulation().make_unique<GaussianRipple>()
                    : memory::make_unique<GaussianRipple>(std::pmr::get_default_resource()))
    , m_particles(m_surface.get(), 1337u)
    , m_spawner(m_particles, m_sim_time, m_goal_status)
{
    sync_context();
}

void SimulationSurfaceGaussian::on_register(SimulationHost& host) {
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
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - Differential Eq",
        .category = "Simulation",
        .scope = PanelScope::Simulation,
        .draw = [this] { draw_differential_panel(); }
    }));
    m_reset_hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = 'R', .mods = 2},
        .label = "Reset Gaussian pursuit",
        .category = "Simulation",
        .callback = [this] { reset_showcase(); }
    });
    m_cloud_hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = 'B', .mods = 2},
        .label = "Gaussian Brownian cloud",
        .category = "Simulation",
        .callback = [this] { spawn_cloud(); }
    });
    m_main_view_handle = host.render().register_view(RenderViewDescriptor{
        .title = "Surface 3D",
        .kind = RenderViewKind::Main,
        .camera_profile = CameraViewProfile::PerspectiveSurface3D,
        .overlays = {
            .show_axes = true,
            .show_grid = true,
            .show_frame = false,
            .show_labels = false,
            .show_hover_frenet = true,
            .show_osculating_circle = true
        }
    }, &m_main_view);
    m_alternate_view_handle = host.render().register_view(RenderViewDescriptor{
        .title = "Surface Alternate",
        .kind = RenderViewKind::Alternate,
        .alternate_mode = AlternateViewMode::Contour,
        .projection = CameraProjection::Orthographic,
        .camera_profile = CameraViewProfile::Orthographic2D,
        .overlays = {
            .show_axes = true,
            .show_grid = false,
            .show_frame = false,
            .show_labels = false,
            .show_hover_frenet = false,
            .show_osculating_circle = false
        },
        .alternate = {
            .isocline_direction_angle = 0.f,
            .isocline_target_slope = 0.f,
            .isocline_tolerance = 0.35f,
            .isocline_bands = 5u,
            .vector_mode = VectorFieldMode::NegativeGradient,
            .vector_samples = 18u,
            .vector_scale = 1.f,
            .flow_seed_count = 8u,
            .flow_steps = 28u,
            .flow_step_size = 0.10f
        }
    }, &m_alternate_view);
    reset_differential_problem();
}

void SimulationSurfaceGaussian::on_start() {
    reset_showcase();
}

void SimulationSurfaceGaussian::on_tick(const TickInfo& tick) {
    apply_surface_commands();

    if (tick.paused || m_paused) {
        m_context.set_tick(TickInfo{.tick_index = tick.tick_index, .dt = 0.f, .time = m_sim_time, .paused = true});
        submit_geometry();
        m_context.commands().clear();
        return;
    }

    m_sim_time = tick.time;
    m_context.set_tick(tick);
    m_surface->advance(tick.dt);
    m_particles.update(tick.dt, m_sim_speed, m_sim_time);
    if (m_ode_advance_with_sim)
        step_differential_problem(static_cast<double>(tick.dt) * static_cast<double>(m_sim_speed));
    m_context.dirty().mark_particles_changed();
    m_context.math_cache().bump_particles();

    submit_geometry();
    m_context.commands().clear();
}

void SimulationSurfaceGaussian::on_stop() {
    m_panel_handles.clear();
    m_reset_hotkey.reset();
    m_cloud_hotkey.reset();
    m_main_view_handle.reset();
    m_alternate_view_handle.reset();
    m_ode_problem.reset();
    m_ode_system.reset();
    m_host = nullptr;
}

SceneSnapshot SimulationSurfaceGaussian::snapshot() const {
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

void SimulationSurfaceGaussian::reset_showcase() {
    m_last_swarm = m_spawner.spawn_showcase();
    sync_context();
    m_context.dirty().mark_particles_changed();
    m_context.math_cache().bump_particles();
}

void SimulationSurfaceGaussian::spawn_cloud() {
    m_last_swarm = m_spawner.spawn_brownian_cloud();
    sync_context();
    m_context.dirty().mark_particles_changed();
    m_context.math_cache().bump_particles();
}

void SimulationSurfaceGaussian::spawn_contour_band() {
    m_last_swarm = m_spawner.spawn_contour_band();
    sync_context();
    m_context.dirty().mark_particles_changed();
    m_context.math_cache().bump_particles();
}

SimulationMetadata SimulationSurfaceGaussian::metadata() const {
    const ndde::math::SurfaceMetadata surface = m_surface->metadata(m_surface->time());
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

void SimulationSurfaceGaussian::draw_control_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 72.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f, 210.f), ImGuiCond_FirstUseEver);
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

void SimulationSurfaceGaussian::draw_swarm_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 300.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f, 230.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Swarms")) { ImGui::End(); return; }
    memory::FrameVector<SwarmRecipeAction> actions{
        {.label = "Reset pursuit", .hotkey = "Ctrl+R", .spawn = [this] { reset_showcase(); return m_last_swarm; }},
        {.label = "Brownian cloud", .hotkey = "Ctrl+B", .spawn = [this] { spawn_cloud(); return m_last_swarm; }},
        {.label = "Contour band", .hotkey = "", .spawn = [this] { spawn_contour_band(); return m_last_swarm; }}
    };
    SwarmRecipePanel::draw(SwarmRecipePanelState{}, actions, m_particles, m_goal_status, &m_last_swarm,
        SwarmRecipePanelOptions{
            .show_level_curve_controls = false,
            .show_brownian_controls = false,
            .show_trail_controls = false,
            .show_sim_controls = false,
            .show_particle_inspector = false,
            .goal_success_text = "Pursuit captured"
        });
    ImGui::End();
}

void SimulationSurfaceGaussian::draw_particle_panel() {
    ImGui::SetNextWindowPos(ImVec2(342.f, 72.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.f, 480.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Particles")) { ImGui::End(); return; }
    ParticleInspectorPanel::draw(m_particles.particles(), ParticleInspectorOptions{
        .label = "Particles",
        .show_level_curve_controls = true,
        .show_brownian_controls = true,
        .show_trail_controls = true
    });
    ImGui::End();
}

void SimulationSurfaceGaussian::draw_goal_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 548.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f, 130.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Goals")) { ImGui::End(); return; }
    GoalStatusPanel::draw(metadata());
    ImGui::End();
}

void SimulationSurfaceGaussian::draw_differential_panel() {
    ImGui::SetNextWindowPos(ImVec2(784.f, 72.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.f, 220.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Differential Eq")) { ImGui::End(); return; }

    if (!m_ode_problem) {
        ImGui::TextUnformatted("No differential problem");
        if (ImGui::Button("Create")) reset_differential_problem();
        ImGui::End();
        return;
    }

    const sim::EquationSystemMetadata meta = m_ode_problem->system().metadata();
    ImGui::Text("System: %s", meta.name.c_str());
    ImGui::Text("Formula: %s", meta.formula.c_str());
    ImGui::Text("Variables: %s", meta.variables.c_str());
    ImGui::Separator();

    ImGui::Checkbox("RK4 solver", &m_ode_use_rk4);
    ImGui::Checkbox("Advance with sim", &m_ode_advance_with_sim);
    float step = static_cast<float>(m_ode_step_size);
    if (ImGui::SliderFloat("Step size", &step, 0.001f, 0.25f, "%.3f"))
        m_ode_step_size = static_cast<double>(step);

    const auto state = m_ode_problem->state();
    ImGui::Text("t = %.4f", m_ode_problem->time());
    if (!state.empty())
        ImGui::Text("y = %.6e", state[0]);
    ImGui::Text("history samples: %zu", m_ode_problem->history_size());

    if (ImGui::Button("Step")) step_differential_problem(m_ode_step_size);
    ImGui::SameLine();
    if (ImGui::Button("Reset")) m_ode_problem->reset();

    ImGui::End();
}

void SimulationSurfaceGaussian::submit_geometry() {
    if (!m_host || m_main_view == 0 || m_alternate_view == 0) return;

    submit_surface_sim_packets(m_host->render(), m_main_view, m_alternate_view,
        *m_surface, m_mesh, m_particles, SurfaceMeshOptions{
        .grid_lines = 32u,
        .time = m_surface->time(),
        .color_scale = 1.75f,
        .wire_color = {0.92f, 0.96f, 1.f, 0.34f},
        .fill_color_mode = SurfaceFillColorMode::HeightCell,
        .build_contour = true
    }, &m_host->interaction(), &m_host->memory(), &m_host->camera());
}

void SimulationSurfaceGaussian::reset_differential_problem() {
    if (!m_host) return;
    const f64 initial[] = {1.0};
    m_ode_problem.reset();
    m_ode_system = m_host->memory().simulation().make_unique<sim::ExponentialGrowthSystem>(1.0);
    m_ode_problem = m_host->memory().simulation().make_unique<sim::InitialValueProblem>(
        m_host->memory(), *m_ode_system, std::span<const f64>{initial});
}

void SimulationSurfaceGaussian::step_differential_problem(double dt) {
    if (!m_ode_problem || dt <= 0.0) return;
    const sim::IOdeSolver& solver = m_ode_use_rk4
        ? static_cast<const sim::IOdeSolver&>(m_rk4_solver)
        : static_cast<const sim::IOdeSolver&>(m_euler_solver);
    const double step = m_ode_step_size > 0.0 ? m_ode_step_size : dt;
    double remaining = dt;
    while (remaining > 1e-12) {
        const double h = std::min(step, remaining);
        m_ode_problem->step(solver, h);
        remaining -= h;
    }
}

void SimulationSurfaceGaussian::apply_surface_commands() {
    if (!m_host || m_main_view == 0) return;

    for (const SurfacePickRequest& command : m_host->interaction().consume_surface_picks(m_main_view)) {
        Vec2 uv = command.fallback_uv;
        const Mat4 mvp = m_host->camera().perspective_mvp(m_main_view);
        const SurfaceHit hit = m_host->interaction().resolve_surface_hit(
            m_main_view, *m_surface, mvp, command.screen_ndc, m_surface->time());
        if (hit.hit) {
            uv = hit.uv;
            m_host->interaction().select_surface(hit);
        }
        const float sign = (command.seed % 2u == 0u) ? 1.f : -1.f;
        auto& params = m_surface->params();
        params.amplitude = sign * std::max(0.05f, command.amplitude);
        params.sigma = std::max(0.25f, command.radius);
        params.wavelength = std::max(0.5f, command.radius * 2.1f);
        params.damping = std::max(0.05f, 0.35f * command.falloff);
        m_surface->set_epicentre(uv.x, uv.y);
        m_mesh.mark_dirty();
        m_context.queue_perturbation(SurfacePerturbation{
            .uv = uv,
            .amplitude = params.amplitude,
            .radius = command.radius,
            .falloff = command.falloff,
            .seed = command.seed
        });
        m_context.math_cache().bump_surface();
    }
}

void SimulationSurfaceGaussian::sync_context() {
    if (m_host) m_particles.bind_memory(&m_host->memory());
    m_particles.set_surface(m_surface.get());
    m_context.set_surface(m_surface.get());
    m_context.set_particles(&m_particles.particles());
    m_context.set_rng(&m_particles.rng());
    m_context.set_time(m_sim_time);
}

} // namespace ndde
