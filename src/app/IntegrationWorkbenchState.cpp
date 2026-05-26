#include "app/IntegrationWorkbenchState.hpp"

#include <array>
#include <utility>

namespace ndde {

namespace integration = math::integration;

IntegrationDisplayConfigBuilder& IntegrationDisplayConfigBuilder::mode(IntegrationDisplayMode mode) noexcept {
    m_config.mode = mode;
    return *this;
}

IntegrationDisplayConfigBuilder&
IntegrationDisplayConfigBuilder::show_domain_boundary(bool enabled) noexcept {
    m_config.show_domain_boundary = enabled;
    return *this;
}

IntegrationDisplayConfigBuilder& IntegrationDisplayConfigBuilder::show_cells(bool enabled) noexcept {
    m_config.show_cells = enabled;
    return *this;
}

IntegrationDisplayConfigBuilder& IntegrationDisplayConfigBuilder::show_samples(bool enabled) noexcept {
    m_config.show_samples = enabled;
    return *this;
}

IntegrationDisplayConfigBuilder& IntegrationDisplayConfigBuilder::show_axes(bool enabled) noexcept {
    m_config.show_axes = enabled;
    return *this;
}

IntegrationDisplayConfig IntegrationDisplayConfigBuilder::build() const noexcept {
    return m_config;
}

IntegrationWorkbenchState::IntegrationWorkbenchState() {
    const auto problem = integration::IntegrationProblem2DBuilder{}
        .rectangle(integration::RectDomain2D{.x_min = -2.0, .x_max = 2.0, .y_min = -2.0, .y_max = 2.0})
        .integrand(integration::IntegrandPreset2D::Gaussian)
        .measure(integration::MeasureElement2D::Area)
        .uniform_grid(integration::UniformGrid2DConfig{.x_cells = 16u, .y_cells = 16u})
        .method(integration::IntegrationMethod2D::Midpoint)
        .build();

    if (problem) {
        m_problem = *problem;
    }
    (void)recompute();
}

std::optional<integration::IntegrationError> IntegrationWorkbenchState::last_error() const noexcept {
    return m_last_error;
}

IntegrationWorkbenchSnapshot IntegrationWorkbenchState::snapshot() const {
    return IntegrationWorkbenchSnapshot{
        .problem = problem_summary(),
        .result = result_summary(),
        .selected_cell = selected_cell_summary(),
        .renderable = RenderableIntegration2D{
            .domain = m_result.domain,
            .grid = m_result.grid,
            .display = m_display,
            .hovered_cell_id = m_hovered_cell_id,
            .selected_cell_id = m_selected_cell_id,
            .cells = m_result.contributions
        },
        .metadata = IntegrationMetadataSummary2D{
            .revision = m_revision,
            .stale = false,
            .last_error = m_last_error.value_or(integration::IntegrationError::InvalidPartition),
            .has_error = m_last_error.has_value()
        }
    };
}

IntegrationAnalysisSnapshot IntegrationWorkbenchState::analysis_snapshot() const {
    constexpr std::array<integration::UniformGrid2DConfig, 4> grids{{
        {.x_cells = 8u, .y_cells = 8u},
        {.x_cells = 16u, .y_cells = 16u},
        {.x_cells = 32u, .y_cells = 32u},
        {.x_cells = 64u, .y_cells = 64u},
    }};

    IntegrationAnalysisSnapshot snapshot{
        .problem_revision = m_revision,
        .selected_cell = selected_cell_summary(),
        .stale = m_last_error.has_value()
    };

    if (!m_last_error) {
        auto series = integration::compute_convergence_series(m_problem, grids);
        if (series) {
            snapshot.convergence = std::move(*series);
        } else {
            snapshot.stale = true;
        }

        for (const integration::IntegrationMethod2D method : {
                 integration::IntegrationMethod2D::Midpoint,
                 integration::IntegrationMethod2D::TensorProductTrapezoid}) {
            integration::IntegrationProblem2D run = m_problem;
            run.method = method;
            auto result = integration::integrate_uniform_grid(run);
            if (!result) {
                snapshot.stale = true;
                continue;
            }
            snapshot.method_comparison.push_back(MethodComparisonRow2D{
                .method = method,
                .estimate = result->estimate,
                .absolute_error = result->absolute_error,
                .relative_error = result->relative_error,
                .evaluation_count = result->evaluation_count
            });
        }
    }

    return snapshot;
}

bool IntegrationWorkbenchState::set_problem(integration::IntegrationProblem2D problem) {
    m_problem = std::move(problem);
    return recompute();
}

bool IntegrationWorkbenchState::set_domain(integration::RectDomain2D domain) {
    auto problem = integration::IntegrationProblem2DBuilder{}
        .rectangle(domain)
        .integrand(m_problem.integrand_preset)
        .measure(m_problem.measure)
        .uniform_grid(m_problem.grid)
        .method(m_problem.method)
        .reference_value(integration::preset_reference(m_problem.integrand_preset, domain))
        .build();
    if (!problem) {
        m_last_error = problem.error();
        return false;
    }
    m_problem = std::move(*problem);
    return recompute();
}

bool IntegrationWorkbenchState::set_integrand(integration::IntegrandPreset2D preset) {
    auto problem = integration::IntegrationProblem2DBuilder{}
        .rectangle(m_problem.domain)
        .integrand(preset)
        .measure(m_problem.measure)
        .uniform_grid(m_problem.grid)
        .method(m_problem.method)
        .build();
    if (!problem) {
        m_last_error = problem.error();
        return false;
    }
    m_problem = std::move(*problem);
    return recompute();
}

bool IntegrationWorkbenchState::set_grid(integration::UniformGrid2DConfig grid) {
    m_problem.grid = grid;
    return recompute();
}

bool IntegrationWorkbenchState::set_method(integration::IntegrationMethod2D method) {
    m_problem.method = method;
    return recompute();
}

void IntegrationWorkbenchState::set_display_config(IntegrationDisplayConfig display) noexcept {
    m_display = display;
}

void IntegrationWorkbenchState::set_display_mode(IntegrationDisplayMode mode) noexcept {
    m_display.mode = mode;
}

void IntegrationWorkbenchState::select_cell(std::optional<u32> cell_id) noexcept {
    m_selected_cell_id = clamp_cell_id(cell_id, static_cast<u64>(m_result.contributions.size()));
}

void IntegrationWorkbenchState::hover_cell(std::optional<u32> cell_id) noexcept {
    m_hovered_cell_id = clamp_cell_id(cell_id, static_cast<u64>(m_result.contributions.size()));
}

std::optional<u32> IntegrationWorkbenchState::select_cell_at(f64 x, f64 y) noexcept {
    m_selected_cell_id = integration::cell_id_at(m_problem.domain, m_problem.grid, x, y);
    return m_selected_cell_id;
}

std::optional<u32> IntegrationWorkbenchState::hover_cell_at(f64 x, f64 y) noexcept {
    m_hovered_cell_id = integration::cell_id_at(m_problem.domain, m_problem.grid, x, y);
    return m_hovered_cell_id;
}

bool IntegrationWorkbenchState::recompute() {
    auto result = integration::integrate_uniform_grid(m_problem);
    if (!result) {
        m_last_error = result.error();
        m_result = {};
        ++m_revision;
        return false;
    }

    m_result = std::move(*result);
    m_last_error.reset();
    m_selected_cell_id = clamp_cell_id(m_selected_cell_id,
                                       static_cast<u64>(m_result.contributions.size()));
    m_hovered_cell_id = clamp_cell_id(m_hovered_cell_id,
                                      static_cast<u64>(m_result.contributions.size()));
    ++m_revision;
    return true;
}

SelectedCellSummary2D IntegrationWorkbenchState::selected_cell_summary() const {
    if (!m_selected_cell_id || *m_selected_cell_id >= m_result.contributions.size()) {
        return {};
    }
    return SelectedCellSummary2D{
        .valid = true,
        .cell_id = *m_selected_cell_id,
        .contribution = m_result.contributions[*m_selected_cell_id]
    };
}

IntegrationProblemSummary2D IntegrationWorkbenchState::problem_summary() const noexcept {
    return IntegrationProblemSummary2D{
        .domain = m_problem.domain,
        .integrand_preset = m_problem.integrand_preset,
        .measure = m_problem.measure,
        .grid = m_problem.grid,
        .method = m_problem.method
    };
}

IntegrationResultSummary2D IntegrationWorkbenchState::result_summary() const noexcept {
    return IntegrationResultSummary2D{
        .estimate = m_result.estimate,
        .reference_value = m_result.reference_value,
        .absolute_error = m_result.absolute_error,
        .relative_error = m_result.relative_error,
        .evaluation_count = m_result.evaluation_count,
        .cell_count = static_cast<u64>(m_result.contributions.size())
    };
}

std::optional<u32> IntegrationWorkbenchState::clamp_cell_id(std::optional<u32> cell_id,
                                                            u64 cell_count) noexcept {
    if (!cell_id || cell_count == u64(0) || static_cast<u64>(*cell_id) >= cell_count) {
        return std::nullopt;
    }
    return cell_id;
}

} // namespace ndde
