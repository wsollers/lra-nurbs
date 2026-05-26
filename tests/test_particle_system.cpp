#include <gtest/gtest.h>

#include "app/ParticleBehaviors.hpp"
#include "app/ParticleSwarmFactory.hpp"
#include "app/ParticleSystem.hpp"
#include "math/Surfaces.hpp"
#include "memory/MemoryService.hpp"
#include "simulation/fields/IField.hpp"

using ndde::ParticleRole;
using ndde::TrailMode;

namespace {

ndde::math::Paraboloid flat_surface() {
    return ndde::math::Paraboloid(0.001f, 10.f, -10.f, 10.f);
}

class ConstantWindField final : public ndde::simulation::IField {
public:
    [[nodiscard]] glm::vec2 drift_contribution(const ndde::sim::ParticleState&,
                                               const ndde::math::ISurface&,
                                               ndde::f32) const override {
        return {1.f, 0.f};
    }

    [[nodiscard]] std::string_view name() const noexcept override { return "ConstantWindField"; }
};

class ConstantDiffusionField final : public ndde::simulation::IField {
public:
    [[nodiscard]] glm::vec2 diffusion_contribution(const ndde::sim::ParticleState&,
                                                   const ndde::math::ISurface&,
                                                   ndde::f32) const override {
        return {2.f, 3.f};
    }

    [[nodiscard]] std::string_view name() const noexcept override { return "ConstantDiffusionField"; }
};

} // namespace

TEST(ParticleSystem, MetadataIncludesRoleAndBehaviors) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 1u);

    auto builder = system.factory().particle()
        .named("Probe")
        .role(ParticleRole::Avoider)
        .at({1.f, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.1f, 0.f})
        .with_behavior<ndde::BrownianBehavior>(ndde::BrownianBehavior::Params{
            .sigma = 0.05f,
            .drift_strength = 0.f
        });

    ndde::Particle& p = system.spawn(std::move(builder));
    const ndde::ParticleMetadata md = p.metadata();

    EXPECT_EQ(md.role, "Avoider");
    EXPECT_NE(p.metadata_label().find("Probe"), std::string::npos);
    EXPECT_NE(p.metadata_label().find("Constant Drift"), std::string::npos);
    EXPECT_NE(p.metadata_label().find("Brownian"), std::string::npos);
}

TEST(ParticleSystem, BindsSimulationContainersToMemoryService) {
    auto surface = flat_surface();
    ndde::memory::MemoryService memory;
    ndde::ParticleSystem system(&surface, 101u);
    system.bind_memory(&memory);

    EXPECT_EQ(system.particles().get_allocator().resource(), memory.simulation().resource());

    ndde::Particle& particle = system.spawn(system.factory().particle()
        .role(ParticleRole::Leader)
        .at({0.f, 0.f})
        .trail(ndde::TrailConfig{.mode = TrailMode::Persistent, .min_spacing = 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.1f, 0.f}));

    system.update(0.1f, 1.f, 0.1f);
    particle.enable_history(8u, 0.f);
    particle.push_history(0.1f);
    ASSERT_NE(particle.history(), nullptr);
    EXPECT_EQ(particle.history()->to_vector().get_allocator().resource(), memory.history().resource());
}

TEST(ParticleSystem, SeekBehaviorMovesTowardNearestRoleTarget) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 2u);

    system.spawn(system.factory().particle()
        .role(ParticleRole::Leader)
        .at({3.f, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.f, 0.f}));

    ndde::SeekParticleBehavior::Params seek;
    seek.target = ndde::TargetSelector::nearest(ParticleRole::Leader);
    seek.speed = 1.f;

    ndde::Particle& chaser = system.spawn(system.factory().particle()
        .role(ParticleRole::Chaser)
        .at({1.f, 0.f})
        .with_behavior<ndde::SeekParticleBehavior>(seek));

    const float before = chaser.head_uv().x;
    system.update(0.1f, 1.f, 0.1f);
    EXPECT_GT(chaser.head_uv().x, before);
}

TEST(ParticleSystem, FieldCompositorContributesDriftToParticles) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 15u);
    ndde::simulation::FieldCompositor fields;
    fields.add(std::make_shared<ConstantWindField>());

    ndde::Particle& particle = system.spawn(system.factory().particle()
        .role(ParticleRole::Neutral)
        .at({0.f, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.f, 0.f}));

    system.update(0.1f, 1.f, 0.1f, &fields);

    EXPECT_GT(particle.head_uv().x, 0.f);
    EXPECT_NEAR(particle.head_uv().y, 0.f, 1e-6f);
}

