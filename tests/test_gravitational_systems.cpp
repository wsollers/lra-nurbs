#include "sim/DifferentialSystem.hpp"
#include "memory/MemoryService.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

namespace {

using namespace ndde;

TEST(GravitationalSystems, SimplePendulumConservesEnergyWithRk4OverShortRun) {
    memory::MemoryService memory;
    sim::SimplePendulumSystem system{9.80665, 1.0, 0.0};
    sim::Rk4OdeSolver solver;
    const std::array<f64, 2> initial{0.35, 0.0};
    sim::InitialValueProblem problem{memory, system, initial, 0.0};

    const f64 e0 = system.energy(problem.state());
    for (int i = 0; i < 1000; ++i)
        problem.step(solver, 0.001);
    const f64 e1 = system.energy(problem.state());

    EXPECT_NEAR(e1, e0, 1.0e-8);
}

TEST(GravitationalSystems, TwoBodyCircularOrbitReturnsNearStart) {
    memory::MemoryService memory;
    sim::PlanarNBodyGravitySystem system{{1.0, 1.0}, 1.0, 0.0};
    sim::Rk4OdeSolver solver;

    // Equal masses at +/-0.5 orbit their barycentre with angular speed sqrt(2).
    const f64 omega = std::sqrt(2.0);
    const std::array<f64, 8> initial{
        -0.5, 0.0, 0.0, -0.5 * omega,
         0.5, 0.0, 0.0,  0.5 * omega
    };
    sim::InitialValueProblem problem{memory, system, initial, 0.0};
    const f64 e0 = system.total_energy(problem.state());
    const f64 period = 2.0 * std::acos(-1.0) / omega;

    constexpr int steps = 4000;
    const f64 dt = period / static_cast<f64>(steps);
    for (int i = 0; i < steps; ++i)
        problem.step(solver, dt);

    const auto state = problem.state();
    EXPECT_NEAR(state[0], initial[0], 2.0e-5);
    EXPECT_NEAR(state[1], initial[1], 2.0e-5);
    EXPECT_NEAR(state[4], initial[4], 2.0e-5);
    EXPECT_NEAR(state[5], initial[5], 2.0e-5);
    EXPECT_NEAR(system.total_energy(state), e0, 1.0e-8);
}

TEST(GravitationalSystems, NBodyMetadataMatchesStateLayout) {
    sim::PlanarNBodyGravitySystem system{{1.0, 0.001, 0.0001}, 39.47841760435743, 1.0e-4};
    const sim::EquationSystemMetadata metadata = system.metadata();

    EXPECT_EQ(metadata.name, "Planar N-body gravity");
    EXPECT_EQ(system.body_count(), 3u);
    EXPECT_EQ(system.dimension(), 12u);
    EXPECT_FALSE(metadata.formula.empty());
    EXPECT_FALSE(metadata.variables.empty());
}

} // namespace
