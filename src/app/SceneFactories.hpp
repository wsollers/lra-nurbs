#pragma once
// app/SceneFactories.hpp
// Simulation registration helpers used by Engine.

#include "engine/SimulationRegistry.hpp"

#include <functional>

namespace ndde {

using SimulationSwitchRequest = std::function<void(std::size_t)>;

void register_default_simulations(SimulationRegistry& registry,
                                  SimulationSwitchRequest switch_request = {});
void register_learning_simulations(SimulationRegistry& registry);

} // namespace ndde
