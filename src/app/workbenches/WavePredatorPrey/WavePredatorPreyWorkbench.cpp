#include "app/workbenches/WavePredatorPrey/WavePredatorPreyWorkbench.hpp"

#include "app/workbenches/WavePredatorPrey/SimulationWavePredatorPrey.hpp"
#include "engine/SimulationRegistry.hpp"

namespace ndde {

void register_wave_predator_prey_workbench(SimulationRegistry& registry) {
    registry.add_runtime<SimulationWavePredatorPrey>("Smoke Test - Wave Predator-Prey");
}

} // namespace ndde
