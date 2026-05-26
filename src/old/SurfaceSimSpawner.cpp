#include "app/SurfaceSimSpawner.hpp"

#include "sim/LevelCurveWalker.hpp"

namespace ndde {

SurfaceSimSpawner::SurfaceSimSpawner(ParticleSystem& particles,
                                     float& sim_time,
                                     GoalStatus& goal_status) noexcept
    : m_particles(particles)
    , m_sim_time(sim_time)
    , m_goal_status(goal_status)
{}

void SurfaceSimSpawner::reset_particles() noexcept {
    m_particles.clear();
    m_particles.clear_goals();
    m_goal_status = GoalStatus::Running;
    m_sim_time = 0.f;
}

SwarmBuildResult SurfaceSimSpawner::clear_all() noexcept {
    reset_particles();
    return SwarmBuildResult{.metadata = SwarmRecipeMetadata{
        .family_name = "Clear All",
        .requested_count = 0u,
        .roles_emitted = {},
        .goals_added = false
    }};
}

SwarmBuildResult SurfaceSimSpawner::spawn_showcase() {
    reset_particles();
    ParticleSwarmFactory swarms(m_particles);
    SwarmBuildResult swarm = swarms.leader_pursuit(ParticleSwarmFactory::LeaderPursuitParams{
        .leader_count = 2u,
        .chaser_count = 6u,
        .center = {-0.25f, 0.20f},
        .leader_radius = 0.90f,
        .chaser_radius = 3.10f,
        .leader_speed = 0.52f,
        .chaser_speed = 0.84f,
        .delay_seconds = 0.55f,
        .capture_radius = 0.20f,
        .leader_noise = {.sigma = 0.08f, .drift_strength = -0.03f},
        .chaser_noise = {.sigma = 0.045f, .drift_strength = 0.f},
        .leader_trail = {TrailMode::Persistent, 2200u, 0.010f},
        .chaser_trail = {TrailMode::Finite, 1400u, 0.012f},
        .add_capture_goal = true,
        .leader_label = "Leader - Gaussian Drift - Brownian",
        .chaser_label = "Chaser - Delayed Pursuit - Brownian"
    });

    for (int i = 0; i < 120; ++i) {
        m_sim_time += 1.f / 60.f;
        m_particles.update(1.f / 60.f, 1.f, m_sim_time);
    }
    return swarm;
}

SwarmBuildResult SurfaceSimSpawner::spawn_brownian_cloud() {
    ParticleSwarmFactory swarms(m_particles);
    return swarms.brownian_cloud(ParticleSwarmFactory::BrownianCloudParams{
        .count = 16u,
        .center = {0.f, 0.f},
        .radius = 2.75f,
        .role = ParticleRole::Neutral,
        .brownian = {.sigma = 0.12f, .drift_strength = 0.025f},
        .trail = {TrailMode::Finite, 950u, 0.014f},
        .label = "Neutral - Gaussian Brownian Cloud"
    });
}

SwarmBuildResult SurfaceSimSpawner::spawn_contour_band() {
    ParticleSwarmFactory swarms(m_particles);
    ndde::sim::LevelCurveWalker::Params walker;
    walker.epsilon = 0.14f;
    walker.walk_speed = 0.58f;
    walker.turn_rate = 2.6f;
    walker.tangent_floor = 0.42f;
    return swarms.contour_band(ParticleSwarmFactory::ContourBandParams{
        .count = 10u,
        .center = {0.f, 0.f},
        .radius = 2.1f,
        .shared_level = true,
        .role = ParticleRole::Leader,
        .walker = walker,
        .noise = {.sigma = 0.016f, .drift_strength = 0.f},
        .trail = {TrailMode::Persistent, 2200u, 0.010f},
        .label = "Leader - Gaussian Contour Band"
    });
}

} // namespace ndde
