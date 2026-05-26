#pragma once
// app/ParticleSwarmFactory.hpp
// Recipe layer for emitting particle groups with shared behavior/goals.

#include "app/ParticleBehaviors.hpp"
#include "app/ParticleGoals.hpp"
#include "app/ParticleSystem.hpp"
#include "memory/Containers.hpp"
#include "sim/LevelCurveWalker.hpp"
#include "numeric/ops.hpp"
#include <algorithm>
#include <random>
#include <string>
#include <utility>

namespace ndde {

struct SwarmRecipeMetadata {
    std::string family_name;
    u32 requested_count = 0;
    memory::FrameVector<ParticleRole> roles_emitted;
    bool goals_added = false;

    [[nodiscard]] std::string roles_label() const {
        std::string out;
        for (ParticleRole role : roles_emitted) {
            const std::string_view name = role_name(role);
            if (out.find(name) != std::string::npos) continue;
            if (!out.empty()) out += ", ";
            out += name;
        }
        return out.empty() ? "None" : out;
    }
};

struct SwarmBuildResult {
    memory::FrameVector<ParticleId> particle_ids;
    SwarmRecipeMetadata metadata;

    [[nodiscard]] std::size_t size() const noexcept { return particle_ids.size(); }
    [[nodiscard]] bool empty() const noexcept { return particle_ids.empty(); }
};

class ParticleSwarmFactory {
public:
    explicit ParticleSwarmFactory(ParticleSystem& system) : m_system(system) {}

    struct BrownianCloudParams {
        u32 count = 16;
        glm::vec2 center{0.f, 0.f};
        f32 radius = 1.f;
        ParticleRole role = ParticleRole::Neutral;
        BrownianBehavior::Params brownian{.sigma = 0.16f, .drift_strength = 0.f};
        TrailConfig trail{TrailMode::Finite, 600u, 0.015f};
        std::string label = "Brownian Cloud";
    };

    struct LeaderPursuitParams {
        u32 leader_count = 1;
        u32 chaser_count = 6;
        glm::vec2 center{0.f, 0.f};
        f32 leader_radius = 0.7f;
        f32 chaser_radius = 2.2f;
        f32 leader_speed = 0.42f;
        f32 chaser_speed = 0.86f;
        f32 delay_seconds = 0.35f;
        f32 capture_radius = 0.20f;
        BrownianBehavior::Params leader_noise{.sigma = 0.08f, .drift_strength = 0.03f};
        BrownianBehavior::Params chaser_noise{.sigma = 0.055f, .drift_strength = 0.f};
        TrailConfig leader_trail{TrailMode::Persistent, 1600u, 0.012f};
        TrailConfig chaser_trail{TrailMode::Finite, 1200u, 0.012f};
        bool add_capture_goal = true;
        std::string leader_label = "Prey - Avoid - Brownian";
        std::string chaser_label = "Predator - Delayed Seek - Brownian";
    };

    struct ContourBandParams {
        u32 count = 10;
        glm::vec2 center{0.f, 0.f};
        f32 radius = 1.2f;
        bool shared_level = true;
        ParticleRole role = ParticleRole::Leader;
        ndde::sim::LevelCurveWalker::Params walker{};
        BrownianBehavior::Params noise{.sigma = 0.025f, .drift_strength = 0.f};
        TrailConfig trail{TrailMode::Persistent, 1800u, 0.010f};
        std::string label = "Contour Band";
    };

    struct GradientDriftParams {
        u32 count = 14;
        glm::vec2 center{0.f, 0.f};
        f32 radius = 1.4f;
        ParticleRole role = ParticleRole::Neutral;
        GradientDriftBehavior::Params gradient{.mode = GradientDriftBehavior::Mode::Uphill, .speed = 0.55f};
        BrownianBehavior::Params noise{.sigma = 0.035f, .drift_strength = 0.f};
        TrailConfig trail{TrailMode::Finite, 1000u, 0.012f};
        std::string label = "Gradient Drift Swarm";
    };

