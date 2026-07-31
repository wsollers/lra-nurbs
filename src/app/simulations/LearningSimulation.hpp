#pragma once
// app/simulations/LearningSimulation.hpp
// App-owned simulation shell for learning workbenches.

#include "app/simulations/learning/IWorkbench.hpp"
#include "app/simulations/learning/WorkbenchRegistry.hpp"
#include "engine/ISimulation.hpp"

namespace ndde {

class LearningSimulation final : public ISimulation {
public:
    explicit LearningSimulation(memory::MemoryService* memory = nullptr);

    [[nodiscard]] std::string_view name() const override { return "Learning Workbench"; }

    void on_register(SimulationHost& host) override;
    void on_start() override;
    void on_tick(const TickInfo& tick) override;
    void on_simulation_tick(const TickInfo& tick) override;
    void on_submit_render() override;
    void on_stop() override;

    [[nodiscard]] SceneSnapshot snapshot() const override;
    [[nodiscard]] SimulationMetadata metadata() const override;

private:
    memory::MemoryService* m_memory = nullptr;
    SimulationHost* m_host = nullptr;
    RenderViewHandle m_main_handle;
    RenderViewId m_main_view = RenderViewId(0);
    WorkbenchRegistry m_workbenches;
    memory::Unique<IWorkbench> m_active_workbench;
    f32 m_time = 0.f;
    std::string m_status = "Ready";

    [[nodiscard]] WorkbenchRenderContext render_context();
};

} // namespace ndde
