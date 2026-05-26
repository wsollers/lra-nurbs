#include "app/IntegrationLabAnalyticsPackets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <string_view>

namespace ndde {
namespace {

[[nodiscard]] Vec4 axis_color() noexcept { return Vec4{0.56f, 0.58f, 0.64f, 0.65f}; }
[[nodiscard]] Vec4 convergence_color() noexcept { return Vec4{0.28f, 0.78f, 1.00f, 0.95f}; }
[[nodiscard]] Vec4 comparison_midpoint_color() noexcept { return Vec4{1.00f, 0.70f, 0.22f, 0.90f}; }
[[nodiscard]] Vec4 comparison_trapezoid_color() noexcept { return Vec4{0.34f, 0.84f, 0.48f, 0.90f}; }
[[nodiscard]] Vec4 panel_line_color() noexcept { return Vec4{0.18f, 0.24f, 0.31f, 0.95f}; }
[[nodiscard]] Vec4 header_line_color() noexcept { return Vec4{0.30f, 0.38f, 0.48f, 0.95f}; }
[[nodiscard]] Vec4 label_color() noexcept { return Vec4{0.92f, 0.70f, 0.18f, 0.96f}; }
[[nodiscard]] Vec4 value_color() noexcept { return Vec4{0.72f, 0.82f, 0.92f, 0.88f}; }
[[nodiscard]] Vec4 muted_label_color() noexcept { return Vec4{0.44f, 0.52f, 0.62f, 0.82f}; }
[[nodiscard]] Vec4 contribution_color() noexcept { return Vec4{0.64f, 0.42f, 0.92f, 0.82f}; }

constexpr f32 k_title_text = 0.026f;
constexpr f32 k_panel_text = 0.020f;
constexpr f32 k_body_text = 0.017f;
constexpr f32 k_small_text = 0.015f;

void push_line(memory::FrameVector<Vertex>& out, Vec2 a, Vec2 b, Vec4 color) {
    out.push_back(Vertex{.pos = Vec3{a.x, a.y, 0.f}, .color = color});
    out.push_back(Vertex{.pos = Vec3{b.x, b.y, 0.f}, .color = color});
}

void push_rect_lines(memory::FrameVector<Vertex>& out, Vec2 min, Vec2 max, Vec4 color) {
    push_line(out, Vec2{min.x, min.y}, Vec2{max.x, min.y}, color);
    push_line(out, Vec2{max.x, min.y}, Vec2{max.x, max.y}, color);
    push_line(out, Vec2{max.x, max.y}, Vec2{min.x, max.y}, color);
    push_line(out, Vec2{min.x, max.y}, Vec2{min.x, min.y}, color);
}

void push_quad(memory::FrameVector<Vertex>& out, Vec2 min, Vec2 max, Vec4 color) {
    const Vec3 p00{min.x, min.y, 0.f};
    const Vec3 p10{max.x, min.y, 0.f};
    const Vec3 p11{max.x, max.y, 0.f};
    const Vec3 p01{min.x, max.y, 0.f};
    out.push_back(Vertex{.pos = p00, .color = color});
    out.push_back(Vertex{.pos = p10, .color = color});
    out.push_back(Vertex{.pos = p11, .color = color});
    out.push_back(Vertex{.pos = p00, .color = color});
    out.push_back(Vertex{.pos = p11, .color = color});
    out.push_back(Vertex{.pos = p01, .color = color});
}

[[nodiscard]] f32 normalized_error(f64 error, f64 max_error) noexcept {
    if (max_error <= f64(0)) return 0.f;
    const f64 scaled = std::log10(error + f64(1.0e-16)) -
                       std::log10(max_error + f64(1.0e-16));
    return static_cast<f32>(std::clamp((scaled + f64(8)) / f64(8), f64(0), f64(1)));
}

[[nodiscard]] std::array<const char*, 7> glyph_rows(char c) noexcept {
    switch (c) {
        case 'A': return {"01110", "10001", "10001", "11111", "10001", "10001", "10001"};
        case 'B': return {"11110", "10001", "10001", "11110", "10001", "10001", "11110"};
        case 'C': return {"01111", "10000", "10000", "10000", "10000", "10000", "01111"};
        case 'D': return {"11110", "10001", "10001", "10001", "10001", "10001", "11110"};
        case 'E': return {"11111", "10000", "10000", "11110", "10000", "10000", "11111"};
        case 'F': return {"11111", "10000", "10000", "11110", "10000", "10000", "10000"};
        case 'G': return {"01111", "10000", "10000", "10011", "10001", "10001", "01111"};
        case 'H': return {"10001", "10001", "10001", "11111", "10001", "10001", "10001"};
        case 'I': return {"11111", "00100", "00100", "00100", "00100", "00100", "11111"};
        case 'J': return {"00111", "00010", "00010", "00010", "10010", "10010", "01100"};
        case 'K': return {"10001", "10010", "10100", "11000", "10100", "10010", "10001"};
        case 'L': return {"10000", "10000", "10000", "10000", "10000", "10000", "11111"};
        case 'M': return {"10001", "11011", "10101", "10101", "10001", "10001", "10001"};
        case 'N': return {"10001", "11001", "10101", "10011", "10001", "10001", "10001"};
        case 'O': return {"01110", "10001", "10001", "10001", "10001", "10001", "01110"};
        case 'P': return {"11110", "10001", "10001", "11110", "10000", "10000", "10000"};
        case 'Q': return {"01110", "10001", "10001", "10001", "10101", "10010", "01101"};
        case 'R': return {"11110", "10001", "10001", "11110", "10100", "10010", "10001"};
        case 'S': return {"01111", "10000", "10000", "01110", "00001", "00001", "11110"};
        case 'T': return {"11111", "00100", "00100", "00100", "00100", "00100", "00100"};
        case 'U': return {"10001", "10001", "10001", "10001", "10001", "10001", "01110"};
        case 'V': return {"10001", "10001", "10001", "10001", "10001", "01010", "00100"};
        case 'W': return {"10001", "10001", "10001", "10101", "10101", "10101", "01010"};
        case 'X': return {"10001", "10001", "01010", "00100", "01010", "10001", "10001"};
        case 'Y': return {"10001", "10001", "01010", "00100", "00100", "00100", "00100"};
        case 'Z': return {"11111", "00001", "00010", "00100", "01000", "10000", "11111"};
        case '0': return {"01110", "10001", "10011", "10101", "11001", "10001", "01110"};
        case '1': return {"00100", "01100", "00100", "00100", "00100", "00100", "01110"};
        case '2': return {"01110", "10001", "00001", "00010", "00100", "01000", "11111"};
        case '3': return {"11110", "00001", "00001", "01110", "00001", "00001", "11110"};
        case '4': return {"00010", "00110", "01010", "10010", "11111", "00010", "00010"};
        case '5': return {"11111", "10000", "10000", "11110", "00001", "00001", "11110"};
        case '6': return {"01110", "10000", "10000", "11110", "10001", "10001", "01110"};
        case '7': return {"11111", "00001", "00010", "00100", "01000", "01000", "01000"};
        case '8': return {"01110", "10001", "10001", "01110", "10001", "10001", "01110"};
        case '9': return {"01110", "10001", "10001", "01111", "00001", "00001", "01110"};
        case ':': return {"00000", "00100", "00100", "00000", "00100", "00100", "00000"};
        case '.': return {"00000", "00000", "00000", "00000", "00000", "01100", "01100"};
        case ',': return {"00000", "00000", "00000", "00000", "00100", "00100", "01000"};
        case '-': return {"00000", "00000", "00000", "11111", "00000", "00000", "00000"};
        case '+': return {"00000", "00100", "00100", "11111", "00100", "00100", "00000"};
        case '=': return {"00000", "00000", "11111", "00000", "11111", "00000", "00000"};
        case '/': return {"00001", "00010", "00010", "00100", "01000", "01000", "10000"};
        case '(': return {"00010", "00100", "01000", "01000", "01000", "00100", "00010"};
        case ')': return {"01000", "00100", "00010", "00010", "00010", "00100", "01000"};
        case '[': return {"01110", "01000", "01000", "01000", "01000", "01000", "01110"};
        case ']': return {"01110", "00010", "00010", "00010", "00010", "00010", "01110"};
        case '<': return {"00010", "00100", "01000", "10000", "01000", "00100", "00010"};
        case '>': return {"01000", "00100", "00010", "00001", "00010", "00100", "01000"};
        default: return {"00000", "00000", "00000", "00000", "00000", "00000", "00000"};
    }
}

void append_text(memory::FrameVector<Vertex>& out,
                 std::string_view text,
                 Vec2 top_left,
                 f32 size,
                 Vec4 color) {
    const f32 pixel = size / 7.f;
    const f32 advance = pixel * 6.f;
    f32 cursor_x = top_left.x;
    for (char raw : text) {
        const char c = raw >= 'a' && raw <= 'z' ? static_cast<char>(raw - 'a' + 'A') : raw;
        if (c == ' ') {
            cursor_x += advance;
            continue;
        }
        const auto rows = glyph_rows(c);
        for (std::size_t row = 0; row < rows.size(); ++row) {
            for (std::size_t col = 0; col < std::char_traits<char>::length(rows[row]); ++col) {
                if (rows[row][col] != '1') continue;
                const Vec2 min{cursor_x + static_cast<f32>(col) * pixel,
                               top_left.y - static_cast<f32>(row + 1u) * pixel};
                push_quad(out, min, Vec2{min.x + pixel * 0.82f, min.y + pixel * 0.82f}, color);
            }
        }
        cursor_x += advance;
    }
}

void append_panel_label(memory::FrameVector<Vertex>& labels,
                        std::string_view title,
                        Vec2 panel_min,
                        Vec4 color = label_color()) {
    append_text(labels, title, Vec2{panel_min.x + 0.024f, panel_min.y - 0.030f}, k_panel_text, color);
}

} // namespace

void append_integration_analytics_shell(memory::FrameVector<Vertex>& lines,
                                        memory::FrameVector<Vertex>& labels,
                                        const IntegrationAnalysisSnapshot& analysis) {
    const Vec2 outer_min{-0.96f, -0.94f};
    const Vec2 outer_max{0.96f, 0.94f};
    push_rect_lines(lines, outer_min, outer_max, header_line_color());
    push_line(lines, Vec2{-0.96f, 0.78f}, Vec2{0.96f, 0.78f}, header_line_color());

    append_text(labels, "INTEGRATION ANALYSIS", Vec2{-0.93f, 0.90f}, k_title_text, label_color());
    append_text(labels, std::format("REV {}", analysis.problem_revision), Vec2{0.70f, 0.90f}, k_body_text, muted_label_color());
    append_text(labels, "OBJECT", Vec2{-0.93f, 0.82f}, k_small_text, muted_label_color());
    append_text(labels, "ACTIVE PROBLEM", Vec2{-0.79f, 0.82f}, k_small_text, value_color());
    append_text(labels, "METHOD", Vec2{-0.43f, 0.82f}, k_small_text, muted_label_color());
    append_text(labels, "CURRENT", Vec2{-0.29f, 0.82f}, k_small_text, value_color());
    append_text(labels, "STATUS", Vec2{0.10f, 0.82f}, k_small_text, muted_label_color());
    append_text(labels, analysis.stale ? "STALE" : "LIVE SNAPSHOT", Vec2{0.24f, 0.82f}, k_small_text, value_color());

    const Vec2 result_min{-0.94f, 0.34f};
    const Vec2 result_max{-0.04f, 0.72f};
    const Vec2 convergence_min{0.02f, 0.34f};
    const Vec2 convergence_max{0.94f, 0.72f};
    const Vec2 comparison_min{-0.94f, -0.10f};
    const Vec2 comparison_max{-0.04f, 0.26f};
    const Vec2 contribution_min{0.02f, -0.10f};
    const Vec2 contribution_max{0.94f, 0.26f};
    const Vec2 microscope_min{-0.94f, -0.72f};
    const Vec2 microscope_max{-0.04f, -0.18f};
    const Vec2 diagnostics_min{0.02f, -0.72f};
    const Vec2 diagnostics_max{0.94f, -0.18f};

    for (const auto& panel : {
             std::pair{result_min, result_max},
             std::pair{convergence_min, convergence_max},
             std::pair{comparison_min, comparison_max},
             std::pair{contribution_min, contribution_max},
             std::pair{microscope_min, microscope_max},
             std::pair{diagnostics_min, diagnostics_max}}) {
        push_rect_lines(lines, panel.first, panel.second, panel_line_color());
        push_line(lines,
                  Vec2{panel.first.x, panel.second.y - 0.07f},
                  Vec2{panel.second.x, panel.second.y - 0.07f},
                  panel_line_color());
    }

    append_panel_label(labels, "RESULT SUMMARY", result_min);
    append_text(labels, "ESTIMATE", Vec2{-0.90f, 0.62f}, k_body_text, muted_label_color());
    append_text(labels, "REFERENCE", Vec2{-0.90f, 0.54f}, k_body_text, muted_label_color());
    append_text(labels, "ABS ERROR", Vec2{-0.90f, 0.46f}, k_body_text, muted_label_color());
    append_text(labels, "EVALUATIONS", Vec2{-0.90f, 0.38f}, k_body_text, muted_label_color());

    append_panel_label(labels, "CONVERGENCE", convergence_min);
    append_text(labels, "ERROR VS N", Vec2{0.07f, 0.62f}, k_body_text, muted_label_color());

    append_panel_label(labels, "METHOD COMPARISON", comparison_min);
    append_text(labels, "LEFT  RIGHT  MID  TRAP  SIMP", Vec2{-0.90f, 0.10f}, k_small_text, muted_label_color());

    append_panel_label(labels, "CONTRIBUTION DISTRIBUTION", contribution_min);
    append_text(labels, "CELL CONTRIBUTION", Vec2{0.07f, 0.10f}, k_small_text, muted_label_color());

    append_panel_label(labels, "SELECTED CELL MICROSCOPE", microscope_min);
    append_text(labels, "CELL", Vec2{-0.90f, -0.31f}, k_body_text, muted_label_color());
    append_text(labels, "INTERVAL", Vec2{-0.90f, -0.39f}, k_body_text, muted_label_color());
    append_text(labels, "SAMPLE", Vec2{-0.90f, -0.47f}, k_body_text, muted_label_color());
    append_text(labels, "CONTRIBUTION", Vec2{-0.90f, -0.55f}, k_body_text, muted_label_color());
    append_text(labels, analysis.selected_cell.valid ? std::format("{}", analysis.selected_cell.cell_id) : "-",
                Vec2{-0.56f, -0.31f}, k_body_text, value_color());

    append_panel_label(labels, "METADATA DIAGNOSTICS TRACE", diagnostics_min);
    append_text(labels, "WARNINGS", Vec2{0.07f, -0.31f}, k_body_text, muted_label_color());
    append_text(labels, "DOMAIN STATUS", Vec2{0.07f, -0.39f}, k_body_text, muted_label_color());
    append_text(labels, "METHOD STATUS", Vec2{0.07f, -0.47f}, k_body_text, muted_label_color());
    append_text(labels, "TRACE", Vec2{0.07f, -0.55f}, k_body_text, muted_label_color());
    append_text(labels, "NONE", Vec2{0.42f, -0.31f}, k_body_text, value_color());
    append_text(labels, "READY", Vec2{0.42f, -0.39f}, k_body_text, value_color());
    append_text(labels, "COMPATIBLE", Vec2{0.42f, -0.47f}, k_body_text, value_color());
    append_text(labels, "CURRENT SNAPSHOT", Vec2{0.42f, -0.55f}, k_body_text, value_color());
}

void append_integration_convergence_plot(memory::FrameVector<Vertex>& out,
                                         const IntegrationAnalysisSnapshot& analysis) {
    push_line(out, Vec2{0.10f, 0.42f}, Vec2{0.86f, 0.42f}, axis_color());
    push_line(out, Vec2{0.10f, 0.42f}, Vec2{0.10f, 0.64f}, axis_color());

    const auto& rows = analysis.convergence.rows;
    if (rows.size() < 2u) return;

    f64 max_error = f64(0);
    for (const auto& row : rows) {
        if (row.absolute_error) {
            max_error = std::max(max_error, *row.absolute_error);
        }
    }
    if (max_error <= f64(0)) return;

    Vec2 previous{};
    bool have_previous = false;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!rows[i].absolute_error) continue;
        const f32 x = 0.14f + static_cast<f32>(i) *
            (0.68f / static_cast<f32>(std::max<std::size_t>(rows.size() - 1u, 1u)));
        const f32 y = 0.45f + normalized_error(*rows[i].absolute_error, max_error) * 0.16f;
        const Vec2 current{x, y};
        if (have_previous) {
            push_line(out, previous, current, convergence_color());
        }
        push_line(out, Vec2{x - 0.018f, y}, Vec2{x + 0.018f, y}, convergence_color());
        push_line(out, Vec2{x, y - 0.018f}, Vec2{x, y + 0.018f}, convergence_color());
        previous = current;
        have_previous = true;
    }
}

