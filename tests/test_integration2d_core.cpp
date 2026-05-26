#include "math/integration/Integration2D.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>

namespace {

using namespace ndde;
using namespace ndde::math::integration;

TEST(Integration2D, UniformGridCoversRectangleExactly) {
    const RectDomain2D domain{.x_min = -2.0, .x_max = 2.0, .y_min = -1.0, .y_max = 3.0};
    const auto grid = make_uniform_grid(domain, UniformGrid2DConfig{.x_cells = 4u, .y_cells = 2u});

    ASSERT_TRUE(grid.has_value());
    ASSERT_EQ(grid->cells.size(), 8u);
    EXPECT_DOUBLE_EQ(grid->cells.front().x0, -2.0);
    EXPECT_DOUBLE_EQ(grid->cells.front().y0, -1.0);
    EXPECT_DOUBLE_EQ(grid->cells.back().x1, 2.0);
    EXPECT_DOUBLE_EQ(grid->cells.back().y1, 3.0);

    f64 measure_sum = f64(0);
    for (const Cell2D& cell : grid->cells) {
        measure_sum += cell.measure();
    }
    EXPECT_DOUBLE_EQ(measure_sum, domain.area());
}

TEST(Integration2D, RejectsInvalidDomainAndGrid) {
    EXPECT_FALSE(make_uniform_grid(RectDomain2D{.x_min = 1.0, .x_max = 0.0},
                                   UniformGrid2DConfig{.x_cells = 4u, .y_cells = 4u}));
    EXPECT_FALSE(make_uniform_grid(RectDomain2D{},
                                   UniformGrid2DConfig{.x_cells = 0u, .y_cells = 4u}));
}

TEST(Integration2D, MidpointIntegratesConstantExactly) {
    const auto result = integrate_uniform_grid(
        RectDomain2D{.x_min = -2.0, .x_max = 2.0, .y_min = -3.0, .y_max = 1.0},
        [](f64, f64) { return f64(2.5); },
        UniformGrid2DConfig{.x_cells = 8u, .y_cells = 6u},
        IntegrationMethod2D::Midpoint,
        40.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->estimate, 40.0);
    ASSERT_TRUE(result->absolute_error.has_value());
    EXPECT_LT(*result->absolute_error, 1.0e-12);
    EXPECT_EQ(result->evaluation_count, 48u);
    EXPECT_EQ(result->contributions.size(), 48u);
}

TEST(Integration2D, ContributionsSumToEstimate) {
    const auto problem = IntegrationProblem2DBuilder{}
        .rectangle(RectDomain2D{.x_min = -2.0, .x_max = 2.0, .y_min = -2.0, .y_max = 2.0})
        .integrand(IntegrandPreset2D::Gaussian)
        .uniform_grid(UniformGrid2DConfig{.x_cells = 16u, .y_cells = 16u})
        .method(IntegrationMethod2D::Midpoint)
        .build();

    ASSERT_TRUE(problem.has_value());
    const auto result = integrate_uniform_grid(*problem);

    ASSERT_TRUE(result.has_value());
    f64 contribution_sum = f64(0);
    for (const CellContribution2D& contribution : result->contributions) {
        contribution_sum += contribution.contribution;
    }
    EXPECT_NEAR(contribution_sum, result->estimate, 1.0e-12);
    ASSERT_TRUE(result->absolute_error.has_value());
    EXPECT_LT(*result->absolute_error, 0.02);
}

TEST(Integration2D, FluentBuilderValidatesInputs) {
    const auto invalid_domain = IntegrationProblem2DBuilder{}
        .rectangle(RectDomain2D{.x_min = 1.0, .x_max = 1.0})
        .build();
    ASSERT_FALSE(invalid_domain.has_value());
    EXPECT_EQ(invalid_domain.error(), IntegrationError::InvalidDomain);

    const auto invalid_grid = IntegrationProblem2DBuilder{}
        .uniform_grid(UniformGrid2DConfig{.x_cells = 0u, .y_cells = 4u})
        .build();
    ASSERT_FALSE(invalid_grid.has_value());
    EXPECT_EQ(invalid_grid.error(), IntegrationError::InvalidPartition);
}

TEST(Integration2D, PresetReferencesMatchSimpleDomains) {
    const RectDomain2D domain{.x_min = -1.0, .x_max = 1.0, .y_min = -1.0, .y_max = 1.0};
    const auto polynomial = IntegrationProblem2DBuilder{}
        .rectangle(domain)
        .integrand(IntegrandPreset2D::Polynomial)
        .uniform_grid(UniformGrid2DConfig{.x_cells = 8u, .y_cells = 8u})
        .build();

    ASSERT_TRUE(polynomial.has_value());
    ASSERT_TRUE(polynomial->reference_value.has_value());
    EXPECT_DOUBLE_EQ(*polynomial->reference_value, 2.0);
}

TEST(Integration2D, CellIdAtMapsDomainCoordinateToGridCell) {
    const RectDomain2D domain{.x_min = -2.0, .x_max = 2.0, .y_min = -2.0, .y_max = 2.0};
    const UniformGrid2DConfig config{.x_cells = 4u, .y_cells = 4u};

    const auto center = cell_id_at(domain, config, 0.1, 0.1);
    ASSERT_TRUE(center.has_value());
    EXPECT_EQ(*center, 10u);

    const auto upper_edge = cell_id_at(domain, config, 2.0, 2.0);
    ASSERT_TRUE(upper_edge.has_value());
    EXPECT_EQ(*upper_edge, 15u);

    EXPECT_FALSE(cell_id_at(domain, config, 2.1, 0.0).has_value());
}

TEST(Integration2D, SmoothGaussianConvergesUnderRefinement) {
    const auto problem = IntegrationProblem2DBuilder{}
        .rectangle(RectDomain2D{.x_min = -2.0, .x_max = 2.0, .y_min = -2.0, .y_max = 2.0})
        .integrand(IntegrandPreset2D::Gaussian)
        .method(IntegrationMethod2D::Midpoint)
        .build();
    ASSERT_TRUE(problem.has_value());

    constexpr std::array<UniformGrid2DConfig, 4> grids{{
        {.x_cells = 8u, .y_cells = 8u},
        {.x_cells = 16u, .y_cells = 16u},
        {.x_cells = 32u, .y_cells = 32u},
        {.x_cells = 64u, .y_cells = 64u},
    }};

    const auto series = compute_convergence_series(*problem, grids);
    ASSERT_TRUE(series.has_value());
    ASSERT_EQ(series->rows.size(), grids.size());
    ASSERT_TRUE(series->rows.front().absolute_error.has_value());
    ASSERT_TRUE(series->rows.back().absolute_error.has_value());
    EXPECT_LT(*series->rows.back().absolute_error, *series->rows.front().absolute_error);
}

} // namespace
