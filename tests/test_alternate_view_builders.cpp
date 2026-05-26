#include "app/SimulationRenderPackets.hpp"

#include <gtest/gtest.h>

#include <span>

namespace {

class PlaneSurface final : public ndde::math::ISurface {
public:
    PlaneSurface(float a, float b) : m_a(a), m_b(b) {}

    [[nodiscard]] ndde::Vec3 evaluate(float u, float v, float = 0.f) const override {
        return {u, v, m_a * u + m_b * v};
    }
    [[nodiscard]] ndde::Vec3 du(float, float, float = 0.f) const override {
        return {1.f, 0.f, m_a};
    }
    [[nodiscard]] ndde::Vec3 dv(float, float, float = 0.f) const override {
        return {0.f, 1.f, m_b};
    }
    [[nodiscard]] float u_min(float = 0.f) const override { return -1.f; }
    [[nodiscard]] float u_max(float = 0.f) const override { return 1.f; }
    [[nodiscard]] float v_min(float = 0.f) const override { return -1.f; }
    [[nodiscard]] float v_max(float = 0.f) const override { return 1.f; }

private:
    float m_a = 0.f;
    float m_b = 0.f;
};

class QuadraticGraphSurface final : public ndde::math::ISurface {
public:
    [[nodiscard]] ndde::Vec3 evaluate(float u, float v, float = 0.f) const override {
        return {u, v, u * u + v};
    }
    [[nodiscard]] ndde::Vec3 du(float u, float, float = 0.f) const override {
        return {1.f, 0.f, 2.f * u};
    }
    [[nodiscard]] ndde::Vec3 dv(float, float, float = 0.f) const override {
        return {0.f, 1.f, 1.f};
    }
    [[nodiscard]] float u_min(float = 0.f) const override { return -1.f; }
    [[nodiscard]] float u_max(float = 0.f) const override { return 1.f; }
    [[nodiscard]] float v_min(float = 0.f) const override { return -1.f; }
    [[nodiscard]] float v_max(float = 0.f) const override { return 1.f; }
};

} // namespace

TEST(AlternateViewBuilders, LevelCurveExtractionFindsPlaneZeroCrossings) {
    PlaneSurface surface{1.f, 0.f};
    const auto vertices = ndde::build_level_curve_vertices(surface, 16u, 0.f, 1u, {1.f, 1.f, 1.f, 1.f});

    ASSERT_FALSE(vertices.empty());
    ASSERT_EQ(vertices.size() % 2u, 0u);
    for (const ndde::Vertex& vertex : vertices)
        EXPECT_NEAR(vertex.pos.x, 0.f, 1e-4f);
}

TEST(AlternateViewBuilders, VectorFieldModesMatchPlaneGradient) {
    PlaneSurface surface{2.f, -1.f};
    const ndde::Vec2 gradient = glm::normalize(ndde::Vec2{2.f, -1.f});

    const auto up = ndde::build_vector_field_vertices(
        surface, 4u, 0.f, ndde::VectorFieldMode::Gradient, 1.f, {1.f, 1.f, 1.f, 1.f});
    ASSERT_GE(up.size(), 2u);
    ndde::Vec2 delta{up[1].pos.x - up[0].pos.x, up[1].pos.y - up[0].pos.y};
    EXPECT_GT(glm::dot(glm::normalize(delta), gradient), 0.99f);

    const auto down = ndde::build_vector_field_vertices(
        surface, 4u, 0.f, ndde::VectorFieldMode::NegativeGradient, 1.f, {1.f, 1.f, 1.f, 1.f});
    ASSERT_GE(down.size(), 2u);
    delta = {down[1].pos.x - down[0].pos.x, down[1].pos.y - down[0].pos.y};
    EXPECT_LT(glm::dot(glm::normalize(delta), gradient), -0.99f);

    const auto tangent = ndde::build_vector_field_vertices(
        surface, 4u, 0.f, ndde::VectorFieldMode::LevelTangent, 1.f, {1.f, 1.f, 1.f, 1.f});
    ASSERT_GE(tangent.size(), 2u);
    delta = {tangent[1].pos.x - tangent[0].pos.x, tangent[1].pos.y - tangent[0].pos.y};
    EXPECT_NEAR(glm::dot(glm::normalize(delta), gradient), 0.f, 1e-4f);
}

TEST(AlternateViewBuilders, IsoclinesTraceDirectionalSlopeLevelSets) {
    QuadraticGraphSurface surface;
    const auto vertices = ndde::build_isocline_vertices(
        surface, 32u, 0.f, 0.f, 0.f, 0.01f, 1u, {0.2f, 0.8f, 1.f, 1.f});

    ASSERT_FALSE(vertices.empty());
    ASSERT_EQ(vertices.size() % 2u, 0u);
    for (const ndde::Vertex& vertex : vertices)
        EXPECT_NEAR(vertex.pos.x, 0.f, 0.04f);
}

TEST(AlternateViewBuilders, FlowIntegratesThroughVectorField) {
    PlaneSurface surface{1.f, 0.f};
    const auto vertices = ndde::build_flow_vertices(
        surface, 5u, 8u, 0.05f, 0.f, ndde::VectorFieldMode::NegativeGradient, {0.f, 1.f, 0.f, 1.f});

    ASSERT_FALSE(vertices.empty());
    ASSERT_EQ(vertices.size() % 2u, 0u);
    for (std::size_t i = 0; i + 1u < vertices.size(); i += 2u)
        EXPECT_LT(vertices[i + 1u].pos.x, vertices[i].pos.x);
}

TEST(AlternateViewBuilders, BuildersBindReturnedVectorsToFrameMemoryWhenProvided) {
    PlaneSurface surface{1.f, 0.f};
    ndde::memory::MemoryService memory;
    memory.begin_frame();

    const auto vertices = ndde::build_vector_field_vertices(
        surface, 4u, 0.f, ndde::VectorFieldMode::Gradient, 1.f,
        {1.f, 1.f, 1.f, 1.f}, &memory);

    ASSERT_FALSE(vertices.empty());
    EXPECT_EQ(vertices.get_allocator().resource(), memory.frame().resource());
}

TEST(AlternateViewBuilders, TypedAlternateViewDispatchEmitsVectorPacket) {
    PlaneSurface surface{1.f, 0.f};
    ndde::memory::MemoryService memory;
    memory.begin_frame();
    ndde::RenderService render;
    render.set_memory_service(&memory);
    ndde::RenderViewId view = 0;
    const auto handle = render.register_view(ndde::RenderViewDescriptor{
        .title = "test alternate",
        .kind = ndde::RenderViewKind::Alternate,
        .alternate_mode = ndde::AlternateViewMode::VectorField,
        .alternate = {
            .vector_mode = ndde::VectorFieldMode::Gradient,
            .vector_samples = 4u,
            .vector_scale = 1.f
        }
    }, &view);

    ndde::SurfaceMeshCache mesh;
    ndde::ParticleSystem particles(&surface, 1u);
    ndde::submit_typed_alternate_view(render, view, surface, mesh, particles,
        ndde::SurfaceMeshOptions{.grid_lines = 8u, .build_contour = false}, ndde::Mat4{1.f}, &memory);

    EXPECT_EQ(render.packet_count(view), 1u);
    ASSERT_EQ(render.packets().size(), 1u);
    EXPECT_EQ(render.packets()[0].topology, ndde::Topology::LineList);
    EXPECT_FALSE(render.packets()[0].vertices.empty());
    (void)handle;
}
