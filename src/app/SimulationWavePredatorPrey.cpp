// app/SimulationWavePredatorPrey.cpp
// Refactored thin simulation host using ScenarioBuilder + EventBusService.

#include "app/SimulationWavePredatorPrey.hpp"
#include "app/AlternateViewPanel.hpp"
#include "app/FrenetFrame.hpp"
#include "app/SimulationRenderPackets.hpp"
#include "simulation/fields/MetricRipple.hpp"
#include "telemetry/TelemetryRecord.hpp"
#include "memory/Containers.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <format>
#include <limits>

namespace ndde {

namespace {

class FieldVisualSurface final : public math::ISurface {
public:
    FieldVisualSurface(const math::ISurface& base,
                       const simulation::FieldCompositor& fields)
        : m_base(base)
        , m_fields(fields)
    {}

    [[nodiscard]] Vec3 evaluate(f32 u, f32 v, f32 t = 0.f) const override {
        Vec3 p = m_base.evaluate(u, v, t);
        p.z += m_fields.surface_displacement(u, v, t);
        return p;
    }

    [[nodiscard]] f32 u_min(f32 t = 0.f) const override { return m_base.u_min(t); }
    [[nodiscard]] f32 u_max(f32 t = 0.f) const override { return m_base.u_max(t); }
    [[nodiscard]] f32 v_min(f32 t = 0.f) const override { return m_base.v_min(t); }
    [[nodiscard]] f32 v_max(f32 t = 0.f) const override { return m_base.v_max(t); }
    [[nodiscard]] bool is_periodic_u() const override { return m_base.is_periodic_u(); }
    [[nodiscard]] bool is_periodic_v() const override { return m_base.is_periodic_v(); }
    [[nodiscard]] bool is_time_varying() const override {
        return !m_fields.empty() || m_base.is_time_varying();
    }

    [[nodiscard]] math::SurfaceMetadata metadata(f32 t = 0.f) const override {
        math::SurfaceMetadata data = m_base.metadata(t);
        data.deformable = true;
        data.time_varying = is_time_varying();
        return data;
    }

private:
    const math::ISurface& m_base;
    const simulation::FieldCompositor& m_fields;
};

} // namespace

SimulationWavePredatorPrey::SimulationWavePredatorPrey(memory::MemoryService* memory)
    : m_surface(SurfaceRegistry::make_wave_predator_prey(memory, f32(4)))
    , m_particles(m_surface.get(), u32(4242))
{
    sync_context();
}

void SimulationWavePredatorPrey::on_register(SimulationHost& host) {
    m_host = &host;
    m_particles.bind_memory(&host.memory());
    sync_context();

    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - Controls", .category = "Simulation",
        .scope = PanelScope::Simulation, .draw = [this]{ draw_control_panel(); }
    }));
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - Swarms", .category = "Simulation",
        .scope = PanelScope::Simulation, .draw = [this]{ draw_swarm_panel(); }
    }));
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - Particles", .category = "Simulation",
        .scope = PanelScope::Simulation, .draw = [this]{ draw_particle_panel(); }
    }));
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - Goals", .category = "Simulation",
        .scope = PanelScope::Simulation, .draw = [this]{ draw_goal_panel(); }
    }));
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Sim - Events", .category = "Simulation",
        .scope = PanelScope::Simulation, .draw = [this]{ draw_events_panel(); }
    }));

    m_reset_hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = 'R', .mods = 2},
        .label = "Reset predator/prey",
        .callback = [this]{ reset_showcase(); }
    });
    m_cloud_hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = 'B', .mods = 2},
        .label = "Brownian cloud",
        .callback = [this]{ spawn_brownian_cloud(); }
    });
    m_contour_hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = 'L', .mods = 2},
        .label = "Contour band",
        .callback = [this]{ spawn_contour_band(); }
    });

    m_main_handle = host.render().register_view(RenderViewDescriptor{
        .title = "Wave Predator-Prey 3D",
        .kind = RenderViewKind::Main,
        .camera_profile = CameraViewProfile::PerspectiveSurface3D,
        .overlays = {.show_axes = true, .show_grid = true,
                     .show_hover_frenet = true, .show_osculating_circle = true}
    }, &m_main_view);

    m_alt_handle = host.render().register_view(RenderViewDescriptor{
        .title = "Wave Predator-Prey Alternate",
        .kind = RenderViewKind::Alternate,
        .alternate_mode = AlternateViewMode::Flow,
        .projection = CameraProjection::Orthographic,
        .camera_profile = CameraViewProfile::Orthographic2D,
        .overlays = {.show_axes = true},
        .alternate = {
            .vector_mode = VectorFieldMode::LevelTangent,
            .vector_samples = 20u, .vector_scale = 1.1f,
            .flow_seed_count = 11u, .flow_steps = 48u, .flow_step_size = 0.075f
        }
    }, &m_alternate_view);
}

