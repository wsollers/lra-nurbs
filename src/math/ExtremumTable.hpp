#pragma once
// math/ExtremumTable.hpp
// ExtremumTable: cached locations of the global max and min of a surface.
//
// Built by a grid search followed by gradient refinement.
// Cached by simulations that need extremum lookup tables.
// Non-owning pointers to it are held by LeaderSeekerEquation and
// BiasedBrownianLeader -- safe because the scene outlives all particles.

#include "math/Scalars.hpp"
#include <glm/glm.hpp>

namespace ndde::math {

struct ExtremumTable {
    glm::vec2 max_uv = {0.f, 0.f};  ///< parameter-space location of global max
    float     max_z  = 0.f;          ///< f(max_uv)
    glm::vec2 min_uv = {0.f, 0.f};  ///< parameter-space location of global min
    float     min_z  = 0.f;          ///< f(min_uv)
    bool      valid  = false;         ///< false until first build

    // Build the table for any ISurface using a grid search + gradient refinement.
    // grid_n: number of grid points per axis (default 64 -- O(4096) evaluations).
    // Called synchronously on surface swap; cheap enough for 4096 evaluations.
    void build(const class ISurface& surface, float t = 0.f, u32 grid_n = 64u);

    // Invalidate (e.g. before rebuild on deforming surface).
    void invalidate() noexcept { valid = false; }
};

} // namespace ndde::math
