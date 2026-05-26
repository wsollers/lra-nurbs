#pragma once
// app/AlternateViewPanel.hpp
// Shared controls for renderer-owned alternate mathematical views.

#include "engine/RenderService.hpp"

#include <imgui.h>

#include <array>

namespace ndde {

class AlternateViewPanel {
public:
    static void draw_main_overlays(RenderService& render, RenderViewId view) {
        RenderViewDescriptor* descriptor = render.descriptor(view);
        if (!descriptor || descriptor->kind != RenderViewKind::Main)
            return;

        ViewOverlayState& overlays = descriptor->overlays;
        ImGui::SeparatorText("Analysis overlays");
        ImGui::Checkbox("Frenet frame", &overlays.show_hover_frenet);
        ImGui::Checkbox("Darboux frame", &overlays.show_darboux_frame);
        ImGui::Checkbox("Osculating circle", &overlays.show_osculating_circle);
        ImGui::Checkbox("Diffusion ellipse", &overlays.show_diffusion_ellipse);
        ImGui::Checkbox("DDE ghost marker", &overlays.show_ghost_marker);
        ImGui::Checkbox("Metric ellipse", &overlays.show_metric_ellipse);
    }

    static void draw(RenderService& render, RenderViewId view) {
        RenderViewDescriptor* descriptor = render.descriptor(view);
        if (!descriptor || descriptor->kind != RenderViewKind::Alternate)
            return;

        ImGui::SeparatorText("Alternate view");
        const std::array<const char*, 5> modes{
            "Contour", "Level curves", "Vector field", "Isoclines", "Flow"
        };
        int mode = static_cast<int>(descriptor->alternate_mode);
        if (ImGui::Combo("Mode", &mode, modes.data(), static_cast<int>(modes.size())))
            descriptor->alternate_mode = static_cast<AlternateViewMode>(mode);

        AlternateViewSettings& settings = descriptor->alternate;
        switch (descriptor->alternate_mode) {
            case AlternateViewMode::Contour:
            case AlternateViewMode::LevelCurves:
                ImGui::TextDisabled("Uses cached scalar height contours");
                break;
            case AlternateViewMode::Isoclines:
                ImGui::SliderFloat("Direction", &settings.isocline_direction_angle, -3.14159f, 3.14159f, "%.2f rad");
                ImGui::SliderFloat("Target slope", &settings.isocline_target_slope, -4.f, 4.f, "%.2f");
                ImGui::SliderFloat("Tolerance", &settings.isocline_tolerance, 0.01f, 3.f, "%.2f");
                slider_u32("Bands", settings.isocline_bands, 1, 15);
                break;
            case AlternateViewMode::VectorField:
            case AlternateViewMode::Flow:
                draw_vector_settings(settings);
                if (descriptor->alternate_mode == AlternateViewMode::Flow) {
                    slider_u32("Seeds", settings.flow_seed_count, 3, 21);
                    slider_u32("Steps", settings.flow_steps, 4, 96);
                    ImGui::SliderFloat("Step size", &settings.flow_step_size, 0.02f, 0.5f, "%.2f");
                }
                break;
        }
    }

private:
    static void draw_vector_settings(AlternateViewSettings& settings) {
        const std::array<const char*, 4> vector_modes{
            "Gradient", "Negative gradient", "Level tangent", "Particle velocity"
        };
        int vector_mode = static_cast<int>(settings.vector_mode);
        if (ImGui::Combo("Vector field", &vector_mode, vector_modes.data(), static_cast<int>(vector_modes.size())))
            settings.vector_mode = static_cast<VectorFieldMode>(vector_mode);
        slider_u32("Density", settings.vector_samples, 4, 40);
        ImGui::SliderFloat("Scale", &settings.vector_scale, 0.1f, 4.f, "%.2f");
    }

    static void slider_u32(const char* label, u32& value, int min_value, int max_value) {
        int v = static_cast<int>(value);
        if (ImGui::SliderInt(label, &v, min_value, max_value))
            value = static_cast<u32>(v);
    }
};

} // namespace ndde
