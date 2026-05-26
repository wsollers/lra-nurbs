#include "app/SimulationIntegrationDerivativeLab.hpp"

#include "app/IntegrationLabAnalyticsPackets.hpp"
#include "app/IntegrationLabRenderPackets.hpp"
#include "engine/RenderService.hpp"
#include "engine/metadata/MetadataTypes.hpp"
#include "math/GeometryTypes.hpp"
#include "numeric/ops.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

namespace ndde {

namespace {

constexpr f64 k_pi = f64(3.14159265358979323846264338327950288);

[[nodiscard]] Vec4 graph_color() noexcept { return Vec4{0.20f, 0.76f, 1.00f, 1.0f}; }
[[nodiscard]] Vec4 cell_color() noexcept { return Vec4{0.35f, 0.38f, 0.43f, 0.65f}; }
[[nodiscard]] Vec4 axis_color() noexcept { return Vec4{0.72f, 0.72f, 0.76f, 0.7f}; }
[[nodiscard]] Vec4 tangent_color() noexcept { return Vec4{1.0f, 0.36f, 0.24f, 1.0f}; }

[[nodiscard]] const char* integrand_preset_label(math::integration::IntegrandPreset2D preset) noexcept {
    switch (preset) {
        case math::integration::IntegrandPreset2D::Gaussian: return "Gaussian";
        case math::integration::IntegrandPreset2D::Wave: return "Wave";
        case math::integration::IntegrandPreset2D::Polynomial: return "Polynomial";
        case math::integration::IntegrandPreset2D::StepX: return "Step X";
    }
    return "Unknown";
}

[[nodiscard]] const char* method_2d_label(math::integration::IntegrationMethod2D method) noexcept {
    switch (method) {
        case math::integration::IntegrationMethod2D::Midpoint: return "Midpoint";
        case math::integration::IntegrationMethod2D::TensorProductTrapezoid: return "Trapezoid";
    }
    return "Unknown";
}

[[nodiscard]] const char* display_mode_label(IntegrationDisplayMode mode) noexcept {
    switch (mode) {
        case IntegrationDisplayMode::Value: return "Value";
        case IntegrationDisplayMode::Contribution: return "Contribution";
        case IntegrationDisplayMode::LocalError: return "Local Error";
    }
    return "Unknown";
}

[[nodiscard]] const char* category_label(ObjectCategory category) noexcept {
    switch (category) {
        case ObjectCategory::GeometricObject: return "Geometry";
        case ObjectCategory::FieldObject: return "Field";
        case ObjectCategory::OperatorObject: return "Operator";
        case ObjectCategory::DynamicalObject: return "Dynamics";
        case ObjectCategory::ControlObject: return "Control";
        case ObjectCategory::ObservableObject: return "Observable";
    }
    return "Object";
}

[[nodiscard]] const char* workbench_mode_label(SimulationIntegrationDerivativeLab::WorkbenchMode mode) noexcept {
    switch (mode) {
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Domain: return "Domain";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Partition: return "Partition";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Samples: return "Samples";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Contribution: return "Contribution";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Error: return "Error";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Bounds: return "Bounds";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Analysis: return "Analysis";
    }
    return "Unknown";
}

[[nodiscard]] const char* workbench_mode_short_label(SimulationIntegrationDerivativeLab::WorkbenchMode mode) noexcept {
    switch (mode) {
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Domain: return "D";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Partition: return "P";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Samples: return "S";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Contribution: return "C";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Error: return "E";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Bounds: return "B";
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Analysis: return "A";
    }
    return "?";
}

[[nodiscard]] bool mode_is_live(SimulationIntegrationDerivativeLab::WorkbenchMode mode) noexcept {
    switch (mode) {
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Domain:
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Partition:
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Samples:
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Contribution:
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Error:
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Analysis:
            return true;
        case SimulationIntegrationDerivativeLab::WorkbenchMode::Bounds:
            return false;
    }
    return false;
}

void draw_status_badge(const char* label, bool enabled) {
    if (enabled) {
        ImGui::TextColored(ImVec4{0.45f, 0.95f, 0.70f, 1.0f}, "%s", label);
    } else {
        ImGui::TextDisabled("%s", label);
    }
}

[[nodiscard]] Vec2 view_point_from_ndc(RenderViewDomain domain, Vec2 screen_ndc) noexcept {
    const f32 x_t = (screen_ndc.x + f32(1)) * f32(0.5);
    const f32 y_t = (screen_ndc.y + f32(1)) * f32(0.5);
    return Vec2{
        domain.u_min + (domain.u_max - domain.u_min) * x_t,
        domain.v_min + (domain.v_max - domain.v_min) * y_t
    };
}

[[nodiscard]] bool same_view_point(Vec2 a, Vec2 b) noexcept {
    constexpr f32 eps = 1.0e-5f;
    return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps;
}

void push_line(memory::FrameVector<Vertex>& vertices, Vec2 a, Vec2 b, Vec4 color) {
    vertices.push_back(Vertex{.pos = Vec3{a.x, a.y, 0.f}, .color = color});
    vertices.push_back(Vertex{.pos = Vec3{b.x, b.y, 0.f}, .color = color});
}

[[nodiscard]] bool component_id_equals(ComponentId a, ComponentId b) noexcept {
    return a.value == b.value;
}

[[nodiscard]] bool is_integration_zoo_descriptor(const ComponentDescriptor& descriptor) noexcept {
    return descriptor.id.value.starts_with("integration.zoo.");
}

[[nodiscard]] bool is_live_zoo_component(ComponentId id) noexcept {
    return id.value.starts_with("integration.zoo.function_") ||
        component_id_equals(id, ids::integration_zoo_gaussian) ||
        component_id_equals(id, ids::integration_zoo_wave) ||
        component_id_equals(id, ids::integration_zoo_polynomial) ||
        component_id_equals(id, ids::integration_zoo_step_x);
}

[[nodiscard]] std::optional<math::integration::IntegrandPreset2D> preset_for_zoo_component(ComponentId id) noexcept {
    if (component_id_equals(id, ids::integration_zoo_gaussian)) {
        return math::integration::IntegrandPreset2D::Gaussian;
    }
    if (component_id_equals(id, ids::integration_zoo_wave)) {
        return math::integration::IntegrandPreset2D::Wave;
    }
    if (component_id_equals(id, ids::integration_zoo_polynomial)) {
        return math::integration::IntegrandPreset2D::Polynomial;
    }
    if (component_id_equals(id, ids::integration_zoo_step_x)) {
        return math::integration::IntegrandPreset2D::StepX;
    }
    return std::nullopt;
}

[[nodiscard]] bool is_function_zoo_component(ComponentId id) noexcept {
    return id.value.starts_with("integration.zoo.function_");
}

} // namespace

SimulationIntegrationDerivativeLab::SimulationIntegrationDerivativeLab(memory::MemoryService* memory)
    : m_memory(memory)
    , m_presets{{
        FunctionPreset{
            .id = ids::integration_zoo_function_one,
            .name = "y = 1",
            .formula = "y = 1",
            .function = [](f64) { return f64(1); },
            .antiderivative = [](f64 x) { return x; },
            .interval = {.min = f64(-2), .max = f64(2)}
        },
        FunctionPreset{
            .id = ids::integration_zoo_function_x,
            .name = "y = x",
            .formula = "y = x",
            .function = [](f64 x) { return x; },
            .antiderivative = [](f64 x) { return (x * x) / f64(2); },
            .interval = {.min = f64(-2), .max = f64(2)}
        },
        FunctionPreset{
            .id = ids::integration_zoo_function_exp,
            .name = "y = exp(x)",
            .formula = "y = exp(x)",
            .function = [](f64 x) { return std::exp(x); },
            .antiderivative = [](f64 x) { return std::exp(x); },
            .interval = {.min = f64(-2), .max = f64(2)}
        },
        FunctionPreset{
            .id = ids::integration_zoo_function_ln,
            .name = "y = ln(x)",
            .formula = "y = ln(x)",
            .function = [](f64 x) { return std::log(x); },
            .antiderivative = [](f64 x) { return x * std::log(x) - x; },
            .interval = {.min = f64(0.1), .max = f64(4)}
        },
        FunctionPreset{
            .id = ids::integration_zoo_function_reciprocal,
            .name = "y = 1 / x",
            .formula = "y = 1 / x",
            .function = [](f64 x) { return f64(1) / x; },
            .antiderivative = [](f64 x) { return std::log(std::abs(x)); },
            .interval = {.min = f64(0.25), .max = f64(4)}
        },
        FunctionPreset{
            .id = ComponentId{"integration.zoo.function_sin"},
            .name = "sin(x)",
            .formula = "f(x) = sin(x)",
            .function = [](f64 x) { return std::sin(x); },
            .antiderivative = [](f64 x) { return -std::cos(x); },
            .interval = {.min = f64(0), .max = k_pi}
        },
        FunctionPreset{
            .id = ComponentId{"integration.zoo.function_x_squared"},
            .name = "x^2",
            .formula = "f(x) = x^2",
            .function = [](f64 x) { return x * x; },
            .antiderivative = [](f64 x) { return (x * x * x) / f64(3); },
            .interval = {.min = f64(-1), .max = f64(2)}
        },
        FunctionPreset{
            .id = ComponentId{"integration.zoo.function_cubic_minus_x"},
            .name = "x^3 - x",
            .formula = "f(x) = x^3 - x",
            .function = [](f64 x) { return (x * x * x) - x; },
            .antiderivative = [](f64 x) { return (x * x * x * x) / f64(4) - (x * x) / f64(2); },
            .interval = {.min = f64(-1.5), .max = f64(1.5)}
        },
        FunctionPreset{
            .id = ComponentId{"integration.zoo.function_runge"},
            .name = "1 / (1 + 25x^2)",
            .formula = "f(x) = 1 / (1 + 25x^2)",
            .function = [](f64 x) { return f64(1) / (f64(1) + f64(25) * x * x); },
            .antiderivative = [](f64 x) { return std::atan(f64(5) * x) / f64(5); },
            .interval = {.min = f64(-2), .max = f64(2)}
        }
    }}
{}

void SimulationIntegrationDerivativeLab::on_register(SimulationHost& host) {
    m_host = &host;
    m_memory = &host.memory();
    register_zoo_metadata();

    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Lab - Integration",
        .category = "Mathematics",
        .scope = PanelScope::Simulation,
        .draw = [this] { draw_control_panel(); }
    }));

    m_main_handle = host.render().register_view(RenderViewDescriptor{
        .title = "Integration & Derivative Lab",
        .kind = RenderViewKind::Main,
        .projection = CameraProjection::Orthographic,
        .camera_profile = CameraViewProfile::Orthographic2D,
        .overlays = {.show_axes = false, .show_grid = false}
    }, &m_main_view);

    const auto snapshot = m_workbench.snapshot();
    host.render().set_view_domain(m_main_view, RenderViewDomain{
        .u_min = static_cast<f32>(snapshot.problem.domain.x_min),
        .u_max = static_cast<f32>(snapshot.problem.domain.x_max),
        .v_min = static_cast<f32>(snapshot.problem.domain.y_min),
        .v_max = static_cast<f32>(snapshot.problem.domain.y_max),
        .z_min = -1.f,
        .z_max = 1.f
    });

    m_analytics_handle = host.render().register_view(RenderViewDescriptor{
        .title = "Integration Analytics",
        .kind = RenderViewKind::Alternate,
        .projection = CameraProjection::Orthographic,
        .camera_profile = CameraViewProfile::Orthographic2D,
        .overlays = {.show_axes = false, .show_grid = false}
    }, &m_analytics_view);
}

