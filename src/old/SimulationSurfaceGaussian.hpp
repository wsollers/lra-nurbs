#pragma once
// app/SimulationSurfaceGaussian.hpp
// First ISimulation-based version of the Gaussian surface simulation.

#include "app/GaussianRipple.hpp"
#include "app/GoalStatusPanel.hpp"
#include "app/ParticleGoals.hpp"
#include "app/ParticleInspectorPanel.hpp"
#include "app/ParticleSystem.hpp"
#include "app/SimulationControlPanel.hpp"
#include "app/SimulationContext.hpp"
#include "app/SimulationPanelModels.hpp"
#include "app/SurfaceMeshCache.hpp"
#include "app/SurfaceSimSpawner.hpp"
#include "app/SwarmRecipePanel.hpp"
#include "engine/ISimulation.hpp"
#include "engine/RenderService.hpp"
#include "engine/ScopedServiceHandles.hpp"
#include "memory/Unique.hpp"
#include "sim/DifferentialSystem.hpp"

#include <random>
#include <string_view>

namespace ndde {

class SimulationSurfaceGaussian final : public ISimulation {
public:
    explicit SimulationSurfaceGaussian(memory::MemoryService* memory = nullptr);
    ~SimulationSurfaceGaussian() override = default;
    SimulationSurfaceGaussian(const SimulationSurfaceGaussian&) = delete;
    SimulationSurfaceGaussian& operator=(const SimulationSurfaceGaussian&) = delete;
    SimulationSurfaceGaussian(SimulationSurfaceGaussian&&) = delete;
    SimulationSurfaceGaussian& operator=(SimulationSurfaceGaussian&&) = delete;

    [[nodiscard]] std::string_view name() const override { return "Surface Simulation"; }

    void on_register(SimulationHost& host) override;
    void on_start() override;
    void on_tick(const TickInfo& tick) override;
    void on_stop() override;

    [[nodiscard]] SceneSnapshot snapshot() const override;
    [[nodiscard]] SimulationMetadata metadata() const override;

    [[nodiscard]] const SimulationContext& context() const noexcept { return m_context; }
    [[nodiscard]] SimulationContext& context() noexcept { return m_context; }
    [[nodiscard]] RenderViewId main_view_id() const noexcept { return m_main_view; }
    [[nodiscard]] RenderViewId alternate_view_id() const noexcept { return m_alternate_view; }
    [[nodiscard]] std::size_t particle_count() const noexcept { return m_particles.size(); }

private:
    memory::Unique<GaussianRipple> m_surface;
    ParticleSystem m_particles;
    SurfaceSimSpawner m_spawner;
    SimulationContext m_context;
    SurfaceMeshCache m_mesh;

    SimulationHost* m_host = nullptr;
    ScopedServiceHandles<PanelHandle> m_panel_handles;
    HotkeyHandle m_reset_hotkey;
    HotkeyHandle m_cloud_hotkey;
    RenderViewHandle m_main_view_handle;
    RenderViewHandle m_alternate_view_handle;
    RenderViewId m_main_view = 0;
    RenderViewId m_alternate_view = 0;

    float m_sim_time = 0.f;
    float m_sim_speed = 1.f;
    bool m_paused = false;
    GoalStatus m_goal_status = GoalStatus::Running;
    SwarmBuildResult m_last_swarm;
    memory::Unique<sim::ExponentialGrowthSystem> m_ode_system;
    memory::Unique<sim::InitialValueProblem> m_ode_problem;
    sim::EulerOdeSolver m_euler_solver;
    sim::Rk4OdeSolver m_rk4_solver;
    double m_ode_step_size = 0.05;
    bool m_ode_use_rk4 = true;
    bool m_ode_advance_with_sim = false;

    void reset_showcase();
    void spawn_cloud();
    void spawn_contour_band();
    void draw_control_panel();
    void draw_swarm_panel();
    void draw_particle_panel();
    void draw_goal_panel();
    void draw_differential_panel();
    void submit_geometry();
    void sync_context();
    void apply_surface_commands();
    void reset_differential_problem();
    void step_differential_problem(double dt);
};

} // namespace ndde
