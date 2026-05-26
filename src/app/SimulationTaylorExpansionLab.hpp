#pragma once
// app/SimulationTaylorExpansionLab.hpp
// Minimal Taylor workbench entry point. The numerical/display panels will grow here.

#include "engine/ISimulation.hpp"
#include "engine/ScopedServiceHandles.hpp"

#include <string>
#include <string_view>

namespace ndde {

class SimulationTaylorExpansionLab final : public ISimulation {
public:
    explicit SimulationTaylorExpansionLab(memory::MemoryService* memory = nullptr);

    [[nodiscard]] std::string_view name() const override { return "Taylor Expansion Lab"; }

    void on_register(SimulationHost& host) override;
    void on_start() override;
    void on_tick(const TickInfo& tick) override;
    void on_stop() override;

    [[nodiscard]] SceneSnapshot snapshot() const override;
    [[nodiscard]] SimulationMetadata metadata() const override;

private:
    memory::MemoryService* m_memory = nullptr;
    ScopedServiceHandles<PanelHandle> m_panel_handles;
    f32 m_time = f32(0);
    int m_degree = 5;
    f32 m_center = 0.f;
    f32 m_probe = 1.f;

    void draw_control_panel();
};

} // namespace ndde