void SimulationIntegrationDerivativeLab::register_zoo_metadata() {
    if (!m_host) return;

    auto register_if_missing = [this](ComponentDescriptor descriptor) {
        if (!m_host->metadata().get_descriptor(descriptor.id)) {
            (void)m_host->metadata().register_component(std::move(descriptor));
        }
    };

    for (const FunctionPreset& preset : m_presets) {
        register_if_missing(ComponentDescriptor{
            .id = preset.id,
            .display_name = preset.name,
            .category = ObjectCategory::FieldObject,
            .capabilities = {Capability::ParameterDomain, Capability::EmbeddedEvaluation, Capability::DerivativeOperator, Capability::RenderPacketProducer},
            .assumptions = {Assumption::OrientedDomain},
            .trust = {.level = TrustLevel::Tested, .summary = "Live 1D equation object for the integration workbench."},
            .docs = {.title = "Integration Lab", .path = "docs/INTEGRATION_LAB_DESIGN.md", .section = "Object Zoo Selector"},
            .factory_available = true
        });
    }

    register_if_missing(ComponentDescriptor{
        .id = ids::integration_zoo_gaussian,
        .display_name = "Gaussian scalar field",
        .category = ObjectCategory::FieldObject,
        .capabilities = {Capability::ParameterDomain, Capability::EmbeddedEvaluation, Capability::RenderPacketProducer},
        .assumptions = {Assumption::C2Regularity, Assumption::OrientedDomain},
        .trust = {.level = TrustLevel::Tested, .summary = "Live 2D integration preset with analytic reference support."},
        .docs = {.title = "Integration Lab", .path = "docs/INTEGRATION_LAB_DESIGN.md", .section = "Initial MVP"},
        .factory_available = true
    });
    register_if_missing(ComponentDescriptor{
        .id = ids::integration_zoo_wave,
        .display_name = "Wave scalar field",
        .category = ObjectCategory::FieldObject,
        .capabilities = {Capability::ParameterDomain, Capability::EmbeddedEvaluation, Capability::RenderPacketProducer},
        .assumptions = {Assumption::C2Regularity, Assumption::OrientedDomain},
        .trust = {.level = TrustLevel::Tested, .summary = "Live 2D integration preset with smooth oscillation."},
        .docs = {.title = "Integration Lab", .path = "docs/INTEGRATION_LAB_DESIGN.md", .section = "Initial MVP"},
        .factory_available = true
    });
    register_if_missing(ComponentDescriptor{
        .id = ids::integration_zoo_polynomial,
        .display_name = "Polynomial scalar field",
        .category = ObjectCategory::FieldObject,
        .capabilities = {Capability::ParameterDomain, Capability::EmbeddedEvaluation, Capability::RenderPacketProducer},
        .assumptions = {Assumption::C2Regularity, Assumption::OrientedDomain},
        .trust = {.level = TrustLevel::Tested, .summary = "Live 2D integration preset for simple algebraic behavior."},
        .docs = {.title = "Integration Lab", .path = "docs/INTEGRATION_LAB_DESIGN.md", .section = "Initial MVP"},
        .factory_available = true
    });
    register_if_missing(ComponentDescriptor{
        .id = ids::integration_zoo_step_x,
        .display_name = "Step discontinuity field",
        .category = ObjectCategory::FieldObject,
        .capabilities = {Capability::ParameterDomain, Capability::EmbeddedEvaluation, Capability::RenderPacketProducer},
        .assumptions = {Assumption::OrientedDomain},
        .trust = {.level = TrustLevel::Experimental, .summary = "Live discontinuous preset for local error and refinement diagnostics."},
        .docs = {.title = "Integration Lab", .path = "docs/INTEGRATION_LAB_DESIGN.md", .section = "Initial MVP"},
        .factory_available = true
    });
    register_if_missing(ComponentDescriptor{
        .id = ids::integration_zoo_plane_patch,
        .display_name = "Plane surface patch",
        .category = ObjectCategory::GeometricObject,
        .capabilities = {Capability::ParameterDomain, Capability::EmbeddedEvaluation, Capability::TangentBundle, Capability::RenderPacketProducer},
        .assumptions = {Assumption::C2Regularity, Assumption::OrientedDomain},
        .trust = {.level = TrustLevel::Experimental, .summary = "Planned surface integration object; backend adapter not wired yet."},
        .docs = {.title = "Integration Lab", .path = "docs/INTEGRATION_LAB_DESIGN.md", .section = "2D-First, 3D-Compatible Vocabulary"},
        .factory_available = false
    });
    register_if_missing(ComponentDescriptor{
        .id = ids::integration_zoo_torus_chart,
        .display_name = "Torus manifold chart",
        .category = ObjectCategory::GeometricObject,
        .capabilities = {Capability::ParameterDomain, Capability::EmbeddedEvaluation, Capability::MetricTensor, Capability::GaussianCurvature},
        .assumptions = {Assumption::C2Regularity, Assumption::ChartDomainValid, Assumption::OrientedDomain},
        .trust = {.level = TrustLevel::Experimental, .summary = "Planned manifold/surface measure object; backend adapter not wired yet."},
        .docs = {.title = "Integration Lab", .path = "docs/INTEGRATION_LAB_DESIGN.md", .section = "2D-First, 3D-Compatible Vocabulary"},
        .factory_available = false
    });
}

