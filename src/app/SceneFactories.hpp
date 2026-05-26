#pragma once
// app/SceneFactories.hpp
// Simulation registration helpers used by Engine.

#include "engine/SimulationRuntime.hpp"

#include <functional>

namespace ndde {

using SimulationSwitchRequest = std::function<void(std::size_t)>;

void register_default_simulations(SimulationRegistry& registry,
                                  SimulationSwitchRequest switch_request = {});

} // namespace ndde
