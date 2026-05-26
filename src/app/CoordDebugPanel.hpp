#pragma once
// app/CoordDebugPanel.hpp
// Standalone coordinate-space diagnostic panel.
// Shows every coordinate layer (GLFW, ImGui, Vulkan, Viewport, World, Hover)
// with live current + 120-frame latched snapshots for comparison.

#include "app/Viewport.hpp"
#include "app/HoverResult.hpp"
#include "math/Scalars.hpp"
#include "memory/Containers.hpp"
#include "numeric/ops.hpp"

#include <imgui.h>
#include <string>
#include <utility>
#include <cmath>

namespace ndde {

struct ConicEntry;

class CoordDebugPanel {
public:
    static constexpr int SAMPLE_INTERVAL = 120;

    struct Snapshot {
        // ImGui / display
        f32 imgui_display_w  = 0.f;
        f32 imgui_display_h  = 0.f;
        f32 imgui_fb_scale_x = 0.f;
        f32 imgui_fb_scale_y = 0.f;
        f32 mouse_px_x       = 0.f;
        f32 mouse_px_y       = 0.f;
        bool  mouse_in_window  = false; ///< false when ImGui sentinel (-FLT_MAX)
        bool  want_capture     = false;

        // Vulkan / framebuffer
        f32 fb_w = 0.f;
        f32 fb_h = 0.f;

        // Viewport camera
        f32 vp_dp_w        = 0.f;
        f32 vp_dp_h        = 0.f;
        f32 vp_fb_w        = 0.f;
        f32 vp_fb_h        = 0.f;
        f32 vp_pan_x       = 0.f;
        f32 vp_pan_y       = 0.f;
        f32 vp_zoom        = 0.f;
        f32 vp_base_extent = 0.f;
        f32 vp_half_h      = 0.f;
        f32 vp_half_w      = 0.f;
        f32 vp_fb_aspect   = 0.f;

        // World bounds
        f32 world_left   = 0.f;
        f32 world_right  = 0.f;
        f32 world_bottom = 0.f;
        f32 world_top    = 0.f;

        // Mouse → world → round-trip (only valid when mouse_in_window)
        f32 mouse_world_x    = 0.f;
        f32 mouse_world_y    = 0.f;
        f32 roundtrip_px_x   = 0.f;
        f32 roundtrip_px_y   = 0.f;
        f32 roundtrip_err_x  = 0.f;
        f32 roundtrip_err_y  = 0.f;

        // Hover / snap
        bool        snap_hit       = false;
        f32       snap_world_x   = 0.f;
        f32       snap_world_y   = 0.f;
        f32       snap_dist_px_x = 0.f;
        f32       snap_dist_px_y = 0.f;
        int         snap_curve_idx = -1;
        std::string snap_curve_name;

        // Curve snap cache sizes
        memory::PersistentVector<std::pair<std::string, int>> curve_cache_sizes;

        int frame = 0;
    };

    template<typename ConicsVec>
    void update(const Viewport& vp, const HoverResult& hover,
                const ConicsVec& conics, Vec2 fb_size) noexcept
    {
        ++m_frame_counter;
        m_current = build(vp, hover, conics, fb_size, m_frame_counter);
        if (m_frame_counter % SAMPLE_INTERVAL == 0) {
            m_last    = m_current;
            m_latched = true;
        }
    }

