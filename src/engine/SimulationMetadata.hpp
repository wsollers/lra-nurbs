#pragma once
// engine/SimulationMetadata.hpp
// Lightweight engine-level metadata exposed by ISimulation.

#include "math/Scalars.hpp"

#include <cstddef>
#include <string>

namespace ndde {

struct SimulationMetadata {
    std::string name;
    std::string surface_name;
    std::string surface_formula;
    std::string status;
    f32 sim_time = 0.f;
    f32 sim_speed = 1.f;
    std::size_t particle_count = 0;
    bool paused = false;
    bool goal_succeeded = false;
    bool surface_has_analytic_derivatives = false;
    bool surface_deformable = false;
    bool surface_time_varying = false;
};

} // namespace ndde
