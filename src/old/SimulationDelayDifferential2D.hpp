#pragma once

#include "engine/ISimulation.hpp"
#include "engine/ScopedServiceHandles.hpp"
#include "memory/Unique.hpp"
#include "sim/DelayDifferentialSystem.hpp"

#include <string_view>

namespace ndde {

class SimulationDelayDifferential2D final : public ISimulation {
public:
    explicit SimulationDelayDifferential2D(memory::MemoryService* memory = nullptr);

    [[nodiscard]] std::string_view name() const override { return "Delay Differential Systems"; }
    void on_register(SimulationHost& host) override;
    void on_start() override;
    void on_tick(const TickInfo& tick) override;
    void on_stop() override;

    [[nodiscard]] SceneSnapshot snapshot() const override;
    [[nodiscard]] SimulationMetadata metadata() const override;

    [[nodiscard]] RenderViewId main_view_id() const noexcept { return m_time_view; }
    [[nodiscard]] RenderViewId alternate_view_id() const noexcept { return m_delay_phase_view; }
    [[nodiscard]] std::size_t particle_count() const noexcept {
        return m_problem ? m_problem->history_size() : 0u;
    }

private:
    SimulationHost* m_host = nullptr;
    ScopedServiceHandles<PanelHandle> m_panels;
    HotkeyHandle m_reset_hotkey;
    HotkeyHandle m_run_hotkey;
    RenderViewHandle m_time_handle;
    RenderViewHandle m_delay_phase_handle;
    RenderViewId m_time_view = 0;
    RenderViewId m_delay_phase_view = 0;

    memory::Unique<sim::IDelayDifferentialSystem> m_system;
    memory::Unique<sim::DelayInitialValueProblem> m_problem;
    sim::EulerDdeSolver m_solver;

    double m_initial_value = 0.15;
    double m_history_amplitude = 1.1;
    double m_history_cycles = 1.5;
    double m_step_size = 0.006;
    double m_sim_speed = 0.8;
    double m_damping = 0.55;
    double m_feedback = 1.75;
    double m_delay = 3.2;
    double m_history_sample_dt = 0.02;
    float m_time_window = 70.f;
    bool m_running = true;
    bool m_paused = false;

    void reset_problem();
    void step_problem(double dt);
    void submit_geometry();
    void update_hover();
    void draw_controls_panel();
    void draw_state_panel();

    static void seed_history(f64 t, std::span<f64> out, void* user);

    [[nodiscard]] RenderViewDomain time_domain() const noexcept;
    [[nodiscard]] RenderViewDomain delay_phase_domain() const noexcept;
    [[nodiscard]] Mat4 view_mvp(RenderViewId view) const noexcept;
    [[nodiscard]] double expected_bound() const noexcept;
};

} // namespace ndde