    void draw() {
        if (!m_visible) return;
        ImGui::SetNextWindowSize(ImVec2(540.f, 720.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(1300.f, 20.f),  ImGuiCond_FirstUseEver);
        ImGui::Begin("Coord Debug", &m_visible);

        draw_snapshot("Current  (live, frame " + std::to_string(m_current.frame) + ")",
                      m_current);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (m_latched)
            draw_snapshot("Last snapshot  (frame " + std::to_string(m_last.frame) + ")",
                          m_last);
        else
            ImGui::TextDisabled("Waiting for first %d-frame sample...", SAMPLE_INTERVAL);

        ImGui::End();
    }

    bool& visible() noexcept { return m_visible; }

private:
    Snapshot m_current;
    Snapshot m_last;
    bool     m_latched       = false;
    bool     m_visible       = true;
    int      m_frame_counter = 0;

    template<typename ConicsVec>
    static Snapshot build(const Viewport& vp, const HoverResult& hover,
                          const ConicsVec& conics, Vec2 fb_size, int frame) noexcept
    {
        const ImGuiIO& io = ImGui::GetIO();
        Snapshot s;
        s.frame = frame;

        s.imgui_display_w  = io.DisplaySize.x;
        s.imgui_display_h  = io.DisplaySize.y;
        s.imgui_fb_scale_x = io.DisplayFramebufferScale.x;
        s.imgui_fb_scale_y = io.DisplayFramebufferScale.y;
        s.mouse_px_x       = io.MousePos.x;
        s.mouse_px_y       = io.MousePos.y;
        s.mouse_in_window  = Viewport::mouse_valid(io.MousePos.x, io.MousePos.y);
        s.want_capture     = io.WantCaptureMouse;

        s.fb_w = fb_size.x;
        s.fb_h = fb_size.y;

        s.vp_dp_w        = vp.dp_w;
        s.vp_dp_h        = vp.dp_h;
        s.vp_fb_w        = vp.fb_w;
        s.vp_fb_h        = vp.fb_h;
        s.vp_pan_x       = vp.pan_x;
        s.vp_pan_y       = vp.pan_y;
        s.vp_zoom        = vp.zoom;
        s.vp_base_extent = vp.base_extent;
        s.vp_half_h      = vp.half_h();
        s.vp_half_w      = vp.half_w();
        s.vp_fb_aspect   = vp.fb_aspect();

        s.world_left   = vp.left();
        s.world_right  = vp.right();
        s.world_bottom = vp.bottom();
        s.world_top    = vp.top();

        // Only compute mouse→world when mouse is inside the window
        if (s.mouse_in_window) {
            const Vec2 mw         = vp.pixel_to_world(s.mouse_px_x, s.mouse_px_y);
            s.mouse_world_x       = mw.x;
            s.mouse_world_y       = mw.y;
            const Vec2 rt         = vp.world_to_pixel(mw.x, mw.y);
            s.roundtrip_px_x      = rt.x;
            s.roundtrip_px_y      = rt.y;
            s.roundtrip_err_x     = s.mouse_px_x - rt.x;
            s.roundtrip_err_y     = s.mouse_px_y - rt.y;
        }

        s.snap_hit       = hover.hit;
        s.snap_world_x   = hover.world_x;
        s.snap_world_y   = hover.world_y;
        s.snap_curve_idx = hover.curve_idx;
        if (hover.hit && hover.curve_idx >= 0 &&
            hover.curve_idx < static_cast<int>(conics.size()))
        {
            s.snap_curve_name = conics[hover.curve_idx].name;
            if (s.mouse_in_window) {
                const Vec2 sp    = vp.world_to_pixel(hover.world_x, hover.world_y);
                s.snap_dist_px_x = s.mouse_px_x - sp.x;
                s.snap_dist_px_y = s.mouse_px_y - sp.y;
            }
        }

        for (const auto& c : conics)
            s.curve_cache_sizes.emplace_back(c.name,
                                              static_cast<int>(c.snap_cache.size()));
        return s;
    }

    static void draw_snapshot(const std::string& title, const Snapshot& s) {
        if (!ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 1.0f, 1.f));
        ImGui::SeparatorText("ImGui / Display");
        ImGui::PopStyleColor();
        row("DisplaySize",  s.imgui_display_w, s.imgui_display_h, "logical px");
        row("FB scale",     s.imgui_fb_scale_x, s.imgui_fb_scale_y, "");
        if (s.mouse_in_window)
            row("MousePos", s.mouse_px_x, s.mouse_px_y, "logical px");
        else
            ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "  MousePos       OUT OF WINDOW (ImGui sentinel)");
        flag("WantCaptureMouse", s.want_capture);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.f));
        ImGui::SeparatorText("Vulkan / Framebuffer");
        ImGui::PopStyleColor();
        row("fb_size (engine)", s.fb_w, s.fb_h, "GPU px");
        match("vp.fb_w/h vs fb_w/h", s.vp_fb_w == s.fb_w && s.vp_fb_h == s.fb_h);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 1.0f, 0.6f, 1.f));
        ImGui::SeparatorText("Viewport Camera");
        ImGui::PopStyleColor();
        row("vp.dp_w/h",        s.vp_dp_w, s.vp_dp_h, "logical px");
        row("vp.fb_w/h",        s.vp_fb_w, s.vp_fb_h, "GPU px");
        match("dp matches DisplaySize",
              s.vp_dp_w == s.imgui_display_w && s.vp_dp_h == s.imgui_display_h);
        scalar("fb_aspect",   s.vp_fb_aspect);
        scalar("zoom",        s.vp_zoom);
        scalar("base_extent", s.vp_base_extent);
        row("half_w / half_h", s.vp_half_w, s.vp_half_h, "world");
        row("pan",             s.vp_pan_x, s.vp_pan_y, "world");

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.8f, 1.f));
        ImGui::SeparatorText("World Bounds");
        ImGui::PopStyleColor();
        row("L / R",   s.world_left,   s.world_right,  "world");
        row("B / T",   s.world_bottom, s.world_top,    "world");
        scalar("span X", s.world_right  - s.world_left);
        scalar("span Y", s.world_top    - s.world_bottom);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.5f, 1.f));
        ImGui::SeparatorText("Mouse -> World -> Round-trip");
        ImGui::PopStyleColor();
        if (s.mouse_in_window) {
            row("mouse logical px",      s.mouse_px_x,    s.mouse_px_y,    "logical px");
            row("pixel_to_world(mouse)", s.mouse_world_x, s.mouse_world_y, "world");
            row("world_to_pixel(above)", s.roundtrip_px_x, s.roundtrip_px_y, "logical px");
            row_err("round-trip error",  s.roundtrip_err_x, s.roundtrip_err_y);
            ImGui::TextDisabled("  expected: pixel(L,T)=(0,0)  pixel(R,B)=(dp_w,dp_h)");
        } else {
            ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "  (mouse out of window — no valid transform)");
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.7f, 1.f));
        ImGui::SeparatorText("Hover / Snap");
        ImGui::PopStyleColor();
        if (s.snap_hit) {
            ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f),
                "  Snapped to: %s  [idx %d]",
                s.snap_curve_name.c_str(), s.snap_curve_idx);
            row("snap world", s.snap_world_x, s.snap_world_y, "world");
            if (s.mouse_in_window) {
                row_err("mouse-snap px offset", s.snap_dist_px_x, s.snap_dist_px_y);
                ImGui::TextDisabled("  (offset <= snap_px_radius is expected)");
            }
        } else {
            ImGui::TextDisabled("  No snap active");
        }

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.f));
        ImGui::SeparatorText("Curve Snap Cache Sizes");
        ImGui::PopStyleColor();
        for (const auto& [name, sz] : s.curve_cache_sizes)
            ImGui::TextDisabled("  %-20s  %d pts", name.c_str(), sz);
    }

    static void row(const char* label, f32 a, f32 b, const char* unit) {
        ImGui::TextDisabled("  %-30s", label);
        ImGui::SameLine(240.f);
        ImGui::Text("%-10.4f  %-10.4f  %s", a, b, unit);
    }

    static void scalar(const char* label, f32 v) {
        ImGui::TextDisabled("  %-30s", label);
        ImGui::SameLine(240.f);
        ImGui::Text("%.6f", v);
    }

    static void flag(const char* label, bool v) {
        ImGui::TextDisabled("  %-30s", label);
        ImGui::SameLine(240.f);
        ImGui::TextColored(v ? ImVec4(1,0.5f,0.5f,1) : ImVec4(0.5f,1,0.5f,1),
                           v ? "TRUE" : "false");
    }

    static void match(const char* label, bool ok) {
        ImGui::TextDisabled("  %-30s", label);
        ImGui::SameLine(240.f);
        ImGui::TextColored(ok ? ImVec4(0.5f,1,0.5f,1) : ImVec4(1,0.3f,0.3f,1),
                           ok ? "OK" : "MISMATCH  <---");
    }

    static void row_err(const char* label, f32 a, f32 b) {
        const f32 mag = ops::sqrt(a*a + b*b);
        const ImVec4 col = (mag < 0.5f)
            ? ImVec4(0.5f, 1.f, 0.5f, 1.f)
            : (mag < 2.f)
                ? ImVec4(1.f, 0.85f, 0.f, 1.f)
                : ImVec4(1.f, 0.3f,  0.3f, 1.f);
        ImGui::TextDisabled("  %-30s", label);
        ImGui::SameLine(240.f);
        ImGui::TextColored(col, "%-10.4f  %-10.4f  (mag %.3f px)", a, b, mag);
    }
};

} // namespace ndde
