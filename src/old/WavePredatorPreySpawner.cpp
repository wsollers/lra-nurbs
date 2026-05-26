#include "app/WavePredatorPreySpawner.hpp"

#include "sim/LevelCurveWalker.hpp"

namespace ndde {

WavePredatorPreySpawner::WavePredatorPreySpawner(ParticleSystem& particles,
                                                 float& sim_time,
                                                 GoalStatus& goal_status) noexcept
    : m_particles(particles)
    , m_sim_time(sim_time)
    , m_goal_status(goal_status)
{}

void WavePredatorPreySpawner::reset_particles() noexcept {
    m_particles.clear();
    m_particles.clear_goals();
    m_goal_status = GoalStatus::Running;
    m_sim_time = 0.f;
}

SwarmBuildResult WavePredatorPreySpawner::clear_all() noexcept {
    reset_particles();
    return SwarmBuildResult{.metadata = SwarmRecipeMetadata{
        .family_name = "Clear All",
        .requested_count = 0u,
        .roles_emitted = {},
        .goals_added = false
    }};
}

SwarmBuildResult WavePredatorPreySpawner::spawn_predator_prey_showcase() {
    reset_particles();
    ParticleSwarmFactory swarms(m_particles);
    SwarmBuildResult swarm = swarms.leader_pursuit(ParticleSwarmFactory::LeaderPursuitParams{
        .leader_count = 3u,
        .chaser_count = 9u,
        .center = {0.f, 0.f},
        .leader_radius = 0.85f,
        .chaser_radius = 2.85f,
        .leader_speed = 0.48f,
        .chaser_speed = 0.92f,
        .delay_seconds = 0.42f,
        .capture_radius = 0.18f,
        .leader_noise = {.sigma = 0.09f, .drift_strength = -0.04f},
        .chaser_noise = {.sigma = 0.05f, .drift_strength = 0.f},
        .leader_trail = {TrailMode::Persistent, 2200u, 0.010f},
        .chaser_trail = {TrailMode::Finite, 1400u, 0.012f},
        .add_capture_goal = true,
        .leader_label = "Prey - Delayed Avoid - Brownian",
        .chaser_label = "Predator - Delayed Seek - Brownian"
    });

    for (int i = 0; i < 100; ++i) {
        m_sim_time += 1.f / 60.f;
        m_particles.update(1.f / 60.f, 1.f, m_sim_time);
    }
    return swarm;
}

SwarmBuildResult WavePredatorPreySpawner::spawn_brownian_cloud() {
    ParticleSwarmFactory swarms(m_particles);
    return swarms.brownian_cloud(ParticleSwarmFactory::BrownianCloudParams{
        .count = 18u,
        .center = {0.f, 0.f},
        .radius = 2.2f,
        .role = ParticleRole::Avoider,
        .brownian = {.sigma = 0.13f, .drift_strength = 0.025f},
        .trail = {TrailMode::Finite, 900u, 0.014f},
        .label = "Avoider - Brownian Cloud"
    });
}

SwarmBuildResult WavePredatorPreySpawner::spawn_contour_band() {
    ParticleSwarmFactory swarms(m_particles);
    ndde::sim::LevelCurveWalker::Params walker;
    walker.epsilon = 0.16f;
    walker.walk_speed = 0.54f;
    walker.turn_rate = 2.7f;
    walker.tangent_floor = 0.45f;
    return swarms.contour_band(ParticleSwarmFactory::ContourBandParams{
        .count = 12u,
        .center = {0.f, 0.f},
        .radius = 1.8f,
        .shared_level = true,
        .role = ParticleRole::Neutral,
        .walker = walker,
        .noise = {.sigma = 0.018f, .drift_strength = 0.f},
        .trail = {TrailMode::Persistent, 2200u, 0.010f},
        .label = "Neutral - Contour Band - Brownian"
    });
}

} // namespace ndde
