#include "app/IntegrationLabRenderPackets.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace ndde {
namespace {

[[nodiscard]] Vec4 domain_color() noexcept { return Vec4{0.88f, 0.90f, 0.94f, 0.92f}; }
[[nodiscard]] Vec4 grid_color() noexcept { return Vec4{0.30f, 0.34f, 0.40f, 0.40f}; }
[[nodiscard]] Vec4 axis_2d_color() noexcept { return Vec4{0.72f, 0.72f, 0.76f, 0.56f}; }
[[nodiscard]] Vec4 sample_color() noexcept { return Vec4{1.00f, 0.78f, 0.24f, 0.95f}; }
[[nodiscard]] Vec4 selected_color() noexcept { return Vec4{1.00f, 0.88f, 0.24f, 1.00f}; }
[[nodiscard]] Vec4 hovered_color() noexcept { return Vec4{0.35f, 0.70f, 1.00f, 0.90f}; }

void push_line(memory::FrameVector<Vertex>& out, Vec2 a, Vec2 b, Vec4 color) {
    out.push_back(Vertex{.pos = Vec3{a.x, a.y, 0.f}, .color = color});
    out.push_back(Vertex{.pos = Vec3{b.x, b.y, 0.f}, .color = color});
}

void push_cell_outline(memory::FrameVector<Vertex>& out,
                       const math::integration::Cell2D& cell,
                       Vec4 color) {
    const Vec2 a{static_cast<f32>(cell.x0), static_cast<f32>(cell.y0)};
    const Vec2 b{static_cast<f32>(cell.x1), static_cast<f32>(cell.y0)};
    const Vec2 c{static_cast<f32>(cell.x1), static_cast<f32>(cell.y1)};
    const Vec2 d{static_cast<f32>(cell.x0), static_cast<f32>(cell.y1)};
    push_line(out, a, b, color);
    push_line(out, b, c, color);
    push_line(out, c, d, color);
    push_line(out, d, a, color);
}

void push_cell_quad(memory::FrameVector<Vertex>& out,
                    const math::integration::Cell2D& cell,
                    Vec4 color) {
    const Vec3 p00{static_cast<f32>(cell.x0), static_cast<f32>(cell.y0), 0.f};
    const Vec3 p10{static_cast<f32>(cell.x1), static_cast<f32>(cell.y0), 0.f};
    const Vec3 p11{static_cast<f32>(cell.x1), static_cast<f32>(cell.y1), 0.f};
    const Vec3 p01{static_cast<f32>(cell.x0), static_cast<f32>(cell.y1), 0.f};

    out.push_back(Vertex{.pos = p00, .color = color});
    out.push_back(Vertex{.pos = p10, .color = color});
    out.push_back(Vertex{.pos = p11, .color = color});
    out.push_back(Vertex{.pos = p00, .color = color});
    out.push_back(Vertex{.pos = p11, .color = color});
    out.push_back(Vertex{.pos = p01, .color = color});
}

[[nodiscard]] f64 max_display_abs(const IntegrationWorkbenchSnapshot& snapshot) noexcept {
    f64 max_abs = f64(0);
    for (const auto& cell : snapshot.renderable.cells) {
        max_abs = std::max(max_abs, std::abs(integration_display_scalar(cell, snapshot.renderable.display.mode)));
    }
    return max_abs;
}

[[nodiscard]] Mat4 integration_ortho(const math::integration::RectDomain2D& domain) {
    const f64 width = std::max(domain.width(), f64(1.0e-6));
    const f64 height = std::max(domain.height(), f64(1.0e-6));
    const f64 pad = std::max(width, height) * f64(0.04);
    return glm::ortho(static_cast<f32>(domain.x_min - pad),
                      static_cast<f32>(domain.x_max + pad),
                      static_cast<f32>(domain.y_min - pad),
                      static_cast<f32>(domain.y_max + pad),
                      -1.f,
                      1.f);
}

} // namespace

f64 integration_display_scalar(const math::integration::CellContribution2D& cell,
                               IntegrationDisplayMode mode) noexcept {
    switch (mode) {
        case IntegrationDisplayMode::Value: return cell.value;
        case IntegrationDisplayMode::Contribution: return cell.contribution;
        case IntegrationDisplayMode::LocalError: return cell.local_error_estimate;
    }
    return cell.contribution;
}

Vec4 integration_heat_color(f64 value, f64 max_abs) noexcept {
    const f32 t = max_abs > f64(0)
        ? static_cast<f32>(std::clamp(std::abs(value) / max_abs, f64(0), f64(1)))
        : 0.f;

    if (value < f64(0)) {
        return Vec4{0.23f + 0.12f * t, 0.48f + 0.18f * t, 0.92f, 0.20f + 0.62f * t};
    }
    return Vec4{0.18f + 0.82f * t, 0.50f + 0.25f * (1.f - t), 0.24f, 0.20f + 0.62f * t};
}

void append_integration_heatmap(memory::FrameVector<Vertex>& out,
                                const IntegrationWorkbenchSnapshot& snapshot) {
    const f64 max_abs = max_display_abs(snapshot);
    out.reserve(out.size() + snapshot.renderable.cells.size() * 6u);
    for (const auto& cell : snapshot.renderable.cells) {
        push_cell_quad(out,
                       cell.cell,
                       integration_heat_color(integration_display_scalar(cell, snapshot.renderable.display.mode),
                                              max_abs));
    }
}

