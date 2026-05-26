// app/SceneFactories.cpp
// Registers the active launcher, smoke-test sim, and learning labs.

#include "app/SceneFactories.hpp"
#include "app/SimulationIntegrationDerivativeLab.hpp"
#include "app/SimulationLabPicker.hpp"
#include "app/SimulationTaylorExpansionLab.hpp"
#include "app/SimulationWavePredatorPrey.hpp"

#include <utility>

namespace ndde {

void register_default_simulations(SimulationRegistry& registry,
                                  SimulationSwitchRequest switch_request) {
    registry.add_runtime<SimulationLabPicker>("Lab Picker", std::move(switch_request));
    registry.add_runtime<SimulationWavePredatorPrey>("Smoke Test - Wave Predator-Prey");
    registry.add_runtime<SimulationIntegrationDerivativeLab>("Integration & Derivative Lab");
    registry.add_runtime<SimulationTaylorExpansionLab>("Taylor Expansion Lab");
}

} // namespace ndde
