#pragma once
// app/SimulationControlPanel.hpp
// Reusable high-level simulation controls.

#include "app/SimulationPanelModels.hpp"

#include <imgui.h>

namespace ndde {

class SimulationControlPanel {
public:
    static void draw(const SimulationMetadata& metadata,
                     const SimulationControls& controls,
                     const SimulationCommandList* commands = nullptr) {
        ImGui::TextDisabled("%s", metadata.name.c_str());
        ImGui::TextDisabled("%s", metadata.surface_name.c_str());
        if (!metadata.surface_formula.empty())
            ImGui::TextDisabled("%s", metadata.surface_formula.c_str());

        ImGui::SeparatorText("State");
        ImGui::TextDisabled("t %.2f   particles %llu",
            metadata.sim_time,
            static_cast<unsigned long long>(metadata.particle_count));
        ImGui::TextDisabled("%s", metadata.status.c_str());

        if (controls.paused)
            ImGui::Checkbox("Paused", controls.paused);
        if (controls.sim_speed)
            ImGui::SliderFloat("Sim speed", controls.sim_speed, 0.05f, 5.f, "%.2f");
        if (controls.reset && ImGui::Button("Reset", ImVec2(-1.f, 0.f)))
            controls.reset();

        if (commands && !commands->commands.empty()) {
            ImGui::SeparatorText("Commands");
            for (const SimulationCommand& command : commands->commands) {
                if (!command.execute) continue;
                const std::string label = command.hotkey.empty()
                    ? command.label
                    : command.label + "  [" + command.hotkey + "]";
                if (ImGui::Button(label.c_str(), ImVec2(-1.f, 0.f)))
                    command.execute();
            }
        }
    }
};

} // namespace ndde