    struct AvoidanceParams {
        u32 count = 12;
        glm::vec2 center{0.f, 0.f};
        f32 radius = 1.3f;
        ParticleRole role = ParticleRole::Avoider;
        ParticleRole avoid_role = ParticleRole::Chaser;
        f32 speed = 0.65f;
        f32 delay_seconds = 0.f;
        BrownianBehavior::Params noise{.sigma = 0.045f, .drift_strength = 0.f};
        TrailConfig trail{TrailMode::Finite, 900u, 0.014f};
        std::string label = "Avoidance Swarm";
    };

    struct CentroidSwarmParams {
        u32 count = 10;
        glm::vec2 center{0.f, 0.f};
        f32 radius = 1.8f;
        ParticleRole role = ParticleRole::Chaser;
        ParticleRole target_role = ParticleRole::Chaser;
        f32 speed = 0.55f;
        BrownianBehavior::Params noise{.sigma = 0.035f, .drift_strength = 0.f};
        TrailConfig trail{TrailMode::Finite, 1000u, 0.014f};
        std::string label = "Centroid Swarm";
    };

    struct RingOrbitParams {
        u32 count = 16;
        glm::vec2 center{0.f, 0.f};
        f32 radius = 1.8f;
        ParticleRole role = ParticleRole::Neutral;
        OrbitBehavior::Params orbit{};
        BrownianBehavior::Params noise{.sigma = 0.02f, .drift_strength = 0.f};
        TrailConfig trail{TrailMode::Persistent, 1800u, 0.010f};
        std::string label = "Ring Orbit Swarm";
    };

    struct FlockingParams {
        u32 count = 18;
        glm::vec2 center{0.f, 0.f};
        f32 radius = 1.6f;
        ParticleRole role = ParticleRole::Neutral;
        FlockingBehavior::Params flock{};
        BrownianBehavior::Params noise{.sigma = 0.025f, .drift_strength = 0.f};
        TrailConfig trail{TrailMode::Finite, 1200u, 0.012f};
        std::string label = "Flocking Swarm";
    };

    [[nodiscard]] SwarmBuildResult brownian_cloud(const BrownianCloudParams& p) {
        SwarmBuildResult result;
        result.metadata = recipe_metadata(p.label, p.count, {p.role}, false);
        result.particle_ids.reserve(p.count);
        for (u32 i = 0; i < p.count; ++i) {
            Particle& particle = m_system.spawn(m_system.factory().particle()
                .named(p.label)
                .role(p.role)
                .at(spawn_disc(p.center, p.radius, i, p.count))
                .trail(p.trail)
                .stochastic()
                .with_behavior<BrownianBehavior>(p.brownian));
            result.particle_ids.push_back(particle.id());
        }
        return result;
    }

    [[nodiscard]] SwarmBuildResult leader_pursuit(const LeaderPursuitParams& p) {
        SwarmBuildResult result;
        result.metadata = recipe_metadata("Leader Pursuit", p.leader_count + p.chaser_count,
                                          {ParticleRole::Leader, ParticleRole::Chaser},
                                          p.add_capture_goal);
        result.particle_ids.reserve(static_cast<std::size_t>(p.leader_count) + p.chaser_count);

        for (u32 i = 0; i < p.leader_count; ++i) {
            const f32 a = angle(i, std::max(p.leader_count, 1u));
            AvoidParticleBehavior::Params avoid;
            avoid.target = TargetSelector::nearest(ParticleRole::Chaser);
            avoid.speed = p.leader_speed;
            avoid.delay_seconds = p.delay_seconds * 0.55f;

            Particle& particle = m_system.spawn(m_system.factory().particle()
                .named(p.leader_label)
                .role(ParticleRole::Leader)
                .at(p.center + glm::vec2{p.leader_radius * ops::cos(a), p.leader_radius * ops::sin(a)})
                .trail(p.leader_trail)
                .history()
                .stochastic()
                .with_behavior<AvoidParticleBehavior>(avoid)
                .with_behavior<BrownianBehavior>(0.55f, p.leader_noise));
            result.particle_ids.push_back(particle.id());
        }

        for (u32 i = 0; i < p.chaser_count; ++i) {
            const f32 a = angle(i, std::max(p.chaser_count, 1u)) + 0.35f;
            SeekParticleBehavior::Params seek;
            seek.target = TargetSelector::nearest(ParticleRole::Leader);
            seek.speed = p.chaser_speed;
            seek.delay_seconds = p.delay_seconds;

            Particle& particle = m_system.spawn(m_system.factory().particle()
                .named(p.chaser_label)
                .role(ParticleRole::Chaser)
                .at(p.center + glm::vec2{p.chaser_radius * ops::cos(a), p.chaser_radius * ops::sin(a)})
                .trail(p.chaser_trail)
                .stochastic()
                .with_behavior<SeekParticleBehavior>(seek)
                .with_behavior<BrownianBehavior>(0.30f, p.chaser_noise));
            result.particle_ids.push_back(particle.id());
        }

        if (p.add_capture_goal) {
            m_system.add_goal<CaptureGoal>(CaptureGoal::Params{
                .seeker_role = ParticleRole::Chaser,
                .target_role = ParticleRole::Leader,
                .radius = p.capture_radius
            });
        }
        return result;
    }