TEST(ParticleSystem, UpdateContextDoesNotEscapePastUpdate) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 16u);
    ndde::simulation::FieldCompositor fields;
    fields.add(std::make_shared<ConstantDiffusionField>());

    ndde::Particle& particle = system.spawn(system.factory().particle()
        .role(ParticleRole::Neutral)
        .at({0.f, 0.f})
        .stochastic()
        .with_behavior<ndde::BrownianBehavior>(ndde::BrownianBehavior::Params{
            .sigma = 0.5f,
            .drift_strength = 0.f
        }));

    system.update(0.1f, 1.f, 0.1f, &fields);
    const glm::vec2 unbound_sigma =
        particle.equation()->noise_coefficient(particle.walk_state(), surface, 0.1f);
    EXPECT_EQ(unbound_sigma, glm::vec2(0.f, 0.f));

    ndde::SimulationContext stable_context(&surface, &system.particles(), &system.rng(), &fields);
    stable_context.set_time(0.1f);
    system.set_behavior_context(&stable_context);
    const glm::vec2 rebound_sigma =
        particle.equation()->noise_coefficient(particle.walk_state(), surface, 0.1f);
    EXPECT_EQ(rebound_sigma, glm::vec2(1.f, 1.5f));
}

TEST(ParticleSystem, AvoidBehaviorMovesAwayFromNearestRoleTarget) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 3u);

    system.spawn(system.factory().particle()
        .role(ParticleRole::Chaser)
        .at({3.f, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.f, 0.f}));

    ndde::AvoidParticleBehavior::Params avoid;
    avoid.target = ndde::TargetSelector::nearest(ParticleRole::Chaser);
    avoid.speed = 1.f;

    ndde::Particle& avoider = system.spawn(system.factory().particle()
        .role(ParticleRole::Avoider)
        .at({2.f, 0.f})
        .with_behavior<ndde::AvoidParticleBehavior>(avoid));

    const float before = avoider.head_uv().x;
    system.update(0.1f, 1.f, 0.1f);
    EXPECT_LT(avoider.head_uv().x, before);
}

TEST(ParticleSystem, CentroidSeekMovesTowardGroupCenter) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 4u);

    system.spawn(system.factory().particle()
        .role(ParticleRole::Chaser)
        .at({4.f, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.f, 0.f}));
    system.spawn(system.factory().particle()
        .role(ParticleRole::Chaser)
        .at({6.f, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.f, 0.f}));

    ndde::Particle& seeker = system.spawn(system.factory().particle()
        .role(ParticleRole::Leader)
        .at({1.f, 0.f})
        .with_behavior<ndde::CentroidSeekBehavior>(ndde::CentroidSeekBehavior::Params{
            .role = ParticleRole::Chaser,
            .speed = 1.f
        }));

    const float before = seeker.head_uv().x;
    system.update(0.1f, 1.f, 0.1f);
    EXPECT_GT(seeker.head_uv().x, before);
}

TEST(ParticleSystem, FiniteTrailTruncatesAndPersistentTrailDoesNot) {
    auto surface = flat_surface();
    ndde::ParticleSystem finite_system(&surface, 5u);
    ndde::Particle& finite = finite_system.spawn(finite_system.factory().particle()
        .role(ParticleRole::Neutral)
        .at({1.f, 0.f})
        .trail({TrailMode::Finite, 3u, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.2f, 0.f}));

    for (int i = 0; i < 12; ++i)
        finite_system.update(0.1f, 1.f, static_cast<float>(i + 1) * 0.1f);
    EXPECT_LE(finite.trail_vertex_count(), 3u);

    ndde::ParticleSystem persistent_system(&surface, 6u);
    ndde::Particle& persistent = persistent_system.spawn(persistent_system.factory().particle()
        .role(ParticleRole::Neutral)
        .at({1.f, 0.f})
        .trail({TrailMode::Persistent, 3u, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.2f, 0.f}));

    for (int i = 0; i < 12; ++i)
        persistent_system.update(0.1f, 1.f, static_cast<float>(i + 1) * 0.1f);
    EXPECT_GT(persistent.trail_vertex_count(), 3u);
}

TEST(ParticleSystem, TrailSamplesRetainParameterCoordinatesAndTimes) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 14u);
    ndde::Particle& particle = system.spawn(system.factory().particle()
        .role(ParticleRole::Neutral)
        .at({1.f, 0.f})
        .trail({TrailMode::Persistent, 16u, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.2f, 0.f}));

    system.update(0.1f, 1.f, 0.1f);
    system.update(0.1f, 1.f, 0.2f);

    ASSERT_GE(particle.trail_size(), 2u);
    const auto last = particle.trail_size() - 1u;
    EXPECT_NEAR(particle.trail_uv(last).x, particle.head_uv().x, 1e-5f);
    EXPECT_NEAR(particle.trail_uv(last).y, particle.head_uv().y, 1e-5f);
    EXPECT_FLOAT_EQ(particle.trail_time(last), 0.2f);
    EXPECT_EQ(particle.trail_pt(last), particle.trail_sample(last).world);
}