void SimulationIntegrationDerivativeLab::on_start() {
    const auto& current = preset();
    m_probe_x = (current.interval.min + current.interval.max) * f64(0.5);
    recompute();
}

void SimulationIntegrationDerivativeLab::on_tick(const TickInfo& tick) {
    update_workbench_state(tick);
    submit_geometry();
}

void SimulationIntegrationDerivativeLab::on_simulation_tick(const TickInfo& tick) {
    update_workbench_state(tick);
}

void SimulationIntegrationDerivativeLab::update_workbench_state(const TickInfo& tick) {
    m_time = tick.time;
    if (!m_show_1d_bridge) {
        handle_2d_workbench_interaction();
    }
    if (!tick.paused) {
        const auto& current = preset();
        const f64 span = current.interval.length();
        const f64 phase = std::fmod(static_cast<f64>(tick.time) * f64(0.15), f64(1));
        m_probe_x = current.interval.min + phase * span;
        recompute();
    }
}

void SimulationIntegrationDerivativeLab::handle_2d_workbench_interaction() {
    if (!m_host || m_main_view == RenderViewId(0)) return;

    const RenderViewDomain domain = m_host->render().view_domain(m_main_view);
    const ViewMouseState mouse = m_host->interaction().mouse_state(m_main_view);
    if (mouse.enabled) {
        const Vec2 point = view_point_from_ndc(domain, mouse.ndc);
        const auto hovered = m_workbench.hover_cell_at(point.x, point.y);
        if (hovered) {
            m_host->interaction().set_hover_view_point(m_main_view, point, Vec3{point.x, point.y, 0.f});
        }
    } else {
        m_workbench.hover_cell(std::nullopt);
    }

    auto picks = m_host->interaction().consume_view_point_picks(m_main_view);
    for (const ViewPointPickRequest& pick : picks) {
        const Vec2 point = view_point_from_ndc(domain, pick.screen_ndc);
        if (m_workbench.select_cell_at(point.x, point.y)) {
            m_host->interaction().select_view_point(m_main_view, point, Vec3{point.x, point.y, 0.f});
            m_last_selected_view_point = point;
        }
    }

    const InteractionTarget selected = m_host->interaction().selected_target(m_main_view);
    if (selected.kind == InteractionTargetKind::ViewPoint2D && selected.valid) {
        const Vec2 point = selected.point2d;
        if (!m_last_selected_view_point || !same_view_point(*m_last_selected_view_point, point)) {
            (void)m_workbench.select_cell_at(point.x, point.y);
            m_last_selected_view_point = point;
        }
    }
}

void SimulationIntegrationDerivativeLab::on_submit_render() {
    submit_geometry();
}

void SimulationIntegrationDerivativeLab::on_stop() {
    m_panel_handles.clear();
    m_main_handle.reset();
    m_analytics_handle.reset();
    m_analytics_view = RenderViewId(0);
    m_host = nullptr;
}

SceneSnapshot SimulationIntegrationDerivativeLab::snapshot() const {
    return SceneSnapshot{
        .name = std::string(name()),
        .paused = false,
        .sim_time = m_time,
        .sim_speed = f32(1),
        .particle_count = 0u,
        .status = m_status
    };
}

