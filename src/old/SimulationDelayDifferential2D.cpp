#include "app/SimulationDelayDifferential2D.hpp"

#include "app/Curve2DOverlay.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <glm/gtc/matrix_inverse.hpp>
#include <imgui.h>

namespace ndde {

namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;

void add_line(memory::FrameVector<Vertex>& out, Vec2 a, Vec2 b, Vec4 color) {
    out.push_back(Vertex{.pos = {a.x, a.y, 0.f}, .color = color});
    out.push_back(Vertex{.pos = {b.x, b.y, 0.f}, .color = color});
}

} // namespace

SimulationDelayDifferential2D::SimulationDelayDifferential2D(memory::MemoryService*) {}

void SimulationDelayDifferential2D::on_register(SimulationHost& host) {
    m_host = &host;
    m_panels.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - DDE Controls",
        .category = "Simulation",
        .scope = PanelScope::Simulation,
        .draw = [this] { draw_controls_panel(); }
    }));
    m_panels.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - DDE State",
        .category = "Simulation",
        .scope = PanelScope::Simulation,
        .draw = [this] { draw_state_panel(); }
    }));

    m_reset_hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = 'R', .mods = 2},
        .label = "Reset delay differential system",
        .callback = [this] { reset_problem(); }
    });
    m_run_hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = ' ', .mods = 2},
        .label = "Run delay differential system",
        .callback = [this] { m_running = !m_running; }
    });

    m_time_handle = host.render().register_view(RenderViewDescriptor{
        .title = "DDE Time Series",
        .kind = RenderViewKind::Main,
        .projection = CameraProjection::Orthographic,
        .camera_profile = CameraViewProfile::Orthographic2D,
        .overlays = {.show_axes = true, .show_grid = true}
    }, &m_time_view);
    m_delay_phase_handle = host.render().register_view(RenderViewDescriptor{
        .title = "DDE Delay Phase",
        .kind = RenderViewKind::Alternate,
        .alternate_mode = AlternateViewMode::VectorField,
        .projection = CameraProjection::Orthographic,
        .camera_profile = CameraViewProfile::Orthographic2D,
        .overlays = {.show_axes = true, .show_grid = true}
    }, &m_delay_phase_view);

    reset_problem();
}

void SimulationDelayDifferential2D::on_start() {
    if (!m_problem)
        reset_problem();
}

void SimulationDelayDifferential2D::on_tick(const TickInfo& tick) {
    if (!tick.paused && !m_paused && m_running) {
        step_problem(static_cast<double>(tick.dt) * m_sim_speed);
    }
    submit_geometry();
}

void SimulationDelayDifferential2D::on_stop() {
    m_panels.clear();
    m_reset_hotkey.reset();
    m_run_hotkey.reset();
    m_time_handle.reset();
    m_delay_phase_handle.reset();
    m_problem.reset();
    m_system.reset();
    m_host = nullptr;
}

SceneSnapshot SimulationDelayDifferential2D::snapshot() const {
    memory::FrameVector<ParticleSnapshot> particles =
        m_host ? m_host->memory().frame().make_vector<ParticleSnapshot>() : memory::FrameVector<ParticleSnapshot>{};
    if (m_problem && !m_problem->state().empty()) {
        const float x = static_cast<float>(m_problem->state()[0]);
        particles.push_back(ParticleSnapshot{
            .id = 1,
            .role = "State",
            .label = "DDE state",
            .u = static_cast<float>(m_problem->time()),
            .v = x,
            .x = static_cast<float>(m_problem->time()),
            .y = x,
            .z = 0.f
        });
    }
    return SceneSnapshot{
        .name = std::string(name()),
        .paused = m_paused || !m_running,
        .sim_time = static_cast<float>(m_problem ? m_problem->time() : 0.0),
        .sim_speed = static_cast<float>(m_sim_speed),
        .particle_count = particle_count(),
        .status = m_running ? "Running" : "Paused",
        .particles = std::move(particles)
    };
}

