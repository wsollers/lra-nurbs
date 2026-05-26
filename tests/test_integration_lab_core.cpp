#include "math/integration/Integration1D.hpp"

#include <gtest/gtest.h>
#include <cmath>

namespace {

using namespace ndde;
using namespace ndde::math::integration;

TEST(Integration1D, UniformPartitionCoversInterval) {
    const auto partition = make_uniform_partition(Interval1D{.min = -1.0, .max = 1.0},
                                                  UniformPartitionConfig{.cell_count = 4u});
    ASSERT_TRUE(partition.has_value());
    ASSERT_EQ(partition->size(), 4u);
    EXPECT_DOUBLE_EQ((*partition)[0].a, -1.0);
    EXPECT_DOUBLE_EQ((*partition)[0].b, -0.5);
    EXPECT_DOUBLE_EQ((*partition)[3].a, 0.5);
    EXPECT_DOUBLE_EQ((*partition)[3].b, 1.0);
}

TEST(Integration1D, MidpointIntegratesQuadraticAccuratelyWithSmallError) {
    const auto result = integrate_uniform(
        Interval1D{.min = 0.0, .max = 1.0},
        [](f64 x) { return x * x; },
        UniformPartitionConfig{.cell_count = 128u},
        QuadratureMethod::Midpoint,
        1.0 / 3.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->estimate, 1.0 / 3.0, 1.0e-5);
    ASSERT_TRUE(result->absolute_error.has_value());
    EXPECT_LT(*result->absolute_error, 1.0e-5);
    EXPECT_EQ(result->contributions.size(), 128u);
}

TEST(Integration1D, SimpsonRequiresEvenCellCount) {
    const auto result = integrate_uniform(
        Interval1D{.min = 0.0, .max = 1.0},
        [](f64 x) { return x; },
        UniformPartitionConfig{.cell_count = 3u},
        QuadratureMethod::Simpson);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), IntegrationError::SimpsonRequiresEvenCellCount);
}

TEST(Integration1D, SimpsonIntegratesCubicExactlyWithinFloatingPointTolerance) {
    const auto result = integrate_uniform(
        Interval1D{.min = -1.0, .max = 2.0},
        [](f64 x) { return (x * x * x) - x; },
        UniformPartitionConfig{.cell_count = 32u},
        QuadratureMethod::Simpson,
        2.25);

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->estimate, 2.25, 1.0e-12);
}

TEST(Integration1D, CentralDifferenceApproximatesDerivative) {
    const auto derivative = central_difference(
        [](f64 x) { return std::sin(x); },
        0.25);

    EXPECT_NEAR(derivative, std::cos(0.25), 1.0e-8);
}

} // namespace
