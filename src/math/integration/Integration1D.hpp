#pragma once
// math/integration/Integration1D.hpp
// Small 1D integration kernel for the mathematical laboratory.

#include "math/Scalars.hpp"

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ndde::math::integration {

enum class IntegrationError {
    InvalidDomain,
    InvalidPartition,
    SimpsonRequiresEvenCellCount,
};

enum class QuadratureMethod : u8 {
    LeftRiemann,
    RightRiemann,
    Midpoint,
    Trapezoidal,
    Simpson,
};

struct Interval1D {
    f64 min = f64(0);
    f64 max = f64(1);

    [[nodiscard]] f64 length() const noexcept { return max - min; }
    [[nodiscard]] bool valid() const noexcept { return max > min; }
};

struct UniformPartitionConfig {
    u32 cell_count = u32(32);
};

struct Cell1D {
    u32 id = u32(0);
    f64 a = f64(0);
    f64 b = f64(0);

    [[nodiscard]] f64 width() const noexcept { return b - a; }
    [[nodiscard]] f64 midpoint() const noexcept { return (a + b) * f64(0.5); }
};

struct CellContribution1D {
    Cell1D cell{};
    f64 sample = f64(0);
    f64 value = f64(0);
    f64 measure = f64(0);
    f64 contribution = f64(0);
};

struct IntegrationResult1D {
    QuadratureMethod method = QuadratureMethod::Midpoint;
    f64 estimate = f64(0);
    std::optional<f64> reference_value;
    std::optional<f64> absolute_error;
    u64 evaluation_count = u64(0);
    std::vector<CellContribution1D> contributions;
};

using ScalarFunction1D = std::function<f64(f64)>;

[[nodiscard]] std::string_view method_name(QuadratureMethod method) noexcept;

[[nodiscard]] std::expected<std::vector<Cell1D>, IntegrationError>
make_uniform_partition(Interval1D interval, UniformPartitionConfig config);

[[nodiscard]] std::expected<IntegrationResult1D, IntegrationError>
integrate_uniform(Interval1D interval,
                  const ScalarFunction1D& integrand,
                  UniformPartitionConfig config,
                  QuadratureMethod method,
                  std::optional<f64> reference_value = std::nullopt);

[[nodiscard]] f64 central_difference(const ScalarFunction1D& function,
                                     f64 x,
                                     f64 step = f64(1.0e-5));

[[nodiscard]] std::string describe_error(IntegrationError error);

} // namespace ndde::math::integration