SimulationMetadata SimulationDelayDifferential2D::metadata() const {
    const sim::EquationSystemMetadata meta = m_system ? m_system->metadata() : sim::EquationSystemMetadata{};
    return SimulationMetadata{
        .name = std::string(name()),
        .surface_name = "Bounded DDE phase space",
        .surface_formula = meta.formula,
        .status = m_running ? "Running" : "Paused",
        .sim_time = static_cast<float>(m_problem ? m_problem->time() : 0.0),
        .sim_speed = static_cast<float>(m_sim_speed),
        .particle_count = particle_count(),
        .paused = m_paused || !m_running,
        .surface_has_analytic_derivatives = true,
        .surface_deformable = false,
        .surface_time_varying = true
    };
}

void SimulationDelayDifferential2D::reset_problem() {
    if (!m_host) return;
    auto& sim_memory = m_host->memory().simulation();
    m_problem.reset();
    m_system.reset();
    m_delay = std::max(m_delay, 0.01);
    m_damping = std::max(m_damping, 0.01);
    m_system = sim_memory.make_unique_as<sim::IDelayDifferentialSystem, sim::BoundedDelayedFeedbackSystem>(
        m_damping, m_feedback, m_delay);
    const double initial[] = {m_initial_value};
    m_problem = sim_memory.make_unique<sim::DelayInitialValueProblem>(
        m_host->memory(),
        *m_system,
        std::span<const double>{initial, 1u},
        0.0,
        m_history_sample_dt,
        seed_history,
        this);
}

void SimulationDelayDifferential2D::step_problem(double dt) {
    if (!m_problem || dt <= 0.0) return;
    double remaining = dt;
    const double max_step = std::max(m_step_size, 1.0e-4);
    while (remaining > 1.0e-12) {
        const double h = std::min(max_step, remaining);
        m_problem->step(m_solver, h);
        remaining -= h;
    }
}

