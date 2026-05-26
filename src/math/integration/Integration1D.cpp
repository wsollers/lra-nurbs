#include "math/integration/Integration1D.hpp"

#include <cmath>
#include <format>

namespace ndde::math::integration {

std::string_view method_name(QuadratureMethod method) noexcept {
    switch (method) {
        case QuadratureMethod::LeftRiemann: return "Left Riemann";
        case QuadratureMethod::RightRiemann: return "Right Riemann";
        case QuadratureMethod::Midpoint: return "Midpoint";
        case QuadratureMethod::Trapezoidal: return "Trapezoidal";
        case QuadratureMethod::Simpson: return "Simpson";
    }
    return "Unknown";
}

std::expected<std::vector<Cell1D>, IntegrationError>
make_uniform_partition(Interval1D interval, UniformPartitionConfig config) {
    if (!interval.valid()) {
        return std::unexpected(IntegrationError::InvalidDomain);
    }
    if (config.cell_count == u32(0)) {
        return std::unexpected(IntegrationError::InvalidPartition);
    }

    std::vector<Cell1D> cells;
    cells.reserve(config.cell_count);
    const f64 dx = interval.length() / static_cast<f64>(config.cell_count);
    for (u32 i = u32(0); i < config.cell_count; ++i) {
        const f64 a = interval.min + static_cast<f64>(i) * dx;
        cells.push_back(Cell1D{
            .id = i,
            .a = a,
            .b = (i + u32(1) == config.cell_count) ? interval.max : a + dx
        });
    }
    return cells;
}

std::expected<IntegrationResult1D, IntegrationError>
integrate_uniform(Interval1D interval,
                  const ScalarFunction1D& integrand,
                  UniformPartitionConfig config,
                  QuadratureMethod method,
                  std::optional<f64> reference_value) {
    if (method == QuadratureMethod::Simpson && (config.cell_count % u32(2)) != u32(0)) {
        return std::unexpected(IntegrationError::SimpsonRequiresEvenCellCount);
    }

    auto partition = make_uniform_partition(interval, config);
    if (!partition) {
        return std::unexpected(partition.error());
    }

    IntegrationResult1D result{
        .method = method,
        .reference_value = reference_value,
    };

    if (method == QuadratureMethod::Simpson) {
        const f64 h = interval.length() / static_cast<f64>(config.cell_count);
        f64 sum = integrand(interval.min) + integrand(interval.max);
        result.evaluation_count += u64(2);
        for (u32 i = u32(1); i < config.cell_count; ++i) {
            const f64 x = interval.min + static_cast<f64>(i) * h;
            const f64 weight = (i % u32(2)) == u32(0) ? f64(2) : f64(4);
            sum += weight * integrand(x);
            ++result.evaluation_count;
        }
        result.estimate = (h / f64(3)) * sum;
    } else {
        result.contributions.reserve(partition->size());
        for (const Cell1D& cell : *partition) {
            f64 sample = cell.midpoint();
            f64 measure = cell.width();
            f64 contribution = f64(0);
            f64 value = f64(0);

            switch (method) {
                case QuadratureMethod::LeftRiemann:
                    sample = cell.a;
                    value = integrand(sample);
                    contribution = value * measure;
                    ++result.evaluation_count;
                    break;
                case QuadratureMethod::RightRiemann:
                    sample = cell.b;
                    value = integrand(sample);
                    contribution = value * measure;
                    ++result.evaluation_count;
                    break;
                case QuadratureMethod::Midpoint:
                    sample = cell.midpoint();
                    value = integrand(sample);
                    contribution = value * measure;
                    ++result.evaluation_count;
                    break;
                case QuadratureMethod::Trapezoidal: {
                    const f64 left = integrand(cell.a);
                    const f64 right = integrand(cell.b);
                    value = (left + right) * f64(0.5);
                    contribution = value * measure;
                    result.evaluation_count += u64(2);
                    break;
                }
                case QuadratureMethod::Simpson:
                    break;
            }

            result.estimate += contribution;
            result.contributions.push_back(CellContribution1D{
                .cell = cell,
                .sample = sample,
                .value = value,
                .measure = measure,
                .contribution = contribution
            });
        }
    }

    if (reference_value) {
        result.absolute_error = std::abs(result.estimate - *reference_value);
    }
    return result;
}

f64 central_difference(const ScalarFunction1D& function, f64 x, f64 step) {
    const f64 h = std::abs(step) > f64(0) ? std::abs(step) : f64(1.0e-5);
    return (function(x + h) - function(x - h)) / (f64(2) * h);
}

std::string describe_error(IntegrationError error) {
    switch (error) {
        case IntegrationError::InvalidDomain:
            return "invalid integration domain";
        case IntegrationError::InvalidPartition:
            return "invalid integration partition";
        case IntegrationError::SimpsonRequiresEvenCellCount:
            return "Simpson's rule requires an even cell count";
    }
    return std::format("unknown integration error {}", static_cast<int>(error));
}

} // namespace ndde::math::integration
