#pragma once

#include "app/AnimatedCurve.hpp"
#include "app/ParticleGoals.hpp"
#include "app/ParticleSwarmFactory.hpp"
#include "app/ParticleSystem.hpp"
#include "math/SineRationalSurface.hpp"

namespace ndde {

class AnalysisSpawner {
public:
    AnalysisSpawner(ndde::math::SineRationalSurface& surface,
                    ParticleSystem& particles,
                    u32& spawn_count,
                    float& epsilon,
                    float& walk_speed,
                    float& noise_sigma,
                    float& sim_time,
                    float& sim_speed,
                    GoalStatus& goal_status) noexcept;

    [[nodiscard]] SwarmBuildResult spawn_showcase_service();
    [[nodiscard]] SwarmBuildResult spawn_walker();
    [[nodiscard]] SwarmBuildResult clear_all() noexcept;

private:
    ndde::math::SineRationalSurface& m_surface;
    ParticleSystem& m_particles;
    u32& m_spawn_count;
    float& m_epsilon;
    float& m_walk_speed;
    float& m_noise_sigma;
    float& m_sim_time;
    float& m_sim_speed;
    GoalStatus& m_goal_status;
};

} // namespace ndde
