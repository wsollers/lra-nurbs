#pragma once
// app/ProjectedParticleOverlay.hpp
// ImGui overlay for projected surface scenes: trails, heads, hover Frenet, osc circle.

#include "app/AnimatedCurve.hpp"
#include "app/ParticleFactory.hpp"
#include "memory/Containers.hpp"
#include "numeric/ops.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace ndde {

struct ProjectedParticleOverlayOptions {
    bool draw_trails = true;
    bool draw_heads = true;
    bool hover_enabled = true;
    bool show_frenet = true;
    bool show_osculating_circle = true;
    bool show_T = true;
    bool show_N = true;
    bool show_B = true;
    f32 snap_radius_px = 22.f;
    f32 frame_scale = 0.34f;
};

struct ProjectedParticleOverlayResult {
    bool snapped = false;
    int curve = -1;
    int trail_idx = -1;
};

namespace detail {

[[nodiscard]] inline ImU32 imgui_color(Vec4 c) noexcept {
    const auto u8 = [](f32 v) {
        return static_cast<int>(ops::clamp(v, 0.f, 1.f) * 255.f + 0.5f);
    };
    return IM_COL32(u8(c.r), u8(c.g), u8(c.b), u8(c.a));
}

inline void draw_projected_arrow(ImDrawList* dl,
                                 Vec3 origin,
                                 Vec3 dir,
                                 f32 length,
                                 ImU32 color,
                                 const auto& project) {
    const ImVec2 a = project(origin);
    const ImVec2 b = project(origin + dir * length);
    dl->AddLine(a, b, color, 2.0f);
    dl->AddCircleFilled(b, 3.0f, color, 10);
}

} // namespace detail

template <class ProjectFn>
[[nodiscard]] inline ProjectedParticleOverlayResult draw_projected_particle_overlay(
    ImDrawList* dl,
    const memory::SimVector<Particle>& particles,
    const ImVec2& cpos,
    const ImVec2& csz,
    ProjectFn project,
    const ProjectedParticleOverlayOptions& options = {})
{
    ProjectedParticleOverlayResult result;
    dl->PushClipRect(cpos, ImVec2(cpos.x + csz.x, cpos.y + csz.y), true);

    f32 best = options.snap_radius_px;
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse = io.MousePos;

    for (u32 ci = 0; ci < static_cast<u32>(particles.size()); ++ci) {
        const Particle& particle = particles[ci];
        const u32 n = particle.trail_size();

        if (options.draw_trails && n >= 2) {
            for (u32 i = 1; i < n; ++i) {
                const f32 age = static_cast<f32>(i) / static_cast<f32>(std::max(n - 1u, 1u));
                Vec4 color = AnimatedCurve::trail_colour(particle.role(), particle.colour_slot(), age);
                color.a = std::max(color.a, 0.58f);
                dl->AddLine(project(particle.trail_pt(i - 1)),
                            project(particle.trail_pt(i)),
                            detail::imgui_color(color),
                            particle.particle_role() == ParticleRole::Leader ? 2.1f : 1.6f);
            }
        }

        if (options.draw_heads) {
            const ImVec2 head = project(particle.head_world());
            const f32 radius = particle.particle_role() == ParticleRole::Leader ? 5.2f : 4.0f;
            dl->AddCircleFilled(head, radius, detail::imgui_color(particle.head_colour()), 18);
            dl->AddCircle(head, radius + 1.0f, IM_COL32(245, 245, 245, 160), 18, 1.1f);
        }

        if (options.hover_enabled) {
            for (u32 i = 0; i < n; ++i) {
                const ImVec2 p = project(particle.trail_pt(i));
                const f32 dx = p.x - mouse.x;
                const f32 dy = p.y - mouse.y;
                const f32 d = ops::sqrt(dx * dx + dy * dy);
                if (d < best) {
                    best = d;
                    result.snapped = true;
                    result.curve = static_cast<int>(ci);
                    result.trail_idx = static_cast<int>(i);
                }
            }
        }
    }

    if (result.snapped && options.show_frenet &&
        result.curve >= 0 && result.curve < static_cast<int>(particles.size()) &&
        result.trail_idx >= 0)
    {
        const Particle& particle = particles[static_cast<std::size_t>(result.curve)];
        const u32 idx = static_cast<u32>(result.trail_idx);
        if (idx > 0 && idx < particle.trail_size() - 1u) {
            const FrenetFrame fr = particle.frenet_at(idx);
            const Vec3 o = particle.trail_pt(idx);
            if (options.show_T)
                detail::draw_projected_arrow(dl, o, fr.T, options.frame_scale, IM_COL32(255, 95, 35, 245), project);
            if (options.show_N)
                detail::draw_projected_arrow(dl, o, fr.N, options.frame_scale, IM_COL32(60, 255, 95, 245), project);
            if (options.show_B)
                detail::draw_projected_arrow(dl, o, fr.B, options.frame_scale, IM_COL32(80, 150, 255, 245), project);

            if (options.show_osculating_circle && fr.kappa > 1e-5f) {
                const f32 R = 1.f / fr.kappa;
                const Vec3 centre = o + fr.N * R;
                constexpr u32 seg = 72;
                ImVec2 prev = project(centre + R * (-fr.N));
                for (u32 i = 1; i <= seg; ++i) {
                    const f32 th = ops::two_pi_v<f32> * static_cast<f32>(i) / static_cast<f32>(seg);
                    const Vec3 p = centre + R * (-ops::cos(th) * fr.N + ops::sin(th) * fr.T);
                    const ImVec2 cur = project(p);
                    dl->AddLine(prev, cur, IM_COL32(185, 95, 255, 215), 1.7f);
                    prev = cur;
                }
            }

            char buf[96]{};
            std::snprintf(buf, sizeof(buf), "k=%.4f  osc.r=%.3f", fr.kappa,
                          fr.kappa > 1e-5f ? 1.f / fr.kappa : 0.f);
            const ImVec2 p = project(o);
            dl->AddText(ImVec2(p.x + 10.f, p.y - 18.f), IM_COL32(235, 235, 235, 230), buf);
        }
    }

    dl->PopClipRect();
    return result;
}

} // namespace ndde