void append_integration_method_comparison(memory::FrameVector<Vertex>& out,
                                          const IntegrationAnalysisSnapshot& analysis) {
    if (analysis.method_comparison.empty()) return;

    f64 max_abs_error = f64(0);
    for (const auto& row : analysis.method_comparison) {
        if (row.absolute_error) {
            max_abs_error = std::max(max_abs_error, *row.absolute_error);
        }
    }
    if (max_abs_error <= f64(0)) max_abs_error = f64(1);

    const f32 base_y = -0.04f;
    const f32 left = -0.86f;
    const f32 width = 0.16f;
    for (std::size_t i = 0; i < analysis.method_comparison.size(); ++i) {
        const auto& row = analysis.method_comparison[i];
        const f32 height = row.absolute_error
            ? std::max(0.025f, normalized_error(*row.absolute_error, max_abs_error) * 0.22f)
            : 0.04f;
        const f32 x0 = left + static_cast<f32>(i) * 0.24f;
        const Vec4 color = row.method == math::integration::IntegrationMethod2D::Midpoint
            ? comparison_midpoint_color()
            : comparison_trapezoid_color();
        push_quad(out, Vec2{x0, base_y}, Vec2{x0 + width, base_y + height}, color);
    }
}

void append_integration_contribution_distribution(memory::FrameVector<Vertex>& out,
                                                  const IntegrationAnalysisSnapshot& analysis) {
    if (!analysis.selected_cell.valid) {
        push_quad(out, Vec2{0.12f, -0.02f}, Vec2{0.84f, 0.00f}, contribution_color());
        return;
    }

    const f64 contribution = std::abs(analysis.selected_cell.contribution.contribution);
    const f32 height = std::clamp(static_cast<f32>(contribution), 0.04f, 0.24f);
    push_quad(out, Vec2{0.18f, -0.04f}, Vec2{0.36f, -0.04f + height}, contribution_color());
}

