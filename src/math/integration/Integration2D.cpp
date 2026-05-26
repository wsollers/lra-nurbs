#include "math/integration/Integration2D.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace ndde::math::integration {
namespace {

[[nodiscard]] bool valid_grid(UniformGrid2DConfig config) noexcept {
    return config.x_cells > u32(0) && config.y_cells > u32(0);
}

[[nodiscard]] f64 gaussian_reference(RectDomain2D domain) {
    const auto erf_delta = [](f64 min, f64 max) {
        return std::erf(max) - std::erf(min);
    };
    return (std::numbers::pi / f64(4)) *
           erf_delta(domain.x_min, domain.x_max) *
           erf_delta(domain.y_min, domain.y_max);
}

[[nodiscard]] f64 wave_reference(RectDomain2D domain) {
    const f64 x_part = (std::cos(f64(2) * domain.x_min) -
                        std::cos(f64(2) * domain.x_max)) / f64(2);
    const f64 y_part = (std::sin(f64(2) * domain.y_max) -
                        std::sin(f64(2) * domain.y_min)) / f64(2);
    return x_part * y_part + f64(0.2) * domain.area();
}

[[nodiscard]] f64 polynomial_reference(RectDomain2D domain) {
    const f64 x_part = (domain.x_max * domain.x_max - domain.x_min * domain.x_min) / f64(2);
    const f64 y_part = (domain.y_max * domain.y_max - domain.y_min * domain.y_min) / f64(2);
    return x_part * y_part + f64(0.5) * domain.area();
}

[[nodiscard]] f64 step_x_reference(RectDomain2D domain) {
    const f64 negative_width = std::max(f64(0), std::min(f64(0), domain.x_max) - domain.x_min);
    const f64 positive_width = std::max(f64(0), domain.x_max - std::max(f64(0), domain.x_min));
    return (f64(0.3) * negative_width + f64(1.4) * positive_width) * domain.height();
}

[[nodiscard]] CellContribution2D midpoint_contribution(const Cell2D& cell,
                                                       const ScalarFunction2D& integrand,
                                                       u64& evaluation_count) {
    const f64 x = cell.center_x();
    const f64 y = cell.center_y();
    const f64 value = integrand(x, y);
    ++evaluation_count;
    const f64 measure = cell.measure();
    return CellContribution2D{
        .cell = cell,
        .sample = Vec2{static_cast<f32>(x), static_cast<f32>(y)},
        .value = value,
        .measure = measure,
        .contribution = value * measure,
        .local_error_estimate = f64(0)
    };
}

[[nodiscard]] CellContribution2D trapezoid_contribution(const Cell2D& cell,
                                                        const ScalarFunction2D& integrand,
                                                        u64& evaluation_count) {
    const f64 f00 = integrand(cell.x0, cell.y0);
    const f64 f10 = integrand(cell.x1, cell.y0);
    const f64 f01 = integrand(cell.x0, cell.y1);
    const f64 f11 = integrand(cell.x1, cell.y1);
    evaluation_count += u64(4);
    const f64 value = (f00 + f10 + f01 + f11) * f64(0.25);
    const f64 measure = cell.measure();
    return CellContribution2D{
        .cell = cell,
        .sample = Vec2{static_cast<f32>(cell.center_x()), static_cast<f32>(cell.center_y())},
        .value = value,
        .measure = measure,
        .contribution = value * measure,
        .local_error_estimate = std::abs(value - integrand(cell.center_x(), cell.center_y())) * measure
    };
}

} // namespace

IntegrationProblem2DBuilder& IntegrationProblem2DBuilder::rectangle(RectDomain2D domain) noexcept {
    m_problem.domain = domain;
    if (!m_custom_integrand) {
        m_problem.reference_value = preset_reference(m_problem.integrand_preset, domain);
    }
    return *this;
}

IntegrationProblem2DBuilder& IntegrationProblem2DBuilder::integrand(IntegrandPreset2D preset) {
    m_problem.integrand_preset = preset;
    m_problem.integrand = preset_function(preset);
    m_problem.reference_value = preset_reference(preset, m_problem.domain);
    m_custom_integrand = false;
    return *this;
}

IntegrationProblem2DBuilder& IntegrationProblem2DBuilder::integrand(ScalarFunction2D function) {
    m_problem.integrand = std::move(function);
    m_problem.reference_value = std::nullopt;
    m_custom_integrand = true;
    return *this;
}