TEST(ParticleSwarmFactory, BrownianCloudSpawnsRequestedCount) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 7u);
    ndde::ParticleSwarmFactory swarms(system);

    const ndde::SwarmBuildResult result = swarms.brownian_cloud({
        .count = 12u,
        .center = {0.f, 0.f},
        .radius = 1.f,
        .role = ParticleRole::Avoider,
        .label = "Test Cloud"
    });

    EXPECT_EQ(result.size(), 12u);
    EXPECT_EQ(result.metadata.family_name, "Test Cloud");
    EXPECT_EQ(result.metadata.requested_count, 12u);
    EXPECT_EQ(result.metadata.roles_label(), "Avoider");
    EXPECT_FALSE(result.metadata.goals_added);
    EXPECT_EQ(system.size(), 12u);
    EXPECT_EQ(system.particles().front().particle_role(), ParticleRole::Avoider);
    EXPECT_NE(system.particles().front().metadata_label().find("Brownian"), std::string::npos);
}

TEST(ParticleSwarmFactory, LeaderPursuitAddsLeadersChasersAndCaptureGoal) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 8u);
    ndde::ParticleSwarmFactory swarms(system);

    const ndde::SwarmBuildResult result = swarms.leader_pursuit({
        .leader_count = 2u,
        .chaser_count = 5u,
        .center = {0.f, 0.f},
        .capture_radius = 0.25f
    });

    EXPECT_EQ(result.size(), 7u);
    EXPECT_EQ(result.metadata.family_name, "Leader Pursuit");
    EXPECT_EQ(result.metadata.requested_count, 7u);
    EXPECT_EQ(result.metadata.roles_label(), "Leader, Chaser");
    EXPECT_TRUE(result.metadata.goals_added);
    EXPECT_EQ(system.size(), 7u);
    EXPECT_EQ(system.particles()[0].particle_role(), ParticleRole::Leader);
    EXPECT_EQ(system.particles()[2].particle_role(), ParticleRole::Chaser);
    EXPECT_EQ(system.evaluate_goals(0.f), ndde::GoalStatus::Running);
}

TEST(ParticleSwarmFactory, ContourBandBuildsLevelCurveParticles) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 9u);
    ndde::ParticleSwarmFactory swarms(system);

    ndde::sim::LevelCurveWalker::Params walker;
    walker.epsilon = 0.20f;
    walker.walk_speed = 0.4f;

    const ndde::SwarmBuildResult result = swarms.contour_band({
        .count = 6u,
        .center = {0.f, 0.f},
        .radius = 0.8f,
        .role = ParticleRole::Leader,
        .walker = walker,
        .label = "Test Contour"
    });

    EXPECT_EQ(result.size(), 6u);
    EXPECT_EQ(result.metadata.family_name, "Test Contour");
    EXPECT_EQ(result.metadata.roles_label(), "Leader");
    EXPECT_EQ(system.size(), 6u);
    EXPECT_NE(system.particles().front().metadata_label().find("LevelCurveWalker"), std::string::npos);
}

TEST(ParticleSystem, GradientDriftMovesDownhillOnParaboloid) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 10u);

    ndde::Particle& particle = system.spawn(system.factory().particle()
        .role(ParticleRole::Neutral)
        .at({2.f, 0.f})
        .with_behavior<ndde::GradientDriftBehavior>(ndde::GradientDriftBehavior::Params{
            .mode = ndde::GradientDriftBehavior::Mode::Downhill,
            .speed = 1.f
        }));

    const float before = particle.head_uv().x;
    system.update(0.1f, 1.f, 0.1f);
    EXPECT_LT(particle.head_uv().x, before);
}

TEST(ParticleSystem, OrbitBehaviorCorrectsTowardRadius) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 11u);

    ndde::Particle& particle = system.spawn(system.factory().particle()
        .role(ParticleRole::Neutral)
        .at({3.f, 0.f})
        .with_behavior<ndde::OrbitBehavior>(ndde::OrbitBehavior::Params{
            .center = {0.f, 0.f},
            .radius = 1.f,
            .angular_speed = 0.f,
            .radial_strength = 1.f
        }));

    const float before = particle.head_uv().x;
    system.update(0.1f, 1.f, 0.1f);
    EXPECT_LT(particle.head_uv().x, before);
}

