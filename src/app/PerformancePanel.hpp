#pragma once
// app/PerformancePanel.hpp
// Header-only debug statistics window with 120-frame sparkline history.
//
// Metrics tracked:
//   FPS / frame-time (ms)
//   Arena utilisation (%, bytes, vertex count)
//   Draw call count
//   Swapchain resolution
//
// Each metric has its own ring buffer.  All five buffers advance together
// at the end of draw() — exactly one slot per frame regardless of visibility.
// This ensures sparkline history is correct even when the window is closed.
//
// Sparklines are bar charts drawn with raw ImDrawList calls.  Bars are
// colour-coded green → yellow → red based on whether the value is in a
// healthy range for that metric.
//
// Usage (in Scene):
//   PerformancePanel m_perf_panel;          // member
//   m_perf_panel.draw(m_api.debug_stats()); // in on_frame()

#include <imgui.h>
#include "engine/EngineAPI.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace ndde {

class PerformancePanel {
public:
    static constexpr int k_history = 120;  ///< frames of ring-buffer history

    bool& visible() noexcept { return m_visible; }

    // Called once per frame from Scene::on_frame().
    // Pushes stats into ring buffers AND renders the window if visible.
    // Always advances the ring buffer write-head — history accumulates
    // correctly even while the window is closed.
    void draw(const DebugStats& s) {
        // Push all metrics into the current ring slot
        push(m_fps_buf,        s.fps);
        push(m_ms_buf,         s.frame_ms);
        push(m_arena_pct_buf,  s.arena_utilisation * 100.f);
        push(m_draw_calls_buf, static_cast<f32>(s.draw_calls));
        push(m_vertices_buf,   static_cast<f32>(s.arena_vertex_count));

        if (m_visible) render(s);

        // Advance write head once per frame so all five buffers stay in sync
        m_head = (m_head + 1) % k_history;
    }

private:
    bool m_visible = false;
    int  m_head    = 0;  // slot being written this frame (oldest after advance)

    std::array<f32, k_history> m_fps_buf        {};
    std::array<f32, k_history> m_ms_buf         {};
    std::array<f32, k_history> m_arena_pct_buf  {};
    std::array<f32, k_history> m_draw_calls_buf {};
    std::array<f32, k_history> m_vertices_buf   {};

    // ── Ring buffer ───────────────────────────────────────────────────────────

    void push(std::array<f32, k_history>& buf, f32 v) noexcept {
        buf[static_cast<std::size_t>(m_head)] = v;
    }

    // ── Rendering ─────────────────────────────────────────────────────────────

