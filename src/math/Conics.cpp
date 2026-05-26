// math/Conics.cpp
#include "math/Conics.hpp"
#include "numeric/ops.hpp"
#include <glm/glm.hpp>
#include <cmath>
#include <stdexcept>
#include <limits>

namespace ndde::math {

// ── IConic base ───────────────────────────────────────────────────────────────

Vec3 IConic::derivative(float t) const {
    const float span = t_max() - t_min();
    const float hh   = span * 1e-4f;
    const float tl   = std::max(t - hh, t_min());
    const float tr   = std::min(t + hh, t_max());
    return (evaluate(tr) - evaluate(tl)) / (tr - tl);
}

Vec3 IConic::second_derivative(float t) const {
    const float span = t_max() - t_min();
    const float hh   = span * 1e-4f;
    const float tl   = std::max(t - hh, t_min());
    const float tr   = std::min(t + hh, t_max());
    return (evaluate(tr) - 2.f * evaluate(t) + evaluate(tl)) / (hh * hh);
}

Vec3 IConic::third_derivative(float t) const {
    const float span = t_max() - t_min();
    const float hh   = span * 1e-3f;
    const float tl   = std::max(t - hh, t_min());
    const float tr   = std::min(t + hh, t_max());
    return (second_derivative(tr) - second_derivative(tl)) / (tr - tl);
}

float IConic::curvature(float t) const {
    const Vec3  d1  = derivative(t);
    const Vec3  d2  = second_derivative(t);
    const float len = ops::length(d1);
    if (len < 1e-8f) return 0.f;
    return ops::length(ops::cross(d1, d2)) / (len * len * len);
}

float IConic::torsion(float t) const {
    const Vec3  d1    = derivative(t);
    const Vec3  d2    = second_derivative(t);
    const Vec3  d3    = third_derivative(t);
    const Vec3  cross = ops::cross(d1, d2);
    const float denom = ops::dot(cross, cross);
    if (denom < 1e-12f) return 0.f;
    return ops::dot(cross, d3) / denom;
}

Vec3 IConic::unit_tangent(float t) const {
    const Vec3  d   = derivative(t);
    const float len = ops::length(d);
    if (len < 1e-8f) return Vec3{1.f, 0.f, 0.f};
    return d / len;
}

Vec3 IConic::unit_normal(float t) const {
    const Vec3  T    = unit_tangent(t);
    const Vec3  d2   = second_derivative(t);
    const Vec3  proj = d2 - ops::dot(d2, T) * T;
    const float len  = ops::length(proj);
    if (len < 1e-8f) {
        Vec3 perp = ops::cross(T, Vec3{0.f, 0.f, 1.f});
        if (ops::length(perp) < 1e-8f) perp = ops::cross(T, Vec3{0.f, 1.f, 0.f});
        return ops::normalize(perp);
    }
    return proj / len;
}

Vec3 IConic::unit_binormal(float t) const {
    return ops::normalize(ops::cross(unit_tangent(t), unit_normal(t)));
}

void IConic::tessellate(std::span<Vertex> out, u32 n, Vec4 color) const {
    if (out.size() < static_cast<std::size_t>(n + 1u))
        throw std::invalid_argument("[IConic::tessellate] output span too small");
    const float t0   = t_min();
    const float step = (t_max() - t0) / static_cast<float>(n);
    for (u32 i = 0; i <= n; ++i)
        out[i] = Vertex{ evaluate(t0 + static_cast<float>(i) * step), color };
}

// ── Parabola ──────────────────────────────────────────────────────────────────

Parabola::Parabola(float a, float b, float c, float tmin, float tmax)
    : m_a(a), m_b(b), m_c(c), m_tmin(tmin), m_tmax(tmax)
{
    if (tmin >= tmax)
        throw std::invalid_argument("[Parabola] t_min must be < t_max");
}

Vec3 Parabola::evaluate(float t)         const { return Vec3{ t, m_a*t*t + m_b*t + m_c, 0.f }; }
Vec3 Parabola::derivative(float t)        const { return Vec3{ 1.f, 2.f*m_a*t + m_b, 0.f }; }
Vec3 Parabola::second_derivative(float)   const { return Vec3{ 0.f, 2.f*m_a, 0.f }; }
Vec3 Parabola::third_derivative(float)    const { return Vec3{ 0.f, 0.f, 0.f }; }

float Parabola::curvature(float t) const {
    const float dy = 2.f * m_a * t + m_b;
    const float d  = ops::pow(1.f + dy * dy, 1.5f);
    return (d < 1e-8f) ? 0.f : ops::abs(2.f * m_a) / d;
}

Vec2 Parabola::vertex() const noexcept {
    if (ops::abs(m_a) < 1e-8f) return Vec2{0.f, m_c};
    const float xv = -m_b / (2.f * m_a);
    return Vec2{ xv, m_a * xv * xv + m_b * xv + m_c };
}

// ── Hyperbola ─────────────────────────────────────────────────────────────────

Hyperbola::Hyperbola(float a, float b, float h, float k,
                     HyperbolaAxis axis, float half_range)
    : m_a(a), m_b(b), m_h(h), m_k(k), m_axis(axis), m_half_range(half_range)
{
    if (a <= 0.f || b <= 0.f)  throw std::invalid_argument("[Hyperbola] a and b must be > 0");
    if (half_range <= 0.f)      throw std::invalid_argument("[Hyperbola] half_range must be > 0");
}

Vec3 Hyperbola::eval_branch(float t, bool positive) const noexcept {
    const float sign = positive ? 1.f : -1.f;
    if (m_axis == HyperbolaAxis::Horizontal)
        return Vec3{ m_h + sign * m_a * ops::cosh(t), m_k + m_b * ops::sinh(t), 0.f };
    else
        return Vec3{ m_h + m_b * ops::sinh(t), m_k + sign * m_a * ops::cosh(t), 0.f };
}

Vec3 Hyperbola::evaluate(float t)         const { return eval_branch(t, true); }
Vec3 Hyperbola::derivative(float t)        const {
    if (m_axis == HyperbolaAxis::Horizontal)
        return Vec3{ m_a*ops::sinh(t), m_b*ops::cosh(t), 0.f };
    else
        return Vec3{ m_b*ops::cosh(t), m_a*ops::sinh(t), 0.f };
}
Vec3 Hyperbola::second_derivative(float t) const {
    if (m_axis == HyperbolaAxis::Horizontal)
        return Vec3{ m_a*ops::cosh(t), m_b*ops::sinh(t), 0.f };
    else
        return Vec3{ m_b*ops::sinh(t), m_a*ops::cosh(t), 0.f };
}
Vec3 Hyperbola::third_derivative(float t)  const { return derivative(t); }

void Hyperbola::tessellate_two_branch(std::span<Vertex> out, u32 n, Vec4 color) const {
    const u32 needed = two_branch_vertex_count(n);
    if (out.size() < static_cast<std::size_t>(needed))
        throw std::invalid_argument("[Hyperbola::tessellate_two_branch] output span too small");
    const float t0   = -m_half_range;
    const float step = (2.f * m_half_range) / static_cast<float>(n);
    for (u32 i = 0; i <= n; ++i)
        out[i] = Vertex{ eval_branch(t0 + static_cast<float>(i) * step, true), color };
    const float nan_val = std::numeric_limits<float>::quiet_NaN();
    out[n + 1u] = Vertex{ Vec3{nan_val, nan_val, nan_val}, Vec4{0.f,0.f,0.f,0.f} };
    const u32 base = n + 2u;
    for (u32 i = 0; i <= n; ++i) {
        const u32 src = n - i;
        out[base + i] = Vertex{ eval_branch(t0 + static_cast<float>(src)*step, false), color };
    }
}

// ── Helix ─────────────────────────────────────────────────────────────────────

Helix::Helix(float r, float pitch, float tmin, float tmax)
    : m_r(r), m_pitch(pitch), m_b(pitch / ops::two_pi_v<float>),
      m_tmin(tmin), m_tmax(tmax)
{
    if (r <= 0.f)      throw std::invalid_argument("[Helix] radius must be > 0");
    if (tmin >= tmax)  throw std::invalid_argument("[Helix] t_min must be < t_max");
}

Vec3  Helix::evaluate(float t)          const { return Vec3{ m_r*ops::cos(t), m_r*ops::sin(t), m_b*t }; }
Vec3  Helix::derivative(float t)         const { return Vec3{ -m_r*ops::sin(t), m_r*ops::cos(t), m_b }; }
Vec3  Helix::second_derivative(float t)  const { return Vec3{ -m_r*ops::cos(t), -m_r*ops::sin(t), 0.f }; }
Vec3  Helix::third_derivative(float t)   const { return Vec3{ m_r*ops::sin(t), -m_r*ops::cos(t), 0.f }; }
float Helix::curvature(float /*t*/)      const { return m_r / (m_r*m_r + m_b*m_b); }
float Helix::torsion(float /*t*/)        const { return m_b / (m_r*m_r + m_b*m_b); }

// ── ParaboloidCurve ───────────────────────────────────────────────────────────
// p(t) = (t·cosθ,  t·sinθ,  a·t²)
//
// This is the meridional curve of the paraboloid z = a(x²+y²) at angle θ.
// It is the intersection of the paraboloid with the vertical plane
//   { y/x = tanθ, x ≥ 0 }  — a literal parabola lying on the surface.
//
// Derivatives (all exact):
//   p'(t)  = (cosθ,  sinθ,  2at)
//   p''(t) = (0,     0,     2a )
//   p'''(t)= (0,     0,     0  )
//
// Since p''' = 0, torsion τ = 0 for all t. The curve is planar.
// Its curvature κ = 2|a| / (1 + 4a²t²)^(3/2) equals κ₁(u=t) of the paraboloid,
// confirming it is a principal curvature line in the meridional direction.

ParaboloidCurve::ParaboloidCurve(float a, float theta, float tmin, float tmax)
    : m_a(a), m_theta(theta),
      m_ct(ops::cos(theta)), m_st(ops::sin(theta)),
      m_tmin(tmin), m_tmax(tmax)
{
    if (tmin >= tmax) throw std::invalid_argument("[ParaboloidCurve] t_min must be < t_max");
}

Vec3 ParaboloidCurve::evaluate(float t) const {
    return Vec3{ t * m_ct,  t * m_st,  m_a * t * t };
}

Vec3 ParaboloidCurve::derivative(float t) const {
    return Vec3{ m_ct,  m_st,  2.f * m_a * t };
}

Vec3 ParaboloidCurve::second_derivative(float /*t*/) const {
    return Vec3{ 0.f,  0.f,  2.f * m_a };
}

Vec3 ParaboloidCurve::third_derivative(float /*t*/) const {
    return Vec3{ 0.f, 0.f, 0.f };
}

float ParaboloidCurve::curvature(float t) const {
    // κ = |p' × p''| / |p'|³
    //
    // p' = (cosθ, sinθ, 2at)    |p'|² = 1 + 4a²t²
    // p'' = (0, 0, 2a)
    //
    // p' × p'' = | i         j        k   |
    //            | cosθ      sinθ      2at |
    //            | 0         0         2a  |
    //          = (sinθ·2a - 0, 0 - cosθ·2a, 0)
    //          = (2a·sinθ, -2a·cosθ, 0)
    //
    // |p' × p''| = 2|a|  (since sin²θ + cos²θ = 1)
    //
    // κ = 2|a| / (1 + 4a²t²)^(3/2)
    const float s = 1.f + 4.f * m_a * m_a * t * t;
    return 2.f * ops::abs(m_a) / ops::pow(s, 1.5f);
}

} // namespace ndde::math
