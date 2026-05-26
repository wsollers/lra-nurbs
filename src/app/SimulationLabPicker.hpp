#pragma once
// app/SimulationLabPicker.hpp
// Lightweight startup launcher for choosing the active lab/smoke-test sim.

#include "engine/ISimulation.hpp"
#include "engine/ScopedServiceHandles.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace ndde {

class SimulationLabPicker final : public ISimulation {
public:
    using SwitchSimulationFn = std::function<void(std::size_t)>;

    explicit SimulationLabPicker(memory::MemoryService* memory = nullptr,
                                 SwitchSimulationFn switch_simulation = {});

    [[nodiscard]] std::string_view name() const override { return "Lab Picker"; }

    void on_register(SimulationHost& host) override;
    void on_start() override;
    void on_tick(const TickInfo& tick) override;
    void on_stop() override;

    [[nodiscard]] SceneSnapshot snapshot() const override;
    [[nodiscard]] SimulationMetadata metadata() const override;

private:
    memory::MemoryService* m_memory = nullptr;
    SwitchSimulationFn m_switch_simulation;
    ScopedServiceHandles<PanelHandle> m_panel_handles;
    f32 m_time = f32(0);

    void draw_picker_panel();
    void request_switch(std::size_t index);
};

} // namespace ndde
