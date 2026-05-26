#pragma once

#include "app/ParticleGoals.hpp"
#include "app/ParticleSwarmFactory.hpp"
#include "app/ParticleSystem.hpp"

namespace ndde {

class SurfaceSimSpawner {
public:
    SurfaceSimSpawner(ParticleSystem& particles,
                      float& sim_time,
                      GoalStatus& goal_status) noexcept;

    void reset_particles() noexcept;
    [[nodiscard]] SwarmBuildResult spawn_showcase();
    [[nodiscard]] SwarmBuildResult spawn_brownian_cloud();
    [[nodiscard]] SwarmBuildResult spawn_contour_band();
    [[nodiscard]] SwarmBuildResult clear_all() noexcept;

private:
    ParticleSystem& m_particles;
    float& m_sim_time;
    GoalStatus& m_goal_status;
};

} // namespace ndde