    [[nodiscard]] SwarmBuildResult contour_band(const ContourBandParams& p) {
        SwarmBuildResult result;
        result.metadata = recipe_metadata(p.label, p.count, {p.role}, false);
        result.particle_ids.reserve(p.count);
        const f32 z0 = surface().evaluate(p.center.x, p.center.y).z;

        for (u32 i = 0; i < p.count; ++i) {
            const glm::vec2 uv = spawn_disc(p.center, p.radius, i, p.count);
            auto walker_params = p.walker;
            walker_params.z0 = p.shared_level ? z0 : surface().evaluate(uv.x, uv.y).z;

            Particle& particle = m_system.spawn(m_system.factory().particle()
                .named(p.label)
                .role(p.role)
                .at(uv)
                .trail(p.trail)
                .stochastic()
                .with_equation_type<ndde::sim::LevelCurveWalker>(walker_params)
                .with_behavior<BrownianBehavior>(0.18f, p.noise));
            result.particle_ids.push_back(particle.id());
        }
        return result;
    }

    [[nodiscard]] SwarmBuildResult gradient_drift(const GradientDriftParams& p) {
        SwarmBuildResult result;
        result.metadata = recipe_metadata(p.label, p.count, {p.role}, false);
        result.particle_ids.reserve(p.count);
        for (u32 i = 0; i < p.count; ++i) {
            Particle& particle = m_system.spawn(m_system.factory().particle()
                .named(p.label)
                .role(p.role)
                .at(spawn_disc(p.center, p.radius, i, p.count))
                .trail(p.trail)
                .stochastic()
                .with_behavior<GradientDriftBehavior>(p.gradient)
                .with_behavior<BrownianBehavior>(0.25f, p.noise));
            result.particle_ids.push_back(particle.id());
        }
        return result;
    }

    [[nodiscard]] SwarmBuildResult avoidance_swarm(const AvoidanceParams& p) {
        SwarmBuildResult result;
        result.metadata = recipe_metadata(p.label, p.count, {p.role}, false);
        result.particle_ids.reserve(p.count);
        AvoidParticleBehavior::Params avoid;
        avoid.target = TargetSelector::nearest(p.avoid_role);
        avoid.speed = p.speed;
        avoid.delay_seconds = p.delay_seconds;

        for (u32 i = 0; i < p.count; ++i) {
            Particle& particle = m_system.spawn(m_system.factory().particle()
                .named(p.label)
                .role(p.role)
                .at(spawn_disc(p.center, p.radius, i, p.count))
                .trail(p.trail)
                .stochastic()
                .with_behavior<AvoidParticleBehavior>(avoid)
                .with_behavior<BrownianBehavior>(0.30f, p.noise));
            result.particle_ids.push_back(particle.id());
        }
        return result;
    }