void SimulationWavePredatorPrey::on_start() { reset_showcase(); }

void SimulationWavePredatorPrey::on_tick(const TickInfo& tick) {
    m_context.set_tick(tick);
    handle_poke(tick);
    log_ripple_diagnostics(tick);
    advance_state(tick, true);
    submit_geometry();
}

void SimulationWavePredatorPrey::on_simulation_tick(const TickInfo& tick) {
    m_context.set_tick(tick);
    advance_state(tick, false);
}

void SimulationWavePredatorPrey::on_simulation_command(const SimulationThreadCommand& command) {
    if (command.kind == SimulationThreadCommandKind::SurfacePoke) {
        apply_surface_poke(command.surface_poke, command.tick, false);
    }
}

void SimulationWavePredatorPrey::on_submit_render() {
    submit_geometry();
}

void SimulationWavePredatorPrey::advance_state(const TickInfo& tick, bool allow_host_events) {
    if (!tick.paused && !m_paused) {
        m_sim_time = tick.time;
        if (!m_fields.empty())
            m_mesh.mark_dirty();
        m_particles.update(tick.dt, m_sim_speed, m_sim_time, &m_fields);
        m_context.dirty().mark_particles_changed();

        const GoalStatus gs = m_particles.evaluate_goals(m_sim_time);
        if (gs != GoalStatus::Running) {
            m_goal_status = gs;
            m_paused = true;
            if (gs == GoalStatus::Succeeded) {
                if (allow_host_events && m_host) {
                    m_host->events().publish(EventChannelId::Simulation, simulation::events::AgentCaptured{
                        .pursuer_id = u64(0), .prey_id = u64(0),
                        .distance   = f32(0),
                        .sim_time   = m_sim_time,
                        .tick       = tick.tick_index
                    });
                } else if (m_host) {
                    m_host->threads().publish_event_record(
                        EventChannelId::Simulation,
                        events::make_agent_captured(u64(0), u64(0), f32(0), m_sim_time, tick.tick_index));
                }
            }
        }

        if (allow_host_events) {
            evaluate_alerts(tick);
            sweep_decayed_fields(tick);
        } else {
            const auto removed = m_fields.sweep_decayed(m_sim_time);
            if (!removed.empty()) {
                m_mesh.mark_dirty();
            }
            if (m_host) {
                for (const auto& fname : removed) {
                    m_host->threads().publish_event_record(
                        EventChannelId::Simulation,
                        events::make_field_removed(runtime_node_id_from_text(fname).value, m_sim_time, tick.tick_index));
                    m_host->threads().publish_event_record(
                        EventChannelId::Simulation,
                        events::make_perturbation_decayed(f32(0), f32(0), m_sim_time, tick.tick_index));
                }
            }
        }
        if (allow_host_events && m_host)
            m_host->events().drain(EventChannelId::Simulation, m_sim_time, tick.tick_index);
    }
}

void SimulationWavePredatorPrey::on_stop() {
    if (m_host) {
        m_host->events().publish(EventChannelId::Simulation, simulation::events::ScenarioStopped{
            .scenario    = runtime_node_id_from_text(name()),
            .sim_time    = m_sim_time,
            .total_ticks = u64(0)
        });
        m_host->events().drain(EventChannelId::Simulation, m_sim_time, u64(0));
    }
    m_panel_handles.clear();
    m_reset_hotkey.reset();
    m_cloud_hotkey.reset();
    m_contour_hotkey.reset();
    m_main_handle.reset();
    m_alt_handle.reset();
    m_host = nullptr;
}

