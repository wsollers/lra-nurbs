// tests/test_numeric.cpp
// Unit tests for ndde_numeric: Constants, MathTraits, Vec3Ops, Differentiator.
//
// These tests are the VALIDATION ORACLE — they verify that our ndde::numeric
// implementations agree with std:: / GLM to within expected error bounds.
// When we replace the std:: delegation with in-house Taylor series, these
// tests define "passing" and become the regression suite.
//
// Design:
//   - No Vulkan. No GLM vertex types. No GPU headers.
//   - Pure math: f32 / f64 scalars and glm::vec3 / glm::dvec3 only.
//   - Tolerances are set to 10x the theoretical FD error bound where possible.

#include <gtest/gtest.h>
#include "numeric/Constants.hpp"
#include "numeric/MathTraits.hpp"
#include "numeric/ops.hpp"
#include "numeric/Vec3Ops.hpp"
#include "numeric/Differentiator.hpp"
#include <cmath>
#include <numbers>
#include <glm/glm.hpp>

using namespace ndde;
using namespace ndde::numeric;

// ══════════════════════════════════════════════════════════════════════════════
// Constants
// ══════════════════════════════════════════════════════════════════════════════

TEST(Constants, PiF32VsStd) {
    constexpr f32 ndde_pi = Constants<f32>::PI;
    EXPECT_NEAR(ndde_pi, std::numbers::pi_v<f32>, 1e-6f);
}

TEST(Constants, PiF64VsStd) {
    constexpr f64 ndde_pi = Constants<f64>::PI;
    EXPECT_NEAR(ndde_pi, std::numbers::pi, 1e-14);
}

TEST(Constants, TwoPiIsDoublePi) {
    EXPECT_NEAR(Constants<f64>::TWO_PI, 2.0 * Constants<f64>::PI, 1e-14);
}

TEST(Constants, EulerF64VsStd) {
    EXPECT_NEAR(Constants<f64>::E, std::exp(1.0), 1e-14);
}

TEST(Constants, Sqrt2Squared) {
    const f64 s = Constants<f64>::SQRT_2;
    EXPECT_NEAR(s * s, 2.0, 1e-14);
}

TEST(Constants, SqrtTwoPiSquared) {
    const f64 s = Constants<f64>::SQRT_TWO_PI;
    EXPECT_NEAR(s * s, Constants<f64>::TWO_PI, 1e-13);
}

TEST(Constants, AliasMatchesStruct) {
    EXPECT_EQ(pi<f32>,    Constants<f32>::PI);
    EXPECT_EQ(two_pi<f64>,Constants<f64>::TWO_PI);
    EXPECT_EQ(sqrt2<f64>, Constants<f64>::SQRT_2);
}

// ══════════════════════════════════════════════════════════════════════════════
// MathTraits — scalar functions
// ══════════════════════════════════════════════════════════════════════════════

TEST(MathTraits, CosF32VsStd) {
    for (float t : {0.f, 0.5f, 1.f, 1.5f, 2.f, 3.14f}) {
        EXPECT_NEAR(MathTraits<f32>::cos(t), std::cos(t), 1e-6f) << "t=" << t;
    }
}

TEST(MathTraits, SinF64VsStd) {
    for (double t : {0.0, 0.3, 0.7, 1.1, 2.2, 3.1}) {
        EXPECT_NEAR(MathTraits<f64>::sin(t), std::sin(t), 1e-14) << "t=" << t;
    }
}

TEST(MathTraits, TaylorSinF64VsStdOnReducedDomain) {
    for (double t : {-Constants<f64>::PI, -2.4, -1.7, -1.0, -0.3,
                      0.0, 0.3, 1.0, 1.7, 2.4, Constants<f64>::PI}) {
        EXPECT_NEAR(MathTraits<f64>::taylor_sin(t), std::sin(t), 5e-15) << "t=" << t;
    }
}

TEST(MathTraits, TaylorSinF32VsStdAcrossSimulationAngles) {
    for (float t : {-50.f, -17.25f, -6.2f, -3.1415926f, -1.0f,
                     0.f, 1.0f, 3.1415926f, 6.2f, 17.25f, 50.f}) {
        EXPECT_NEAR(MathTraits<f32>::taylor_sin(t), std::sin(t), 2e-6f) << "t=" << t;
    }
}

TEST(MathTraits, TaylorSinIsOdd) {
    for (double t : {0.1, 0.7, 1.3, 2.2, 8.0}) {
        EXPECT_NEAR(MathTraits<f64>::taylor_sin(-t), -MathTraits<f64>::taylor_sin(t), 1e-14)
            << "t=" << t;
    }
}

