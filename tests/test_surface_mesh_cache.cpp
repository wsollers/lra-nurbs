#include "app/SurfaceMeshCache.hpp"
#include "math/Surfaces.hpp"
#include "memory/MemoryService.hpp"

#include <gtest/gtest.h>

namespace {

TEST(SurfaceMeshCache, BuildsExpectedVertexCounts) {
    ndde::math::Paraboloid surface;
    ndde::SurfaceMeshCache cache;

    constexpr ndde::u32 grid = 12;
    cache.rebuild_if_needed(surface, ndde::SurfaceMeshOptions{
        .grid_lines = grid,
        .color_scale = 1.f,
        .wire_color = {1.f, 1.f, 1.f, 1.f},
        .fill_color_mode = ndde::SurfaceFillColorMode::HeightCell,
        .build_contour = true
    });

    EXPECT_EQ(cache.fill_count(), grid * grid * 6u);
    EXPECT_EQ(cache.contour_count(), grid * grid * 6u);
    EXPECT_EQ(cache.wire_count(), surface.wireframe_vertex_count(grid, grid));
}

TEST(SurfaceMeshCache, DirtyRebuildCanChangeResolution) {
    ndde::math::Paraboloid surface;
    ndde::SurfaceMeshCache cache;

    cache.rebuild_if_needed(surface, ndde::SurfaceMeshOptions{
        .grid_lines = 8,
        .build_contour = false
    });
    EXPECT_EQ(cache.fill_count(), 8u * 8u * 6u);
    EXPECT_EQ(cache.contour_count(), 0u);

    cache.mark_dirty();
    cache.rebuild_if_needed(surface, ndde::SurfaceMeshOptions{
        .grid_lines = 10,
        .build_contour = true
    });
    EXPECT_EQ(cache.fill_count(), 10u * 10u * 6u);
    EXPECT_EQ(cache.contour_count(), 10u * 10u * 6u);
}

TEST(SurfaceMeshCache, CanSkipFillGeometryAndRebuildWhenModeChanges) {
    ndde::math::Paraboloid surface;
    ndde::SurfaceMeshCache cache;

    cache.rebuild_if_needed(surface, ndde::SurfaceMeshOptions{
        .grid_lines = 8,
        .build_fill = false,
        .build_contour = true
    });
    EXPECT_EQ(cache.fill_count(), 0u);
    EXPECT_EQ(cache.contour_count(), 8u * 8u * 6u);
    EXPECT_EQ(cache.wire_count(), surface.wireframe_vertex_count(8u, 8u));

    cache.rebuild_if_needed(surface, ndde::SurfaceMeshOptions{
        .grid_lines = 8,
        .build_fill = true,
        .build_contour = true
    });
    EXPECT_EQ(cache.fill_count(), 8u * 8u * 6u);
    EXPECT_EQ(cache.contour_count(), 8u * 8u * 6u);
}

TEST(SurfaceMeshCache, BindsVerticesToCacheMemoryScope) {
    ndde::memory::MemoryService memory;
    ndde::math::Paraboloid surface;
    ndde::SurfaceMeshCache cache;
    cache.bind_memory(&memory);

    cache.rebuild_if_needed(surface, ndde::SurfaceMeshOptions{
        .grid_lines = 8,
        .build_contour = true
    });

    EXPECT_EQ(cache.fill_vertices().get_allocator().resource(), memory.cache().resource());
    EXPECT_EQ(cache.wire_vertices().get_allocator().resource(), memory.cache().resource());
    EXPECT_EQ(cache.contour_vertices().get_allocator().resource(), memory.cache().resource());
}

} // namespace