    void render(const DebugStats& s) {
        ImGui::SetNextWindowSize(ImVec2(380.f, 0.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(340.f, 20.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.88f);
        if (!ImGui::Begin("Performance", &m_visible)) { ImGui::End(); return; }

        // ── Timing ────────────────────────────────────────────────────────────
        ImGui::SeparatorText("Timing");
        sparkline("FPS",        m_fps_buf,        "%.1f",  s.fps,
                  /*hi_good*/true,   0.f, 200.f);
        sparkline("Frame (ms)", m_ms_buf,         "%.2f",  s.frame_ms,
                  /*hi_good*/false,  0.f,  33.f);

        // ── Arena ─────────────────────────────────────────────────────────────
        ImGui::SeparatorText("Arena");
        {
            // Filled utilisation bar spanning the full panel width
            const f32 pct    = s.arena_utilisation;
            const f32 bar_w  = ImGui::GetContentRegionAvail().x;
            const f32 bar_h  = 12.f;
            const ImVec2 p     = ImGui::GetCursorScreenPos();
            ImDrawList*  dl    = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p, {p.x + bar_w,       p.y + bar_h}, IM_COL32(35,35,40,220), 3.f);
            dl->AddRectFilled(p, {p.x + bar_w * pct, p.y + bar_h}, to_u32(health(pct, false, 0.f, 1.f)), 3.f);
            dl->AddRect      (p, {p.x + bar_w,       p.y + bar_h}, IM_COL32(70,70,80,200), 3.f);
            ImGui::Dummy({bar_w, bar_h});
            const f32 used_mb  = static_cast<f32>(s.arena_bytes_used)  / (1024.f*1024.f);
            const f32 total_mb = static_cast<f32>(s.arena_bytes_total) / (1024.f*1024.f);
            ImGui::TextDisabled("  %.2f / %.0f MiB  (%.1f%%)", used_mb, total_mb, pct*100.f);
        }
        sparkline("Arena %%",   m_arena_pct_buf,  "%.1f%%",
                  s.arena_utilisation * 100.f, false, 0.f, 100.f);
        sparkline("Vertices",   m_vertices_buf,   "%.0f",
                  static_cast<f32>(s.arena_vertex_count), false, 0.f, 30000.f);

        // ── Renderer ──────────────────────────────────────────────────────────
        ImGui::SeparatorText("Renderer");
        sparkline("Draw calls", m_draw_calls_buf, "%.0f",
                  static_cast<f32>(s.draw_calls), false, 0.f, 150.f);

        // ── Swapchain ─────────────────────────────────────────────────────────
        ImGui::SeparatorText("Swapchain");
        ImGui::TextDisabled("  %u \xc3\x97 %u px", s.swapchain_w, s.swapchain_h); // UTF-8 ×

        ImGui::End();
    }

    // ── Colour helpers ────────────────────────────────────────────────────────
    // hi_good=true  → high value = green  (e.g. FPS — want high)
    // hi_good=false → low  value = green  (e.g. frame-ms, arena % — want low)

    static ImVec4 health(f32 v, bool hi_good, f32 lo, f32 hi) noexcept {
        const f32 t = (hi > lo) ? std::clamp((v - lo) / (hi - lo), 0.f, 1.f) : 0.f;
        // g is the "greenness" factor in [0,1]
        const f32 g = hi_good ? t : (1.f - t);
        // Map g → RGB:  g=1 → (0.20, 0.85, 0.20) green
        //               g=0.5 → (0.85, 0.75, 0.05) yellow
        //               g=0 → (0.85, 0.20, 0.20) red
        const f32 r  = (g >= 0.5f) ? (0.85f - (g - 0.5f) * 1.3f) : 0.85f;
        const f32 g2 = (g >= 0.5f) ? 0.85f : (0.20f + g * 1.1f);
        return ImVec4(r, g2, 0.20f, 1.f);
    }

    static ImU32 to_u32(ImVec4 c) noexcept {
        return IM_COL32(
            static_cast<int>(std::clamp(c.x, 0.f, 1.f) * 255.f),
            static_cast<int>(std::clamp(c.y, 0.f, 1.f) * 255.f),
            static_cast<int>(std::clamp(c.z, 0.f, 1.f) * 255.f),
            static_cast<int>(std::clamp(c.w, 0.f, 1.f) * 255.f));
    }

    // ── Sparkline ─────────────────────────────────────────────────────────────
    // Draws label + bar-chart sparkline + current-value readout on one row.
    // Tooltip shows min/avg/max over the ring buffer on hover.

    void sparkline(const char* label,
                   const std::array<f32, k_history>& buf,
                   const char* fmt, f32 current,
                   bool hi_good, f32 scale_lo, f32 scale_hi)
    {
        constexpr f32 k_h       = 26.f;  // sparkline height in pixels
        constexpr f32 k_label_w = 90.f;  // label column width
        constexpr f32 k_val_w   = 64.f;  // value column width

        const f32 avail   = ImGui::GetContentRegionAvail().x;
        const f32 spark_w = avail - k_label_w - k_val_w - 6.f;
        if (spark_w < 4.f) return;

        // Label column
        ImGui::TextDisabled("  %s", label);
        ImGui::SameLine(k_label_w);

        // Reserve pixel space for the sparkline
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList*  dl     = ImGui::GetWindowDrawList();

        // Background track
        dl->AddRectFilled(origin, {origin.x + spark_w, origin.y + k_h},
                          IM_COL32(28, 28, 33, 220), 3.f);

        // Find observed max for bar height normalisation
        f32 buf_max = scale_hi;
        for (f32 v : buf) buf_max = std::max(buf_max, v);
        if (buf_max < 1e-6f) buf_max = 1.f;

        const f32 bar_w = spark_w / static_cast<f32>(k_history);

        // Bars, oldest-first (chronological left→right)
        for (int i = 0; i < k_history; ++i) {
            const int   slot = (m_head + i) % k_history;  // oldest slot = m_head (just overwritten)
            const f32 v    = buf[static_cast<std::size_t>(slot)];
            const f32 frac = std::clamp(v / buf_max, 0.f, 1.f);
            const f32 h    = frac * k_h;
            if (h < 0.5f) continue;

            const f32 x0 = origin.x + static_cast<f32>(i) * bar_w;
            const f32 x1 = x0 + std::max(bar_w - 1.f, 1.f);
            dl->AddRectFilled({x0, origin.y + k_h - h}, {x1, origin.y + k_h},
                              to_u32(health(v, hi_good, scale_lo, scale_hi)));
        }

        // Border
        dl->AddRect(origin, {origin.x + spark_w, origin.y + k_h},
                    IM_COL32(55, 55, 65, 200), 3.f);

        ImGui::Dummy({spark_w, k_h});
        ImGui::SameLine();

        // Value readout, colour-coded
        char val_str[32];
        std::snprintf(val_str, sizeof(val_str), fmt, current);
        ImGui::TextColored(health(current, hi_good, scale_lo, scale_hi), "%s", val_str);

        // Tooltip: min / avg / max over history
        const ImVec2 spark_br = {origin.x + spark_w, origin.y + k_h};
        if (ImGui::IsMouseHoveringRect(origin, spark_br)) {
            f32 sum = 0.f, mn = buf[0], mx = buf[0];
            for (f32 v : buf) { sum += v; mn = std::min(mn,v); mx = std::max(mx,v); }
            const f32 avg = sum / static_cast<f32>(k_history);

            char mn_s[32], avg_s[32], mx_s[32];
            std::snprintf(mn_s,  sizeof(mn_s),  fmt, mn);
            std::snprintf(avg_s, sizeof(avg_s), fmt, avg);
            std::snprintf(mx_s,  sizeof(mx_s),  fmt, mx);

            ImGui::BeginTooltip();
            ImGui::TextColored(health(mn,  hi_good, scale_lo, scale_hi), "min %s", mn_s);
            ImGui::SameLine(90.f);
            ImGui::TextColored(health(avg, hi_good, scale_lo, scale_hi), "avg %s", avg_s);
            ImGui::SameLine(180.f);
            ImGui::TextColored(health(mx,  hi_good, scale_lo, scale_hi), "max %s", mx_s);
            ImGui::EndTooltip();
        }
    }
};

} // namespace ndde