SceneSnapshot SimulationWavePredatorPrey::snapshot() const {
    return SceneSnapshot{
        .name           = std::string(name()),
        .paused         = m_paused,
        .sim_time       = m_sim_time,
        .sim_speed      = m_sim_speed,
        .particle_count = m_particles.size(),
        .status         = m_goal_status == GoalStatus::Succeeded ? "Succeeded" : "Running",
        .particles      = m_particles.snapshot_particles()
    };
}

SimulationMetadata SimulationWavePredatorPrey::metadata() const {
    const auto surface = m_surface->metadata(m_sim_time);
    return SimulationMetadata{
        .name                             = std::string(name()),
        .surface_name                     = std::string(surface.name),
        .surface_formula                  = std::string(surface.formula),
        .status                           = m_goal_status == GoalStatus::Succeeded ? "Succeeded" : "Running",
        .sim_time                         = m_sim_time,
        .sim_speed                        = m_sim_speed,
        .particle_count                   = m_particles.size(),
        .paused                           = m_paused,
        .goal_succeeded                   = m_goal_status == GoalStatus::Succeeded,
        .surface_has_analytic_derivatives = surface.has_analytic_derivatives,
        .surface_deformable               = surface.deformable,
        .surface_time_varying             = surface.time_varying
    };
}

// ============================================================================
// Spawn / Reset
// ============================================================================

void SimulationWavePredatorPrey::reset_particles() {
    m_particles.clear_all();
    m_fields.clear();
    m_alerts.clear();
    m_goal_status = GoalStatus::Running;
    m_sim_time    = f32(0);
    m_mesh.mark_dirty();
}

void SimulationWavePredatorPrey::reset_showcase() {
    reset_particles();
    sync_context();
    if (!m_host) return;

    // Prey agent prototype — Leaders that avoid the nearest Chaser with delay
    simulation::AgentSpec prey_spec;
    prey_spec.label            = "Prey - Delayed Avoid - Brownian";
    prey_spec.role             = ParticleRole::Leader;
    prey_spec.colour_slot      = u32(0);
    prey_spec.spawn.mode       = simulation::SpawnMode::RingPack;
    prey_spec.spawn.center     = {f32(0), f32(0)};
    prey_spec.spawn.radius     = f32(0.85);
    prey_spec.noise.sigma          = f32(0.09);
    prey_spec.noise.drift_strength = f32(-0.04);
    prey_spec.has_pursuit      = true;
    prey_spec.pursuit.speed         = f32(0.48);
    prey_spec.pursuit.delay_seconds = f32(0.23);
    prey_spec.pursuit.target_role   = ParticleRole::Chaser;
    prey_spec.pursuit.avoid         = true;
    prey_spec.use_history      = true;
    prey_spec.history_capacity = u64(4096);
    prey_spec.trail_mode       = TrailMode::Persistent;
    prey_spec.trail_max_points = u32(2200);
    prey_spec.trail_min_spacing = f32(0.010);

    // Predator agent prototype — Chasers that seek the nearest Leader with delay
    simulation::AgentSpec pred_spec;
    pred_spec.label             = "Predator - Delayed Seek - Brownian";
    pred_spec.role              = ParticleRole::Chaser;
    pred_spec.colour_slot       = u32(0);
    pred_spec.spawn.mode        = simulation::SpawnMode::RingPack;
    pred_spec.spawn.center      = {f32(0), f32(0)};
    pred_spec.spawn.radius      = f32(2.85);
    pred_spec.spawn.phase_offset = f32(0.35);
    pred_spec.noise.sigma           = f32(0.05);
    pred_spec.has_pursuit       = true;
    pred_spec.pursuit.speed          = f32(0.92);
    pred_spec.pursuit.delay_seconds  = f32(0.42);
    pred_spec.pursuit.target_role    = ParticleRole::Leader;
    pred_spec.pursuit.avoid          = false;
    pred_spec.trail_mode        = TrailMode::Finite;
    pred_spec.trail_max_points  = u32(1400);
    pred_spec.trail_min_spacing = f32(0.012);

    simulation::ScenarioBuilder builder;
    builder.named("Wave Predator-Prey")
           .on_surface(m_surface.get())
           .with_field(std::make_shared<simulation::DampingField>(f32(0.02)))
           .add_agents(u32(3),  prey_spec)
           .add_agents(u32(9),  pred_spec)
           .alert_proximity(f32(0.5))
           .alert_escape(f32(3.5))
           .alert_capture_pending(f32(2.0));

    builder.build(m_particles, m_fields, m_alerts,
                  m_host->events(),
                  EventChannelId::Simulation,
                  m_host ? &m_host->memory() : nullptr,
                  m_sim_time, u64(0));

    m_particles.add_goal<CaptureGoal>(CaptureGoal::Params{
        .seeker_role = ParticleRole::Chaser,
        .target_role = ParticleRole::Leader,
        .radius      = f32(0.18)
    });

    for (i32 i = 0; i < 100; ++i) {
        m_sim_time += f32(1.0 / 60.0);
        m_particles.update(f32(1.0 / 60.0), f32(1), m_sim_time, &m_fields);
    }

    sync_context();
}

