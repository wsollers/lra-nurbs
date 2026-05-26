#pragma once
// app/SwarmRecipePanel.hpp
// Reusable controls for spawning named swarm recipes.

#include "app/ParticleInspectorPanel.hpp"
#include "app/ParticleSwarmFactory.hpp"
#include "app/SurfaceMeshCache.hpp"
#include "memory/Containers.hpp"

#include <imgui.h>
#include <algorithm>
#include <functional>
#include <string>

namespace ndde {

struct SwarmRecipeAction {
    const char* label = "";
    const char* hotkey = "";
    std::function<SwarmBuildResult()> spawn;
};

struct SwarmRecipePanelState {
    bool* paused = nullptr;
    f32* sim_speed = nullptr;
    u32* grid_lines = nullptr;
    f32* color_scale = nullptr;
    bool* show_frenet = nullptr;
    bool* show_osculating_circle = nullptr;
    SurfaceMeshCache* mesh = nullptr;
};

struct SwarmRecipePanelOptions {
    const char* surface_formula = nullptr;
    const char* grid_label = "Grid";
    const char* color_scale_label = "Color scale";
    int min_grid = 12;
    int max_grid = 150;
    bool show_level_curve_controls = true;
    bool show_brownian_controls = true;
    bool show_trail_controls = true;
    bool show_sim_controls = true;
    bool show_particle_inspector = true;
    bool show_recipe_metadata = true;
    const char* goal_success_text = nullptr;
};

class SwarmRecipePanel {
public:
    static void draw(SwarmRecipePanelState state,
                     const memory::FrameVector<SwarmRecipeAction>& actions,
                     ParticleSystem& particles,
                     GoalStatus goal_status,
                     SwarmBuildResult* last_result,
                     const SwarmRecipePanelOptions& options = {})
    {
        if (options.surface_formula)
            ImGui::TextDisabled("%s", options.surface_formula);
        if (options.show_sim_controls && state.paused)
            ImGui::Checkbox("Paused  [Ctrl+P]", state.paused);
        if (options.show_sim_controls && state.sim_speed)
            ImGui::SliderFloat("Sim speed", state.sim_speed, 0.1f, 5.f, "%.2f");
        if (options.show_sim_controls && state.grid_lines) {
            int grid = static_cast<int>(*state.grid_lines);
            if (ImGui::SliderInt(options.grid_label, &grid, options.min_grid, options.max_grid)) {
                *state.grid_lines = static_cast<u32>(std::max(grid, options.min_grid));
                if (state.mesh) state.mesh->mark_dirty();
            }
        }
        if (options.show_sim_controls && state.color_scale)
            ImGui::SliderFloat(options.color_scale_label, state.color_scale, 0.2f, 4.f, "%.2f");
        if (options.show_sim_controls && state.show_frenet)
            ImGui::Checkbox("Hover Frenet  [Ctrl+F]", state.show_frenet);
        if (options.show_sim_controls && state.show_osculating_circle)
            ImGui::Checkbox("Osculating circle  [Ctrl+O]", state.show_osculating_circle);

        ImGui::SeparatorText("Recipes");
        for (const SwarmRecipeAction& action : actions) {
            if (!action.spawn) continue;
            const std::string label = action.hotkey && action.hotkey[0] != '\0'
                ? std::string(action.label) + "  [" + action.hotkey + "]"
                : std::string(action.label);
            if (ImGui::Button(label.c_str(), ImVec2(-1.f, 0.f)) && last_result)
                *last_result = action.spawn();
        }

        if (options.show_recipe_metadata && last_result && !last_result->empty()) {
            ImGui::SeparatorText("Last recipe");
            ImGui::TextDisabled("%s", last_result->metadata.family_name.c_str());
            ImGui::TextDisabled("count: %u", last_result->metadata.requested_count);
            ImGui::TextDisabled("roles: %s", last_result->metadata.roles_label().c_str());
            ImGui::TextDisabled("goals: %s", last_result->metadata.goals_added ? "yes" : "no");
        }

        if (goal_status == GoalStatus::Succeeded && options.goal_success_text)
            ImGui::TextColored({0.4f, 1.f, 0.4f, 1.f}, "%s", options.goal_success_text);

        if (options.show_particle_inspector) {
            ParticleInspectorPanel::draw(particles.particles(), ParticleInspectorOptions{
                .label = "Active particles",
                .show_level_curve_controls = options.show_level_curve_controls,
                .show_brownian_controls = options.show_brownian_controls,
                .show_trail_controls = options.show_trail_controls
            });
        }
    }
};

} // namespace ndde
