// app/SceneFactories.cpp
// Registers the active launcher, smoke-test sim, and learning labs.

#include "app/SceneFactories.hpp"
#include "app/SimulationLabPicker.hpp"
#include "app/workbenches/Learning/LearningSimulation.hpp"
#include "app/workbenches/IntegrationDerivativeLab/IntegrationDerivativeLabWorkbench.hpp"
#include "app/workbenches/TaylorExpansionLab/TaylorExpansionLabWorkbench.hpp"
#include "app/workbenches/WavePredatorPrey/WavePredatorPreyWorkbench.hpp"
#include "app/workbenches/WorkbenchRegistry.hpp"

#include <utility>

namespace ndde {

void register_default_simulations(SimulationRegistry& registry,
                                  SimulationSwitchRequest switch_request) {
    registry.add_runtime<SimulationLabPicker>("Lab Picker", std::move(switch_request));
    register_wave_predator_prey_workbench(registry);
    register_integration_derivative_lab_workbench(registry);
    register_taylor_expansion_lab_workbench(registry);
}

void register_learning_simulations(SimulationRegistry& registry) {
    WorkbenchRegistry workbenches;
    register_app_workbenches(workbenches);
    for (std::size_t index = 0; index < workbenches.size(); ++index) {
        const WorkbenchMetadata* metadata = workbenches.metadata(index);
        if (!metadata) continue;
        registry.add_runtime<LearningSimulation>(SimulationDescriptor{
            .id = metadata->id,
            .title = metadata->title,
            .summary = metadata->summary,
            .thumbnail_path = metadata->thumbnail_path
        }, metadata->id);
    }
}

} // namespace ndde
