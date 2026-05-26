// tests/test_conics.cpp
// Unit tests for math::Parabola and math::Hyperbola.
// No Vulkan, no GPU — pure math verification.
#include <gtest/gtest.h>
#include "math/Conics.hpp"
#include <cmath>
#include <vector>

using namespace ndde;
using namespace ndde::math;

// ── Parabola ──────────────────────────────────────────────────────────────────

TEST(Parabola, EvaluateAtOrigin) {
    Parabola p(1.f, 0.f, 0.f, -1.f, 1.f);
    Vec3 v = p.evaluate(0.f);
    EXPECT_FLOAT_EQ(v.x, 0.f);
    EXPECT_FLOAT_EQ(v.y, 0.f);
    EXPECT_FLOAT_EQ(v.z, 0.f);
}

TEST(Parabola, EvaluateQuadratic) {
    // y = 2t^2 - 3t + 1
    Parabola p(2.f, -3.f, 1.f, -2.f, 2.f);
    Vec3 v = p.evaluate(1.f);
    EXPECT_FLOAT_EQ(v.x, 1.f);
    EXPECT_FLOAT_EQ(v.y, 2.f - 3.f + 1.f);
    EXPECT_FLOAT_EQ(v.z, 0.f);
}

TEST(Parabola, DerivativeIsAnalytic) {
    Parabola p(3.f, -1.f, 0.5f, -2.f, 2.f);
    const float t = 0.75f;
    Vec3 d = p.derivative(t);
    EXPECT_FLOAT_EQ(d.x, 1.f);
    EXPECT_NEAR(d.y, 2.f * 3.f * t + (-1.f), 1e-5f);
    EXPECT_FLOAT_EQ(d.z, 0.f);
}

TEST(Parabola, SecondDerivativeIsConstant) {
    // p''(t) = (0, 2a, 0) — independent of t
    Parabola p(2.5f, 1.f, 0.f, -1.f, 1.f);
    Vec3 d2_at_0  = p.second_derivative(0.f);
    Vec3 d2_at_05 = p.second_derivative(0.5f);
    EXPECT_NEAR(d2_at_0.y,  5.f, 1e-5f);
    EXPECT_NEAR(d2_at_05.y, 5.f, 1e-5f);
}

TEST(Parabola, CurvatureAtVertex) {
    // For y = ax^2, k(0) = |2a|
    Parabola p(2.f, 0.f, 0.f, -1.f, 1.f);
    EXPECT_NEAR(p.curvature(0.f), 4.f, 1e-4f);
}

TEST(Parabola, VertexPosition) {
    // y = -x^2 + 4x — vertex at x=2, y=4
    Parabola p(-1.f, 4.f, 0.f, -1.f, 5.f);
    Vec2 v = p.vertex();
    EXPECT_NEAR(v.x, 2.f, 1e-5f);
    EXPECT_NEAR(v.y, 4.f, 1e-5f);
}

TEST(Parabola, InvalidRangeThrows) {
    EXPECT_THROW(Parabola(1.f, 0.f, 0.f, 1.f, -1.f), std::invalid_argument);
    EXPECT_THROW(Parabola(1.f, 0.f, 0.f, 0.f,  0.f), std::invalid_argument);
}

TEST(Parabola, TessellateSpanSize) {
    Parabola p(1.f, 0.f, 0.f, -1.f, 1.f);
    const u32 n = 100;
    std::vector<Vertex> buf(p.vertex_count(n));
    Vec4 col{1.f, 0.f, 0.f, 1.f};
    EXPECT_NO_THROW(p.tessellate({ buf.data(), buf.size() }, n, col));
    EXPECT_EQ(buf.size(), 101u);
}

TEST(Parabola, TessellateSpanTooSmallThrows) {
    Parabola p(1.f, 0.f, 0.f, -1.f, 1.f);
    std::vector<Vertex> buf(5);
    EXPECT_THROW(p.tessellate({ buf.data(), buf.size() }, 100u), std::invalid_argument);
}

// ── Hyperbola ─────────────────────────────────────────────────────────────────

TEST(Hyperbola, EvaluateAtZeroIsOnCurve) {
    Hyperbola h(2.f, 1.f, 0.f, 0.f, HyperbolaAxis::Horizontal, 2.f);
    Vec3 p = h.evaluate(0.f);
    EXPECT_NEAR(p.x, 2.f, 1e-5f);
    EXPECT_NEAR(p.y, 0.f, 1e-5f);
    EXPECT_FLOAT_EQ(p.z, 0.f);
}

TEST(Hyperbola, SatisfiesHyperbolaEquation) {
    const float a = 1.5f, b = 0.8f;
    Hyperbola hyp(a, b, 0.f, 0.f, HyperbolaAxis::Horizontal, 2.f);
    for (float t : {-1.5f, -0.5f, 0.f, 0.5f, 1.5f}) {
        Vec3 p = hyp.evaluate(t);
        const float lhs = (p.x * p.x) / (a * a) - (p.y * p.y) / (b * b);
        EXPECT_NEAR(lhs, 1.f, 1e-4f) << "at t=" << t;
    }
}

TEST(Hyperbola, DerivativeMatchesFD) {
    Hyperbola hyp(1.f, 1.f, 0.f, 0.f, HyperbolaAxis::Horizontal, 2.f);
    const float t = 0.8f;
    const float dt = 1e-4f;
    Vec3 fd = (hyp.evaluate(t + dt) - hyp.evaluate(t - dt)) / (2.f * dt);
    Vec3 an = hyp.derivative(t);
    EXPECT_NEAR(fd.x, an.x, 1e-3f);
    EXPECT_NEAR(fd.y, an.y, 1e-3f);
}

TEST(Hyperbola, InvalidParametersThrow) {
    EXPECT_THROW(Hyperbola(-1.f, 1.f), std::invalid_argument);
    EXPECT_THROW(Hyperbola(1.f, -1.f), std::invalid_argument);
    EXPECT_THROW(Hyperbola(1.f, 1.f, 0.f, 0.f, HyperbolaAxis::Horizontal, -1.f),
                 std::invalid_argument);
}

TEST(Hyperbola, TwoBranchBufferSize) {
    Hyperbola hyp(1.f, 1.f, 0.f, 0.f, HyperbolaAxis::Horizontal, 2.f);
    const u32 n = 50;
    const u32 expected = hyp.two_branch_vertex_count(n);
    EXPECT_EQ(expected, 103u);

    std::vector<Vertex> buf(expected);
    EXPECT_NO_THROW(hyp.tessellate_two_branch({ buf.data(), buf.size() }, n));
    EXPECT_FLOAT_EQ(buf[n + 1u].color.a, 0.f);
}