IntegrationProblem2DBuilder& IntegrationProblem2DBuilder::measure(MeasureElement2D measure) noexcept {
    m_problem.measure = measure;
    return *this;
}

IntegrationProblem2DBuilder& IntegrationProblem2DBuilder::uniform_grid(UniformGrid2DConfig grid) noexcept {
    m_problem.grid = grid;
    return *this;
}

IntegrationProblem2DBuilder& IntegrationProblem2DBuilder::method(IntegrationMethod2D method) noexcept {
    m_problem.method = method;
    return *this;
}

IntegrationProblem2DBuilder&
IntegrationProblem2DBuilder::reference_value(std::optional<f64> value) noexcept {
    m_problem.reference_value = value;
    return *this;
}

std::expected<IntegrationProblem2D, IntegrationError> IntegrationProblem2DBuilder::build() const {
    if (!m_problem.domain.valid()) {
        return std::unexpected(IntegrationError::InvalidDomain);
    }
    if (!valid_grid(m_problem.grid)) {
        return std::unexpected(IntegrationError::InvalidPartition);
    }
    if (!m_problem.integrand) {
        return std::unexpected(IntegrationError::InvalidPartition);
    }
    return m_problem;
}

std::string_view method_name(IntegrationMethod2D method) noexcept {
    switch (method) {
        case IntegrationMethod2D::Midpoint: return "Midpoint 2D";
        case IntegrationMethod2D::TensorProductTrapezoid: return "Tensor-product Trapezoid";
    }
    return "Unknown";
}

std::string_view integrand_name(IntegrandPreset2D preset) noexcept {
    switch (preset) {
        case IntegrandPreset2D::Gaussian: return "exp(-(x*x + y*y))";
        case IntegrandPreset2D::Wave: return "sin(2x)cos(2y) + 0.2";
        case IntegrandPreset2D::Polynomial: return "x*y + 0.5";
        case IntegrandPreset2D::StepX: return "x < 0 ? 0.3 : 1.4";
    }
    return "Unknown";
}

ScalarFunction2D preset_function(IntegrandPreset2D preset) {
    switch (preset) {
        case IntegrandPreset2D::Gaussian:
            return [](f64 x, f64 y) { return std::exp(-(x * x + y * y)); };
        case IntegrandPreset2D::Wave:
            return [](f64 x, f64 y) { return std::sin(f64(2) * x) * std::cos(f64(2) * y) + f64(0.2); };
        case IntegrandPreset2D::Polynomial:
            return [](f64 x, f64 y) { return x * y + f64(0.5); };
        case IntegrandPreset2D::StepX:
            return [](f64 x, f64) { return x < f64(0) ? f64(0.3) : f64(1.4); };
    }
    return {};
}

std::optional<f64> preset_reference(IntegrandPreset2D preset, RectDomain2D domain) {
    if (!domain.valid()) {
        return std::nullopt;
    }

    switch (preset) {
        case IntegrandPreset2D::Gaussian: return gaussian_reference(domain);
        case IntegrandPreset2D::Wave: return wave_reference(domain);
        case IntegrandPreset2D::Polynomial: return polynomial_reference(domain);
        case IntegrandPreset2D::StepX: return step_x_reference(domain);
    }
    return std::nullopt;
}

std::expected<UniformGrid2D, IntegrationError>
make_uniform_grid(RectDomain2D domain, UniformGrid2DConfig config) {
    if (!domain.valid()) {
        return std::unexpected(IntegrationError::InvalidDomain);
    }
    if (!valid_grid(config)) {
        return std::unexpected(IntegrationError::InvalidPartition);
    }

    UniformGrid2D grid{
        .domain = domain,
        .config = config,
    };
    grid.cells.reserve(static_cast<std::size_t>(config.cell_count()));

    const f64 dx = domain.width() / static_cast<f64>(config.x_cells);
    const f64 dy = domain.height() / static_cast<f64>(config.y_cells);
    u32 id = u32(0);
    for (u32 iy = u32(0); iy < config.y_cells; ++iy) {
        const f64 y0 = domain.y_min + static_cast<f64>(iy) * dy;
        const f64 y1 = (iy + u32(1) == config.y_cells) ? domain.y_max : y0 + dy;
        for (u32 ix = u32(0); ix < config.x_cells; ++ix) {
            const f64 x0 = domain.x_min + static_cast<f64>(ix) * dx;
            const f64 x1 = (ix + u32(1) == config.x_cells) ? domain.x_max : x0 + dx;
            grid.cells.push_back(Cell2D{
                .id = id++,
                .ix = ix,
                .iy = iy,
                .x0 = x0,
                .x1 = x1,
                .y0 = y0,
                .y1 = y1
            });
        }
    }

    return grid;
}

