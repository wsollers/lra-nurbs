#include "sim/DifferentialSystem.hpp"
#include "sim/DelayDifferentialSystem.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <span>

namespace {

using namespace ndde;

class HarmonicOscillatorSystem final : public sim::IDifferentialSystem {
public:
    [[nodiscard]] sim::EquationSystemMetadata metadata() const override {
        return {
            .name = "Harmonic oscillator",
            .formula = "x' = v, v' = -x",
            .variables = "x, v"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 2u; }

    void evaluate(f64, std::span<const f64> state, std::span<f64> derivative) const override {
        derivative[0] = state[1];
        derivative[1] = -state[0];
    }
};

class LinearSystem2D final : public sim::IDifferentialSystem {
public:
    [[nodiscard]] sim::EquationSystemMetadata metadata() const override {
        return {
            .name = "Linear saddle",
            .formula = "x' = x, y' = -2y",
            .variables = "x, y"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 2u; }

    void evaluate(f64, std::span<const f64> state, std::span<f64> derivative) const override {
        derivative[0] = state[0];
        derivative[1] = -2.0 * state[1];
    }
};

void constant_history_one(f64, std::span<f64> out, void*) {
    out[0] = 1.0;
}

TEST(DifferentialSystem, Rk4SolvesExponentialGrowthInitialValueProblem) {
    memory::MemoryService memory;
    sim::ExponentialGrowthSystem system{1.0};
    sim::Rk4OdeSolver solver;
    const f64 initial[] = {1.0};
    sim::InitialValueProblem problem{memory, system, initial, 0.0};

    for (int i = 0; i < 10; ++i)
        problem.step(solver, 0.1);

    ASSERT_EQ(problem.dimension(), 1u);
    EXPECT_NEAR(problem.state()[0], std::exp(1.0), 2.5e-6);
    EXPECT_EQ(problem.history_size(), 11u);
    EXPECT_NEAR(problem.history_time(10), 1.0, 1e-12);
    EXPECT_NEAR(problem.history_state(0)[0], 1.0, 1e-12);
}

TEST(DifferentialSystem, EulerSolvesExponentialGrowthLessAccuratelyButInRightDirection) {
    memory::MemoryService memory;
    sim::ExponentialGrowthSystem system{1.0};
    sim::EulerOdeSolver solver;
    const f64 initial[] = {1.0};
    sim::InitialValueProblem problem{memory, system, initial, 0.0};

    for (int i = 0; i < 10; ++i)
        problem.step(solver, 0.1);

    EXPECT_NEAR(problem.state()[0], std::pow(1.1, 10.0), 1e-12);
    EXPECT_LT(problem.state()[0], std::exp(1.0));
}

TEST(DifferentialSystem, Rk4PreservesHarmonicOscillatorEnergyOverShortRun) {
    memory::MemoryService memory;
    HarmonicOscillatorSystem system;
    sim::Rk4OdeSolver solver;
    const f64 initial[] = {1.0, 0.0};
    sim::InitialValueProblem problem{memory, system, initial, 0.0};

    for (int i = 0; i < 100; ++i)
        problem.step(solver, 0.01);

    const f64 x = problem.state()[0];
    const f64 v = problem.state()[1];
    const f64 energy = 0.5 * (x * x + v * v);
    EXPECT_NEAR(energy, 0.5, 1e-9);
    EXPECT_NEAR(x, std::cos(1.0), 1e-9);
    EXPECT_NEAR(v, -std::sin(1.0), 1e-9);
}

TEST(DifferentialSystem, Rk4SolvesTwoDimensionalLinearSystem) {
    memory::MemoryService memory;
    LinearSystem2D system;
    sim::Rk4OdeSolver solver;
    const f64 initial[] = {1.0, 1.0};
    sim::InitialValueProblem problem{memory, system, initial, 0.0};

    for (int i = 0; i < 20; ++i)
        problem.step(solver, 0.05);

    EXPECT_NEAR(problem.state()[0], std::exp(1.0), 2.5e-7);
    EXPECT_NEAR(problem.state()[1], std::exp(-2.0), 2.5e-7);
}

TEST(DifferentialSystem, LorenzSystemReportsThreeDimensionalDerivative) {
    sim::LorenzSystem system{10.0, 28.0, 8.0 / 3.0};
    const f64 state[] = {1.0, 1.0, 1.0};
    f64 derivative[] = {0.0, 0.0, 0.0};

    system.evaluate(0.0, state, derivative);

    const sim::EquationSystemMetadata metadata = system.metadata();
    EXPECT_EQ(metadata.name, "Lorenz attractor");
    EXPECT_EQ(metadata.variables, "x, y, z");
    EXPECT_EQ(system.dimension(), 3u);
    EXPECT_NEAR(derivative[0], 0.0, 1e-12);
    EXPECT_NEAR(derivative[1], 26.0, 1e-12);
    EXPECT_NEAR(derivative[2], -5.0 / 3.0, 1e-12);
}

TEST(DifferentialSystem, LorenzPerturbedPairDivergesFromNearbyInitialCondition) {
    memory::MemoryService memory;
    sim::LorenzSystem system{10.0, 28.0, 8.0 / 3.0};
    sim::Rk4OdeSolver solver;
    const f64 initial[] = {1.0, 1.0, 1.0};
    const f64 perturbed[] = {1.001, 1.0, 1.0};
    sim::InitialValueProblem base{memory, system, initial, 0.0};
    sim::InitialValueProblem twin{memory, system, perturbed, 0.0};

    for (int i = 0; i < 500; ++i) {
        base.step(solver, 0.01);
        twin.step(solver, 0.01);
    }

    const auto a = base.state();
    const auto b = twin.state();
    const f64 dx = b[0] - a[0];
    const f64 dy = b[1] - a[1];
    const f64 dz = b[2] - a[2];
    const f64 final_distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    EXPECT_GT(final_distance, 1.0e-3);
    EXPECT_EQ(base.history_size(), twin.history_size());
}

TEST(DifferentialSystem, ProblemBuffersBindToMemoryServiceScopes) {
    memory::MemoryService memory;
    sim::ExponentialGrowthSystem system{1.0};
    sim::Rk4OdeSolver solver;
    const f64 initial[] = {1.0};
    sim::InitialValueProblem problem{memory, system, initial, 0.0};

    problem.step(solver, 0.1);

    EXPECT_EQ(problem.history_size(), 2u);
    EXPECT_EQ(problem.initial_state()[0], 1.0);
    EXPECT_GT(problem.state()[0], problem.initial_state()[0]);
}

TEST(DelayDifferentialSystem, MetadataAndMaxDelayAreAvailable) {
    sim::LinearDelaySystem system{0.25, 0.75, 1.5};
    const sim::EquationSystemMetadata metadata = system.metadata();

    EXPECT_EQ(metadata.name, "Linear delay");
    EXPECT_EQ(metadata.formula, "y' = a y(t) + b y(t - tau)");
    EXPECT_EQ(metadata.variables, "y");
    EXPECT_EQ(system.dimension(), 1u);
    EXPECT_DOUBLE_EQ(system.max_delay(), 1.5);
}

TEST(DelayDifferentialSystem, EulerUsesInitialHistoryFunctionBeforeDelayWindowExpires) {
    memory::MemoryService memory;
    sim::LinearDelaySystem system{0.0, 1.0, 1.0};
    sim::EulerDdeSolver solver;
    const f64 initial[] = {1.0};
    sim::DelayInitialValueProblem problem{
        memory,
        system,
        initial,
        0.0,
        0.05,
        constant_history_one,
        nullptr
    };

    for (int i = 0; i < 5; ++i)
        problem.step(solver, 0.1);

    ASSERT_EQ(problem.dimension(), 1u);
    EXPECT_NEAR(problem.state()[0], 1.5, 1e-12);
    EXPECT_NEAR(problem.time(), 0.5, 1e-12);
    EXPECT_GT(problem.history_size(), 5u);
}

TEST(DelayDifferentialSystem, EulerQueriesComputedHistoryAfterDelayWindow) {
    memory::MemoryService memory;
    sim::LinearDelaySystem system{0.0, 1.0, 1.0};
    sim::EulerDdeSolver solver;
    const f64 initial[] = {1.0};
    sim::DelayInitialValueProblem problem{
        memory,
        system,
        initial,
        0.0,
        0.01,
        constant_history_one,
        nullptr
    };

    for (int i = 0; i < 150; ++i)
        problem.step(solver, 0.01);

    EXPECT_NEAR(problem.state()[0], 2.6225, 2e-3);
    EXPECT_NEAR(problem.time(), 1.5, 1e-12);
}

TEST(DelayDifferentialSystem, QueryHistoryInterpolatesStoredSamples) {
    memory::MemoryService memory;
    sim::LinearDelaySystem system{0.0, 1.0, 1.0};
    sim::EulerDdeSolver solver;
    const f64 initial[] = {1.0};
    sim::DelayInitialValueProblem problem{
        memory,
        system,
        initial,
        0.0,
        0.1,
        constant_history_one,
        nullptr
    };

    problem.step(solver, 0.1);
    problem.step(solver, 0.1);

    f64 interpolated = 0.0;
    problem.query_history(0.15, std::span<f64>{&interpolated, 1u});

    EXPECT_NEAR(interpolated, 1.15, 1e-12);
}

TEST(DelayDifferentialSystem, ProblemBuffersBindToMemoryServiceScopes) {
    memory::MemoryService memory;
    sim::LinearDelaySystem system{0.0, 1.0, 0.25};
    sim::EulerDdeSolver solver;
    const f64 initial[] = {2.0};
    sim::DelayInitialValueProblem problem{memory, system, initial, 0.0, 0.05};

    problem.step(solver, 0.05);

    EXPECT_EQ(problem.initial_state()[0], 2.0);
    EXPECT_GT(problem.state()[0], problem.initial_state()[0]);
    EXPECT_GT(problem.history_size(), 1u);
    EXPECT_NEAR(problem.history_state(0)[0], 2.0, 1e-12);
}

TEST(DelayDifferentialSystem, BoundedDelayedFeedbackStaysWithinExpectedEnvelope) {
    memory::MemoryService memory;
    sim::BoundedDelayedFeedbackSystem system{0.8, 1.4, 1.2};
    sim::EulerDdeSolver solver;
    const f64 initial[] = {0.75};
    sim::DelayInitialValueProblem problem{
        memory,
        system,
        initial,
        0.0,
        0.02,
        constant_history_one,
        nullptr
    };

    f64 max_abs = 0.0;
    for (int i = 0; i < 3000; ++i) {
        problem.step(solver, 0.01);
        max_abs = std::max(max_abs, std::abs(problem.state()[0]));
    }

    EXPECT_LT(max_abs, system.expected_bound() + 0.2);
    EXPECT_GT(problem.history_size(), 3000u);
}

TEST(DelayDifferentialSystem, RuntimeHistoryIsCompactedToBoundedWindow) {
    memory::MemoryService memory;
    sim::BoundedDelayedFeedbackSystem system{0.8, 1.4, 1.2};
    sim::EulerDdeSolver solver;
    const f64 initial[] = {0.75};
    sim::DelayInitialValueProblem problem{
        memory,
        system,
        initial,
        0.0,
        0.02,
        constant_history_one,
        nullptr
    };

    for (int i = 0; i < 6000; ++i)
        problem.step(solver, 0.01);

    EXPECT_LE(problem.history_size(), 4096u);

    f64 delayed = 0.0;
    problem.query_history(problem.time() - system.max_delay(), std::span<f64>{&delayed, 1u});
    EXPECT_TRUE(std::isfinite(delayed));
}

} // namespace
