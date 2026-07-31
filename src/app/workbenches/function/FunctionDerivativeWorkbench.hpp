#pragma once
// app/workbenches/function/FunctionDerivativeWorkbench.hpp
// Workbench for drawing f(x) = sin(x) and its derivative.

#include "app/workbenches/Workbench.hpp"
#include "engine/coordinates/CoordinateOverlayService.hpp"

namespace ndde {

class FunctionDerivativeWorkbench final : public IWorkbench {
public:
    FunctionDerivativeWorkbench();

    [[nodiscard]] const WorkbenchMetadata& metadata() const noexcept override { return m_metadata; }
    void build(sim::WorkbenchBuildContext& build) override;
    void on_start(WorkbenchRenderContext& context) override;
    void on_tick(const TickInfo& tick) override;
    void on_submit_render(WorkbenchRenderContext& context) override;
    void on_stop() override;

private:
    WorkbenchMetadata m_metadata;
    f32 m_time = 0.f;

    void submit_function_curve(WorkbenchRenderContext& context,
                               CoordinateVisibleBounds2D bounds,
                               bool derivative) const;
};

} // namespace ndde
