#include "simulation/context/SimulationContext.hpp"
#include "simulation/context/WorkbenchBuildContext.hpp"

#include <gtest/gtest.h>

#include <cmath>

using namespace ndde;
using namespace ndde::sim;

TEST(SimulationContextObjects, StoresMultipleCurvesAndOverlaysBehindTypedHandles) {
    SimulationContext context(SimulationContextDescriptor{.name = "Function graph"});

    const OverlayHandle overlay = context.add_overlay(OverlayDescriptor{
        .name = "Cartesian",
        .kind = "coordinate.cartesian2d",
        .x_label = "x",
        .y_label = "y",
        .dynamic_gradations = true
    });
    const CurveHandle sin_curve = context.add_curve(CurveDescriptor{
        .name = "f(x)",
        .formula = "sin(x)",
        .evaluate = [](f32 x) { return std::sin(x); }
    });
    const CurveHandle cos_curve = context.add_curve(CurveDescriptor{
        .name = "f'(x)",
        .formula = "cos(x)",
        .evaluate = [](f32 x) { return std::cos(x); }
    });

    ASSERT_TRUE(overlay);
    ASSERT_TRUE(sin_curve);
    ASSERT_TRUE(cos_curve);
    EXPECT_NE(sin_curve, cos_curve);
    ASSERT_NE(context.overlay(overlay), nullptr);
    ASSERT_NE(context.curve(sin_curve), nullptr);
    ASSERT_NE(context.curve(cos_curve), nullptr);
    EXPECT_EQ(context.curve(sin_curve)->formula, "sin(x)");
    EXPECT_EQ(context.curve(cos_curve)->formula, "cos(x)");

    CurveHandle stale = sin_curve;
    ++stale.generation;
    EXPECT_EQ(context.curve(stale), nullptr);
}

TEST(WorkbenchBuildContext, AssemblesMultipleSimulationContextsWithIndependentObjects) {
    WorkbenchBuildContext build;
    const SimulationContextHandle surface_sim = build.add_simulation({.name = "Surface"});
    const SimulationContextHandle particle_sim = build.add_simulation({.name = "Particles"});

    const SurfaceHandle surface = build.add_surface(surface_sim, SurfaceDescriptor{.name = "Deformable surface"});
    const ParticleSystemHandle particles = build.add_particles(particle_sim, ParticleSystemDescriptor{
        .name = "Stochastic particles",
        .surface = surface
    });

    ASSERT_TRUE(surface_sim);
    ASSERT_TRUE(particle_sim);
    ASSERT_TRUE(surface);
    ASSERT_TRUE(particles);
    ASSERT_NE(build.context(surface_sim), nullptr);
    ASSERT_NE(build.context(particle_sim), nullptr);
    EXPECT_EQ(build.simulation_count(), 2u);
    EXPECT_EQ(build.context(surface_sim)->surfaces().size(), 1u);
    EXPECT_TRUE(build.context(particle_sim)->surfaces().empty());
}
