// tests/test_simulation_context.cpp
// GaussianSurface archived — using WavePredatorPreySurface instead.

#include "app/SimulationContext.hpp"
#include "app/AnimatedCurve.hpp"        // needed: SimVector<AnimatedCurve> requires complete type
#include "app/SurfaceRegistry.hpp"
#include "memory/Containers.hpp"

#include <gtest/gtest.h>
#include <random>

namespace {

using namespace ndde;

TEST(SimulationContextState, TracksTickTimeAndDirtyState) {
    SimulationContext context;
    context.set_tick(TickInfo{.tick_index = 7u, .dt = 0.125f, .time = 2.5f, .paused = false});

    EXPECT_EQ(context.tick().tick_index, 7u);
    EXPECT_FLOAT_EQ(context.tick().dt, 0.125f);
    EXPECT_FLOAT_EQ(context.time(), 2.5f);
    EXPECT_FALSE(context.dirty().any());

    context.dirty().mark_surface_changed();
    EXPECT_TRUE(context.dirty().surface);
    EXPECT_TRUE(context.dirty().main_view);
    EXPECT_TRUE(context.dirty().alternate_view);
    EXPECT_TRUE(context.dirty().hover_math);

    context.clear_frame_state();
    EXPECT_FALSE(context.dirty().any());
    EXPECT_FALSE(context.has_pending_perturbations());
}

TEST(SimulationContextState, SurfacePerturbationMarksSurfaceAndViewsDirty) {
    SimulationContext context;
    const u64 revision = context.math_cache().surface_revision;

    context.queue_perturbation(SurfacePerturbation{
        .uv = {1.f, -0.5f}, .amplitude = 0.2f,
        .radius = 0.4f, .falloff = 1.5f, .seed = 42u
    });

    ASSERT_TRUE(context.has_pending_perturbations());
    EXPECT_EQ(context.commands().surface_perturbations.size(), 1u);
    EXPECT_TRUE(context.dirty().surface);
    EXPECT_TRUE(context.dirty().main_view);
    EXPECT_TRUE(context.dirty().alternate_view);
    EXPECT_GT(context.math_cache().surface_revision, revision);
}

TEST(SimulationContextState, MaintainsLegacySurfaceParticleRngView) {
    WavePredatorPreySurface surface;
    // AnimatedCurve.hpp included above — type is complete, vector is valid
    memory::SimVector<AnimatedCurve> particles{std::pmr::get_default_resource()};
    std::mt19937 rng(123u);
    SimulationContext context(&surface, &particles, &rng);

    EXPECT_TRUE(context.has_surface());
    EXPECT_TRUE(context.has_particles());
    EXPECT_TRUE(context.has_rng());
    EXPECT_EQ(&context.surface(), &surface);
    EXPECT_EQ(&context.rng(), &rng);
    EXPECT_TRUE(context.particles().empty());
}

} // namespace
