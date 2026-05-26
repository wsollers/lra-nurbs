#pragma once
// math/integration/Integration2D.hpp
// 2D integration kernel for the Integration and Approximation Lab.

#include "math/Scalars.hpp"
#include "math/integration/Integration1D.hpp"

#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ndde::math::integration {

enum class IntegrationMethod2D : u8 {
    Midpoint,
    TensorProductTrapezoid,
};

enum class IntegrandPreset2D : u8 {
    Gaussian,
    Wave,
    Polynomial,
    StepX,
};

enum class MeasureElement2D : u8 {
    Area,
};

enum class DiagnosticField2D : u8 {
    Value,
    Contribution,
    LocalError,
};

struct RectDomain2D {
    f64 x_min = f64(-1);
    f64 x_max = f64(1);
    f64 y_min = f64(-1);
    f64 y_max = f64(1);

    [[nodiscard]] f64 width() const noexcept { return x_max - x_min; }
    [[nodiscard]] f64 height() const noexcept { return y_max - y_min; }
    [[nodiscard]] f64 area() const noexcept { return width() * height(); }
    [[nodiscard]] bool valid() const noexcept { return x_max > x_min && y_max > y_min; }
};

struct UniformGrid2DConfig {
    u32 x_cells = u32(32);
    u32 y_cells = u32(32);

    [[nodiscard]] u64 cell_count() const noexcept {
        return static_cast<u64>(x_cells) * static_cast<u64>(y_cells);
    }
};

struct Cell2D {
    u32 id = u32(0);
    u32 ix = u32(0);
    u32 iy = u32(0);
    f64 x0 = f64(0);
    f64 x1 = f64(0);
    f64 y0 = f64(0);
    f64 y1 = f64(0);

    [[nodiscard]] f64 width() const noexcept { return x1 - x0; }
    [[nodiscard]] f64 height() const noexcept { return y1 - y0; }
    [[nodiscard]] f64 measure() const noexcept { return width() * height(); }
    [[nodiscard]] f64 center_x() const noexcept { return (x0 + x1) * f64(0.5); }
    [[nodiscard]] f64 center_y() const noexcept { return (y0 + y1) * f64(0.5); }
    [[nodiscard]] bool contains(f64 x, f64 y) const noexcept {
        return x >= x0 && x <= x1 && y >= y0 && y <= y1;
    }
};

struct CellContribution2D {
    Cell2D cell{};
    Vec2 sample{0.f, 0.f};
    f64 value = f64(0);
    f64 measure = f64(0);
    f64 contribution = f64(0);
    f64 local_error_estimate = f64(0);
};

struct UniformGrid2D {
    RectDomain2D domain{};
    UniformGrid2DConfig config{};
    std::vector<Cell2D> cells;

    [[nodiscard]] f64 dx() const noexcept {
        return domain.width() / static_cast<f64>(config.x_cells);
    }
    [[nodiscard]] f64 dy() const noexcept {
        return domain.height() / static_cast<f64>(config.y_cells);
    }
};

struct IntegrationResult2D {
    IntegrationMethod2D method = IntegrationMethod2D::Midpoint;
    RectDomain2D domain{};
    UniformGrid2DConfig grid{};
    MeasureElement2D measure = MeasureElement2D::Area;
    f64 estimate = f64(0);
    std::optional<f64> reference_value;
    std::optional<f64> absolute_error;
    std::optional<f64> relative_error;
    u64 evaluation_count = u64(0);
    std::vector<CellContribution2D> contributions;
};

struct ConvergenceRow2D {
    UniformGrid2DConfig grid{};
    f64 h = f64(0);
    f64 estimate = f64(0);
    std::optional<f64> absolute_error;
    std::optional<f64> observed_order;
};

struct ConvergenceSeries2D {
    IntegrationMethod2D method = IntegrationMethod2D::Midpoint;
    std::vector<ConvergenceRow2D> rows;
};

using ScalarFunction2D = std::function<f64(f64, f64)>;

[[nodiscard]] std::string_view method_name(IntegrationMethod2D method) noexcept;
[[nodiscard]] std::string_view integrand_name(IntegrandPreset2D preset) noexcept;
[[nodiscard]] ScalarFunction2D preset_function(IntegrandPreset2D preset);
[[nodiscard]] std::optional<f64> preset_reference(IntegrandPreset2D preset,
                                                  RectDomain2D domain);

struct IntegrationProblem2D {
    RectDomain2D domain{};
    IntegrandPreset2D integrand_preset = IntegrandPreset2D::Gaussian;
    ScalarFunction2D integrand;
    MeasureElement2D measure = MeasureElement2D::Area;
    UniformGrid2DConfig grid{};
    IntegrationMethod2D method = IntegrationMethod2D::Midpoint;
    std::optional<f64> reference_value;
};

class IntegrationProblem2DBuilder {
public:
    IntegrationProblem2DBuilder& rectangle(RectDomain2D domain) noexcept;
    IntegrationProblem2DBuilder& integrand(IntegrandPreset2D preset);
    IntegrationProblem2DBuilder& integrand(ScalarFunction2D function);
    IntegrationProblem2DBuilder& measure(MeasureElement2D measure) noexcept;
    IntegrationProblem2DBuilder& uniform_grid(UniformGrid2DConfig grid) noexcept;
    IntegrationProblem2DBuilder& method(IntegrationMethod2D method) noexcept;
    IntegrationProblem2DBuilder& reference_value(std::optional<f64> value) noexcept;

    [[nodiscard]] std::expected<IntegrationProblem2D, IntegrationError> build() const;

private:
    IntegrationProblem2D m_problem{
        .integrand = preset_function(IntegrandPreset2D::Gaussian),
        .reference_value = preset_reference(IntegrandPreset2D::Gaussian, RectDomain2D{})
    };
    bool m_custom_integrand = false;
};

[[nodiscard]] std::expected<UniformGrid2D, IntegrationError>
make_uniform_grid(RectDomain2D domain, UniformGrid2DConfig config);

[[nodiscard]] std::expected<IntegrationResult2D, IntegrationError>
integrate_uniform_grid(const IntegrationProblem2D& problem);

[[nodiscard]] std::expected<IntegrationResult2D, IntegrationError>
integrate_uniform_grid(RectDomain2D domain,
                       const ScalarFunction2D& integrand,
                       UniformGrid2DConfig config,
                       IntegrationMethod2D method,
                       std::optional<f64> reference_value = std::nullopt);

[[nodiscard]] std::expected<ConvergenceSeries2D, IntegrationError>
compute_convergence_series(const IntegrationProblem2D& problem,
                           std::span<const UniformGrid2DConfig> grids);

[[nodiscard]] std::optional<u32> cell_id_at(RectDomain2D domain,
                                            UniformGrid2DConfig config,
                                            f64 x,
                                            f64 y) noexcept;

} // namespace ndde::math::integration