void SimulationWavePredatorPrey::spawn_brownian_cloud() {
    ParticleSwarmFactory swarms(m_particles);
    m_last_swarm = swarms.brownian_cloud({
        .count = 18u, .center = {f32(0), f32(0)}, .radius = f32(2.2),
        .role = ParticleRole::Avoider,
        .brownian = {.sigma = f32(0.13), .drift_strength = f32(0.025)},
        .trail = {TrailMode::Finite, 900u, f32(0.014)},
        .label = "Avoider - Brownian Cloud"
    });
    sync_context();
}

void SimulationWavePredatorPrey::spawn_contour_band() {
    ParticleSwarmFactory swarms(m_particles);
    ndde::sim::LevelCurveWalker::Params walker;
    walker.epsilon = f32(0.16);
    walker.walk_speed = f32(0.54);
    walker.turn_rate = f32(2.7);
    walker.tangent_floor = f32(0.45);
    m_last_swarm = swarms.contour_band({
        .count = 12u, .center = {f32(0), f32(0)}, .radius = f32(1.8),
        .shared_level = true, .role = ParticleRole::Neutral, .walker = walker,
        .noise = {.sigma = f32(0.018)},
        .trail = {TrailMode::Persistent, 2200u, f32(0.010)},
        .label = "Neutral - Contour Band"
    });
    sync_context();
}

// ============================================================================
// Internal helpers
// ============================================================================

void SimulationWavePredatorPrey::handle_poke(const TickInfo& tick) {
    if (!m_host) return;

    const Mat4 mvp = m_host->camera().perspective_mvp(m_main_view);
    auto picks = m_host->interaction().consume_surface_picks(m_main_view);
    if (picks.empty()) return;

    m_host->events().log(EventChannelId::Simulation).push_engine_string(std::format(
            "[{:>7.2f}] DEBUG  surface-pick queue count={} paused={} sim_paused={}",
            m_sim_time, picks.size(), tick.paused ? "yes" : "no", m_paused ? "yes" : "no"),
        events::EventSeverity::Info);

    for (const SurfacePickRequest& pick : picks) {
        const SurfaceHit hit = m_host->interaction().resolve_surface_hit(
            m_main_view, *m_surface, mvp, pick.screen_ndc, m_sim_time);
        const Vec2 uv = hit.hit ? hit.uv : pick.fallback_uv;
        apply_surface_poke(SimulationSurfacePoke{
            .view = m_main_view,
            .uv = uv,
            .fallback_uv = pick.fallback_uv,
            .screen_ndc = pick.screen_ndc,
            .amplitude = pick.amplitude,
            .radius = pick.radius,
            .falloff = pick.falloff,
            .ray_hit = hit.hit,
            .seed = pick.seed
        }, tick, true);
    }
}