    [[nodiscard]] SwarmBuildResult centroid_swarm(const CentroidSwarmParams& p) {
        SwarmBuildResult result;
        result.metadata = recipe_metadata(p.label, p.count, {p.role}, false);
        result.particle_ids.reserve(p.count);
        for (u32 i = 0; i < p.count; ++i) {
            Particle& particle = m_system.spawn(m_system.factory().particle()
                .named(p.label)
                .role(p.role)
                .at(spawn_disc(p.center, p.radius, i, p.count))
                .trail(p.trail)
                .stochastic()
                .with_behavior<CentroidSeekBehavior>(CentroidSeekBehavior::Params{
                    .role = p.target_role,
                    .speed = p.speed
                })
                .with_behavior<BrownianBehavior>(0.25f, p.noise));
            result.particle_ids.push_back(particle.id());
        }
        return result;
    }

    [[nodiscard]] SwarmBuildResult ring_orbit(const RingOrbitParams& p) {
        SwarmBuildResult result;
        result.metadata = recipe_metadata(p.label, p.count, {p.role}, false);
        result.particle_ids.reserve(p.count);
        for (u32 i = 0; i < p.count; ++i) {
            const f32 a = angle(i, std::max(p.count, 1u));
            auto orbit = p.orbit;
            orbit.center = p.center;
            if (orbit.radius <= 0.f) orbit.radius = p.radius;
            const glm::vec2 uv = {
                ops::clamp(p.center.x + p.radius * ops::cos(a), surface().u_min(), surface().u_max()),
                ops::clamp(p.center.y + p.radius * ops::sin(a), surface().v_min(), surface().v_max())
            };
            Particle& particle = m_system.spawn(m_system.factory().particle()
                .named(p.label)
                .role(p.role)
                .at(uv)
                .trail(p.trail)
                .stochastic()
                .with_behavior<OrbitBehavior>(orbit)
                .with_behavior<BrownianBehavior>(0.20f, p.noise));
            result.particle_ids.push_back(particle.id());
        }
        return result;
    }

    [[nodiscard]] SwarmBuildResult flocking_swarm(const FlockingParams& p) {
        SwarmBuildResult result;
        result.metadata = recipe_metadata(p.label, p.count, {p.role}, false);
        result.particle_ids.reserve(p.count);
        auto flock = p.flock;
        flock.role = p.role;
        for (u32 i = 0; i < p.count; ++i) {
            const f32 a = angle(i, std::max(p.count, 1u));
            const glm::vec2 tangent{-ops::sin(a), ops::cos(a)};
            Particle& particle = m_system.spawn(m_system.factory().particle()
                .named(p.label)
                .role(p.role)
                .at(spawn_disc(p.center, p.radius, i, p.count))
                .trail(p.trail)
                .history()
                .stochastic()
                .with_behavior<FlockingBehavior>(flock)
                .with_behavior<ConstantDriftBehavior>(0.25f, tangent * 0.18f)
                .with_behavior<BrownianBehavior>(0.20f, p.noise));
            result.particle_ids.push_back(particle.id());
        }
        return result;
    }

private:
    ParticleSystem& m_system;

    [[nodiscard]] const ndde::math::ISurface& surface() const noexcept {
        return *m_system.surface();
    }

    [[nodiscard]] static f32 angle(u32 i, u32 count) noexcept {
        return ops::two_pi_v<f32> * static_cast<f32>(i) / static_cast<f32>(std::max(count, 1u));
    }

    [[nodiscard]] glm::vec2 spawn_disc(glm::vec2 center, f32 radius, u32 i, u32 count) {
        std::uniform_real_distribution<f32> jitter(0.82f, 1.16f);
        const f32 a = angle(i, std::max(count, 1u)) + 0.37f * static_cast<f32>(i % 5u);
        const f32 r = radius * jitter(m_system.rng());
        const glm::vec2 raw = center + glm::vec2{r * ops::cos(a), r * ops::sin(a)};
        return {
            ops::clamp(raw.x, surface().u_min(), surface().u_max()),
            ops::clamp(raw.y, surface().v_min(), surface().v_max())
        };
    }

    [[nodiscard]] static SwarmRecipeMetadata recipe_metadata(std::string family_name,
                                                             u32 requested_count,
                                                             memory::FrameVector<ParticleRole> roles,
                                                             bool goals_added)
    {
        return SwarmRecipeMetadata{
            .family_name = std::move(family_name),
            .requested_count = requested_count,
            .roles_emitted = std::move(roles),
            .goals_added = goals_added
        };
    }
};

} // namespace ndde
