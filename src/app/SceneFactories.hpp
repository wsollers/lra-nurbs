#pragma once
// app/SceneFactories.hpp
// Simulation registration helpers used by Engine.

#include "engine/SimulationRegistry.hpp"

#include <functional>

namespace ndde {

void register_default_simulations(SimulationRegistry& registry,
                                  std::function<void(std::size_t)> switch_request = {});
void register_learning_simulations(SimulationRegistry& registry);

} // namespace ndde