void SimulationWavePredatorPrey::apply_surface_poke(const SimulationSurfacePoke& poke,
                                                    const TickInfo& tick,
                                                    bool allow_host_events) {
    const Vec2 uv = poke.uv;
    m_last_poke_uv = uv;

    simulation::events::PerturbationFired evt;
    evt.u         = uv.x;
    evt.v         = uv.y;
    evt.amplitude = ((poke.seed % 2u) == 0u ? f32(1) : f32(-1))
        * std::max(f32(0.18), poke.amplitude * f32(2));
    evt.omega     = f32(6.28);
    evt.k_wave    = ops::two_pi_v<f32> / std::max(f32(0.6), poke.radius * f32(2.1));
    evt.alpha     = f32(1) / std::max(f32(0.35), poke.radius);
    evt.beta      = std::max(f32(0.05), poke.falloff * f32(0.35));
    evt.sim_time  = m_sim_time;
    evt.tick      = tick.tick_index;
    evt.seed      = poke.seed != 0u ? poke.seed : m_poke_seed;
    ++m_poke_seed;

    auto ripple = std::make_shared<simulation::MetricRipple>(
        simulation::MetricRipple::from_event(evt));
    m_fields.add(ripple);
    m_mesh.mark_dirty();
    m_ripple_debug_frames = u32(8);

    if (!m_host) return;

    if (!allow_host_events) {
        m_host->threads().publish_event_record(
            EventChannelId::Simulation,
            events::make_perturbation_fired(uv.x, uv.y, evt.amplitude, m_sim_time, tick.tick_index));
        m_host->threads().publish_event_record(
            EventChannelId::Simulation,
            events::make_field_added(runtime_node_id_from_text(ripple->name()).value, m_sim_time, tick.tick_index));
        return;
    }

    const f32 metric_now = m_fields.metric_factor(uv.x, uv.y, m_sim_time);
    const f32 metric_later = m_fields.metric_factor(uv.x, uv.y, m_sim_time + f32(0.25));
    m_host->events().log(EventChannelId::Simulation).push_engine_string(std::format(
        "[{:>7.2f}] DEBUG  pick {} uv=({:.3f},{:.3f}) fallback=({:.3f},{:.3f}) ndc=({:.2f},{:.2f})",
        m_sim_time,
        poke.ray_hit ? "ray-hit" : "fallback",
        uv.x, uv.y,
        poke.fallback_uv.x, poke.fallback_uv.y,
        poke.screen_ndc.x, poke.screen_ndc.y),
        poke.ray_hit ? events::EventSeverity::Notice : events::EventSeverity::Warning);
    m_host->events().log(EventChannelId::Simulation).push_engine_string(std::format(
        "[{:>7.2f}] DEBUG  fields={} metric@poke now={:.5f} metric@poke +0.25s={:.5f}",
        m_sim_time, m_fields.size(), metric_now, metric_later),
        events::EventSeverity::Info);
    m_host->events().log(EventChannelId::Simulation).push_engine_string(std::format(
        "[{:>7.2f}] DEBUG  render note: field-driven radial wave A={:.3f} k={:.3f} alpha={:.3f} beta={:.3f}",
        m_sim_time, evt.amplitude, evt.k_wave, evt.alpha, evt.beta),
        events::EventSeverity::Notice);

    m_host->events().publish(EventChannelId::Simulation, evt);
    m_host->events().publish(EventChannelId::Simulation, simulation::events::FieldAdded{
        .field      = runtime_node_id_from_text(ripple->name()),
        .sim_time   = m_sim_time,
        .tick       = tick.tick_index
    });
}

