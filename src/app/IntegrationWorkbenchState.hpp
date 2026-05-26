#pragma once
// app/IntegrationWorkbenchState.hpp
// UI-neutral state and immutable snapshots for the Integration Lab.

#include "math/integration/Integration2D.hpp"

#include <optional>
#include <vector>

namespace ndde {

enum class IntegrationDisplayMode : u8 {
    Value,
    Contribution,
    LocalError,
};

struct IntegrationDisplayConfig {
    IntegrationDisplayMode mode = IntegrationDisplayMode::Contribution;
    bool show_domain_boundary = true;
    bool show_cells = true;
    bool show_samples = true;
    bool show_axes = true;
};

class IntegrationDisplayConfigBuilder {
public:
    IntegrationDisplayConfigBuilder& mode(IntegrationDisplayMode mode) noexcept;
    IntegrationDisplayConfigBuilder& show_domain_boundary(bool enabled) noexcept;
    IntegrationDisplayConfigBuilder& show_cells(bool enabled) noexcept;
    IntegrationDisplayConfigBuilder& show_samples(bool enabled) noexcept;
    IntegrationDisplayConfigBuilder& show_axes(bool enabled) noexcept;

    [[nodiscard]] IntegrationDisplayConfig build() const noexcept;

private:
    IntegrationDisplayConfig m_config{};
};

struct IntegrationProblemSummary2D {
    math::integration::RectDomain2D domain{};
    math::integration::IntegrandPreset2D integrand_preset = math::integration::IntegrandPreset2D::Gaussian;
    math::integration::MeasureElement2D measure = math::integration::MeasureElement2D::Area;
    math::integration::UniformGrid2DConfig grid{};
    math::integration::IntegrationMethod2D method = math::integration::IntegrationMethod2D::Midpoint;
};

struct IntegrationResultSummary2D {
    f64 estimate = f64(0);
    std::optional<f64> reference_value;
    std::optional<f64> absolute_error;
    std::optional<f64> relative_error;
    u64 evaluation_count = u64(0);
    u64 cell_count = u64(0);
};

struct SelectedCellSummary2D {
    bool valid = false;
    u32 cell_id = u32(0);
    math::integration::CellContribution2D contribution{};
};

struct RenderableIntegration2D {
    math::integration::RectDomain2D domain{};
    math::integration::UniformGrid2DConfig grid{};
    IntegrationDisplayConfig display{};
    std::optional<u32> hovered_cell_id;
    std::optional<u32> selected_cell_id;
    std::vector<math::integration::CellContribution2D> cells;
};

struct IntegrationMetadataSummary2D {
    u64 revision = u64(0);
    bool stale = false;
    math::integration::IntegrationError last_error = math::integration::IntegrationError::InvalidPartition;
    bool has_error = false;
};

struct IntegrationWorkbenchSnapshot {
    IntegrationProblemSummary2D problem{};
    IntegrationResultSummary2D result{};
    SelectedCellSummary2D selected_cell{};
    RenderableIntegration2D renderable{};
    IntegrationMetadataSummary2D metadata{};
};

struct MethodComparisonRow2D {
    math::integration::IntegrationMethod2D method = math::integration::IntegrationMethod2D::Midpoint;
    f64 estimate = f64(0);
    std::optional<f64> absolute_error;
    std::optional<f64> relative_error;
    u64 evaluation_count = u64(0);
};

struct IntegrationAnalysisSnapshot {
    u64 problem_revision = u64(0);
    math::integration::ConvergenceSeries2D convergence{};
    std::vector<MethodComparisonRow2D> method_comparison;
    SelectedCellSummary2D selected_cell{};
    bool stale = false;
};

class IntegrationWorkbenchState {
public:
    IntegrationWorkbenchState();

    [[nodiscard]] const math::integration::IntegrationProblem2D& problem() const noexcept { return m_problem; }
    [[nodiscard]] const math::integration::IntegrationResult2D& result() const noexcept { return m_result; }
    [[nodiscard]] IntegrationDisplayConfig display_config() const noexcept { return m_display; }
    [[nodiscard]] u64 revision() const noexcept { return m_revision; }
    [[nodiscard]] std::optional<math::integration::IntegrationError> last_error() const noexcept;

    [[nodiscard]] IntegrationWorkbenchSnapshot snapshot() const;
    [[nodiscard]] IntegrationAnalysisSnapshot analysis_snapshot() const;

    bool set_problem(math::integration::IntegrationProblem2D problem);
    bool set_domain(math::integration::RectDomain2D domain);
    bool set_integrand(math::integration::IntegrandPreset2D preset);
    bool set_grid(math::integration::UniformGrid2DConfig grid);
    bool set_method(math::integration::IntegrationMethod2D method);
    void set_display_config(IntegrationDisplayConfig display) noexcept;
    void set_display_mode(IntegrationDisplayMode mode) noexcept;

    void select_cell(std::optional<u32> cell_id) noexcept;
    void hover_cell(std::optional<u32> cell_id) noexcept;
    [[nodiscard]] std::optional<u32> select_cell_at(f64 x, f64 y) noexcept;
    [[nodiscard]] std::optional<u32> hover_cell_at(f64 x, f64 y) noexcept;

private:
    math::integration::IntegrationProblem2D m_problem;
    math::integration::IntegrationResult2D m_result;
    IntegrationDisplayConfig m_display{};
    std::optional<u32> m_selected_cell_id;
    std::optional<u32> m_hovered_cell_id;
    std::optional<math::integration::IntegrationError> m_last_error;
    u64 m_revision = u64(0);

    bool recompute();
    [[nodiscard]] SelectedCellSummary2D selected_cell_summary() const;
    [[nodiscard]] IntegrationProblemSummary2D problem_summary() const noexcept;
    [[nodiscard]] IntegrationResultSummary2D result_summary() const noexcept;
    [[nodiscard]] static std::optional<u32> clamp_cell_id(std::optional<u32> cell_id,
                                                          u64 cell_count) noexcept;
};

} // namespace ndde
