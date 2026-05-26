#pragma once
// app/ProjectedSurfaceCanvas.hpp
// Shared ImGui projected surface canvas for analytic surface scenes.

#include "app/ProjectedParticleOverlay.hpp"
#include "app/ParticleSystem.hpp"
#include "app/SurfaceMeshCache.hpp"
#include "app/Viewport.hpp"
#include "engine/CanvasInput.hpp"
#include "numeric/ops.hpp"

#include <imgui.h>
#include <algorithm>

namespace ndde {

struct ProjectedSurfaceCanvasOptions {
    u32 grid_lines = 64;
    f32 color_scale = 1.f;
    Vec4 wire_color{0.92f, 0.96f, 1.f, 0.32f};
    SurfaceFillColorMode fill_color_mode = SurfaceFillColorMode::HeightCell;
    bool show_frenet = true;
    bool show_osculating_circle = true;
    f32 overlay_frame_scale = 0.34f;
    const char* canvas_id = "projected_surface_canvas";
    const char* help_text = nullptr;
    const char* subtitle = nullptr;
    bool paused = false;
};

class ProjectedSurfaceCanvas {
public:
    template <class Surface>
    static void draw(Surface& surface,
                     SurfaceMeshCache& mesh,
                     Viewport& viewport,
                     const ParticleSystem& particles,
                     const ProjectedSurfaceCanvasOptions& options)
    {
        const ImVec2 csz = ImGui::GetContentRegionAvail();
        const CanvasInputFrame canvas = begin_canvas_input(options.canvas_id, csz);
        const ImVec2 cpos = canvas.pos;
        viewport.fb_w = csz.x; viewport.fb_h = csz.y;
        viewport.dp_w = csz.x; viewport.dp_h = csz.y;

        if (canvas.hovered) {
            if (ops::abs(canvas.mouse_wheel) > 0.f)
                viewport.zoom = ops::clamp(viewport.zoom * (1.f + 0.12f * canvas.mouse_wheel), 0.05f, 20.f);
            if (canvas.orbit_drag)
                viewport.orbit(canvas.mouse_delta.x, canvas.mouse_delta.y);
        }

        rebuild(surface, mesh, options);
        draw_surface(mesh, viewport, surface.extent(), cpos, csz);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        [[maybe_unused]] const ProjectedParticleOverlayResult overlay =
            draw_projected_particle_overlay(dl, particles.particles(), cpos, csz,
                [&viewport, ext = surface.extent(), cpos, csz](Vec3 p) {
                    return projected_to_canvas(project_point(p, viewport), cpos, csz, ext);
                },
                ProjectedParticleOverlayOptions{
                    .hover_enabled = canvas.hovered,
                    .show_frenet = options.show_frenet,
                    .show_osculating_circle = options.show_osculating_circle,
                    .frame_scale = options.overlay_frame_scale
                });

        if (options.help_text)
            dl->AddText(ImVec2(cpos.x + 8.f, cpos.y + 6.f), IM_COL32(220, 220, 220, 190), options.help_text);
        if (options.subtitle)
            dl->AddText(ImVec2(cpos.x + 8.f, cpos.y + 24.f), IM_COL32(160, 210, 255, 180), options.subtitle);
        if (options.paused)
            dl->AddText(ImVec2(cpos.x + 8.f, cpos.y + (options.subtitle ? 42.f : 26.f)),
                        IM_COL32(255, 210, 60, 240), "PAUSED  [Ctrl+P]");
    }

    [[nodiscard]] static Vec3 project_point(Vec3 p, const Viewport& viewport) noexcept {
        const f32 cy = ops::cos(viewport.yaw);
        const f32 sy = ops::sin(viewport.yaw);
        const f32 cp = ops::cos(viewport.pitch);
        const f32 sp = ops::sin(viewport.pitch);
        const f32 xr = cy * p.x - sy * p.y;
        const f32 yr = sy * p.x + cy * p.y;
        const f32 screen_y = cp * yr - sp * p.z;
        const f32 inv_zoom = 1.f / ops::clamp(viewport.zoom, 0.05f, 20.f);
        return {xr * inv_zoom, screen_y * inv_zoom, (sp * yr + cp * p.z) * 0.01f};
    }

    [[nodiscard]] static ImVec2 projected_to_canvas(Vec3 p, const ImVec2& cpos, const ImVec2& csz, f32 extent) noexcept {
        const f32 aspect = csz.x / std::max(csz.y, 1.f);
        const f32 half_x = aspect >= 1.f ? extent * aspect : extent;
        const f32 half_y = aspect >= 1.f ? extent : extent / aspect;
        const f32 nx = (p.x + half_x) / (2.f * half_x);
        const f32 ny = 1.f - ((p.y + half_y) / (2.f * half_y));
        return {cpos.x + nx * csz.x, cpos.y + ny * csz.y};
    }

    [[nodiscard]] static ImU32 imgui_color(Vec4 c) noexcept {
        const auto u8 = [](f32 v) { return static_cast<int>(ops::clamp(v, 0.f, 1.f) * 255.f + 0.5f); };
        return IM_COL32(u8(c.r), u8(c.g), u8(c.b), u8(c.a));
    }

private:
    template <class Surface>
    static void rebuild(Surface& surface, SurfaceMeshCache& mesh, const ProjectedSurfaceCanvasOptions& options) {
        mesh.rebuild_if_needed(surface, SurfaceMeshOptions{
            .grid_lines = options.grid_lines,
            .time = 0.f,
            .color_scale = options.color_scale,
            .wire_color = options.wire_color,
            .fill_color_mode = options.fill_color_mode,
            .build_contour = true
        });
    }

    static void draw_surface(const SurfaceMeshCache& mesh, const Viewport& viewport, f32 extent,
                             const ImVec2& cpos, const ImVec2& csz)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(cpos, ImVec2(cpos.x + csz.x, cpos.y + csz.y), true);

        for (u32 i = 0; i + 2 < mesh.fill_count(); i += 3) {
            const Vertex& a = mesh.fill_vertices()[i];
            const Vertex& b = mesh.fill_vertices()[i + 1];
            const Vertex& c = mesh.fill_vertices()[i + 2];
            dl->AddTriangleFilled(
                projected_to_canvas(project_point(a.pos, viewport), cpos, csz, extent),
                projected_to_canvas(project_point(b.pos, viewport), cpos, csz, extent),
                projected_to_canvas(project_point(c.pos, viewport), cpos, csz, extent),
                imgui_color(a.color));
        }

        for (u32 i = 0; i + 1 < mesh.wire_count(); i += 2) {
            dl->AddLine(projected_to_canvas(project_point(mesh.wire_vertices()[i].pos, viewport), cpos, csz, extent),
                        projected_to_canvas(project_point(mesh.wire_vertices()[i + 1].pos, viewport), cpos, csz, extent),
                        imgui_color(mesh.wire_vertices()[i].color), 1.f);
        }

        dl->PopClipRect();
    }
};

} // namespace ndde
