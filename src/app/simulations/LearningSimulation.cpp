#include "app/simulations/LearningSimulation.hpp"

#include "engine/coordinates/CoordinateOverlayService.hpp"

#include <algorithm>
#include <memory_resource>
#include <string>
#include <utility>

namespace ndde {

namespace {

[[nodiscard]] CoordinateSpaceKind to_engine_space(sim::OverlayCoordinateSpace space) noexcept {
    switch (space) {
        case sim::OverlayCoordinateSpace::Polar2D:
            return CoordinateSpaceKind::Polar2D;
        case sim::OverlayCoordinateSpace::Cartesian2D:
        default:
            return CoordinateSpaceKind::Cartesian2D;
    }
}

[[nodiscard]] CoordinateOverlayDescriptor to_engine_overlay(const sim::OverlayDescriptor& overlay) {
    return CoordinateOverlayDescriptor{
        .space = to_engine_space(overlay.coordinate_space),
        .gradations = AxisGradationConfig{
            .major_step = overlay.gradations.major_step,
            .minor_step = overlay.gradations.minor_step,
            .dynamic_steps = overlay.gradations.dynamic_steps
        },
        .labels = AxisLabelConfig{
            .x = overlay.labels.x,
            .y = overlay.labels.y,
            .show = overlay.labels.show
        },
        .show_grid = overlay.show_grid,
        .show_axes = overlay.show_axes,
        .show_polar_rings = overlay.show_polar_rings,
        .show_polar_spokes = overlay.show_polar_spokes
    };
}

void submit_declared_curves(SimulationHost& host,
                            RenderViewId view,
                            const sim::SimulationContext& sim_context,
                            CoordinateVisibleBounds2D bounds,
                            Mat4 mvp) {
    u32 curve_index = 0u;
    sim_context.curves().for_each([&](sim::CurveHandle, const sim::CurveDescriptor& curve) {
        if (!curve.evaluate) return;

        const u32 sample_count = std::max(curve.sampling.sample_count, 2u);
        auto vertices = host.memory().frame().make_vector<Vertex>(sample_count);
        const f32 span = std::max(bounds.width(), 0.001f);
        const f32 z = curve.sampling.z_offset + 0.001f * static_cast<f32>(curve_index++);
        for (u32 sample = 0u; sample < sample_count; ++sample) {
            const f32 t = static_cast<f32>(sample) / static_cast<f32>(sample_count - 1u);
            const f32 x = bounds.left + t * span;
            vertices[sample] = Vertex{Vec3{x, curve.evaluate(x), z}, curve.color};
        }

        host.render().submit(view,
                             vertices,
                             Topology::LineStrip,
                             DrawMode::VertexColor,
                             Vec4{1.f, 1.f, 1.f, 1.f},
                             mvp);
    });
}

[[nodiscard]] std::string curve_formula_summary(const sim::WorkbenchBuildContext& build_context) {
    std::string summary;
    build_context.for_each_simulation([&](sim::SimulationContextHandle, const sim::SimulationContext& sim_context) {
        sim_context.curves().for_each([&](sim::CurveHandle, const sim::CurveDescriptor& curve) {
            if (curve.formula.empty()) return;
            if (!summary.empty()) {
                summary += ", ";
            }
            if (!curve.name.empty()) {
                summary += curve.name;
                summary += " = ";
            }
            summary += curve.formula;
        });
    });
    return summary.empty() ? std::string("No declared curves") : summary;
}

[[nodiscard]] bool has_declared_curve(const sim::WorkbenchBuildContext& build_context) {
    bool has_curve = false;
    build_context.for_each_simulation([&](sim::SimulationContextHandle, const sim::SimulationContext& sim_context) {
        sim_context.curves().for_each([&](sim::CurveHandle, const sim::CurveDescriptor&) {
            has_curve = true;
        });
    });
    return has_curve;
}

} // namespace

LearningSimulation::LearningSimulation(memory::MemoryService* memory)
    : m_memory(memory)
    , m_workbenches(memory ? memory->persistent().resource() : std::pmr::get_default_resource())
{
}

LearningSimulation::LearningSimulation(memory::MemoryService* memory, std::string workbench_id)
    : m_memory(memory)
    , m_workbenches(memory ? memory->persistent().resource() : std::pmr::get_default_resource())
    , m_workbench_id(std::move(workbench_id))
{
}

void LearningSimulation::on_register(SimulationHost& host) {
    m_host = &host;
    m_memory = &host.memory();
    if (m_workbenches.size() == 0) {
        register_app_workbenches(m_workbenches);
    }

    m_main_handle = host.render().register_view(RenderViewDescriptor{
        .title = "Learning XY",
        .kind = RenderViewKind::Main,
        .projection = CameraProjection::Orthographic,
        .camera_profile = CameraViewProfile::Orthographic2D,
        .camera = CameraState{.target = Vec3{0.f, 0.f, 0.f}, .yaw = 0.f, .pitch = 0.f, .zoom = 1.f},
        .overlays = {.show_axes = true, .show_grid = true, .show_labels = true}
    }, &m_main_view);
}

void LearningSimulation::on_start() {
    if (!m_memory || m_workbenches.size() == 0) {
        m_status = "No learning workbenches registered";
        return;
    }
    m_active_workbench = m_workbench_id.empty()
        ? m_workbenches.create(*m_memory, 0)
        : m_workbenches.create(*m_memory, m_workbench_id);
    if (!m_active_workbench) {
        m_status = "Failed to create learning workbench";
        return;
    }
    m_build_context.clear();
    m_active_workbench->build(m_build_context);
    auto context = render_context();
    m_active_workbench->on_start(context);
    m_status = m_active_workbench->metadata().title;
}

void LearningSimulation::on_tick(const TickInfo& tick) {
    on_simulation_tick(tick);
    on_submit_render();
}

void LearningSimulation::on_simulation_tick(const TickInfo& tick) {
    m_time = tick.time;
    if (m_active_workbench) {
        m_active_workbench->on_tick(tick);
    }
}

void LearningSimulation::on_submit_render() {
    if (!m_active_workbench || !m_host) return;
    auto context = render_context();
    const Mat4 mvp = m_host->camera().view_mvp(m_main_view);
    const CoordinateVisibleBounds2D bounds =
        CoordinateOverlayService::visible_bounds(m_host->render(), m_main_view);
    m_build_context.for_each_simulation([&](sim::SimulationContextHandle, const sim::SimulationContext& sim_context) {
        sim_context.overlays().for_each([&](sim::OverlayHandle, const sim::OverlayDescriptor& overlay) {
            if (overlay.kind.rfind("coordinate.", 0) != 0) return;
            CoordinateOverlayService::submit(m_host->render(),
                                             m_host->text(),
                                             m_host->memory(),
                                             m_main_view,
                                             to_engine_overlay(overlay),
                                             mvp);
        });
        submit_declared_curves(*m_host, m_main_view, sim_context, bounds, mvp);
    });
    m_active_workbench->on_submit_render(context);
}

void LearningSimulation::on_stop() {
    if (m_active_workbench) {
        m_active_workbench->on_stop();
        m_active_workbench.reset();
    }
    m_build_context.clear();
    m_main_handle.reset();
    m_main_view = RenderViewId(0);
    m_host = nullptr;
}

SceneSnapshot LearningSimulation::snapshot() const {
    return SceneSnapshot{
        .name = std::string(name()),
        .paused = false,
        .sim_time = m_time,
        .sim_speed = f32(1),
        .particle_count = 0u,
        .status = m_status
    };
}

SimulationMetadata LearningSimulation::metadata() const {
    return SimulationMetadata{
        .name = std::string(name()),
        .surface_name = m_active_workbench ? m_active_workbench->metadata().title : "Learning workbench",
        .surface_formula = curve_formula_summary(m_build_context),
        .status = m_status,
        .sim_time = m_time,
        .sim_speed = f32(1),
        .particle_count = 0u,
        .surface_has_analytic_derivatives = has_declared_curve(m_build_context)
    };
}

WorkbenchRenderContext LearningSimulation::render_context() {
    return WorkbenchRenderContext{
        .host = *m_host,
        .main_view = m_main_view
    };
}

} // namespace ndde
