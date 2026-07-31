#include "app/simulations/learning/FunctionDerivativeWorkbench.hpp"

#include "app/simulations/learning/WorkbenchRegistry.hpp"

#include <algorithm>
#include <cmath>

namespace ndde {

namespace {

constexpr u32 k_samples = 480u;

[[nodiscard]] Vec4 function_color() noexcept { return Vec4{0.10f, 0.72f, 1.00f, 1.f}; }
[[nodiscard]] Vec4 derivative_color() noexcept { return Vec4{1.00f, 0.44f, 0.18f, 1.f}; }

} // namespace

FunctionDerivativeWorkbench::FunctionDerivativeWorkbench()
    : m_metadata{
        .id = "learning.function_derivative.sin",
        .title = "Function and Derivative: sin(x)",
        .summary = "Draws f(x) = sin(x) and f'(x) = cos(x) on a dynamic Cartesian canvas.",
        .thumbnail_path = "assets/thumbnails/workbenches/function-derivative-sin.png"
    }
    , m_overlay{
        .space = CoordinateSpaceKind::Cartesian2D,
        .gradations = AxisGradationConfig{
            .major_step = 1.f,
            .minor_step = 0.2f,
            .dynamic_steps = true
        },
        .labels = AxisLabelConfig{
            .x = "x",
            .y = "y",
            .show = true
        },
        .show_grid = true,
        .show_axes = true,
        .show_polar_rings = false,
        .show_polar_spokes = false
    }
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

void FunctionDerivativeWorkbench::on_tick(const TickInfo& tick) {
    m_time = tick.time;
}

void FunctionDerivativeWorkbench::on_submit_render(WorkbenchRenderContext& context) {
    if (context.main_view == 0) return;

    const Mat4 mvp = context.host.camera().view_mvp(context.main_view);
    CoordinateOverlayService::submit(context.host.render(),
                                     context.host.text(),
                                     context.host.memory(),
                                     context.main_view,
                                     m_overlay,
                                     mvp);

    const CoordinateVisibleBounds2D bounds =
        CoordinateOverlayService::visible_bounds(context.host.render(), context.main_view);
    submit_function_curve(context, bounds, false);
    submit_function_curve(context, bounds, true);
}

void FunctionDerivativeWorkbench::on_stop() {}

void FunctionDerivativeWorkbench::submit_function_curve(WorkbenchRenderContext& context,
                                                       CoordinateVisibleBounds2D bounds,
                                                       bool derivative) const {
    auto vertices = context.host.memory().frame().make_vector<Vertex>(k_samples);
    const f32 span = std::max(bounds.width(), 0.001f);
    const Vec4 color = derivative ? derivative_color() : function_color();
    for (u32 sample = 0u; sample < k_samples; ++sample) {
        const f32 t = static_cast<f32>(sample) / static_cast<f32>(k_samples - 1u);
        const f32 x = bounds.left + t * span;
        const f32 y = derivative ? std::cos(x) : std::sin(x);
        vertices[sample] = Vertex{Vec3{x, y, derivative ? 0.02f : 0.01f}, color};
    }

    context.host.render().submit(context.main_view,
                                 vertices,
                                 Topology::LineStrip,
                                 DrawMode::VertexColor,
                                 Vec4{1.f, 1.f, 1.f, 1.f},
                                 context.host.camera().view_mvp(context.main_view));
}

void register_function_derivative_workbench(WorkbenchRegistry& registry) {
    registry.add(WorkbenchMetadata{
        .id = "learning.function_derivative.sin",
        .title = "Function and Derivative: sin(x)",
        .summary = "Draws f(x) = sin(x) and f'(x) = cos(x) on a dynamic Cartesian canvas.",
        .thumbnail_path = "assets/thumbnails/workbenches/function-derivative-sin.png"
    }, [](memory::MemoryService& memory) {
        return memory.simulation().make_unique_as<IWorkbench, FunctionDerivativeWorkbench>();
    });
}

} // namespace ndde
