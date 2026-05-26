#include "app/IntegrationWorkbenchState.hpp"

#include <gtest/gtest.h>

namespace {

using namespace ndde;
using namespace ndde::math::integration;

TEST(IntegrationWorkbenchState, DefaultStateProducesValidGaussianSnapshot) {
    IntegrationWorkbenchState state;

    const IntegrationWorkbenchSnapshot snapshot = state.snapshot();
    EXPECT_FALSE(snapshot.metadata.has_error);
    EXPECT_EQ(snapshot.problem.integrand_preset, IntegrandPreset2D::Gaussian);
    EXPECT_EQ(snapshot.problem.grid.x_cells, 16u);
    EXPECT_EQ(snapshot.problem.grid.y_cells, 16u);
    EXPECT_EQ(snapshot.result.cell_count, 256u);
    EXPECT_GT(snapshot.result.estimate, 0.0);
    EXPECT_TRUE(snapshot.result.absolute_error.has_value());
    EXPECT_EQ(snapshot.renderable.cells.size(), 256u);
}

TEST(IntegrationWorkbenchState, ChangingGridRecomputesResultAndClampsSelection) {
    IntegrationWorkbenchState state;
    state.select_cell(200u);
    const u64 before_revision = state.revision();

    ASSERT_TRUE(state.set_grid(UniformGrid2DConfig{.x_cells = 4u, .y_cells = 4u}));
    const IntegrationWorkbenchSnapshot snapshot = state.snapshot();

    EXPECT_GT(snapshot.metadata.revision, before_revision);
    EXPECT_EQ(snapshot.result.cell_count, 16u);
    EXPECT_FALSE(snapshot.selected_cell.valid);
}

TEST(IntegrationWorkbenchState, SelectedCellSummaryMatchesContributionData) {
    IntegrationWorkbenchState state;
    state.select_cell(10u);

    const IntegrationWorkbenchSnapshot snapshot = state.snapshot();
    ASSERT_TRUE(snapshot.selected_cell.valid);
    EXPECT_EQ(snapshot.selected_cell.cell_id, 10u);
    ASSERT_LT(snapshot.selected_cell.cell_id, snapshot.renderable.cells.size());
    EXPECT_DOUBLE_EQ(snapshot.selected_cell.contribution.contribution,
                     snapshot.renderable.cells[snapshot.selected_cell.cell_id].contribution);
}

TEST(IntegrationWorkbenchState, SelectCellAtMapsDomainCoordinates) {
    IntegrationWorkbenchState state;

    const auto selected = state.select_cell_at(0.1, 0.1);
    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ(*selected, 136u);
    EXPECT_TRUE(state.snapshot().selected_cell.valid);

    const auto outside = state.select_cell_at(4.0, 0.0);
    EXPECT_FALSE(outside.has_value());
    EXPECT_FALSE(state.snapshot().selected_cell.valid);
}

TEST(IntegrationWorkbenchState, InvalidGridReportsErrorWithoutThrowing) {
    IntegrationWorkbenchState state;

    EXPECT_FALSE(state.set_grid(UniformGrid2DConfig{.x_cells = 0u, .y_cells = 8u}));
    ASSERT_TRUE(state.last_error().has_value());
    EXPECT_EQ(*state.last_error(), IntegrationError::InvalidPartition);

    const IntegrationWorkbenchSnapshot snapshot = state.snapshot();
    EXPECT_TRUE(snapshot.metadata.has_error);
    EXPECT_EQ(snapshot.result.cell_count, 0u);
}

TEST(IntegrationWorkbenchState, DisplayConfigBuilderControlsLayerFlags) {
    const IntegrationDisplayConfig config = IntegrationDisplayConfigBuilder{}
        .mode(IntegrationDisplayMode::LocalError)
        .show_domain_boundary(false)
        .show_cells(true)
        .show_samples(false)
        .show_axes(false)
        .build();

    IntegrationWorkbenchState state;
    state.set_display_config(config);
    const IntegrationWorkbenchSnapshot snapshot = state.snapshot();

    EXPECT_EQ(snapshot.renderable.display.mode, IntegrationDisplayMode::LocalError);
    EXPECT_FALSE(snapshot.renderable.display.show_domain_boundary);
    EXPECT_TRUE(snapshot.renderable.display.show_cells);
    EXPECT_FALSE(snapshot.renderable.display.show_samples);
    EXPECT_FALSE(snapshot.renderable.display.show_axes);
}

TEST(IntegrationWorkbenchState, AnalysisSnapshotIncludesConvergenceAndMethodComparison) {
    IntegrationWorkbenchState state;
    state.select_cell(12u);

    const IntegrationAnalysisSnapshot analysis = state.analysis_snapshot();
    EXPECT_FALSE(analysis.stale);
    EXPECT_EQ(analysis.convergence.rows.size(), 4u);
    EXPECT_GE(analysis.method_comparison.size(), 2u);
    EXPECT_TRUE(analysis.selected_cell.valid);
    EXPECT_EQ(analysis.problem_revision, state.revision());
}

TEST(IntegrationWorkbenchState, ChangingIntegrandUpdatesReferenceAndResult) {
    IntegrationWorkbenchState state;
    const f64 gaussian_estimate = state.snapshot().result.estimate;

    ASSERT_TRUE(state.set_integrand(IntegrandPreset2D::Polynomial));
    const IntegrationWorkbenchSnapshot snapshot = state.snapshot();

    EXPECT_EQ(snapshot.problem.integrand_preset, IntegrandPreset2D::Polynomial);
    ASSERT_TRUE(snapshot.result.reference_value.has_value());
    EXPECT_NE(snapshot.result.estimate, gaussian_estimate);
}

TEST(IntegrationWorkbenchState, ChangingDomainRecomputesAndUpdatesReference) {
    IntegrationWorkbenchState state;
    const f64 original_estimate = state.snapshot().result.estimate;

    ASSERT_TRUE(state.set_domain(RectDomain2D{.x_min = -1.0, .x_max = 1.0, .y_min = -1.0, .y_max = 1.0}));
    const IntegrationWorkbenchSnapshot snapshot = state.snapshot();

    EXPECT_EQ(snapshot.problem.domain.x_min, -1.0);
    EXPECT_EQ(snapshot.problem.domain.x_max, 1.0);
    EXPECT_TRUE(snapshot.result.reference_value.has_value());
    EXPECT_NE(snapshot.result.estimate, original_estimate);
}

} // namespace
