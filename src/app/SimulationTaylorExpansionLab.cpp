#include "app/SimulationTaylorExpansionLab.hpp"

#include <cmath>
#include <imgui.h>

namespace ndde {
namespace {

[[nodiscard]] f32 factorial(u32 n) noexcept {
    f32 value = f32(1);
    for (u32 i = 2u; i <= n; ++i) {
        value *= static_cast<f32>(i);
    }
    return value;
}

[[nodiscard]] f32 exp_taylor(f32 center, int degree, f32 x) noexcept {
    const f32 dx = x - center;
    const f32 scale = std::exp(center);
    f32 sum = f32(0);
    f32 power = f32(1);
    for (int k = 0; k <= degree; ++k) {
        if (k > 0) {
            power *= dx;
        }
        sum += scale * power / factorial(static_cast<u32>(k));
    }
    return sum;
}

} // namespace

SimulationTaylorExpansionLab::SimulationTaylorExpansionLab(memory::MemoryService* memory)
    : m_memory(memory)
{}

void SimulationTaylorExpansionLab::on_register(SimulationHost& host) {
    (void)m_memory;
    m_panel_handles.add(host.panels().register_panel(PanelDescriptor{
        .title = "Lab - Taylor Expansion",
        .category = "Math Lab",
        .scope = PanelScope::Simulation,
        .first_use_pos = ImVec2(24.f, 48.f),
        .first_use_size = ImVec2(420.f, 300.f),
        .background_alpha = 0.90f,
        .draw_body = [this] { draw_control_panel(); }
    }));
}

void SimulationTaylorExpansionLab::on_start() {
    m_time = f32(0);
}

void SimulationTaylorExpansionLab::on_tick(const TickInfo& tick) {
    if (!tick.paused) {
        m_time = tick.time;
    }
}

void SimulationTaylorExpansionLab::on_stop() {
    m_panel_handles.clear();
}

SceneSnapshot SimulationTaylorExpansionLab::snapshot() const {
    return SceneSnapshot{
        .name = std::string(name()),
        .paused = false,
        .sim_time = m_time,
        .sim_speed = f32(1),
        .particle_count = u64(0),
        .status = "Taylor workbench stub"
    };
}

SimulationMetadata SimulationTaylorExpansionLab::metadata() const {
    return SimulationMetadata{
        .name = std::string(name()),
        .status = "Ready for Taylor approximation panels",
        .sim_time = m_time,
        .sim_speed = f32(1),
        .particle_count = u64(0),
        .paused = false
    };
}

void SimulationTaylorExpansionLab::draw_control_panel() {
    ImGui::SeparatorText("Taylor Expansion");
    ImGui::TextDisabled("Initial workbench shell. Function: exp(x).");
    ImGui::SliderInt("Degree", &m_degree, 0, 16);
    ImGui::SliderFloat("Expansion center", &m_center, -3.f, 3.f, "%.3f");
    ImGui::SliderFloat("Probe x", &m_probe, -4.f, 4.f, "%.3f");

    const f32 exact = std::exp(m_probe);
    const f32 estimate = exp_taylor(m_center, m_degree, m_probe);
    const f32 error = std::abs(exact - estimate);

    ImGui::SeparatorText("Readout");
    ImGui::Text("T_%d(%.3f) = %.8f", m_degree, m_probe, estimate);
    ImGui::Text("exp(%.3f) = %.8f", m_probe, exact);
    ImGui::Text("absolute error = %.8e", static_cast<double>(error));

    ImGui::SeparatorText("Next Panels");
    ImGui::BulletText("Approximation graph and error band");
    ImGui::BulletText("Remainder bound / convergence trace");
    ImGui::BulletText("Derivative and coefficient inspector");
}

} // namespace ndde
