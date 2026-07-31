#pragma once
// app/workbenches/Workbench.hpp
// Workbench contract. Gallery entries assemble simulation context legos.

#include "engine/SimulationClock.hpp"
#include "engine/SimulationHost.hpp"
#include "simulation/context/WorkbenchBuildContext.hpp"

#include <string>
#include <string_view>

namespace ndde {

struct WorkbenchMetadata {
    std::string id;
    std::string title;
    std::string summary;
    std::string thumbnail_path;
};

struct WorkbenchRenderContext {
    SimulationHost& host;
    RenderViewId main_view = RenderViewId(0);
};

class IWorkbench {
public:
    virtual ~IWorkbench() = default;

    [[nodiscard]] virtual const WorkbenchMetadata& metadata() const noexcept = 0;
    virtual void build(sim::WorkbenchBuildContext& build) = 0;
    virtual void on_start(WorkbenchRenderContext& context) = 0;
    virtual void on_tick(const TickInfo& tick) = 0;
    virtual void on_submit_render(WorkbenchRenderContext& context) = 0;
    virtual void on_stop() = 0;
};

} // namespace ndde
