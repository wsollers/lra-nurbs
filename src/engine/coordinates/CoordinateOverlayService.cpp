#include "engine/coordinates/CoordinateOverlayService.hpp"

#include "math/Axes.hpp"
#include "math/numeric/Constants.hpp"

#include <algorithm>
#include <cmath>

namespace ndde {

namespace {

[[nodiscard]] f32 nice_grid_step(f32 visible_span) noexcept {
    const f32 safe_span = std::max(visible_span, 0.001f);
    const f32 raw_step = safe_span / 20.f;
    const f32 magnitude = std::pow(10.f, std::floor(std::log10(raw_step)));
    const f32 scaled = raw_step / magnitude;

    if (scaled <= 1.f) return magnitude;
    if (scaled <= 2.f) return 2.f * magnitude;
    if (scaled <= 5.f) return 5.f * magnitude;
    return 10.f * magnitude;
}

[[nodiscard]] f32 min_distance_to_rect_from_origin(CoordinateVisibleBounds2D bounds) noexcept {
    f32 dx = 0.f;
    if (bounds.right < 0.f) {
        dx = -bounds.right;
    } else if (bounds.left > 0.f) {
        dx = bounds.left;
    }

    f32 dy = 0.f;
    if (bounds.top < 0.f) {
        dy = -bounds.top;
    } else if (bounds.bottom > 0.f) {
        dy = bounds.bottom;
    }

    return std::sqrt(dx * dx + dy * dy);
}

void submit_cartesian_grid(RenderService& render,
                           memory::MemoryService& memory,
                           RenderViewId view,
                           CoordinateVisibleBounds2D bounds,
                           f32 minor_step,
                           f32 major_step,
                           Mat4 mvp) {
    const u32 grid_count = math::grid_vp_max_vertices(bounds.left,
                                                      bounds.right,
                                                      bounds.bottom,
                                                      bounds.top,
                                                      minor_step);
    auto grid_vertices = memory.frame().make_vector<Vertex>(grid_count);
    const u32 written = math::build_grid_viewport(grid_vertices,
                                                  bounds.left,
                                                  bounds.right,
                                                  bounds.bottom,
                                                  bounds.top,
                                                  minor_step,
                                                  major_step);
    grid_vertices.resize(written);
    render.submit(view,
                  grid_vertices,
                  Topology::LineList,
                  DrawMode::VertexColor,
                  Vec4{1.f, 1.f, 1.f, 1.f},
                  mvp);
}

void submit_cartesian_axes(RenderService& render,
                           memory::MemoryService& memory,
                           RenderViewId view,
                           CoordinateVisibleBounds2D bounds,
                           Mat4 mvp) {
    auto axis_vertices = memory.frame().make_vector<Vertex>(4u);
    axis_vertices[0] = Vertex{Vec3{bounds.left, 0.f, 0.f}, math::colors::X_AXIS};
    axis_vertices[1] = Vertex{Vec3{bounds.right, 0.f, 0.f}, math::colors::X_AXIS};
    axis_vertices[2] = Vertex{Vec3{0.f, bounds.bottom, 0.f}, math::colors::Y_AXIS};
    axis_vertices[3] = Vertex{Vec3{0.f, bounds.top, 0.f}, math::colors::Y_AXIS};

    render.submit(view,
                  axis_vertices,
                  Topology::LineList,
                  DrawMode::VertexColor,
                  Vec4{1.f, 1.f, 1.f, 1.f},
                  mvp);
}

void submit_polar_overlay(RenderService& render,
                          memory::MemoryService& memory,
                          RenderViewId view,
                          CoordinateVisibleBounds2D bounds,
                          f32 major_step,
                          bool show_rings,
                          bool show_spokes,
                          Mat4 mvp) {
    if (!show_rings && !show_spokes) return;

    constexpr u32 circle_segments = 144u;
    constexpr u32 spoke_count = 24u;
    const f32 min_radius = min_distance_to_rect_from_origin(bounds);
    const f32 max_radius = std::sqrt(
        std::max(bounds.left * bounds.left, bounds.right * bounds.right) +
        std::max(bounds.bottom * bounds.bottom, bounds.top * bounds.top));
    const u32 first_ring = std::max(1u, static_cast<u32>(std::floor(min_radius / major_step)));
    const u32 last_ring = static_cast<u32>(std::ceil(max_radius / major_step));
    const u32 ring_count = last_ring >= first_ring ? last_ring - first_ring + 1u : 0u;
    const u32 max_vertices =
        (show_rings ? ring_count * circle_segments * 2u : 0u) +
        (show_spokes ? spoke_count * 2u : 0u);
    auto polar_vertices = memory.frame().make_vector<Vertex>(max_vertices);

    const Vec4 polar_minor{0.16f, 0.22f, 0.32f, 1.f};
    const Vec4 polar_major{0.28f, 0.36f, 0.52f, 1.f};
    const Vec4 polar_spoke{0.22f, 0.30f, 0.42f, 1.f};
    u32 index = 0;
    const auto push = [&](Vec3 a, Vec3 b, Vec4 color) {
        polar_vertices[index++] = Vertex{a, color};
        polar_vertices[index++] = Vertex{b, color};
    };

    if (show_rings) {
        for (u32 ring = first_ring; ring <= last_ring; ++ring) {
            const f32 radius = major_step * static_cast<f32>(ring);
            const Vec4 color = ring % 5u == 0u ? polar_major : polar_minor;
            for (u32 segment = 0u; segment < circle_segments; ++segment) {
                const f32 a0 = (static_cast<f32>(segment) / static_cast<f32>(circle_segments)) *
                    numeric::two_pi<f32>;
                const f32 a1 = (static_cast<f32>(segment + 1u) / static_cast<f32>(circle_segments)) *
                    numeric::two_pi<f32>;
                push(Vec3{std::cos(a0) * radius, std::sin(a0) * radius, 0.f},
                     Vec3{std::cos(a1) * radius, std::sin(a1) * radius, 0.f},
                     color);
            }
        }
    }

    if (show_spokes) {
        for (u32 spoke = 0u; spoke < spoke_count; ++spoke) {
            const f32 angle = (static_cast<f32>(spoke) / static_cast<f32>(spoke_count)) *
                numeric::two_pi<f32>;
            push(Vec3{0.f, 0.f, 0.f},
                 Vec3{std::cos(angle) * max_radius, std::sin(angle) * max_radius, 0.f},
                 polar_spoke);
        }
    }

    polar_vertices.resize(index);
    render.submit(view,
                  polar_vertices,
                  Topology::LineList,
                  DrawMode::VertexColor,
                  Vec4{1.f, 1.f, 1.f, 1.f},
                  mvp);
}

void submit_axis_labels(TextOverlayService& text,
                        RenderViewId view,
                        CoordinateVisibleBounds2D bounds,
                        const AxisLabelConfig& labels) {
    ResolvedAxisLabels2D resolved = CoordinateOverlayService::resolve_axis_labels(bounds, labels);
    if (!resolved.show) return;
    const auto submit_resolved = [&](const TextDrawCommand& command) {
        text.submit(TextDrawRequest{
            .view = view,
            .space = command.space,
            .anchor = command.anchor,
            .position = command.position,
            .color = command.color,
            .size_px = command.size_px,
            .font = command.font,
            .text = command.text
        });
    };
    submit_resolved(resolved.x_axis);
    submit_resolved(resolved.y_axis);
}

} // namespace

CoordinateVisibleBounds2D CoordinateOverlayService::visible_bounds(RenderService& render,
                                                                   RenderViewId view) noexcept {
    const RenderViewDomain domain = render.view_domain(view);
    const RenderViewDescriptor* descriptor = render.descriptor(view);
    const CameraState camera = descriptor ? descriptor->camera : CameraState{};
    const f32 zoom = std::max(camera.zoom, 0.05f);
    const f32 half_u = 0.54f * (domain.u_max - domain.u_min) / zoom;
    const f32 half_v = 0.54f * (domain.v_max - domain.v_min) / zoom;
    return CoordinateVisibleBounds2D{
        .left = camera.target.x - half_u,
        .right = camera.target.x + half_u,
        .bottom = camera.target.y - half_v,
        .top = camera.target.y + half_v
    };
}

ResolvedAxisGradations CoordinateOverlayService::resolve_gradations(CoordinateVisibleBounds2D bounds,
                                                                    AxisGradationConfig config) noexcept {
    const f32 dynamic_step = nice_grid_step(std::max(bounds.width(), bounds.height()));
    const f32 minor_step = config.dynamic_steps
        ? dynamic_step
        : std::max(config.minor_step, 0.001f);
    const f32 major_step = config.dynamic_steps
        ? dynamic_step * 5.f
        : std::max(config.major_step, minor_step);
    return ResolvedAxisGradations{
        .minor_step = minor_step,
        .major_step = major_step
    };
}

ResolvedAxisLabels2D CoordinateOverlayService::resolve_axis_labels(CoordinateVisibleBounds2D bounds,
                                                                   const AxisLabelConfig& labels) {
    return ResolvedAxisLabels2D{
        .show = labels.show,
        .x_axis = TextDrawCommand{
            .view = RenderViewId(0),
            .space = TextCoordinateSpace::Domain,
            .anchor = TextAnchor::Center,
            .position = Vec2{bounds.right, 0.f},
            .color = math::colors::X_AXIS,
            .size_px = 14.f,
            .font = TextFontRole::Math,
            .text = labels.x
        },
        .y_axis = TextDrawCommand{
            .view = RenderViewId(0),
            .space = TextCoordinateSpace::Domain,
            .anchor = TextAnchor::Center,
            .position = Vec2{0.f, bounds.top},
            .color = math::colors::Y_AXIS,
            .size_px = 14.f,
            .font = TextFontRole::Math,
            .text = labels.y
        }
    };
}

void CoordinateOverlayService::submit(RenderService& render,
                                      TextOverlayService& text,
                                      memory::MemoryService& memory,
                                      RenderViewId view,
                                      const CoordinateOverlayDescriptor& descriptor,
                                      Mat4 mvp) {
    if (view == 0) return;

    const CoordinateVisibleBounds2D bounds = visible_bounds(render, view);
    const ResolvedAxisGradations gradations = resolve_gradations(bounds, descriptor.gradations);

    if (descriptor.show_grid) {
        submit_cartesian_grid(render, memory, view, bounds, gradations.minor_step, gradations.major_step, mvp);
    }
    if (descriptor.space == CoordinateSpaceKind::Polar2D ||
        descriptor.show_polar_rings ||
        descriptor.show_polar_spokes) {
        submit_polar_overlay(render,
                             memory,
                             view,
                             bounds,
                             gradations.major_step,
                             descriptor.space == CoordinateSpaceKind::Polar2D || descriptor.show_polar_rings,
                             descriptor.space == CoordinateSpaceKind::Polar2D || descriptor.show_polar_spokes,
                             mvp);
    }
    if (descriptor.show_axes) {
        submit_cartesian_axes(render, memory, view, bounds, mvp);
    }
    submit_axis_labels(text, view, bounds, descriptor.labels);
}

} // namespace ndde