IntegrationLabAnalyticsRenderStats submit_integration_analytics_packets(
    RenderService& render,
    RenderViewId view,
    const IntegrationAnalysisSnapshot& analysis,
    memory::MemoryService* memory) {
    if (view == RenderViewId(0)) return {};

    memory::FrameVector<Vertex> convergence =
        memory ? memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};
    memory::FrameVector<Vertex> comparison =
        memory ? memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};
    memory::FrameVector<Vertex> contribution =
        memory ? memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};
    memory::FrameVector<Vertex> shell =
        memory ? memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};
    memory::FrameVector<Vertex> labels =
        memory ? memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};

    append_integration_analytics_shell(shell, labels, analysis);
    append_integration_convergence_plot(convergence, analysis);
    append_integration_method_comparison(comparison, analysis);
    append_integration_contribution_distribution(contribution, analysis);

    render.set_view_domain(view, RenderViewDomain{
        .u_min = -1.f,
        .u_max = 1.f,
        .v_min = -1.f,
        .v_max = 1.f,
        .z_min = -1.f,
        .z_max = 1.f
    });
    const Mat4 mvp = glm::ortho(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);
    render.submit(view, shell, Topology::LineList, DrawMode::VertexColor, panel_line_color(), mvp);
    render.submit(view, labels, Topology::TriangleList, DrawMode::VertexColor, label_color(), mvp);
    render.submit(view, convergence, Topology::LineList, DrawMode::VertexColor, convergence_color(), mvp);
    render.submit(view, comparison, Topology::TriangleList, DrawMode::VertexColor, comparison_midpoint_color(), mvp);
    render.submit(view, contribution, Topology::TriangleList, DrawMode::VertexColor, contribution_color(), mvp);

    return IntegrationLabAnalyticsRenderStats{
        .shell_vertices = static_cast<u64>(shell.size()),
        .label_vertices = static_cast<u64>(labels.size()),
        .convergence_vertices = static_cast<u64>(convergence.size()),
        .comparison_vertices = static_cast<u64>(comparison.size()),
        .contribution_vertices = static_cast<u64>(contribution.size())
    };
}

} // namespace ndde
