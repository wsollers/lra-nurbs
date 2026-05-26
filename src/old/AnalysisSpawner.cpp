#include "app/AnalysisSpawner.hpp"

#include "app/ParticleBehaviors.hpp"
#include "sim/LevelCurveWalker.hpp"

#include <cmath>
#include <memory>
#include <random>
#include <utility>

namespace ndde {

AnalysisSpawner::AnalysisSpawner(ndde::math::SineRationalSurface& surface,
                                 ParticleSystem& particles,
                                 u32& spawn_count,
                                 float& epsilon,
                                 float& walk_speed,
                                 float& noise_sigma,
                                 float& sim_time,
                                 float& sim_speed,
                                 GoalStatus& goal_status) noexcept
    : m_surface(surface)
    , m_particles(particles)
    , m_spawn_count(spawn_count)
    , m_epsilon(epsilon)
    , m_walk_speed(walk_speed)
    , m_noise_sigma(noise_sigma)
    , m_sim_time(sim_time)
    , m_sim_speed(sim_speed)
    , m_goal_status(goal_status)
{}

namespace {
SwarmBuildResult make_analysis_result(std::string name, u32 count, bool goal) {
    return SwarmBuildResult{.metadata = SwarmRecipeMetadata{
        .family_name = std::move(name),
        .requested_count = count,
        .roles_emitted = {ParticleRole::Leader, ParticleRole::Chaser},
        .goals_added = goal
    }};
}
}

SwarmBuildResult AnalysisSpawner::clear_all() noexcept {
    m_particles.clear();
    m_particles.clear_goals();
    m_spawn_count = 0;
    m_goal_status = GoalStatus::Running;
    return make_analysis_result("Analysis Clear", 0u, false);
}

SwarmBuildResult AnalysisSpawner::spawn_showcase_service() {
    (void)clear_all();
    m_sim_time = 0.f;

    ndde::sim::LevelCurveWalker::Params p;
    p.z0 = m_surface.height(-1.25f, 0.65f);
    p.epsilon = 0.18f;
    p.walk_speed = 0.72f;
    p.tangent_floor = 0.42f;

    ParticleBuilder leader_builder = m_particles.factory().particle();
    leader_builder
        .named("Leader - Level Curve - Brownian")
        .role(ParticleRole::Leader)
        .at({-1.25f, 0.65f})
        .history(640, 1.f / 120.f)
        .trail({TrailMode::Finite, AnimatedCurve::MAX_TRAIL, 0.012f})
        .stochastic()
        .with_equation_type<ndde::sim::LevelCurveWalker>(p)
        .with_behavior<BrownianBehavior>(0.20f, BrownianBehavior::Params{
            .sigma = 0.045f,
            .drift_strength = 0.f
        });
    AnimatedCurve& leader = m_particles.spawn(std::move(leader_builder));
    ++m_spawn_count;

    for (int i = 0; i < 180; ++i) {
        m_sim_time += 1.f / 60.f;
        m_particles.update(1.f / 60.f, 1.f, m_sim_time);
    }

    SeekParticleBehavior::Params seek;
    seek.target = TargetSelector::nearest(ParticleRole::Leader);
    seek.speed = 0.86f;
    seek.delay_seconds = 0.85f;

    const glm::vec2 leader_uv = leader.head_uv();
    ParticleBuilder seeker_builder = m_particles.factory().particle();
    seeker_builder
        .named("Chaser - Delayed Seek - Brownian")
        .role(ParticleRole::Chaser)
        .at({leader_uv.x + 1.0f, leader_uv.y - 0.75f})
        .trail({TrailMode::Finite, AnimatedCurve::MAX_TRAIL, 0.012f})
        .stochastic()
        .with_behavior<SeekParticleBehavior>(seek)
        .with_behavior<BrownianBehavior>(0.18f, BrownianBehavior::Params{
            .sigma = 0.035f,
            .drift_strength = 0.f
        });
    m_particles.spawn(std::move(seeker_builder));
    ++m_spawn_count;

    m_particles.add_goal<CaptureGoal>(CaptureGoal::Params{
        .seeker_role = ParticleRole::Chaser,
        .target_role = ParticleRole::Leader,
        .radius = 0.18f
    });
    m_goal_status = GoalStatus::Running;
    return make_analysis_result("Analysis Leader/Delayed Seeker", m_spawn_count, true);
}

SwarmBuildResult AnalysisSpawner::spawn_walker() {
    std::uniform_real_distribution<float> du(m_surface.u_min() + 0.5f, m_surface.u_max() - 0.5f);
    std::uniform_real_distribution<float> dv(m_surface.v_min() + 0.5f, m_surface.v_max() - 0.5f);

    const float u0 = du(m_particles.rng());
    const float v0 = dv(m_particles.rng());
    const float z0 = m_surface.height(u0, v0);

    ndde::sim::LevelCurveWalker::Params p;
    p.z0 = z0;
    p.epsilon = m_epsilon;
    p.walk_speed = m_walk_speed;

    ParticleBuilder builder = m_particles.factory().particle();
    builder.named("Walker")
        .role(ParticleRole::Leader)
        .at({u0, v0})
        .trail({TrailMode::Finite, AnimatedCurve::MAX_TRAIL, 0.015f})
        .with_equation_type<ndde::sim::LevelCurveWalker>(p);

    if (m_noise_sigma > 1e-6f) {
        builder.stochastic()
            .with_behavior<BrownianBehavior>(BrownianBehavior::Params{
                .sigma = m_noise_sigma,
                .drift_strength = 0.f
            });
    }

    AnimatedCurve& c = m_particles.spawn(std::move(builder));

    SimulationContext context = m_particles.context(m_sim_time);
    context.set_time(m_sim_time);
    c.set_behavior_context(&context);

    for (int i = 0; i < 120; ++i) {
        context.set_time(m_sim_time + static_cast<float>(i) / 60.f);
        c.advance(1.f / 60.f, m_sim_speed);
    }
    ++m_spawn_count;
    return make_analysis_result("Analysis Level Walker", 1u, false);
}

} // namespace ndde
