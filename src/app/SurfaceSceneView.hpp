#pragma once

#include "app/ContourWindowRenderer.hpp"
#include "app/ParticleSystem.hpp"
#include "app/ProjectedSurfaceCanvas.hpp"
#include "app/SurfaceMeshCache.hpp"
#include "app/Viewport.hpp"
#include "engine/EngineAPI.hpp"

#include <imgui.h>

namespace ndde {

struct SurfaceSceneViewOptions {
    const char* window_title = "Surface 3D";
    ImVec2 default_pos{330.f, 20.f};
    ImVec2 default_size{850.f, 700.f};
    ProjectedSurfaceCanvasOptions canvas;
    ContourWindowOptions contour;
};

class SurfaceSceneView {
public:
    template <class Surface>
    void rebuild_geometry_if_needed(Surface& surface,
                                    SurfaceMeshCache& mesh,
                                    const ProjectedSurfaceCanvasOptions& options)
    {
        mesh.rebuild_if_needed(surface, SurfaceMeshOptions{
            .grid_lines = options.grid_lines,
            .time = 0.f,
            .color_scale = options.color_scale,
            .wire_color = options.wire_color,
            .fill_color_mode = options.fill_color_mode,
            .build_contour = true
        });
    }

    template <class Surface>
    void draw_canvas(Surface& surface,
                     SurfaceMeshCache& mesh,
                     Viewport& viewport,
                     const ParticleSystem& particles,
                     const SurfaceSceneViewOptions& options)
    {
        draw_canvas(surface, mesh, viewport, particles, options,
            [](ImDrawList&, ImVec2, ImVec2) {});
    }

    template <class Surface, class OverlayFn>
    void draw_canvas(Surface& surface,
                     SurfaceMeshCache& mesh,
                     Viewport& viewport,
                     const ParticleSystem& particles,
                     const SurfaceSceneViewOptions& options,
                     OverlayFn&& overlay)
    {
        ImGui::SetNextWindowPos(options.default_pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(options.default_size, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.f);
        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground;
        ImGui::Begin(options.window_title, nullptr, flags);

        ProjectedSurfaceCanvas::draw(surface, mesh, viewport, particles, options.canvas);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 cpos = ImGui::GetItemRectMin();
        const ImVec2 csz = ImGui::GetItemRectSize();
        overlay(*dl, cpos, csz);

        ImGui::End();
    }

    template <class Surface>
    void submit_contour(EngineAPI& api,
                        Surface& surface,
                        SurfaceMeshCache& mesh,
                        const ParticleSystem& particles,
                        const SurfaceSceneViewOptions& options)
    {
        rebuild_geometry_if_needed(surface, mesh, options.canvas);
        ContourWindowOptions contour = options.contour;
        contour.extent = surface.extent();
        submit_contour_window(api, mesh, particles, contour);
    }
};

} // namespace ndde
