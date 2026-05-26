// tests/test_axes.cpp
// Unit tests for math::build_grid() and math::build_axes().
#include <gtest/gtest.h>
#include "math/Axes.hpp"
#include <vector>

using namespace ndde;
using namespace ndde::math;

TEST(Axes, AxesVertexCount2D) {
    AxesConfig cfg; cfg.is_3d = false;
    EXPECT_EQ(axes_vertex_count(cfg), 4u);
}

TEST(Axes, AxesVertexCount3D) {
    AxesConfig cfg; cfg.is_3d = true;
    EXPECT_EQ(axes_vertex_count(cfg), 6u);
}

TEST(Axes, BuildAxes2DColors) {
    AxesConfig cfg; cfg.extent = 2.f; cfg.is_3d = false;
    const u32 n = axes_vertex_count(cfg);
    std::vector<Vertex> buf(n);
    EXPECT_NO_THROW(build_axes({ buf.data(), n }, cfg));
    EXPECT_GT(buf[0].color.r, buf[0].color.g);
    EXPECT_GT(buf[0].color.r, buf[0].color.b);
    EXPECT_GT(buf[2].color.g, buf[2].color.r);
    EXPECT_GT(buf[2].color.g, buf[2].color.b);
}

TEST(Axes, BuildAxes3DZAxis) {
    AxesConfig cfg; cfg.extent = 2.f; cfg.is_3d = true;
    const u32 n = axes_vertex_count(cfg);
    std::vector<Vertex> buf(n);
    build_axes({ buf.data(), n }, cfg);
    EXPECT_GT(buf[4].color.b, buf[4].color.r);
    EXPECT_GT(buf[4].color.b, buf[4].color.g);
    EXPECT_NE(buf[4].pos.z, 0.f);
}

TEST(Axes, BuildAxesSpanTooSmallThrows) {
    AxesConfig cfg;
    std::vector<Vertex> buf(1);
    EXPECT_THROW(build_axes({ buf.data(), buf.size() }, cfg), std::invalid_argument);
}

TEST(Grid, VertexCountIsEven) {
    AxesConfig cfg;
    EXPECT_EQ(grid_vertex_count(cfg) % 2u, 0u);
}

TEST(Grid, BuildGridDoesNotThrow) {
    AxesConfig cfg;
    const u32 n = grid_vertex_count(cfg);
    std::vector<Vertex> buf(n);
    EXPECT_NO_THROW(build_grid({ buf.data(), n }, cfg));
}

TEST(Grid, BuildGridSpanTooSmallThrows) {
    AxesConfig cfg;
    std::vector<Vertex> buf(1);
    EXPECT_THROW(build_grid({ buf.data(), buf.size() }, cfg), std::invalid_argument);
}

TEST(Grid, GridLinesDoNotIncludeOrigin) {
    AxesConfig cfg;
    cfg.extent = 2.f; cfg.major_step = 1.f; cfg.minor_step = 0.5f;
    const u32 n = grid_vertex_count(cfg);
    std::vector<Vertex> buf(n);
    build_grid({ buf.data(), n }, cfg);
    for (const auto& v : buf) {
        const bool is_origin = (std::abs(v.pos.x) < 1e-4f && std::abs(v.pos.y) < 1e-4f);
        EXPECT_FALSE(is_origin) << "Grid vertex at origin: (" << v.pos.x << ", " << v.pos.y << ")";
    }
}