void append_integration_lines(memory::FrameVector<Vertex>& out,
                              const IntegrationWorkbenchSnapshot& snapshot) {
    const auto& domain = snapshot.renderable.domain;
    const Vec2 bottom_left{static_cast<f32>(domain.x_min), static_cast<f32>(domain.y_min)};
    const Vec2 bottom_right{static_cast<f32>(domain.x_max), static_cast<f32>(domain.y_min)};
    const Vec2 top_right{static_cast<f32>(domain.x_max), static_cast<f32>(domain.y_max)};
    const Vec2 top_left{static_cast<f32>(domain.x_min), static_cast<f32>(domain.y_max)};

    if (snapshot.renderable.display.show_axes) {
        if (domain.y_min <= f64(0) && domain.y_max >= f64(0)) {
            push_line(out,
                      Vec2{static_cast<f32>(domain.x_min), 0.f},
                      Vec2{static_cast<f32>(domain.x_max), 0.f},
                      axis_2d_color());
        }
        if (domain.x_min <= f64(0) && domain.x_max >= f64(0)) {
            push_line(out,
                      Vec2{0.f, static_cast<f32>(domain.y_min)},
                      Vec2{0.f, static_cast<f32>(domain.y_max)},
                      axis_2d_color());
        }
    }

    if (snapshot.renderable.display.show_cells) {
        out.reserve(out.size() + snapshot.renderable.cells.size() * 8u);
        for (const auto& cell : snapshot.renderable.cells) {
            push_cell_outline(out, cell.cell, grid_color());
        }
    }

    if (snapshot.renderable.display.show_domain_boundary) {
        push_line(out, bottom_left, bottom_right, domain_color());
        push_line(out, bottom_right, top_right, domain_color());
        push_line(out, top_right, top_left, domain_color());
        push_line(out, top_left, bottom_left, domain_color());
    }

    if (snapshot.renderable.hovered_cell_id &&
        *snapshot.renderable.hovered_cell_id < snapshot.renderable.cells.size()) {
        push_cell_outline(out,
                          snapshot.renderable.cells[*snapshot.renderable.hovered_cell_id].cell,
                          hovered_color());
    }

    if (snapshot.renderable.selected_cell_id &&
        *snapshot.renderable.selected_cell_id < snapshot.renderable.cells.size()) {
        push_cell_outline(out,
                          snapshot.renderable.cells[*snapshot.renderable.selected_cell_id].cell,
                          selected_color());
    }
}

void append_integration_samples(memory::FrameVector<Vertex>& out,
                                const IntegrationWorkbenchSnapshot& snapshot) {
    if (!snapshot.renderable.display.show_samples) return;

    const f32 tick = static_cast<f32>(std::min(snapshot.renderable.domain.width(),
                                             snapshot.renderable.domain.height())) * 0.004f;
    out.reserve(out.size() + snapshot.renderable.cells.size() * 4u);
    for (const auto& cell : snapshot.renderable.cells) {
        const Vec2 p = cell.sample;
        push_line(out, Vec2{p.x - tick, p.y}, Vec2{p.x + tick, p.y}, sample_color());
        push_line(out, Vec2{p.x, p.y - tick}, Vec2{p.x, p.y + tick}, sample_color());
    }
}

IntegrationLabRenderStats submit_integration_workbench_packets(RenderService& render,
                                                               RenderViewId view,
                                                               const IntegrationWorkbenchSnapshot& snapshot,
                                                               memory::MemoryService* memory) {
    if (view == RenderViewId(0)) return {};
    memory::FrameVector<Vertex> heatmap =
        memory ? memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};
    memory::FrameVector<Vertex> lines =
        memory ? memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};
    memory::FrameVector<Vertex> samples =
        memory ? memory->frame().make_vector<Vertex>() : memory::FrameVector<Vertex>{};

    append_integration_heatmap(heatmap, snapshot);
    append_integration_lines(lines, snapshot);
    append_integration_samples(samples, snapshot);

    const auto& domain = snapshot.renderable.domain;
    render.set_view_domain(view, RenderViewDomain{
        .u_min = static_cast<f32>(domain.x_min),
        .u_max = static_cast<f32>(domain.x_max),
        .v_min = static_cast<f32>(domain.y_min),
        .v_max = static_cast<f32>(domain.y_max),
        .z_min = -1.f,
        .z_max = 1.f
    });

    const Mat4 mvp = integration_ortho(domain);
    render.submit(view,
                  heatmap,
                  Topology::TriangleList,
                  DrawMode::VertexColor,
                  Vec4{1.f, 1.f, 1.f, 1.f},
                  mvp);
    render.submit(view,
                  lines,
                  Topology::LineList,
                  DrawMode::VertexColor,
                  domain_color(),
                  mvp);
    render.submit(view,
                  samples,
                  Topology::LineList,
                  DrawMode::VertexColor,
                  sample_color(),
                  mvp);

    return IntegrationLabRenderStats{
        .heatmap_vertices = static_cast<u64>(heatmap.size()),
        .line_vertices = static_cast<u64>(lines.size()),
        .sample_vertices = static_cast<u64>(samples.size())
    };
}

} // namespace ndde
