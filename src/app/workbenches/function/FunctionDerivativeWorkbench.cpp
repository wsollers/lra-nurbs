#include "app/workbenches/function/FunctionDerivativeWorkbench.hpp"

#include "app/workbenches/WorkbenchRegistry.hpp"

#include <cmath>

namespace ndde {

namespace {

[[nodiscard]] Vec4 function_color() noexcept { return Vec4{0.10f, 0.72f, 1.00f, 1.f}; }
[[nodiscard]] Vec4 derivative_color() noexcept { return Vec4{1.00f, 0.44f, 0.18f, 1.f}; }

[[nodiscard]] WorkbenchMetadata function_derivative_metadata() {
    return WorkbenchMetadata{
        .id = "learning.function_derivative.sin",
        .title = "Function and Derivative: sin(x)",
        .summary = "Draws f(x) = sin(x) and f'(x) = cos(x) on a dynamic Cartesian canvas.",
        .thumbnail_path = "assets/thumbnails/workbenches/function-derivative-sin.png"
    };
}

} // namespace

FunctionDerivativeWorkbench::FunctionDerivativeWorkbench()
    : m_metadata(function_derivative_metadata())
{}

void FunctionDerivativeWorkbench::on_start(WorkbenchRenderContext& context) {
    if (context.main_view == 0) return;
    context.host.render().set_view_domain(context.main_view, RenderViewDomain{
        .u_min = -10.f,
        .u_max = 10.f,
        .v_min = -2.5f,
        .v_max = 2.5f,
        .z_min = -1.f,
        .z_max = 1.f
    });
}

void FunctionDerivativeWorkbench::build(sim::WorkbenchBuildContext& build) {
    const sim::SimulationContextHandle sim = build.add_simulation(sim::SimulationContextDescriptor{
        .name = "Function graph"
    });
    (void)build.add_overlay(sim, sim::OverlayDescriptor{
        .name = "Cartesian function canvas",
        .kind = "coordinate.cartesian2d",
        .coordinate_space = sim::OverlayCoordinateSpace::Cartesian2D,
        .gradations = sim::OverlayGradationDescriptor{
            .major_step = 1.f,
            .minor_step = 0.2f,
            .dynamic_steps = true
        },
        .labels = sim::OverlayLabelDescriptor{
            .x = "x",
            .y = "y",
            .show = true
        },
        .show_grid = true,
        .show_axes = true
    });
    (void)build.add_curve(sim, sim::CurveDescriptor{
        .name = "f(x)",
        .formula = "sin(x)",
        .color = function_color(),
        .sampling = sim::CurveSamplingDescriptor{
            .sample_count = 480u,
            .z_offset = 0.01f
        },
        .evaluate = [](f32 x) { return std::sin(x); }
    });
    (void)build.add_curve(sim, sim::CurveDescriptor{
        .name = "f'(x)",
        .formula = "cos(x)",
        .color = derivative_color(),
        .sampling = sim::CurveSamplingDescriptor{
            .sample_count = 480u,
            .z_offset = 0.02f
        },
        .evaluate = [](f32 x) { return std::cos(x); }
    });
}

void FunctionDerivativeWorkbench::on_tick(const TickInfo& tick) {
    m_time = tick.time;
}

void FunctionDerivativeWorkbench::on_submit_render(WorkbenchRenderContext& context) {
    (void)context;
}

void FunctionDerivativeWorkbench::on_stop() {}

void register_function_derivative_workbench(WorkbenchRegistry& registry) {
    registry.add(function_derivative_metadata(), [](memory::MemoryService& memory) {
        return memory.simulation().make_unique_as<IWorkbench, FunctionDerivativeWorkbench>();
    });
}

} // namespace ndde