SimulationMetadata SimulationIntegrationDerivativeLab::metadata() const {
    const auto snapshot = m_workbench.snapshot();
    return SimulationMetadata{
        .name = std::string(name()),
        .surface_name = "2D rectangle domain",
        .surface_formula = std::string(math::integration::integrand_name(snapshot.problem.integrand_preset)),
        .status = m_status,
        .sim_time = m_time,
        .sim_speed = f32(1),
        .particle_count = static_cast<std::size_t>(snapshot.result.cell_count),
        .surface_has_analytic_derivatives = true
    };
}

f64 SimulationIntegrationDerivativeLab::reference_value() const {
    const auto& current = preset();
    return current.antiderivative(current.interval.max) - current.antiderivative(current.interval.min);
}

void SimulationIntegrationDerivativeLab::recompute() {
    const auto& current = preset();
    auto result = math::integration::integrate_uniform(
        current.interval,
        current.function,
        m_partition,
        m_method,
        reference_value());
    if (!result) {
        m_status = math::integration::describe_error(result.error());
        return;
    }
    m_result = std::move(*result);
    m_probe_x = std::clamp(m_probe_x, current.interval.min, current.interval.max);
    m_probe_derivative = math::integration::central_difference(current.function, m_probe_x);
    m_status = "Ready";
}

void SimulationIntegrationDerivativeLab::draw_control_panel() {
    draw_2d_top_bar();
}

void SimulationIntegrationDerivativeLab::draw_2d_top_bar() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse;
    const ImVec2 pos{viewport->WorkPos.x, viewport->WorkPos.y};
    const ImVec2 size{std::max(520.f, viewport->WorkSize.x), 88.f};
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    if (!ImGui::Begin("IntegrationWorkbenchTopBar", nullptr, flags)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("nurbs_dde");
    ImGui::SameLine();
    ImGui::TextDisabled("Integration Observatory");
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(28.f, 0.f));

    draw_zoo_selector();
    ImGui::SameLine();

    bool changed = false;
    const char* method_names[] = {"Left", "Right", "Midpoint", "Trapezoid", "Simpson"};
    int method_index = static_cast<int>(m_method);
    ImGui::TextDisabled("Method");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.f);
    if (ImGui::Combo("##MethodTop", &method_index, method_names, IM_ARRAYSIZE(method_names))) {
        m_method = static_cast<math::integration::QuadratureMethod>(method_index);
        if (m_method == math::integration::QuadratureMethod::Simpson &&
            (m_partition.cell_count % u32(2)) != u32(0)) {
            ++m_partition.cell_count;
        }
        changed = true;
    }
    ImGui::SameLine();

    int cells = static_cast<int>(m_partition.cell_count);
    ImGui::TextDisabled("N");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(170.f);
    if (ImGui::SliderInt("##NTop", &cells, 2, 256)) {
        m_partition.cell_count = static_cast<u32>(std::max(cells, 2));
        if (m_method == math::integration::QuadratureMethod::Simpson &&
            (m_partition.cell_count % u32(2)) != u32(0)) {
            ++m_partition.cell_count;
        }
        changed = true;
    }
    ImGui::SameLine();
    ImGui::Text("%u", m_partition.cell_count);
    ImGui::SameLine();

    ImGui::TextDisabled("Interval");
    ImGui::SameLine();
    double min = preset().interval.min;
    double max = preset().interval.max;
    ImGui::SetNextItemWidth(96.f);
    if (ImGui::InputDouble("##IntervalMinTop", &min, 0.0, 0.0, "%.3f")) {
        m_presets[m_preset_index].interval.min = static_cast<f64>(min);
        changed = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(96.f);
    if (ImGui::InputDouble("##IntervalMaxTop", &max, 0.0, 0.0, "%.3f")) {
        m_presets[m_preset_index].interval.max = static_cast<f64>(max);
        changed = true;
    }

    if (changed) {
        const auto& current = preset();
        m_probe_x = (current.interval.min + current.interval.max) * f64(0.5);
        recompute();
    }

    ImGui::End();
}

void SimulationIntegrationDerivativeLab::draw_zoo_selector() {
    if (!m_host) return;

    const ComponentDescriptor* selected = m_host->metadata().get_descriptor(m_selected_zoo_component);
    const char* selected_label = selected ? selected->display_name.c_str() : "Select object";
    ImGui::TextDisabled("Equation");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.f);
    if (!ImGui::BeginCombo("##EquationTop", selected_label)) {
        return;
    }

    for (const ComponentDescriptor& descriptor : m_host->metadata().descriptors()) {
        if (!is_integration_zoo_descriptor(descriptor)) continue;
        if (!is_function_zoo_component(descriptor.id)) continue;

        const bool is_selected = component_id_equals(descriptor.id, m_selected_zoo_component);
        ImGui::PushID(descriptor.id.value.data());
        const bool chosen = ImGui::Selectable(descriptor.display_name.c_str(), is_selected);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\n%s\n%s",
                              category_label(descriptor.category),
                              descriptor.trust.summary.c_str(),
                              "Live backend");
        }
        if (chosen) {
            m_selected_zoo_component = descriptor.id;
            for (std::size_t i = 0; i < m_presets.size(); ++i) {
                if (!component_id_equals(m_presets[i].id, descriptor.id)) continue;
                m_preset_index = i;
                const auto& current = preset();
                m_probe_x = (current.interval.min + current.interval.max) * f64(0.5);
                m_show_1d_bridge = true;
                recompute();
                break;
            }
        }
        ImGui::PopID();
    }

    ImGui::EndCombo();
}

void SimulationIntegrationDerivativeLab::draw_2d_tool_tray() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse;
    const float top = viewport->WorkPos.y + 116.f;
    const float bottom = viewport->WorkPos.y + viewport->WorkSize.y - 64.f;
    const ImVec2 pos{viewport->WorkPos.x + 12.f, top};
    const ImVec2 size{320.f, std::max(220.f, bottom - top)};
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);
    if (!ImGui::Begin("IntegrationWorkbenchTools", nullptr, flags)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Workbench: %s", workbench_mode_label(m_workbench_mode));
    ImGui::Separator();
    if (m_show_1d_bridge) {
        draw_1d_bridge_panel();
    } else {
        draw_2d_mode_body();
    }
    ImGui::End();
}

void SimulationIntegrationDerivativeLab::draw_2d_inspector_panel() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse;
    const float width = 300.f;
    const float top = viewport->WorkPos.y + 116.f;
    const float bottom = viewport->WorkPos.y + viewport->WorkSize.y - 64.f;
    const ImVec2 pos{viewport->WorkPos.x + viewport->WorkSize.x - width - 12.f, top};
    const ImVec2 size{width, std::max(220.f, bottom - top)};
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);
    if (!ImGui::Begin("IntegrationWorkbenchInspector", nullptr, flags)) {
        ImGui::End();
        return;
    }

    draw_2d_live_inspector();
    ImGui::End();
}