TEST(MathTraits, OpsSinUsesTaylorWhenConfigured) {
    const f64 x = 1.2345;
    if constexpr (!ndde::use_builtin_math && ndde::use_taylor_sin) {
        EXPECT_EQ(ops::sin(x), MathTraits<f64>::taylor_sin(x));
    } else {
        EXPECT_NEAR(ops::sin(x), std::sin(x), 1e-14);
    }
}

TEST(MathTraits, CoshSinhIdentity) {
    // cosh²(x) - sinh²(x) = 1  ∀ x
    for (double x : {0.0, 0.5, 1.0, 2.0, 3.0}) {
        const f64 c = MathTraits<f64>::cosh(x);
        const f64 s = MathTraits<f64>::sinh(x);
        EXPECT_NEAR(c*c - s*s, 1.0, 1e-12) << "x=" << x;
    }
}

TEST(MathTraits, SqrtSquareRoundTrip) {
    for (f64 x : {0.0, 0.25, 1.0, 2.0, 9.0, 100.0}) {
        const f64 s = MathTraits<f64>::sqrt(x);
        EXPECT_NEAR(s * s, x, 1e-12) << "x=" << x;
    }
}

TEST(MathTraits, Pow1_5) {
    // x^1.5 = x * sqrt(x)
    for (f64 x : {1.0, 2.0, 4.0, 9.0}) {
        EXPECT_NEAR(MathTraits<f64>::pow_1_5(x), std::pow(x, 1.5), 1e-12) << "x=" << x;
    }
}

TEST(MathTraits, NearZeroTrue) {
    EXPECT_TRUE(MathTraits<f32>::near_zero(0.f));
    EXPECT_TRUE(MathTraits<f32>::near_zero(5e-7f));
    EXPECT_TRUE(MathTraits<f64>::near_zero(5e-13));
}

TEST(MathTraits, NearZeroFalse) {
    EXPECT_FALSE(MathTraits<f32>::near_zero(1e-5f));
    EXPECT_FALSE(MathTraits<f64>::near_zero(1e-11));
}

TEST(MathTraits, SafeSqrtNegativeInput) {
    // Must not produce NaN — returns sqrt(0) = 0.
    EXPECT_EQ(MathTraits<f32>::safe_sqrt(-1.f), 0.f);
    EXPECT_EQ(MathTraits<f64>::safe_sqrt(-1.0), 0.0);
}

TEST(MathTraits, SafeDivByZeroReturnsFallback) {
    EXPECT_EQ(MathTraits<f32>::safe_div(1.f, 0.f, 99.f), 99.f);
    EXPECT_EQ(MathTraits<f64>::safe_div(1.0, 0.0, 99.0), 99.0);
}

TEST(MathTraits, SafeDivNormalCase) {
    EXPECT_NEAR(MathTraits<f64>::safe_div(6.0, 2.0), 3.0, 1e-14);
}

TEST(MathTraits, AbsNegative) {
    EXPECT_EQ(MathTraits<f32>::abs(-3.5f), 3.5f);
    EXPECT_EQ(MathTraits<f64>::abs(-3.5),  3.5);
}

TEST(MathTraits, FmodBasic) {
    EXPECT_NEAR(MathTraits<f64>::fmod(5.3, 2.0), std::fmod(5.3, 2.0), 1e-14);
}

TEST(MathTraits, ClampRange) {
    EXPECT_EQ(MathTraits<f32>::clamp(-2.f, 0.f, 1.f), 0.f);
    EXPECT_EQ(MathTraits<f32>::clamp( 2.f, 0.f, 1.f), 1.f);
    EXPECT_EQ(MathTraits<f32>::clamp( 0.5f,0.f, 1.f), 0.5f);
}

// ── Lipschitz verification ────────────────────────────────────────────────────
// |sin(x) − sin(y)| ≤ L_sin · |x − y|  with L_sin = 1.
// Sample 100 pairs and verify the inequality holds.

TEST(MathTraits, SinLipschitz) {
    constexpr f64 L = MathTraits<f64>::L_sin;
    for (int i = 0; i < 100; ++i) {
        const f64 x = static_cast<f64>(i) * 0.1;
        const f64 y = x + 0.37;
        const f64 lhs = std::abs(MathTraits<f64>::sin(x) - MathTraits<f64>::sin(y));
        const f64 rhs = L * std::abs(x - y);
        EXPECT_LE(lhs, rhs + 1e-14) << "Lipschitz violated at x=" << x;
    }
}

