#pragma once
// app/GoalStatusPanel.hpp
// Reusable display for simulation goal/win-condition state.

#include "app/SimulationPanelModels.hpp"

#include <imgui.h>

namespace ndde {

class GoalStatusPanel {
public:
    static void draw(const SimulationMetadata& metadata) {
        ImGui::SeparatorText("Goal");
        if (metadata.goal_succeeded) {
            ImGui::TextColored({0.4f, 1.f, 0.4f, 1.f}, "Succeeded");
        } else {
            ImGui::TextDisabled("Running");
        }
        ImGui::TextDisabled("status %s", metadata.status.c_str());
        ImGui::TextDisabled("particles %llu",
            static_cast<unsigned long long>(metadata.particle_count));
    }
};

} // namespace ndde
