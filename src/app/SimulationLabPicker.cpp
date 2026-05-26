#include "app/SimulationLabPicker.hpp"

#include <imgui.h>

namespace ndde {
namespace {

constexpr std::size_t lab_picker_index = 0u;
constexpr std::size_t smoke_test_index = 1u;
constexpr std::size_t integration_lab_index = 2u;
constexpr std::size_t taylor_lab_index = 3u;

} // namespace

SimulationLabPicker::SimulationLabPicker(memory::MemoryService* memory,
                                         SwitchSimulationFn switch_simulation)
    : m_memory(memory)
    , m_switch_simulation(std::move(switch_simulation))
{}

void SimulationLabPicker::on_register(SimulationHost& host) {
    (void)m_memory;
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Lab Picker",
        .category = "Launcher",
        .scope = PanelScope::Simulation,
        .first_use_pos = ImVec2(24.f, 48.f),
        .first_use_size = ImVec2(440.f, 260.f),
        .background_alpha = 0.92f,
        .draw_body = [this] { draw_picker_panel(); }
    }));
}

void SimulationLabPicker::on_start() {
    m_time = f32(0);
}

void SimulationLabPicker::on_tick(const TickInfo& tick) {
    if (!tick.paused) {
        m_time = tick.time;
    }
}

void SimulationLabPicker::on_stop() {
    m_panel_handles.clear();
}

SceneSnapshot SimulationLabPicker::snapshot() const {
    return SceneSnapshot{
        .name = std::string(name()),
        .paused = false,
        .sim_time = m_time,
        .sim_speed = f32(1),
        .particle_count = u64(0),
        .status = "Choose a workbench"
    };
}

SimulationMetadata SimulationLabPicker::metadata() const {
    return SimulationMetadata{
        .name = std::string(name()),
        .status = "Launcher panel only",
        .sim_time = m_time,
        .sim_speed = f32(1),
        .particle_count = u64(0),
        .paused = false
    };
}

void SimulationLabPicker::draw_picker_panel() {
    ImGui::TextDisabled("Choose what to run.");
    ImGui::Separator();

    if (ImGui::Button("Smoke Test Sim", ImVec2(-1.f, 0.f))) {
        request_switch(smoke_test_index);
    }
    ImGui::TextDisabled("Current Vulkan/render-thread predator-prey simulation.");

    ImGui::Spacing();
    if (ImGui::Button("Integration Workbench", ImVec2(-1.f, 0.f))) {
        request_switch(integration_lab_index);
    }
    ImGui::TextDisabled("Current 1D integration and derivative lab slice.");

    ImGui::Spacing();
    if (ImGui::Button("Taylor Expansion Workbench", ImVec2(-1.f, 0.f))) {
        request_switch(taylor_lab_index);
    }
    ImGui::TextDisabled("Taylor approximation entry point for the analysis observatory.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Hotkeys: Ctrl+1 picker, Ctrl+2 smoke test, Ctrl+3 integration, Ctrl+4 Taylor.");
}

void SimulationLabPicker::request_switch(std::size_t index) {
    if (index == lab_picker_index) return;
    if (m_switch_simulation) {
        m_switch_simulation(index);
    }
}

} // namespace ndde