void SimulationIntegrationDerivativeLab::draw_2d_bottom_status() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse;
    const ImVec2 size{std::max(420.f, viewport->WorkSize.x - 24.f), 42.f};
    const ImVec2 pos{viewport->WorkPos.x + 12.f, viewport->WorkPos.y + viewport->WorkSize.y - size.y - 12.f};
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.90f);
    if (!ImGui::Begin("IntegrationWorkbenchStatus", nullptr, flags)) {
        ImGui::End();
        return;
    }

    const auto snapshot = m_workbench.snapshot();
    ImGui::Text("Estimate %.10f", snapshot.result.estimate);
    ImGui::SameLine();
    if (snapshot.result.absolute_error) {
        ImGui::TextDisabled("abs err %.3e", *snapshot.result.absolute_error);
    } else {
        ImGui::TextDisabled("abs err n/a");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("cells %llu", static_cast<unsigned long long>(snapshot.result.cell_count));
    ImGui::SameLine();
    if (snapshot.selected_cell.valid) {
        ImGui::TextDisabled("selected cell %u", snapshot.selected_cell.cell_id);
    } else {
        ImGui::TextDisabled("selected cell none");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("mode %s", workbench_mode_label(m_workbench_mode));
    ImGui::End();
}

void SimulationIntegrationDerivativeLab::draw_2d_mode_strip() {
    constexpr std::array modes{
        WorkbenchMode::Domain,
        WorkbenchMode::Partition,
        WorkbenchMode::Samples,
        WorkbenchMode::Contribution,
        WorkbenchMode::Error,
        WorkbenchMode::Bounds,
        WorkbenchMode::Analysis
    };

    for (WorkbenchMode mode : modes) {
        const bool selected = m_workbench_mode == mode;
        if (ImGui::Selectable(workbench_mode_short_label(mode), selected, 0, ImVec2(34.f, 28.f))) {
            m_workbench_mode = mode;
            if (mode == WorkbenchMode::Contribution) {
                m_workbench.set_display_mode(IntegrationDisplayMode::Contribution);
            } else if (mode == WorkbenchMode::Error) {
                m_workbench.set_display_mode(IntegrationDisplayMode::LocalError);
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s%s",
                              workbench_mode_label(mode),
                              mode_is_live(mode) ? "" : " (planned)");
        }
        ImGui::SameLine();
    }
    ImGui::NewLine();

    ImGui::Text("Mode: %s", workbench_mode_label(m_workbench_mode));
    ImGui::SameLine();
    draw_status_badge(mode_is_live(m_workbench_mode) ? "Live" : "Planned", mode_is_live(m_workbench_mode));
}

void SimulationIntegrationDerivativeLab::draw_2d_mode_body() {
    switch (m_workbench_mode) {
        case WorkbenchMode::Domain:
            draw_2d_problem_tab();
            break;
        case WorkbenchMode::Partition:
            draw_2d_partition_tab();
            break;
        case WorkbenchMode::Samples:
            draw_2d_samples_tab();
            break;
        case WorkbenchMode::Contribution:
            draw_2d_display_tab();
            break;
        case WorkbenchMode::Error:
            draw_2d_error_tab();
            break;
        case WorkbenchMode::Bounds:
            draw_2d_bounds_tab();
            break;
        case WorkbenchMode::Analysis:
            draw_2d_analysis_tab();
            break;
    }
}

void SimulationIntegrationDerivativeLab::draw_2d_problem_tab() {
    auto snapshot = m_workbench.snapshot();
    bool changed = false;

    ImGui::SeparatorText("Domain");
    const char* domain_names[] = {"Rectangle"};
    int domain_index = 0;
    ImGui::Combo("Domain", &domain_index, domain_names, IM_ARRAYSIZE(domain_names));
    ImGui::SameLine();
    draw_status_badge("Live", true);

    ImGui::BeginDisabled();
    const char* future_domains[] = {
        "Disk",
        "Annulus",
        "Triangle",
        "Polygon",
        "Region under curve",
        "Surface patch"
    };
    int future_domain_index = 0;
    ImGui::Combo("Next domains", &future_domain_index, future_domains, IM_ARRAYSIZE(future_domains));
    ImGui::EndDisabled();

    ImGui::SeparatorText("Integrand");
    const char* integrand_names[] = {"Gaussian", "Wave", "Polynomial", "Step X"};
    int integrand_index = static_cast<int>(snapshot.problem.integrand_preset);
    if (ImGui::Combo("Integrand", &integrand_index, integrand_names, IM_ARRAYSIZE(integrand_names))) {
        changed |= m_workbench.set_integrand(static_cast<math::integration::IntegrandPreset2D>(integrand_index));
    }
    ImGui::TextDisabled("%s", math::integration::integrand_name(snapshot.problem.integrand_preset).data());

    ImGui::SeparatorText("Rectangle Bounds");
    float x_min = static_cast<float>(snapshot.problem.domain.x_min);
    float x_max = static_cast<float>(snapshot.problem.domain.x_max);
    float y_min = static_cast<float>(snapshot.problem.domain.y_min);
    float y_max = static_cast<float>(snapshot.problem.domain.y_max);
    bool domain_changed = false;
    domain_changed |= ImGui::SliderFloat("x min", &x_min, -8.f, 7.5f);
    domain_changed |= ImGui::SliderFloat("x max", &x_max, -7.5f, 8.f);
    domain_changed |= ImGui::SliderFloat("y min", &y_min, -8.f, 7.5f);
    domain_changed |= ImGui::SliderFloat("y max", &y_max, -7.5f, 8.f);
    if (domain_changed) {
        changed |= m_workbench.set_domain(math::integration::RectDomain2D{
            .x_min = static_cast<f64>(x_min),
            .x_max = static_cast<f64>(x_max),
            .y_min = static_cast<f64>(y_min),
            .y_max = static_cast<f64>(y_max)
        });
    }

    ImGui::SeparatorText("Measure");
    ImGui::TextUnformatted("dA");
    ImGui::SameLine();
    draw_status_badge("Live", true);
    ImGui::BeginDisabled();
    bool dx_dy = true;
    bool jacobian = false;
    bool surface_area = false;
    ImGui::Checkbox("dx dy", &dx_dy);
    ImGui::Checkbox("Jacobian weighted du dv", &jacobian);
    ImGui::Checkbox("surface element dS", &surface_area);
    ImGui::EndDisabled();

    if (changed) {
        snapshot = m_workbench.snapshot();
        m_status = snapshot.metadata.has_error ? "2D workbench configuration error" : "Ready";
    }
}

void SimulationIntegrationDerivativeLab::draw_2d_partition_tab() {
    auto snapshot = m_workbench.snapshot();
    bool changed = false;

    ImGui::SeparatorText("Partition");
    const char* partition_names[] = {"Uniform Grid"};
    int partition_index = 0;
    ImGui::Combo("Partition", &partition_index, partition_names, IM_ARRAYSIZE(partition_names));
    ImGui::SameLine();
    draw_status_badge("Live", true);

    ImGui::BeginDisabled();
    const char* future_partitions[] = {
        "Tagged nonuniform",
        "Adaptive quadtree",
        "Triangular mesh",
        "Monte Carlo",
        "NURBS knot aware"
    };
    int future_partition = 0;
    ImGui::Combo("Next partitioners", &future_partition, future_partitions, IM_ARRAYSIZE(future_partitions));
    ImGui::EndDisabled();

    ImGui::SeparatorText("Resolution");
    int x_cells = static_cast<int>(snapshot.problem.grid.x_cells);
    int y_cells = static_cast<int>(snapshot.problem.grid.y_cells);
    if (ImGui::SliderInt("X cells", &x_cells, 2, 160)) {
        changed |= m_workbench.set_grid(math::integration::UniformGrid2DConfig{
            .x_cells = static_cast<u32>(std::max(x_cells, 2)),
            .y_cells = snapshot.problem.grid.y_cells
        });
    }
    snapshot = m_workbench.snapshot();
    if (ImGui::SliderInt("Y cells", &y_cells, 2, 160)) {
        changed |= m_workbench.set_grid(math::integration::UniformGrid2DConfig{
            .x_cells = snapshot.problem.grid.x_cells,
            .y_cells = static_cast<u32>(std::max(y_cells, 2))
        });
    }

    snapshot = m_workbench.snapshot();
    int selected_cell = snapshot.selected_cell.valid
        ? static_cast<int>(snapshot.selected_cell.cell_id)
        : 0;
    const int max_cell = static_cast<int>(std::max<u64>(snapshot.result.cell_count, u64(1)) - u64(1));
    if (ImGui::SliderInt("Selected cell", &selected_cell, 0, max_cell)) {
        m_workbench.select_cell(static_cast<u32>(selected_cell));
    }

    if (changed) {
        m_status = m_workbench.snapshot().metadata.has_error ? "2D workbench configuration error" : "Ready";
    }
}

void SimulationIntegrationDerivativeLab::draw_2d_samples_tab() {
    const auto snapshot = m_workbench.snapshot();
    IntegrationDisplayConfig display = m_workbench.display_config();

    ImGui::SeparatorText("Sample Points");
    ImGui::Checkbox("Show sample points", &display.show_samples);
    ImGui::Checkbox("Show cell grid", &display.show_cells);
    m_workbench.set_display_config(display);

    ImGui::Text("Cells: %llu", static_cast<unsigned long long>(snapshot.result.cell_count));
    ImGui::Text("Evaluations: %llu", static_cast<unsigned long long>(snapshot.result.evaluation_count));
    ImGui::Text("Default tag: midpoint");

    ImGui::SeparatorText("Selected Sample");
    if (snapshot.selected_cell.valid) {
        const auto& selected = snapshot.selected_cell.contribution;
        ImGui::Text("cell %u", selected.cell.id);
        ImGui::Text("sample x %.6f", static_cast<double>(selected.sample.x));
        ImGui::Text("sample y %.6f", static_cast<double>(selected.sample.y));
        ImGui::Text("f(sample) %.8f", selected.value);
    } else {
        ImGui::TextDisabled("Click a cell in the viewport to pin a sample.");
    }

    ImGui::SeparatorText("Planned Tag Lab");
    ImGui::BeginDisabled();
    bool draggable_tags = false;
    bool random_tags = false;
    bool show_tag_error = false;
    ImGui::Checkbox("draggable tags", &draggable_tags);
    ImGui::Checkbox("random tags", &random_tags);
    ImGui::Checkbox("tag perturbation stability", &show_tag_error);
    ImGui::EndDisabled();
}

void SimulationIntegrationDerivativeLab::draw_2d_display_tab() {
    IntegrationDisplayConfig display = m_workbench.display_config();

    ImGui::SeparatorText("Contribution View");
    const char* display_names[] = {"Value", "Contribution", "Local Error"};
    int display_index = static_cast<int>(display.mode);
    if (ImGui::Combo("Display mode", &display_index, display_names, IM_ARRAYSIZE(display_names))) {
        display.mode = static_cast<IntegrationDisplayMode>(display_index);
    }
    ImGui::Checkbox("Domain boundary", &display.show_domain_boundary);
    ImGui::Checkbox("Cell grid", &display.show_cells);
    ImGui::Checkbox("Sample points", &display.show_samples);
    ImGui::Checkbox("Axes", &display.show_axes);
    m_workbench.set_display_config(display);

    ImGui::SeparatorText("Layer Plan");
    ImGui::BeginDisabled();
    bool contour_lines = false;
    bool contribution_labels = false;
    bool selected_cell_trace = false;
    ImGui::Checkbox("contour lines", &contour_lines);
    ImGui::Checkbox("cell contribution labels", &contribution_labels);
    ImGui::Checkbox("selected-cell trace path", &selected_cell_trace);
    ImGui::EndDisabled();
}

void SimulationIntegrationDerivativeLab::draw_2d_error_tab() {
    IntegrationDisplayConfig display = m_workbench.display_config();
    display.mode = IntegrationDisplayMode::LocalError;
    ImGui::SeparatorText("Error Map");
    ImGui::TextUnformatted("Showing sampled local-error estimate in the main viewport.");
    ImGui::Checkbox("Domain boundary", &display.show_domain_boundary);
    ImGui::Checkbox("Cell grid", &display.show_cells);
    ImGui::Checkbox("Sample points", &display.show_samples);
    m_workbench.set_display_config(display);

    const auto snapshot = m_workbench.snapshot();
    if (snapshot.result.absolute_error) {
        ImGui::Text("Global abs error %.6e", *snapshot.result.absolute_error);
    } else {
        ImGui::TextDisabled("Global abs error unavailable for this preset.");
    }

    ImGui::SeparatorText("Refinement Queue");
    ImGui::BeginDisabled();
    bool split_largest_error = true;
    bool split_near_discontinuity = false;
    bool refine_boundary = false;
    ImGui::Checkbox("split largest-error cell", &split_largest_error);
    ImGui::Checkbox("refine near discontinuity", &split_near_discontinuity);
    ImGui::Checkbox("refine near boundary", &refine_boundary);
    ImGui::Button("Refine once");
    ImGui::SameLine();
    ImGui::Button("Refine to tolerance");
    ImGui::EndDisabled();
}

void SimulationIntegrationDerivativeLab::draw_2d_bounds_tab() {
    ImGui::SeparatorText("Darboux Bounds");
    draw_status_badge("Planned", false);
    ImGui::TextWrapped("This pane will compare sampled lower and upper sums against the active partition.");

    ImGui::BeginDisabled();
    bool show_lower = true;
    bool show_upper = true;
    bool show_gap = true;
    int probe_samples = 5;
    ImGui::Checkbox("lower sum layer", &show_lower);
    ImGui::Checkbox("upper sum layer", &show_upper);
    ImGui::Checkbox("U - L gap layer", &show_gap);
    ImGui::SliderInt("samples per cell", &probe_samples, 3, 17);
    ImGui::Button("Compute sampled bounds");
    ImGui::EndDisabled();
}

void SimulationIntegrationDerivativeLab::draw_2d_analysis_tab() {
    const IntegrationAnalysisSnapshot analysis = m_workbench.analysis_snapshot();
    const char* method_names[] = {"Midpoint", "Trapezoid"};
    int method_index = static_cast<int>(m_workbench.snapshot().problem.method);
    if (ImGui::Combo("Active method", &method_index, method_names, IM_ARRAYSIZE(method_names))) {
        (void)m_workbench.set_method(static_cast<math::integration::IntegrationMethod2D>(method_index));
    }

    ImGui::SeparatorText("Convergence");
    if (ImGui::BeginTable("ConvergenceTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Grid");
        ImGui::TableSetupColumn("Estimate");
        ImGui::TableSetupColumn("Abs error");
        ImGui::TableSetupColumn("Order");
        ImGui::TableHeadersRow();
        for (const auto& row : analysis.convergence.rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%u x %u", row.grid.x_cells, row.grid.y_cells);
            ImGui::TableNextColumn();
            ImGui::Text("%.8f", row.estimate);
            ImGui::TableNextColumn();
            row.absolute_error ? ImGui::Text("%.3e", *row.absolute_error) : ImGui::TextDisabled("-");
            ImGui::TableNextColumn();
            row.observed_order ? ImGui::Text("%.2f", *row.observed_order) : ImGui::TextDisabled("-");
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Method Comparison");
    for (const auto& row : analysis.method_comparison) {
        ImGui::Text("%s  estimate %.8f  error %.3e",
                    method_2d_label(row.method),
                    row.estimate,
                    row.absolute_error.value_or(f64(0)));
    }

    ImGui::SeparatorText("Second Window");
    ImGui::TextUnformatted("Convergence and method bars render in Integration Analytics.");
    ImGui::BeginDisabled();
    bool result_trace = false;
    bool theorem_notes = false;
    bool export_report = false;
    ImGui::Checkbox("result trace rows", &result_trace);
    ImGui::Checkbox("theorem/context notes", &theorem_notes);
    ImGui::Checkbox("export diagnostic report", &export_report);
    ImGui::EndDisabled();
}

void SimulationIntegrationDerivativeLab::draw_2d_live_inspector() {
    const auto snapshot = m_workbench.snapshot();

    ImGui::SeparatorText("Problem");
    ImGui::Text("Domain");
    ImGui::TextDisabled("[%.2f, %.2f] x [%.2f, %.2f]",
                        snapshot.problem.domain.x_min,
                        snapshot.problem.domain.x_max,
                        snapshot.problem.domain.y_min,
                        snapshot.problem.domain.y_max);
    ImGui::Text("Integrand");
    ImGui::TextDisabled("%s", integrand_preset_label(snapshot.problem.integrand_preset));
    ImGui::Text("Measure");
    ImGui::TextDisabled("dA");
    ImGui::Text("Partition");
    ImGui::TextDisabled("%u x %u = %llu cells",
                        snapshot.problem.grid.x_cells,
                        snapshot.problem.grid.y_cells,
                        static_cast<unsigned long long>(snapshot.result.cell_count));
    ImGui::Text("Method");
    ImGui::TextDisabled("%s", method_2d_label(snapshot.problem.method));
    ImGui::Text("Display");
    ImGui::TextDisabled("%s", display_mode_label(snapshot.renderable.display.mode));

    ImGui::SeparatorText("Live Result");
    ImGui::Text("Estimate");
    ImGui::TextDisabled("%.12f", snapshot.result.estimate);
    if (snapshot.result.reference_value) {
        ImGui::Text("Reference");
        ImGui::TextDisabled("%.12f", *snapshot.result.reference_value);
    }
    if (snapshot.result.absolute_error) {
        ImGui::Text("Abs error");
        ImGui::TextDisabled("%.6e", *snapshot.result.absolute_error);
    }
    if (snapshot.result.relative_error) {
        ImGui::Text("Rel error");
        ImGui::TextDisabled("%.6e", *snapshot.result.relative_error);
    }
    ImGui::Text("Evaluations");
    ImGui::TextDisabled("%llu", static_cast<unsigned long long>(snapshot.result.evaluation_count));

    ImGui::SeparatorText("Cell Inspector");
    if (snapshot.selected_cell.valid) {
        const auto& selected = snapshot.selected_cell.contribution;
        ImGui::Text("id %u", selected.cell.id);
        ImGui::TextDisabled("grid (%u, %u)", selected.cell.ix, selected.cell.iy);
        ImGui::TextDisabled("x[%.3f, %.3f]", selected.cell.x0, selected.cell.x1);
        ImGui::TextDisabled("y[%.3f, %.3f]", selected.cell.y0, selected.cell.y1);
        ImGui::Text("f %.8f", selected.value);
        ImGui::Text("dA %.8f", selected.measure);
        ImGui::Text("contrib %.8f", selected.contribution);
        ImGui::Text("local err %.3e", selected.local_error_estimate);
    } else {
        ImGui::TextDisabled("Hover or click a viewport cell.");
    }

    draw_2d_status_strip();
}

void SimulationIntegrationDerivativeLab::draw_2d_status_strip() {
    const auto snapshot = m_workbench.snapshot();

    ImGui::SeparatorText("Status");
    if (snapshot.metadata.has_error) {
        ImGui::TextDisabled("Last error: %s",
                            math::integration::describe_error(snapshot.metadata.last_error).c_str());
    } else {
        ImGui::TextDisabled("%s", m_status.c_str());
    }
    ImGui::TextDisabled("Main viewport: heatmap/cells/samples");
    ImGui::TextDisabled("Analytics viewport: convergence/comparison");
}

void SimulationIntegrationDerivativeLab::draw_1d_bridge_panel() {
    bool changed = false;
    ImGui::SeparatorText("1D Bridge");
    if (ImGui::BeginCombo("Function", preset().name)) {
        for (std::size_t i = 0; i < m_presets.size(); ++i) {
            const bool selected = i == m_preset_index;
            if (ImGui::Selectable(m_presets[i].name, selected)) {
                m_preset_index = i;
                m_selected_zoo_component = m_presets[i].id;
                const auto& current = preset();
                m_probe_x = (current.interval.min + current.interval.max) * f64(0.5);
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    const char* method_names[] = {"Left", "Right", "Midpoint", "Trapezoid", "Simpson"};
    int method_index = static_cast<int>(m_method);
    if (ImGui::Combo("Method", &method_index, method_names, IM_ARRAYSIZE(method_names))) {
        m_method = static_cast<math::integration::QuadratureMethod>(method_index);
        if (m_method == math::integration::QuadratureMethod::Simpson &&
            (m_partition.cell_count % u32(2)) != u32(0)) {
            ++m_partition.cell_count;
        }
        changed = true;
    }

    int cells = static_cast<int>(m_partition.cell_count);
    if (ImGui::SliderInt("Cells", &cells, 2, 256)) {
        m_partition.cell_count = static_cast<u32>(std::max(cells, 2));
        if (m_method == math::integration::QuadratureMethod::Simpson &&
            (m_partition.cell_count % u32(2)) != u32(0)) {
            ++m_partition.cell_count;
        }
        changed = true;
    }

    const auto& current = preset();
    float probe = static_cast<float>(m_probe_x);
    if (ImGui::SliderFloat("Probe x", &probe,
                           static_cast<float>(current.interval.min),
                           static_cast<float>(current.interval.max))) {
        m_probe_x = static_cast<f64>(probe);
        changed = true;
    }
    changed |= ImGui::Checkbox("Cells", &m_show_cells);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Tangent", &m_show_tangent);

    if (changed) {
        recompute();
    }

    ImGui::SeparatorText("Result");
    ImGui::TextUnformatted(current.formula);
    ImGui::Text("Method: %s", math::integration::method_name(m_method).data());
    ImGui::Text("Estimate: %.12f", m_result.estimate);
    ImGui::Text("Reference: %.12f", reference_value());
    ImGui::Text("Abs error: %.6e", m_result.absolute_error.value_or(f64(0)));
    ImGui::Text("Evaluations: %llu", static_cast<unsigned long long>(m_result.evaluation_count));
    ImGui::SeparatorText("Derivative Probe");
    ImGui::Text("x: %.6f", m_probe_x);
    ImGui::Text("f(x): %.6f", current.function(m_probe_x));
    ImGui::Text("f'(x): %.6f", m_probe_derivative);
    ImGui::TextDisabled("%s", m_status.c_str());
}

void SimulationIntegrationDerivativeLab::submit_geometry() {
    if (!m_host || m_main_view == RenderViewId(0)) return;

    (void)submit_integration_analytics_packets(
        m_host->render(),
        m_analytics_view,
        m_workbench.analysis_snapshot(),
        m_memory);

    if (!m_show_1d_bridge) {
        (void)submit_integration_workbench_packets(
            m_host->render(),
            m_main_view,
            m_workbench.snapshot(),
            m_memory);
        return;
    }

    const auto& current = preset();
    const u32 samples = u32(240);
    memory::FrameVector<Vertex> graph =
        m_memory ? m_memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};
    graph.reserve(samples + u32(1));

    f64 y_min = f64(0);
    f64 y_max = f64(0);
    for (u32 i = u32(0); i <= samples; ++i) {
        const f64 alpha = static_cast<f64>(i) / static_cast<f64>(samples);
        const f64 x = current.interval.min + alpha * current.interval.length();
        const f64 y = current.function(x);
        y_min = std::min(y_min, y);
        y_max = std::max(y_max, y);
        graph.push_back(Vertex{
            .pos = Vec3{static_cast<f32>(x), static_cast<f32>(y), 0.f},
            .color = graph_color()
        });
    }

    for (const auto& contribution : m_result.contributions) {
        y_min = std::min(y_min, contribution.value);
        y_max = std::max(y_max, contribution.value);
    }
    const f64 y_span = std::max(y_max - y_min, f64(0.5));
    const f64 y_pad = y_span * f64(0.18);
    const f32 left = static_cast<f32>(current.interval.min);
    const f32 right = static_cast<f32>(current.interval.max);
    const f32 bottom = static_cast<f32>(y_min - y_pad);
    const f32 top = static_cast<f32>(y_max + y_pad);
    const Mat4 mvp = glm::ortho(left, right, bottom, top, -1.f, 1.f);

    m_host->render().set_view_domain(m_main_view, RenderViewDomain{
        .u_min = left,
        .u_max = right,
        .v_min = bottom,
        .v_max = top,
        .z_min = -1.f,
        .z_max = 1.f
    });

    memory::FrameVector<Vertex> axes =
        m_memory ? m_memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};
    push_line(axes, Vec2{left, 0.f}, Vec2{right, 0.f}, axis_color());
    push_line(axes, Vec2{0.f, bottom}, Vec2{0.f, top}, axis_color());
    m_host->render().submit(m_main_view, axes, Topology::LineList, DrawMode::VertexColor, axis_color(), mvp);

    if (m_show_cells && !m_result.contributions.empty()) {
        memory::FrameVector<Vertex> cells =
            m_memory ? m_memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};
        cells.reserve(m_result.contributions.size() * 8u);
        for (const auto& contribution : m_result.contributions) {
            const f32 a = static_cast<f32>(contribution.cell.a);
            const f32 b = static_cast<f32>(contribution.cell.b);
            const f32 y = static_cast<f32>(contribution.value);
            push_line(cells, Vec2{a, 0.f}, Vec2{a, y}, cell_color());
            push_line(cells, Vec2{a, y}, Vec2{b, y}, cell_color());
            push_line(cells, Vec2{b, y}, Vec2{b, 0.f}, cell_color());
            push_line(cells, Vec2{a, 0.f}, Vec2{b, 0.f}, cell_color());
        }
        m_host->render().submit(m_main_view, cells, Topology::LineList, DrawMode::VertexColor, cell_color(), mvp);
    }

    m_host->render().submit(m_main_view, graph, Topology::LineStrip, DrawMode::VertexColor, graph_color(), mvp);

    if (m_show_tangent) {
        const f64 y = current.function(m_probe_x);
        const f64 half_width = current.interval.length() * f64(0.08);
        memory::FrameVector<Vertex> tangent =
            m_memory ? m_memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};
        tangent.reserve(4u);
        push_line(tangent,
                  Vec2{static_cast<f32>(m_probe_x - half_width),
                       static_cast<f32>(y - m_probe_derivative * half_width)},
                  Vec2{static_cast<f32>(m_probe_x + half_width),
                       static_cast<f32>(y + m_probe_derivative * half_width)},
                  tangent_color());
        push_line(tangent,
                  Vec2{static_cast<f32>(m_probe_x), bottom},
                  Vec2{static_cast<f32>(m_probe_x), top},
                  Vec4{1.f, 0.9f, 0.35f, 0.55f});
        m_host->render().submit(m_main_view, tangent, Topology::LineList, DrawMode::VertexColor, tangent_color(), mvp);
    }
}

} // namespace ndde
