#pragma once
// app/IntegrationLabAnalyticsPackets.hpp
// Render-packet adapter for the Integration Lab second-window analytics view.

#include "app/IntegrationWorkbenchState.hpp"
#include "engine/RenderService.hpp"
#include "memory/MemoryService.hpp"

namespace ndde {

struct IntegrationLabAnalyticsRenderStats {
    u64 shell_vertices = u64(0);
    u64 label_vertices = u64(0);
    u64 convergence_vertices = u64(0);
    u64 comparison_vertices = u64(0);
    u64 contribution_vertices = u64(0);
};

void append_integration_analytics_shell(memory::FrameVector<Vertex>& lines,
                                        memory::FrameVector<Vertex>& labels,
                                        const IntegrationAnalysisSnapshot& analysis);

void append_integration_convergence_plot(memory::FrameVector<Vertex>& out,
                                         const IntegrationAnalysisSnapshot& analysis);

void append_integration_method_comparison(memory::FrameVector<Vertex>& out,
                                          const IntegrationAnalysisSnapshot& analysis);

void append_integration_contribution_distribution(memory::FrameVector<Vertex>& out,
                                                  const IntegrationAnalysisSnapshot& analysis);

IntegrationLabAnalyticsRenderStats submit_integration_analytics_packets(
    RenderService& render,
    RenderViewId view,
    const IntegrationAnalysisSnapshot& analysis,
    memory::MemoryService* memory);

} // namespace ndde
