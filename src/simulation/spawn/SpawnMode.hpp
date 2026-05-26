#pragma once
// simulation/spawn/SpawnMode.hpp
// Placement strategies for spawning particle groups on a surface.

#include "math/Scalars.hpp"
#include <glm/glm.hpp>

namespace ndde::simulation {

enum class SpawnMode : u8 {
    RandomDisc,    // jittered disc — random angle + radius within [0, radius]
    RingPack,      // equally spaced on a ring at exact radius
    PoissonDisc,   // dart-throwing with guaranteed min_separation between points
    Grid,          // regular grid within [center ± radius]
};

struct SpawnConfig {
    SpawnMode mode           = SpawnMode::RandomDisc;
    glm::vec2 center         = {f32(0), f32(0)};
    f32       radius         = f32(1.8);
    f32       min_separation = f32(0);    // PoissonDisc only
    u32       grid_cols      = u32(0);    // Grid: 0 = sqrt(count)
    u32       seed           = u32(42);
    f32       phase_offset   = f32(0);    // RingPack: angular offset in radians
};

} // namespace ndde::simulation
