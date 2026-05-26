#pragma once

#include "engine/RenderService.hpp"
#include "math/GeometryTypes.hpp"
#include "memory/MemoryService.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

namespace ndde {

inline void add_curve2d_line(memory::FrameVector<Vertex>& out, Vec2 a, Vec2 b, Vec4 color) {
    out.push_back(Vertex{.pos = {a.x, a.y, 0.f}, .color = color});
    out.push_back(Vertex{.pos = {b.x, b.y, 0.f}, .color = color});
}

inline void add_curve2d_wire_circle(memory::FrameVector<Vertex>& out,
                                    Vec2 center,
                                    f32 radius,
                                    Vec4 color,
                                    u32 segments = 56u) {
    if (radius <= 0.f || segments < 3u)
        return;
    constexpr f32 two_pi = 6.28318530717958647692f;
    for (u32 i = 0; i < segments; ++i) {
        const f32 a0 = two_pi * static_cast<f32>(i) / static_cast<f32>(segments);
        const f32 a1 = two_pi * static_cast<f32>(i + 1u) / static_cast<f32>(segments);
        const Vec2 q0{center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius};
        const Vec2 q1{center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius};
        add_curve2d_line(out, q0, q1, color);
    }
}

struct Curve2DHoverOverlayOptions {
    bool show_frenet = false;
    bool show_osculating_circle = false;
    bool show_velocity_arrow = false;
    bool show_delay_ghost = false;
    bool show_delay_cone = false;
    bool has_velocity = false;
    bool has_delay_ghost = false;
    Vec2 velocity{};
    Vec2 delay_ghost{};
    f32 delay_cone_radius = 0.f;
};

struct Curve2DHoverOverlayResult {
    memory::FrameVector<Vertex> vertices;
    bool snapped = false;
    std::size_t sample_index = 0u;
    Vec2 sample{};
    Vec2 tangent{1.f, 0.f};
    Vec2 normal{0.f, 1.f};
    f32 curvature = 0.f;
};

inline Curve2DHoverOverlayResult build_curve2d_hover_overlay(std::span<const Vec2> curve,
                                                             Vec2 hover,
                                                             const RenderViewDomain& domain,
                                                             const Curve2DHoverOverlayOptions& options,
                                                             memory::MemoryService* memory_service) {
    Curve2DHoverOverlayResult result{
        .vertices = memory_service ? memory_service->frame().make_vector<Vertex>()
                                   : memory::FrameVector<Vertex>{}
    };
    const bool wants_curve_snap = options.show_frenet || options.show_osculating_circle
        || options.show_velocity_arrow || options.show_delay_ghost || options.show_delay_cone;
    if (curve.size() < 4u || !wants_curve_snap)
        return result;

    const f32 span_u = std::max(domain.u_max - domain.u_min, 0.01f);
    const f32 span_v = std::max(domain.v_max - domain.v_min, 0.01f);
    const f32 diagonal = std::sqrt(span_u * span_u + span_v * span_v);
    const f32 snap_radius = diagonal * 0.055f;

    std::size_t best = 1u;
    f32 best_d2 = snap_radius * snap_radius;
    for (std::size_t i = 1u; i + 1u < curve.size(); ++i) {
        const Vec2 d = curve[i] - hover;
        const f32 d2 = d.x * d.x + d.y * d.y;
        if (d2 < best_d2) {
            best_d2 = d2;
            best = i;
        }
    }
    if (best_d2 >= snap_radius * snap_radius)
        return result;

    const Vec2 p0 = curve[best - 1u];
    const Vec2 p = curve[best];
    const Vec2 p2 = curve[best + 1u];
    Vec2 tangent = p2 - p0;
    const f32 tangent_len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
    if (tangent_len < 1.0e-6f)
        return result;
    tangent /= tangent_len;
    Vec2 normal{-tangent.y, tangent.x};

    const Vec2 d1 = (p2 - p0) * 0.5f;
    const Vec2 d2 = p2 - 2.f * p + p0;
    const f32 cross = d1.x * d2.y - d1.y * d2.x;
    const f32 speed2 = d1.x * d1.x + d1.y * d1.y;
    const f32 speed = std::sqrt(std::max(speed2, 0.f));
    const f32 kappa = speed > 1.0e-6f ? cross / (speed * speed * speed) : 0.f;
    if (kappa < 0.f)
        normal = -normal;

    result.snapped = true;
    result.sample_index = best;
    result.sample = p;
    result.tangent = tangent;
    result.normal = normal;
    result.curvature = kappa;

    const f32 axis_len = diagonal * 0.035f;
    if (options.show_frenet) {
        add_curve2d_line(result.vertices, p, p + tangent * axis_len, {1.f, 0.48f, 0.12f, 0.98f});
        add_curve2d_line(result.vertices, p, p + normal * axis_len, {0.1f, 1.f, 0.45f, 0.98f});
    }

    const f32 abs_kappa = std::abs(kappa);
    if (options.show_osculating_circle && abs_kappa > 1.0e-5f) {
        const f32 radius = std::min(1.f / abs_kappa, diagonal * 0.22f);
        add_curve2d_wire_circle(result.vertices, p + normal * radius, radius, {1.f, 0.92f, 0.18f, 0.74f});
    }

    if (options.show_velocity_arrow && options.has_velocity) {
        Vec2 velocity = options.velocity;
        const f32 len = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
        if (len > 1.0e-6f) {
            velocity /= len;
            const Vec2 tip = p + velocity * (axis_len * 1.45f);
            add_curve2d_line(result.vertices, p, tip, {0.25f, 0.75f, 1.f, 0.96f});
            const Vec2 left{-velocity.y, velocity.x};
            add_curve2d_line(result.vertices, tip, tip - velocity * (axis_len * 0.25f) + left * (axis_len * 0.12f), {0.25f, 0.75f, 1.f, 0.96f});
            add_curve2d_line(result.vertices, tip, tip - velocity * (axis_len * 0.25f) - left * (axis_len * 0.12f), {0.25f, 0.75f, 1.f, 0.96f});
        }
    }

    if (options.show_delay_ghost && options.has_delay_ghost) {
        const Vec2 ghost = options.delay_ghost;
        const f32 ghost_r = axis_len * 0.32f;
        add_curve2d_wire_circle(result.vertices, ghost, ghost_r, {1.f, 0.63f, 0.18f, 0.86f}, 24u);
        add_curve2d_line(result.vertices, p, ghost, {1.f, 0.63f, 0.18f, 0.72f});
    }

    if (options.show_delay_cone && options.delay_cone_radius > 0.f)
        add_curve2d_wire_circle(result.vertices, p, options.delay_cone_radius, {0.95f, 0.48f, 0.16f, 0.38f}, 48u);

    return result;
}

inline memory::FrameVector<Vertex> build_curve2d_frenet_hover_overlay(std::span<const Vec2> curve,
                                                                       Vec2 hover,
                                                                       const RenderViewDomain& domain,
                                                                       bool show_frenet,
                                                                       bool show_osculating_circle,
                                                                       memory::MemoryService* memory_service) {
    const Curve2DHoverOverlayOptions options{
        .show_frenet = show_frenet,
        .show_osculating_circle = show_osculating_circle
    };
    return std::move(build_curve2d_hover_overlay(curve, hover, domain, options, memory_service).vertices);
}

} // namespace ndde