void SimulationWavePredatorPrey::log_ripple_diagnostics(const TickInfo& tick) {
    if (m_ripple_debug_frames == u32(0) || m_fields.empty()) return;
    --m_ripple_debug_frames;

    const f32 t = m_sim_time;
    const f32 sample_radius = f32(0.35);
    const std::array<Vec2, 5> samples{{
        m_last_poke_uv,
        {m_last_poke_uv.x + sample_radius, m_last_poke_uv.y},
        {m_last_poke_uv.x - sample_radius, m_last_poke_uv.y},
        {m_last_poke_uv.x, m_last_poke_uv.y + sample_radius},
        {m_last_poke_uv.x, m_last_poke_uv.y - sample_radius}
    }};

    f32 min_factor = std::numeric_limits<f32>::max();
    f32 max_factor = std::numeric_limits<f32>::lowest();
    for (const Vec2 uv : samples) {
        const f32 factor = m_fields.metric_factor(uv.x, uv.y, t);
        min_factor = std::min(min_factor, factor);
        max_factor = std::max(max_factor, factor);
    }

    if (!m_host) return;
    m_host->events().log(EventChannelId::Simulation).push_engine_string(std::format(
            "[{:>7.2f}] DEBUG  ripple-sample tick={} fields={} metric range [{:.5f}, {:.5f}] near ({:.3f},{:.3f})",
            t, tick.tick_index, m_fields.size(), min_factor, max_factor, m_last_poke_uv.x, m_last_poke_uv.y),
        events::EventSeverity::Info);
}

void SimulationWavePredatorPrey::evaluate_alerts(const TickInfo& tick) {
    if (m_alerts.empty() || !m_host) return;
    memory::FrameVector<events::AlertParticleView> alert_particles =
        m_host->memory().frame().make_vector<events::AlertParticleView>();
    alert_particles.reserve(m_particles.size());
    for (const AnimatedCurve& particle : m_particles.particles()) {
        f32 kappa_g = f32(0);
        const u32 trail_size = particle.trail_size();
        if (trail_size >= u32(4)) {
            const FrenetFrame frenet = particle.frenet_at(trail_size - u32(1));
            const Vec2 uv = particle.head_uv();
            const SurfaceFrame sf = make_surface_frame(*m_surface, uv.x, uv.y, m_sim_time, &frenet);
            kappa_g = sf.kappa_g;
        }
        alert_particles.push_back(events::AlertParticleView{
            .id = particle.id(),
            .role = particle.particle_role(),
            .uv = particle.head_uv(),
            .trail_size = trail_size,
            .geodesic_curvature = kappa_g
        });
    }
    const events::AlertContext ctx{
        .sim_time       = m_sim_time,
        .tick           = tick.tick_index,
        .surface        = m_surface.get(),
        .particles      = alert_particles.data(),
        .particle_count = static_cast<u64>(alert_particles.size())
    };
    for (auto& rule : m_alerts)
        rule->evaluate(ctx, m_host->events().log(EventChannelId::Simulation).ring());
}

void SimulationWavePredatorPrey::sweep_decayed_fields(const TickInfo& tick) {
    const auto removed = m_fields.sweep_decayed(m_sim_time);
    if (!removed.empty())
        m_mesh.mark_dirty();
    for (const auto& fname : removed) {
        if (!m_host) continue;
        m_host->events().publish(EventChannelId::Simulation, simulation::events::FieldRemoved{
            .field = runtime_node_id_from_text(fname), .sim_time = m_sim_time, .tick = tick.tick_index
        });
        m_host->events().publish(EventChannelId::Simulation, simulation::events::PerturbationDecayed{
            .u = f32(0), .v = f32(0), .sim_time = m_sim_time, .tick = tick.tick_index
        });
    }
}

void SimulationWavePredatorPrey::sync_context() {
    if (m_host) m_particles.bind_memory(&m_host->memory());
    m_particles.set_surface(m_surface.get());
    m_context.set_surface(m_surface.get());
    m_context.set_particles(&m_particles.particles());
    m_context.set_rng(&m_particles.rng());
    m_context.set_fields(&m_fields);
    m_context.set_time(m_sim_time);
    m_particles.set_behavior_context(&m_context);
}

void SimulationWavePredatorPrey::submit_geometry() {
    if (!m_host) return;
    const FieldVisualSurface visual_surface{
        *m_surface,
        m_fields
    };
    submit_surface_sim_packets(
        m_host->render(), m_main_view, m_alternate_view,
        visual_surface, m_mesh, m_particles,
        SurfaceMeshOptions{
            .grid_lines = 80u, .time = m_sim_time, .color_scale = f32(1.05),
            .wire_color = {f32(0.92), f32(0.96), f32(1), f32(0.30)},
            .fill_color_mode = SurfaceFillColorMode::HeightCell,
            .build_fill = false,
            .build_contour = true
        },
        &m_host->interaction(), &m_host->memory(), &m_host->camera());
}