void SimulationDelayDifferential2D::submit_geometry() {
    if (!m_host || !m_problem) return;
    auto& render = m_host->render();
    auto& memory = m_host->memory();
    const RenderViewDomain time_d = time_domain();
    const RenderViewDomain phase_d = delay_phase_domain();
    render.set_view_domain(m_time_view, time_d);
    render.set_view_domain(m_delay_phase_view, phase_d);
    update_hover();

    const Mat4 time_mvp = view_mvp(m_time_view);
    const Mat4 phase_mvp = view_mvp(m_delay_phase_view);

    memory::FrameVector<Vertex> time_axes = memory.frame().make_vector<Vertex>();
    add_line(time_axes, {time_d.u_min, 0.f}, {time_d.u_max, 0.f}, {0.86f, 0.20f, 0.18f, 1.f});
    add_line(time_axes, {time_d.u_min, time_d.v_min}, {time_d.u_min, time_d.v_max}, {0.20f, 0.82f, 0.34f, 1.f});
    render.submit(m_time_view, time_axes, Topology::LineList, DrawMode::VertexColor, {1, 1, 1, 1}, time_mvp);

    memory::FrameVector<Vertex> phase_axes = memory.frame().make_vector<Vertex>();
    add_line(phase_axes, {phase_d.u_min, 0.f}, {phase_d.u_max, 0.f}, {0.86f, 0.20f, 0.18f, 1.f});
    add_line(phase_axes, {0.f, phase_d.v_min}, {0.f, phase_d.v_max}, {0.20f, 0.82f, 0.34f, 1.f});
    render.submit(m_delay_phase_view, phase_axes, Topology::LineList, DrawMode::VertexColor, {1, 1, 1, 1}, phase_mvp);

    memory::FrameVector<Vertex> time_series = memory.frame().make_vector<Vertex>();
    memory::FrameVector<Vertex> delay_phase = memory.frame().make_vector<Vertex>();
    memory::FrameVector<Vec2> time_points = memory.frame().make_vector<Vec2>();
    memory::FrameVector<Vec2> delay_phase_points = memory.frame().make_vector<Vec2>();
    time_series.reserve(m_problem->history_size());
    delay_phase.reserve(m_problem->history_size());
    time_points.reserve(m_problem->history_size());
    delay_phase_points.reserve(m_problem->history_size());
    for (std::size_t i = 0; i < m_problem->history_size(); ++i) {
        const f64 t = m_problem->history_time(i);
        if (t < time_d.u_min) continue;
        const auto sample = m_problem->history_state(i);
        if (sample.empty()) continue;
        const float x = static_cast<float>(sample[0]);
        time_points.push_back({static_cast<float>(t), x});
        time_series.push_back(Vertex{.pos = {static_cast<float>(t), x, 0.f}, .color = {1.f, 0.72f, 0.18f, 1.f}});

        f64 delayed = 0.0;
        m_problem->query_history(t - m_delay, std::span<f64>{&delayed, 1u});
        delay_phase_points.push_back({x, static_cast<float>(delayed)});
        delay_phase.push_back(Vertex{
            .pos = {x, static_cast<float>(delayed), 0.f},
            .color = {0.35f, 0.75f, 1.f, 1.f}
        });
    }
    render.submit(m_time_view, time_series, Topology::LineStrip, DrawMode::VertexColor, {1, 1, 1, 1}, time_mvp);
    render.submit(m_delay_phase_view, delay_phase, Topology::LineStrip, DrawMode::VertexColor, {1, 1, 1, 1}, phase_mvp);

    if (!m_problem->state().empty()) {
        const float t = static_cast<float>(m_problem->time());
        const float x = static_cast<float>(m_problem->state()[0]);
        const float r_time = std::max((time_d.u_max - time_d.u_min) * 0.005f, 0.03f);
        memory::FrameVector<Vertex> marker = memory.frame().make_vector<Vertex>();
        add_line(marker, {t - r_time, x}, {t + r_time, x}, {1.f, 0.95f, 0.20f, 1.f});
        add_line(marker, {t, x - r_time}, {t, x + r_time}, {1.f, 0.95f, 0.20f, 1.f});
        render.submit(m_time_view, marker, Topology::LineList, DrawMode::VertexColor, {1, 1, 1, 1}, time_mvp);

        f64 delayed = 0.0;
        m_problem->query_history(m_problem->time() - m_delay, std::span<f64>{&delayed, 1u});
        const float r_phase = static_cast<float>(expected_bound()) * 0.025f;
        memory::FrameVector<Vertex> phase_marker = memory.frame().make_vector<Vertex>();
        add_line(phase_marker, {x - r_phase, static_cast<float>(delayed)}, {x + r_phase, static_cast<float>(delayed)}, {1.f, 0.95f, 0.20f, 1.f});
        add_line(phase_marker, {x, static_cast<float>(delayed) - r_phase}, {x, static_cast<float>(delayed) + r_phase}, {1.f, 0.95f, 0.20f, 1.f});
        render.submit(m_delay_phase_view, phase_marker, Topology::LineList, DrawMode::VertexColor, {1, 1, 1, 1}, phase_mvp);
    }

    auto submit_hover_marker = [&](RenderViewId view, const RenderViewDomain& domain, const Mat4& mvp) {
        const InteractionTarget hover = m_host->interaction().hover_target(view);
        if (!hover.valid || hover.kind != InteractionTargetKind::ViewPoint2D)
            return;
        const Vec2 p = hover.point2d;
        const float r = std::min(domain.u_max - domain.u_min, domain.v_max - domain.v_min) * 0.015f;
        memory::FrameVector<Vertex> hover_marker = memory.frame().make_vector<Vertex>();
        add_line(hover_marker, p + Vec2{-r, 0.f}, p + Vec2{r, 0.f}, {0.35f, 1.f, 0.95f, 1.f});
        add_line(hover_marker, p + Vec2{0.f, -r}, p + Vec2{0.f, r}, {0.35f, 1.f, 0.95f, 1.f});
        render.submit(view, hover_marker, Topology::LineList, DrawMode::VertexColor, {1, 1, 1, 1}, mvp);

        const RenderViewDescriptor* desc = render.descriptor(view);
        const auto& curve = view == m_time_view ? time_points : delay_phase_points;
        if (desc) {
            Curve2DHoverOverlayOptions options{
                .show_frenet = desc->overlays.show_hover_frenet,
                .show_osculating_circle = desc->overlays.show_osculating_circle,
                .show_velocity_arrow = desc->overlays.show_darboux_frame,
                .show_delay_ghost = desc->overlays.show_ghost_marker && view == m_time_view,
            };
            if (view == m_time_view) {
                f64 delayed = 0.0;
                m_problem->query_history(static_cast<f64>(p.x) - m_delay, std::span<f64>{&delayed, 1u});
                const float dxdt = static_cast<float>(-m_damping * static_cast<f64>(p.y)
                    + m_feedback * std::tanh(delayed));
                options.has_velocity = true;
                options.velocity = {1.f, dxdt};
                options.has_delay_ghost = true;
                options.delay_ghost = {p.x - static_cast<float>(m_delay), static_cast<float>(delayed)};
            }

            auto frame = build_curve2d_hover_overlay(
                std::span<const Vec2>{curve.data(), curve.size()},
                p,
                domain,
                options,
                &memory).vertices;
            render.submit(view, frame, Topology::LineList, DrawMode::VertexColor, {1, 1, 1, 1}, mvp);
        }
    };
    submit_hover_marker(m_time_view, time_d, time_mvp);
    submit_hover_marker(m_delay_phase_view, phase_d, phase_mvp);
}

