// app/SceneFactories.cpp
// Registers the active launcher, smoke-test sim, and learning labs.

#include "app/SceneFactories.hpp"
#include "app/SimulationLabPicker.hpp"
#include "app/simulations/LearningSimulation.hpp"
#include "app/simulations/examples/SimulationIntegrationDerivativeLab.hpp"
#include "app/simulations/examples/SimulationTaylorExpansionLab.hpp"
#include "app/simulations/examples/SimulationWavePredatorPrey.hpp"
#include "app/workbenches/WorkbenchRegistry.hpp"

#include <utility>

namespace ndde {

void register_default_simulations(SimulationRegistry& registry,
                                  SimulationSwitchRequest switch_request) {
    registry.add_runtime<SimulationLabPicker>("Lab Picker", std::move(switch_request));
    registry.add_runtime<SimulationWavePredatorPrey>("Smoke Test - Wave Predator-Prey");
    registry.add_runtime<SimulationIntegrationDerivativeLab>("Integration & Derivative Lab");
    registry.add_runtime<SimulationTaylorExpansionLab>("Taylor Expansion Lab");
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
