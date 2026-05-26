#pragma once
// app/SimulationWavePredatorPrey.hpp
// Refactored thin simulation host.
// Delegates particle construction to ScenarioBuilder,
// environmental effects to FieldCompositor,
// and alert evaluation to the registered AlertRules.
// All events flow through EventBusService's Simulation channel.

#include "simulation/scenario/ScenarioBuilder.hpp"
#include "simulation/fields/IField.hpp"
#include "simulation/fields/DampingField.hpp"
#include "simulation/fields/MetricRipple.hpp"
#include "simulation/events/AlertRule.hpp"
#include "simulation/events/SimEventTypes.hpp"
#include "app/GoalStatusPanel.hpp"
#include "app/ParticleInspectorPanel.hpp"
#include "app/ParticleSystem.hpp"
#include "app/SimulationControlPanel.hpp"
#include "app/SimulationContext.hpp"
#include "app/SimulationPanelModels.hpp"
#include "app/SurfaceMeshCache.hpp"
#include "app/SurfaceRegistry.hpp"
#include "app/SwarmRecipePanel.hpp"
#include "engine/ISimulation.hpp"
#include "engine/ScopedServiceHandles.hpp"
#include "memory/Unique.hpp"

#include <string_view>
#include <vector>

namespace ndde {

class SimulationWavePredatorPrey final : public ISimulation {
public:
    explicit SimulationWavePredatorPrey(memory::MemoryService* memory = nullptr);

    [[nodiscard]] std::string_view name() const override { return "Wave Predator-Prey"; }

    void on_register(SimulationHost& host) override;
    void on_start() override;
    void on_tick(const TickInfo& tick) override;
    void on_simulation_tick(const TickInfo& tick) override;
    void on_simulation_command(const SimulationThreadCommand& command) override;
    void on_submit_render() override;
    void on_stop() override;

    [[nodiscard]] SceneSnapshot    snapshot()  const override;
    [[nodiscard]] SimulationMetadata metadata() const override;

    [[nodiscard]] RenderViewId main_view_id()      const noexcept { return m_main_view; }
    [[nodiscard]] RenderViewId alternate_view_id() const noexcept { return m_alternate_view; }
    [[nodiscard]] std::size_t  particle_count()    const noexcept { return m_particles.size(); }
    [[nodiscard]] std::size_t  active_field_count() const noexcept { return m_fields.size(); }

    // Public for panel callbacks
    void reset_showcase();
    void spawn_brownian_cloud();
    void spawn_contour_band();

private:
    // ── Surface ───────────────────────────────────────────────────────────────
    memory::Unique<WavePredatorPreySurface> m_surface;

    // ── Simulation domain ─────────────────────────────────────────────────────
    ParticleSystem      m_particles;
    SimulationContext   m_context;
    SurfaceMeshCache    m_mesh;

    // ── Fields ────────────────────────────────────────────────────────────────
    simulation::FieldCompositor  m_fields;

    // ── Alert rules ───────────────────────────────────────────────────────────
    std::vector<events::AlertRulePtr> m_alerts;

    // ── Simulation state ──────────────────────────────────────────────────────
    f32        m_sim_time  = f32(0);
    f32        m_sim_speed = f32(1);
    bool       m_paused    = false;
    GoalStatus m_goal_status = GoalStatus::Running;
    u32        m_poke_seed = u32(0);
    Vec2       m_last_poke_uv{};
    u32        m_ripple_debug_frames = u32(0);

    // ── Engine host + handles ─────────────────────────────────────────────────
    SimulationHost*                       m_host = nullptr;
    ScopedServiceHandles<PanelHandle>     m_panel_handles;
    HotkeyHandle                          m_reset_hotkey;
    HotkeyHandle                          m_cloud_hotkey;
    HotkeyHandle                          m_contour_hotkey;
    RenderViewHandle                      m_main_handle;
    RenderViewHandle                      m_alt_handle;
    RenderViewId                          m_main_view      = RenderViewId(0);
    RenderViewId                          m_alternate_view = RenderViewId(0);
    SwarmBuildResult                      m_last_swarm;

    // ── Internal helpers ──────────────────────────────────────────────────────
    void handle_poke(const TickInfo& tick);
    void apply_surface_poke(const SimulationSurfacePoke& poke, const TickInfo& tick, bool allow_host_events);
    void log_ripple_diagnostics(const TickInfo& tick);
    void evaluate_alerts(const TickInfo& tick);
    void sweep_decayed_fields(const TickInfo& tick);
    void advance_state(const TickInfo& tick, bool allow_host_events);
    void sync_context();
    void submit_geometry();
    void reset_particles();

    // Panels
    void draw_control_panel();
    void draw_swarm_panel();
    void draw_particle_panel();
    void draw_goal_panel();
    void draw_events_panel();
};

} // namespace ndde