void SimulationDelayDifferential2D::update_hover() {
    if (!m_host || !m_problem) return;
    auto& interaction = m_host->interaction();
    RenderViewId view = m_delay_phase_view;
    ViewMouseState mouse = interaction.mouse_state(view);
    if (!mouse.enabled) {
        view = m_time_view;
        mouse = interaction.mouse_state(view);
    }
    if (!mouse.enabled) return;

    const Mat4 inv = glm::inverse(view_mvp(view));
    const glm::vec4 world4 = inv * glm::vec4(mouse.ndc.x, mouse.ndc.y, 0.f, 1.f);
    if (std::abs(world4.w) < 1.0e-6f) return;
    const Vec3 world = Vec3{world4.x, world4.y, 0.f} / world4.w;
    interaction.set_hover_view_point(view, {world.x, world.y}, world);
}

void SimulationDelayDifferential2D::draw_controls_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 72.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(330.f, 310.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - DDE Controls")) { ImGui::End(); return; }

    ImGui::TextUnformatted("Bounded delayed feedback");
    ImGui::TextWrapped("x' = -a x(t) + b tanh(x(t - tau))");
    ImGui::Checkbox("Run", &m_running);
    ImGui::Checkbox("Paused", &m_paused);
    float speed = static_cast<float>(m_sim_speed);
    if (ImGui::SliderFloat("Sim speed", &speed, 0.05f, 4.f, "%.2f")) m_sim_speed = speed;
    float step = static_cast<float>(m_step_size);
    if (ImGui::SliderFloat("Step size", &step, 0.001f, 0.08f, "%.3f")) m_step_size = step;
    float initial = static_cast<float>(m_initial_value);
    if (ImGui::SliderFloat("Initial x", &initial, -3.f, 3.f, "%.2f")) m_initial_value = initial;
    float history_amp = static_cast<float>(m_history_amplitude);
    float history_cycles = static_cast<float>(m_history_cycles);
    if (ImGui::SliderFloat("History amp", &history_amp, 0.f, 2.5f, "%.2f")) m_history_amplitude = history_amp;
    if (ImGui::SliderFloat("History waves", &history_cycles, 0.5f, 3.5f, "%.2f")) m_history_cycles = history_cycles;
    float damping = static_cast<float>(m_damping);
    float feedback = static_cast<float>(m_feedback);
    float delay = static_cast<float>(m_delay);
    if (ImGui::SliderFloat("a damping", &damping, 0.05f, 3.f, "%.2f")) m_damping = damping;
    if (ImGui::SliderFloat("b feedback", &feedback, 0.05f, 4.f, "%.2f")) m_feedback = feedback;
    if (ImGui::SliderFloat("tau delay", &delay, 0.05f, 5.f, "%.2f")) m_delay = delay;
    if (ImGui::Button("Reset [Ctrl+R]")) reset_problem();
    ImGui::SameLine();
    if (ImGui::Button("Step")) step_problem(m_step_size);
    ImGui::End();
}

