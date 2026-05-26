#include "app/IntegrationLabRenderPackets.hpp"
#include "engine/SimulationHost.hpp"

#include <gtest/gtest.h>

namespace {

using namespace ndde;

TEST(IntegrationLabRenderPackets, HeatmapEmitsTwoTrianglesPerCell) {
    memory::MemoryService memory;
    IntegrationWorkbenchState state;
    ASSERT_TRUE(state.set_grid(math::integration::UniformGrid2DConfig{.x_cells = 4u, .y_cells = 3u}));
    const IntegrationWorkbenchSnapshot snapshot = state.snapshot();
    auto vertices = memory.frame().make_vector<Vertex>();

    append_integration_heatmap(vertices, snapshot);

    EXPECT_EQ(vertices.size(), 4u * 3u * 6u);
}

TEST(IntegrationLabRenderPackets, LinesIncludeCellsBoundaryAxesAndSelection) {
    memory::MemoryService memory;
    IntegrationWorkbenchState state;
    ASSERT_TRUE(state.set_grid(math::integration::UniformGrid2DConfig{.x_cells = 2u, .y_cells = 2u}));
    state.select_cell(1u);
    state.hover_cell(2u);
    const IntegrationWorkbenchSnapshot snapshot = state.snapshot();
    auto vertices = memory.frame().make_vector<Vertex>();

    append_integration_lines(vertices, snapshot);

    constexpr std::size_t axes = 4u;       // x and y axes, each one line
    constexpr std::size_t cells = 4u * 8u; // four outlines, four lines each
    constexpr std::size_t boundary = 8u;   // one rectangle outline
    constexpr std::size_t hover = 8u;
    constexpr std::size_t selected = 8u;
    EXPECT_EQ(vertices.size(), axes + cells + boundary + hover + selected);
}

TEST(IntegrationLabRenderPackets, SamplesEmitCrossPerCellWhenEnabled) {
    memory::MemoryService memory;
    IntegrationWorkbenchState state;
    ASSERT_TRUE(state.set_grid(math::integration::UniformGrid2DConfig{.x_cells = 3u, .y_cells = 2u}));
    const IntegrationWorkbenchSnapshot snapshot = state.snapshot();
    auto vertices = memory.frame().make_vector<Vertex>();

    append_integration_samples(vertices, snapshot);

    EXPECT_EQ(vertices.size(), 3u * 2u * 4u);
}

TEST(IntegrationLabRenderPackets, SubmitCreatesExpectedRenderPackets) {
    EngineServices services;
    RenderViewId view = RenderViewId(0);
    auto handle = services.render().register_view(RenderViewDescriptor{
        .title = "Integration",
        .kind = RenderViewKind::Main,
        .projection = CameraProjection::Orthographic,
        .camera_profile = CameraViewProfile::Orthographic2D
    }, &view);
    ASSERT_TRUE(handle);

    IntegrationWorkbenchState state;
    ASSERT_TRUE(state.set_grid(math::integration::UniformGrid2DConfig{.x_cells = 2u, .y_cells = 2u}));

    const IntegrationLabRenderStats stats = submit_integration_workbench_packets(
        services.render(),
        view,
        state.snapshot(),
        &services.memory());

    EXPECT_EQ(stats.heatmap_vertices, 2u * 2u * 6u);
    EXPECT_EQ(stats.sample_vertices, 2u * 2u * 4u);
    EXPECT_EQ(services.render().packet_count(view), 3u);
    EXPECT_EQ(services.render().packets()[0].topology, Topology::TriangleList);
    EXPECT_EQ(services.render().packets()[1].topology, Topology::LineList);
    EXPECT_EQ(services.render().packets()[2].topology, Topology::LineList);
}

} // namespace
