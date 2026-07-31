#pragma once
// app/simulations/LearningSimulation.hpp
// App-owned simulation shell for the selected workbench gallery entry.

#include "app/workbenches/Workbench.hpp"
#include "app/workbenches/WorkbenchRegistry.hpp"
#include "engine/ISimulation.hpp"
#include "simulation/context/WorkbenchBuildContext.hpp"

#include <string>

namespace ndde {

class LearningSimulation final : public ISimulation {
public:
    explicit LearningSimulation(memory::MemoryService* memory = nullptr);
    LearningSimulation(memory::MemoryService* memory, std::string workbench_id);

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
    sim::WorkbenchBuildContext m_build_context;
    std::string m_workbench_id;
    f32 m_time = 0.f;
    std::string m_status = "Ready";

    [[nodiscard]] WorkbenchRenderContext render_context();
};

} // namespace ndde
