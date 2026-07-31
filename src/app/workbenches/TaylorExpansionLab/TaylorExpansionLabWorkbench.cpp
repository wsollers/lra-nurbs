#include "app/workbenches/TaylorExpansionLab/TaylorExpansionLabWorkbench.hpp"

#include "app/workbenches/TaylorExpansionLab/SimulationTaylorExpansionLab.hpp"
#include "engine/SimulationRegistry.hpp"

namespace ndde {

void register_taylor_expansion_lab_workbench(SimulationRegistry& registry) {
    registry.add_runtime<SimulationTaylorExpansionLab>("Taylor Expansion Lab");
}

} // namespace ndde