TEST(ParticleSystem, SlopeVelocityTransformScalesFinalVelocityPerParticle) {
    auto surface = ndde::math::Paraboloid(0.25f, 10.f, -10.f, 10.f);

    ndde::ParticleSystem baseline_system(&surface, 12u);
    ndde::Particle& baseline = baseline_system.spawn(baseline_system.factory().particle()
        .role(ParticleRole::Neutral)
        .at({1.f, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{1.f, 0.f}));

    ndde::ParticleSystem transformed_system(&surface, 13u);
    ndde::Particle& transformed = transformed_system.spawn(transformed_system.factory().particle()
        .role(ParticleRole::Neutral)
        .at({1.f, 0.f})
        .slope_velocity_transform(ndde::SlopeVelocityTransform{
            .enabled = true,
            .intercept = 1.f,
            .slope_gain = 1.f,
            .min_scale = 0.1f,
            .max_scale = 3.f
        })
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{1.f, 0.f}));

    baseline_system.update(0.1f, 1.f, 0.1f);
    transformed_system.update(0.1f, 1.f, 0.1f);

    EXPECT_GT(transformed.head_uv().x - 1.f, baseline.head_uv().x - 1.f);
    EXPECT_NE(transformed.metadata_label().find("Slope Velocity Transform"), std::string::npos);
}

TEST(ParticleSwarmFactory, GradientAvoidCentroidOrbitAndFlockingFamiliesBuild) {
    auto surface = flat_surface();
    ndde::ParticleSystem system(&surface, 12u);
    ndde::ParticleSwarmFactory swarms(system);

    const auto gradient = swarms.gradient_drift({
        .count = 4u,
        .center = {0.f, 0.f},
        .radius = 0.6f,
        .gradient = {.mode = ndde::GradientDriftBehavior::Mode::LevelTangent, .speed = 0.4f},
        .label = "Test Gradient"
    });

    system.spawn(system.factory().particle()
        .role(ParticleRole::Chaser)
        .at({0.f, 0.f})
        .with_behavior<ndde::ConstantDriftBehavior>(glm::vec2{0.f, 0.f}));

    const auto avoid = swarms.avoidance_swarm({
        .count = 3u,
        .center = {0.f, 0.f},
        .radius = 0.8f,
        .avoid_role = ParticleRole::Chaser,
        .label = "Test Avoid"
    });

    const auto centroid = swarms.centroid_swarm({
        .count = 3u,
        .center = {1.f, 0.f},
        .radius = 0.7f,
        .role = ParticleRole::Leader,
        .target_role = ParticleRole::Chaser,
        .label = "Test Centroid"
    });

    const auto orbit = swarms.ring_orbit({
        .count = 5u,
        .center = {0.f, 0.f},
        .radius = 1.1f,
        .label = "Test Orbit"
    });

    const auto flock = swarms.flocking_swarm({
        .count = 5u,
        .center = {0.f, 0.f},
        .radius = 1.2f,
        .role = ParticleRole::Neutral,
        .label = "Test Flock"
    });

    EXPECT_EQ(gradient.size(), 4u);
    EXPECT_EQ(avoid.size(), 3u);
    EXPECT_EQ(centroid.size(), 3u);
    EXPECT_EQ(orbit.size(), 5u);
    EXPECT_EQ(flock.size(), 5u);
    EXPECT_EQ(gradient.metadata.family_name, "Test Gradient");
    EXPECT_EQ(avoid.metadata.roles_label(), "Avoider");
    EXPECT_EQ(centroid.metadata.roles_label(), "Leader");
    EXPECT_EQ(orbit.metadata.family_name, "Test Orbit");
    EXPECT_EQ(flock.metadata.family_name, "Test Flock");

    bool saw_gradient = false, saw_avoid = false, saw_centroid = false, saw_orbit = false, saw_flock = false;
    for (const ndde::Particle& particle : system.particles()) {
        const std::string label = particle.metadata_label();
        saw_gradient = saw_gradient || label.find("Gradient Level Tangent") != std::string::npos;
        saw_avoid = saw_avoid || label.find("Avoid") != std::string::npos;
        saw_centroid = saw_centroid || label.find("Centroid Seek") != std::string::npos;
        saw_orbit = saw_orbit || label.find("Orbit") != std::string::npos;
        saw_flock = saw_flock || label.find("Flocking") != std::string::npos;
    }
    EXPECT_TRUE(saw_gradient);
    EXPECT_TRUE(saw_avoid);
    EXPECT_TRUE(saw_centroid);
    EXPECT_TRUE(saw_orbit);
    EXPECT_TRUE(saw_flock);
}