// ============================================================================
// UI Panels
// ============================================================================

void SimulationWavePredatorPrey::draw_control_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 72.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f, 200.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Controls")) { ImGui::End(); return; }
    SimulationControlPanel::draw(metadata(), SimulationControls{
        .paused = &m_paused, .sim_speed = &m_sim_speed,
        .reset = [this]{ reset_showcase(); }
    });
    if (m_host) {
        AlternateViewPanel::draw_main_overlays(m_host->render(), m_main_view);
        AlternateViewPanel::draw(m_host->render(), m_alternate_view);
    }
    ImGui::End();
}

void SimulationWavePredatorPrey::draw_swarm_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 300.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f, 230.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Swarms")) { ImGui::End(); return; }
    memory::FrameVector<SwarmRecipeAction> actions{
        {.label="Reset predator/prey", .hotkey="Ctrl+R",
         .spawn=[this]{ reset_showcase();     return m_last_swarm; }},
        {.label="Brownian cloud",      .hotkey="Ctrl+B",
         .spawn=[this]{ spawn_brownian_cloud(); return m_last_swarm; }},
        {.label="Contour band",        .hotkey="Ctrl+L",
         .spawn=[this]{ spawn_contour_band();  return m_last_swarm; }}
    };
    SwarmRecipePanel::draw(SwarmRecipePanelState{}, actions, m_particles,
        m_goal_status, &m_last_swarm,
        SwarmRecipePanelOptions{.show_sim_controls=false, .show_particle_inspector=false});
    ImGui::End();
}

void SimulationWavePredatorPrey::draw_particle_panel() {
    ImGui::SetNextWindowPos(ImVec2(342.f, 72.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.f, 480.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Particles")) { ImGui::End(); return; }
    ParticleInspectorPanel::draw(m_particles.particles());
    ImGui::End();
}

void SimulationWavePredatorPrey::draw_goal_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 548.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.f, 130.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Goals")) { ImGui::End(); return; }
    GoalStatusPanel::draw(metadata());
    ImGui::End();
}

void SimulationWavePredatorPrey::draw_events_panel() {
    ImGui::SetNextWindowPos(ImVec2(24.f, 700.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500.f, 300.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sim - Events")) { ImGui::End(); return; }
    if (!m_host) { ImGui::End(); return; }

    const events::EventLog& event_log = m_host->events().log(EventChannelId::Simulation);

    ImGui::TextDisabled("Ring: %llu queued  %llu dropped",
        static_cast<unsigned long long>(event_log.approx_queued()),
        static_cast<unsigned long long>(event_log.total_dropped()));
    ImGui::Separator();

    static bool show_info    = true;
    static bool show_notices = true;
    static bool show_alerts  = true;
    ImGui::Checkbox("Info", &show_info); ImGui::SameLine();
    ImGui::Checkbox("Notice/Spawn", &show_notices); ImGui::SameLine();
    ImGui::Checkbox("Alerts", &show_alerts);
    ImGui::Separator();

    ImGui::BeginChild("event_scroll", ImVec2(0.f, 0.f), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& entry : event_log.entries()) {
        using S = events::EventSeverity;
        if (entry.severity == S::Info    && !show_info)    continue;
        if (entry.severity == S::Notice  && !show_notices) continue;
        if (entry.severity >= S::Warning && !show_alerts)  continue;

        ImVec4 color;
        switch (entry.severity) {
            case S::Info:     color = {0.7f, 0.7f, 0.7f, 1.f}; break;
            case S::Notice:   color = {0.4f, 0.8f, 1.0f, 1.f}; break;
            case S::Warning:  color = {1.0f, 0.9f, 0.2f, 1.f}; break;
            case S::Alert:    color = {1.0f, 0.5f, 0.1f, 1.f}; break;
            case S::Critical: color = {1.0f, 0.2f, 0.2f, 1.f}; break;
            default:          color = {0.7f, 0.7f, 0.7f, 1.f}; break;
        }
        ImGui::TextColored(color, "%s", entry.text.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

} // namespace ndde