TEST(MathTraits, CosLipschitz) {
    constexpr f64 L = MathTraits<f64>::L_cos;
    for (int i = 0; i < 100; ++i) {
        const f64 x = static_cast<f64>(i) * 0.1;
        const f64 y = x + 0.5;
        const f64 lhs = std::abs(MathTraits<f64>::cos(x) - MathTraits<f64>::cos(y));
        const f64 rhs = L * std::abs(x - y);
        EXPECT_LE(lhs, rhs + 1e-14) << "Lipschitz violated at x=" << x;
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Vec3Ops
// ══════════════════════════════════════════════════════════════════════════════

TEST(Vec3Ops, DotOrthogonal) {
    const glm::vec3 u{1.f, 0.f, 0.f};
    const glm::vec3 v{0.f, 1.f, 0.f};
    EXPECT_NEAR(Vec3Ops<f32>::dot(u, v), 0.f, 1e-7f);
}

TEST(Vec3Ops, DotParallel) {
    const glm::vec3 u{1.f, 2.f, 3.f};
    EXPECT_NEAR(Vec3Ops<f32>::dot(u, u), 14.f, 1e-6f);  // 1+4+9=14
}

TEST(Vec3Ops, LengthUnit) {
    EXPECT_NEAR(Vec3Ops<f32>::length({1.f, 0.f, 0.f}), 1.f, 1e-7f);
    EXPECT_NEAR(Vec3Ops<f32>::length({0.f, 1.f, 0.f}), 1.f, 1e-7f);
}

TEST(Vec3Ops, LengthGeneral) {
    // ‖(3,4,0)‖ = 5
    EXPECT_NEAR(Vec3Ops<f32>::length({3.f, 4.f, 0.f}), 5.f, 1e-6f);
}

TEST(Vec3Ops, CrossOrthogonality) {
    // Cross product must be perpendicular to both inputs (P5)
    const glm::vec3 u{1.f, 2.f, 3.f};
    const glm::vec3 v{4.f, 5.f, 6.f};
    const glm::vec3 c = Vec3Ops<f32>::cross(u, v);
    EXPECT_NEAR(Vec3Ops<f32>::dot(c, u), 0.f, 1e-5f);
    EXPECT_NEAR(Vec3Ops<f32>::dot(c, v), 0.f, 1e-5f);
}

TEST(Vec3Ops, CrossAntiCommutative) {
    const glm::vec3 u{1.f, 0.f, 0.f};
    const glm::vec3 v{0.f, 1.f, 0.f};
    const glm::vec3 uv = Vec3Ops<f32>::cross(u, v);
    const glm::vec3 vu = Vec3Ops<f32>::cross(v, u);
    EXPECT_NEAR(uv.x, -vu.x, 1e-7f);
    EXPECT_NEAR(uv.y, -vu.y, 1e-7f);
    EXPECT_NEAR(uv.z, -vu.z, 1e-7f);
}

TEST(Vec3Ops, NormalizeUnit) {
    const glm::vec3 v{3.f, 4.f, 0.f};
    const glm::vec3 n = Vec3Ops<f32>::normalize(v);
    EXPECT_NEAR(Vec3Ops<f32>::length(n), 1.f, 1e-6f);  // ‖normalize(v)‖ = 1
}

TEST(Vec3Ops, NormalizeSafeZeroVector) {
    // Zero vector should return fallback, not NaN.
    const glm::vec3 zero{0.f};
    const glm::vec3 fallback{1.f, 0.f, 0.f};
    const glm::vec3 result = Vec3Ops<f32>::normalize_safe(zero, fallback);
    EXPECT_NEAR(result.x, fallback.x, 1e-7f);
}

TEST(Vec3Ops, RejectOrthogonality) {
    // reject(u, v) must be orthogonal to v
    const glm::vec3 u{1.f, 2.f, 3.f};
    const glm::vec3 v{1.f, 0.f, 0.f};
    const glm::vec3 r = Vec3Ops<f32>::reject(u, v);
    EXPECT_NEAR(Vec3Ops<f32>::dot(r, v), 0.f, 1e-6f);
}

TEST(Vec3Ops, ProjPlusRejectEqualsOriginal) {
    // proj(u,v) + reject(u,v) = u  (partition of u)
    const glm::vec3 u{2.f, 3.f, -1.f};
    const glm::vec3 v{0.f, 1.f,  0.f};
    const glm::vec3 p = Vec3Ops<f32>::proj(u, v);
    const glm::vec3 r = Vec3Ops<f32>::reject(u, v);
    EXPECT_NEAR((p + r).x, u.x, 1e-6f);
    EXPECT_NEAR((p + r).y, u.y, 1e-6f);
    EXPECT_NEAR((p + r).z, u.z, 1e-6f);
}

// ── Cauchy–Schwarz verification ───────────────────────────────────────────────
// |⟨u,v⟩| ≤ ‖u‖·‖v‖

TEST(Vec3Ops, CauchySchwarz) {
    const glm::vec3 u{1.f, 2.f, 3.f};
    const glm::vec3 v{4.f, -1.f, 2.f};
    const f32 lhs = std::abs(Vec3Ops<f32>::dot(u, v));
    const f32 rhs = Vec3Ops<f32>::length(u) * Vec3Ops<f32>::length(v);
    EXPECT_LE(lhs, rhs + 1e-5f);
}

// ══════════════════════════════════════════════════════════════════════════════
// Differentiator — finite differences
// ══════════════════════════════════════════════════════════════════════════════

TEST(Differentiator, D1HelixtVsAnalytic) {
    // p(t) = (cos t, sin t, 0)
    // p'(t) = (-sin t, cos t, 0)  — analytic
    const auto p = [](f64 t) -> glm::dvec3 {
        return { std::cos(t), std::sin(t), 0.0 };
    };
    const f64 t   = 1.2;
    const auto fd = Diff3d::d1(p, t, 0.0, 6.283185, Diff3d::k_h1_rel);
    EXPECT_NEAR(fd.x, -std::sin(t), 1e-7);
    EXPECT_NEAR(fd.y,  std::cos(t), 1e-7);
    EXPECT_NEAR(fd.z,  0.0,         1e-10);
}

TEST(Differentiator, D2ParabolaIsConstant) {
    // p(t) = (t, t², 0)  →  p''(t) = (0, 2, 0)  for all t
    const auto p = [](f64 t) -> glm::dvec3 {
        return { t, t * t, 0.0 };
    };
    for (f64 t : {0.0, 0.5, 1.0, 1.5}) {
        const auto fd = Diff3d::d2(p, t, -2.0, 2.0, Diff3d::k_h2_rel);
        EXPECT_NEAR(fd.x, 0.0, 1e-5) << "t=" << t;
        EXPECT_NEAR(fd.y, 2.0, 1e-5) << "t=" << t;
        EXPECT_NEAR(fd.z, 0.0, 1e-10) << "t=" << t;
    }
}

TEST(Differentiator, CurvatureOfCircle) {
    // Unit circle: p(t) = (cos t, sin t, 0)
    // Curvature κ = 1 everywhere (radius = 1).
    const auto p = [](f32 t) -> Vec3 {
        return { std::cos(t), std::sin(t), 0.f };
    };
    const f32 t_min = 0.f;
    const f32 t_max = 6.28318f;
    for (f32 t : {0.5f, 1.0f, 1.5f, 2.0f, 3.0f}) {
        const Vec3 d1v = Diff3f::d1(p, t, t_min, t_max);
        const Vec3 d2v = Diff3f::d2(p, t, t_min, t_max);
        const f32  k   = Diff3f::curvature(d1v, d2v);
        EXPECT_NEAR(k, 1.f, 5e-3f) << "t=" << t;   // 5e-3 tolerance for f32 FD
    }
}

TEST(Differentiator, TorsionOfHelix) {
    // Circular helix: p(t) = (r·cos t, r·sin t, b·t)
    // Analytic torsion: τ = b / (r² + b²)
    const f64 r = 1.0, b = 0.5;
    const f64 tau_analytic = b / (r*r + b*b);
    const auto p = [r, b](f64 t) -> glm::dvec3 {
        return { r*std::cos(t), r*std::sin(t), b*t };
    };
    for (f64 t : {0.5, 1.0, 2.0, 3.0}) {
        const auto d1v = Diff3d::d1(p, t, 0.0, 12.566, Diff3d::k_h1_rel);
        const auto d2v = Diff3d::d2(p, t, 0.0, 12.566, Diff3d::k_h2_rel);
        const auto d3v = Diff3d::d3(p, t, 0.0, 12.566, Diff3d::k_h3_rel);
        const f64  tau = Diff3d::torsion(d1v, d2v, d3v);
        EXPECT_NEAR(tau, tau_analytic, 1e-5) << "t=" << t;
    }
}

TEST(Differentiator, TorsionOfPlanarCurveIsZero) {
    // Parabola p(t) = (t, t², 0) lies in the xy-plane → τ = 0.
    const auto p = [](f64 t) -> glm::dvec3 { return { t, t*t, 0.0 }; };
    for (f64 t : {0.2, 0.8, 1.3}) {
        const auto d1v = Diff3d::d1(p, t, -2.0, 2.0);
        const auto d2v = Diff3d::d2(p, t, -2.0, 2.0);
        const auto d3v = Diff3d::d3(p, t, -2.0, 2.0);
        EXPECT_NEAR(Diff3d::torsion(d1v, d2v, d3v), 0.0, 1e-9) << "t=" << t;
    }
}
