#pragma once
// app/PanelHost.hpp
// Small ImGui panel helper for scene-specific panels.

#include "memory/Containers.hpp"

#include <imgui.h>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace ndde {

struct PanelSpec {
    std::string title;
    ImVec2 default_pos{20.f, 20.f};
    ImVec2 default_size{300.f, 220.f};
    f32 bg_alpha = 0.88f;
    bool* visible = nullptr;
    std::function<void()> draw_body;
};

class PanelHost {
public:
    void clear() { m_panels.clear(); }

    void add(PanelSpec spec) {
        m_panels.push_back(std::move(spec));
    }

    void draw_all() {
        for (PanelSpec& panel : m_panels)
            draw(panel);
    }

    void draw_visibility_menu(std::string_view title = "Panels") {
        if (!ImGui::BeginMenu(title.data())) return;
        for (PanelSpec& panel : m_panels) {
            if (!panel.visible) continue;
            ImGui::MenuItem(panel.title.c_str(), nullptr, panel.visible);
        }
        ImGui::EndMenu();
    }

private:
    memory::ViewVector<PanelSpec> m_panels;

    static void draw(PanelSpec& panel) {
        if (panel.visible && !*panel.visible) return;
        ImGui::SetNextWindowPos(panel.default_pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(panel.default_size, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(panel.bg_alpha);
        bool open = panel.visible ? *panel.visible : true;
        if (!ImGui::Begin(panel.title.c_str(), panel.visible ? &open : nullptr)) {
            if (panel.visible) *panel.visible = open;
            ImGui::End();
            return;
        }
        if (panel.visible) *panel.visible = open;
        if (panel.draw_body) panel.draw_body();
        ImGui::End();
    }
};

} // namespace ndde