std::expected<IntegrationResult2D, IntegrationError>
integrate_uniform_grid(const IntegrationProblem2D& problem) {
    return integrate_uniform_grid(problem.domain,
                                  problem.integrand,
                                  problem.grid,
                                  problem.method,
                                  problem.reference_value);
}

std::expected<IntegrationResult2D, IntegrationError>
integrate_uniform_grid(RectDomain2D domain,
                       const ScalarFunction2D& integrand,
                       UniformGrid2DConfig config,
                       IntegrationMethod2D method,
                       std::optional<f64> reference_value) {
    if (!integrand) {
        return std::unexpected(IntegrationError::InvalidPartition);
    }

    auto grid = make_uniform_grid(domain, config);
    if (!grid) {
        return std::unexpected(grid.error());
    }

    IntegrationResult2D result{
        .method = method,
        .domain = domain,
        .grid = config,
        .reference_value = reference_value,
    };
    result.contributions.reserve(grid->cells.size());

    for (const Cell2D& cell : grid->cells) {
        CellContribution2D contribution{};
        switch (method) {
            case IntegrationMethod2D::Midpoint:
                contribution = midpoint_contribution(cell, integrand, result.evaluation_count);
                break;
            case IntegrationMethod2D::TensorProductTrapezoid:
                contribution = trapezoid_contribution(cell, integrand, result.evaluation_count);
                ++result.evaluation_count;
                break;
        }
        result.estimate += contribution.contribution;
        result.contributions.push_back(contribution);
    }

    if (reference_value) {
        result.absolute_error = std::abs(result.estimate - *reference_value);
        const f64 denom = std::max(f64(1), std::abs(*reference_value));
        result.relative_error = *result.absolute_error / denom;
    }

    return result;
}

std::expected<ConvergenceSeries2D, IntegrationError>
compute_convergence_series(const IntegrationProblem2D& problem,
                           std::span<const UniformGrid2DConfig> grids) {
    if (grids.empty()) {
        return std::unexpected(IntegrationError::InvalidPartition);
    }

    ConvergenceSeries2D series{.method = problem.method};
    series.rows.reserve(grids.size());

    for (UniformGrid2DConfig grid_config : grids) {
        IntegrationProblem2D run = problem;
        run.grid = grid_config;
        auto result = integrate_uniform_grid(run);
        if (!result) {
            return std::unexpected(result.error());
        }

        std::optional<f64> observed_order;
        if (!series.rows.empty() &&
            series.rows.back().absolute_error &&
            result->absolute_error &&
            *series.rows.back().absolute_error > f64(0) &&
            *result->absolute_error > f64(0)) {
            const f64 error_ratio = *result->absolute_error / *series.rows.back().absolute_error;
            const f64 h_ratio = (problem.domain.width() / static_cast<f64>(grid_config.x_cells)) /
                                series.rows.back().h;
            if (h_ratio > f64(0) && h_ratio != f64(1)) {
                observed_order = std::log(error_ratio) / std::log(h_ratio);
            }
        }

        series.rows.push_back(ConvergenceRow2D{
            .grid = grid_config,
            .h = problem.domain.width() / static_cast<f64>(grid_config.x_cells),
            .estimate = result->estimate,
            .absolute_error = result->absolute_error,
            .observed_order = observed_order
        });
    }

    return series;
}

std::optional<u32> cell_id_at(RectDomain2D domain,
                              UniformGrid2DConfig config,
                              f64 x,
                              f64 y) noexcept {
    if (!domain.valid() || !valid_grid(config)) {
        return std::nullopt;
    }
    if (x < domain.x_min || x > domain.x_max || y < domain.y_min || y > domain.y_max) {
        return std::nullopt;
    }

    const f64 dx = domain.width() / static_cast<f64>(config.x_cells);
    const f64 dy = domain.height() / static_cast<f64>(config.y_cells);
    const auto ix = static_cast<u32>(std::min<f64>(
        static_cast<f64>(config.x_cells - u32(1)),
        std::floor((x - domain.x_min) / dx)));
    const auto iy = static_cast<u32>(std::min<f64>(
        static_cast<f64>(config.y_cells - u32(1)),
        std::floor((y - domain.y_min) / dy)));
    return iy * config.x_cells + ix;
}

} // namespace ndde::math::integration
