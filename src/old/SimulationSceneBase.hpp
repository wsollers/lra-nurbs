#pragma once
// app/SimulationSceneBase.hpp
// Shared lifecycle state for interactive particle simulations.

#include "engine/IScene.hpp"
#include "app/HotkeyManager.hpp"
#include "app/ParticleGoals.hpp"
#include "app/ParticleSystem.hpp"
#include "memory/Containers.hpp"

#include <imgui.h>
#include <cstddef>
#include <string>

namespace ndde {

class SimulationSceneBase : public IScene {
public:
    void set_paused(bool paused) override { m_paused = paused; }
    [[nodiscard]] bool paused() const noexcept override { return m_paused; }

    [[nodiscard]] SceneSnapshot snapshot() const override {
        return SceneSnapshot{
            .name = std::string(name()),
            .paused = m_paused,
            .sim_time = m_sim_time,
            .sim_speed = m_sim_speed,
            .particle_count = m_snapshot_particle_count,
            .status = goal_status_label(m_goal_status),
            .particles = m_snapshot_particles
        };
    }

protected:
    float m_sim_time = 0.f;
    float m_sim_speed = 1.f;
    bool m_paused = false;
    bool m_show_hotkeys = false;
    GoalStatus m_goal_status = GoalStatus::Running;
    std::size_t m_snapshot_particle_count = 0;
    memory::FrameVector<ParticleSnapshot> m_snapshot_particles;

    void reset_simulation_clock() noexcept {
        m_sim_time = 0.f;
        m_goal_status = GoalStatus::Running;
    }

    void advance_particles(ParticleSystem& particles, f32 dt) {
        m_snapshot_particle_count = particles.size();
        m_snapshot_particles = particles.snapshot_particles();
        if (m_paused) return;
        m_sim_time += dt;
        particles.update(dt, m_sim_speed, m_sim_time);
        m_snapshot_particle_count = particles.size();
        m_snapshot_particles = particles.snapshot_particles();
        evaluate_goals(particles);
    }

    void evaluate_goals(ParticleSystem& particles) {
        m_goal_status = particles.evaluate_goals(m_sim_time);
        if (m_goal_status == GoalStatus::Succeeded)
            m_paused = true;
    }

    void draw_dockspace_root(const char* window_id, const char* dockspace_id) const {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::SetNextWindowViewport(vp->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        constexpr ImGuiWindowFlags dock_flags =
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        ImGui::Begin(window_id, nullptr, dock_flags);
        ImGui::PopStyleVar(3);
        ImGui::DockSpace(ImGui::GetID(dockspace_id), ImVec2(0.f, 0.f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }

    void draw_hotkey_panel(HotkeyManager& hotkeys) {
        hotkeys.draw_panel("Hotkeys  [Ctrl+H]", m_show_hotkeys);
    }

    void update_snapshot_particles(const ParticleSystem& particles) {
        m_snapshot_particle_count = particles.size();
        m_snapshot_particles = particles.snapshot_particles();
    }

private:
    [[nodiscard]] static const char* goal_status_label(GoalStatus status) noexcept {
        switch (status) {
            case GoalStatus::Running:   return "Running";
            case GoalStatus::Succeeded: return "Succeeded";
            case GoalStatus::Failed:    return "Failed";
        }
        return "Unknown";
    }
};

} // namespace ndde
