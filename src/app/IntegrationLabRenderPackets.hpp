#pragma once
// app/IntegrationLabRenderPackets.hpp
// Render-packet adapter for the 2D Integration Lab workbench.

#include "app/IntegrationWorkbenchState.hpp"
#include "engine/RenderService.hpp"
#include "memory/MemoryService.hpp"

namespace ndde {

struct IntegrationLabRenderStats {
    u64 heatmap_vertices = u64(0);
    u64 line_vertices = u64(0);
    u64 sample_vertices = u64(0);
};

[[nodiscard]] f64 integration_display_scalar(const math::integration::CellContribution2D& cell,
                                             IntegrationDisplayMode mode) noexcept;

[[nodiscard]] Vec4 integration_heat_color(f64 value, f64 max_abs) noexcept;

void append_integration_heatmap(memory::FrameVector<Vertex>& out,
                                const IntegrationWorkbenchSnapshot& snapshot);

void append_integration_lines(memory::FrameVector<Vertex>& out,
                              const IntegrationWorkbenchSnapshot& snapshot);

void append_integration_samples(memory::FrameVector<Vertex>& out,
                                const IntegrationWorkbenchSnapshot& snapshot);

IntegrationLabRenderStats submit_integration_workbench_packets(RenderService& render,
                                                               RenderViewId view,
                                                               const IntegrationWorkbenchSnapshot& snapshot,
                                                               memory::MemoryService* memory);

} // namespace ndde