void SimulationDelayDifferential2D::draw_state_panel() {
    ImGui::SetNextWindowPos(ImVec2(380.f, 72.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.f, 220.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - DDE State")) { ImGui::End(); return; }
    if (m_problem && m_system) {
        const auto meta = m_system->metadata();
        ImGui::TextUnformatted(meta.name.c_str());
        ImGui::TextWrapped("%s", meta.formula.c_str());
        ImGui::Text("t %.3f", m_problem->time());
        ImGui::Text("x(t) %.6f", m_problem->state()[0]);
        f64 delayed = 0.0;
        m_problem->query_history(m_problem->time() - m_delay, std::span<f64>{&delayed, 1u});
        ImGui::Text("x(t-tau) %.6f", delayed);
        ImGui::Text("expected bound %.3f", expected_bound());
        ImGui::Text("history seed %.2f + %.2f sin(%.2f waves)",
                    m_initial_value, m_history_amplitude, m_history_cycles);
        ImGui::Text("history samples %zu", m_problem->history_size());
    }
    ImGui::End();
}

void SimulationDelayDifferential2D::seed_history(f64 t, std::span<f64> out, void* user) {
    const auto* self = static_cast<const SimulationDelayDifferential2D*>(user);
    if (!self) {
        out[0] = 0.0;
        return;
    }

    const double tau = std::max(self->m_delay, 1.0e-6);
    const double normalized = std::clamp((t + tau) / tau, 0.0, 1.0);
    const double wave = std::sin(kTwoPi * self->m_history_cycles * normalized);
    out[0] = self->m_initial_value + self->m_history_amplitude * wave;
}

RenderViewDomain SimulationDelayDifferential2D::time_domain() const noexcept {
    const float t = static_cast<float>(m_problem ? m_problem->time() : 0.0);
    const float bound = static_cast<float>(std::max(expected_bound(), std::abs(m_initial_value)) + 0.5);
    return {
        .u_min = std::max(0.f, t - m_time_window),
        .u_max = std::max(m_time_window, t),
        .v_min = -bound,
        .v_max = bound,
        .z_min = -1.f,
        .z_max = 1.f
    };
}

RenderViewDomain SimulationDelayDifferential2D::delay_phase_domain() const noexcept {
    const float bound = static_cast<float>(std::max(expected_bound(), std::abs(m_initial_value)) + 0.5);
    return {.u_min = -bound, .u_max = bound, .v_min = -bound, .v_max = bound, .z_min = -1.f, .z_max = 1.f};
}

Mat4 SimulationDelayDifferential2D::view_mvp(RenderViewId view) const noexcept {
    return m_host ? m_host->camera().orthographic_mvp(view, 0.06f) : Mat4{1.f};
}

double SimulationDelayDifferential2D::expected_bound() const noexcept {
    return m_damping > 0.0 ? std::abs(m_feedback / m_damping) : std::abs(m_feedback);
}

} // namespace ndde
