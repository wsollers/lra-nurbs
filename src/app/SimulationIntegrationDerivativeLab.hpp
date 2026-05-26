#pragma once
// app/SimulationIntegrationDerivativeLab.hpp
// First vertical slice of the integration and derivative mathematical lab.

#include "app/IntegrationWorkbenchState.hpp"
#include "engine/ISimulation.hpp"
#include "engine/RuntimeIds.hpp"
#include "engine/ScopedServiceHandles.hpp"
#include "math/integration/Integration1D.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace ndde {

class SimulationIntegrationDerivativeLab final : public ISimulation {
public:
    enum class WorkbenchMode : u8 {
        Domain = 0,
        Partition,
        Samples,
        Contribution,
        Error,
        Bounds,
        Analysis
    };

    explicit SimulationIntegrationDerivativeLab(memory::MemoryService* memory = nullptr);

    [[nodiscard]] std::string_view name() const override { return "Integration & Derivative Lab"; }

    void on_register(SimulationHost& host) override;
    void on_start() override;
    void on_tick(const TickInfo& tick) override;
    void on_simulation_tick(const TickInfo& tick) override;
    void on_submit_render() override;
    void on_stop() override;

    [[nodiscard]] SceneSnapshot snapshot() const override;
    [[nodiscard]] SimulationMetadata metadata() const override;

    [[nodiscard]] RenderViewId main_view_id() const noexcept { return m_main_view; }
    [[nodiscard]] RenderViewId analytics_view_id() const noexcept { return m_analytics_view; }
    [[nodiscard]] const math::integration::IntegrationResult1D& result() const noexcept { return m_result; }
    [[nodiscard]] const IntegrationWorkbenchState& workbench() const noexcept { return m_workbench; }

private:
    struct FunctionPreset {
        ComponentId id = ids::unknown_component;
        const char* name = "";
        const char* formula = "";
        math::integration::ScalarFunction1D function;
        math::integration::ScalarFunction1D antiderivative;
        math::integration::Interval1D interval{};
    };

    memory::MemoryService* m_memory = nullptr;
    SimulationHost* m_host = nullptr;
    ScopedServiceHandles<PanelHandle> m_panel_handles;
    RenderViewHandle m_main_handle;
    RenderViewHandle m_analytics_handle;
    RenderViewId m_main_view = RenderViewId(0);
    RenderViewId m_analytics_view = RenderViewId(0);

    std::array<FunctionPreset, 9> m_presets;
    std::size_t m_preset_index = 8;
    math::integration::QuadratureMethod m_method = math::integration::QuadratureMethod::RightRiemann;
    math::integration::UniformPartitionConfig m_partition{.cell_count = u32(16)};
    math::integration::IntegrationResult1D m_result;
    IntegrationWorkbenchState m_workbench;
    std::string m_status = "Ready";
    f32 m_time = f32(0);
    f64 m_probe_x = f64(1.5707963267948966);
    f64 m_probe_derivative = f64(0);
    bool m_show_cells = true;
    bool m_show_tangent = true;
    bool m_show_1d_bridge = true;
    WorkbenchMode m_workbench_mode = WorkbenchMode::Contribution;
    ComponentId m_selected_zoo_component = ComponentId{"integration.zoo.function_runge"};
    std::optional<Vec2> m_last_selected_view_point;

    void register_zoo_metadata();
    void draw_zoo_selector();
    void recompute();
    void draw_control_panel();
    void draw_2d_top_bar();
    void draw_2d_tool_tray();
    void draw_2d_inspector_panel();
    void draw_2d_bottom_status();
    void draw_2d_mode_strip();
    void draw_2d_mode_body();
    void draw_2d_problem_tab();
    void draw_2d_partition_tab();
    void draw_2d_samples_tab();
    void draw_2d_display_tab();
    void draw_2d_error_tab();
    void draw_2d_bounds_tab();
    void draw_2d_analysis_tab();
    void draw_2d_live_inspector();
    void draw_2d_status_strip();
    void draw_1d_bridge_panel();
    void handle_2d_workbench_interaction();
    void update_workbench_state(const TickInfo& tick);
    void submit_geometry();
    [[nodiscard]] const FunctionPreset& preset() const noexcept { return m_presets[m_preset_index]; }
    [[nodiscard]] f64 reference_value() const;
};

} // namespace ndde
