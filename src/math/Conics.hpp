#pragma once
// math/Conics.hpp
// Parametric curve interface and concrete conic sections + space curves.
// Zero-copy design: tessellate() writes directly into GPU-visible memory.

#include "math/Scalars.hpp"
#include "numeric/ops.hpp"
#include "math/GeometryTypes.hpp"
#include <span>
#include <stdexcept>
#include <cmath>

namespace ndde::math {

// ── IConic ────────────────────────────────────────────────────────────────────

class IConic {
public:
    virtual ~IConic() = default;

    [[nodiscard]] virtual Vec3  evaluate(float t)         const = 0;
    [[nodiscard]] virtual float t_min()                   const = 0;
    [[nodiscard]] virtual float t_max()                   const = 0;

    [[nodiscard]] virtual Vec3  derivative(float t)        const;
    [[nodiscard]] virtual Vec3  second_derivative(float t) const;
    [[nodiscard]] virtual Vec3  third_derivative(float t)  const;
    [[nodiscard]] virtual float curvature(float t)         const;
    [[nodiscard]] virtual float torsion(float t)           const;
    [[nodiscard]]         Vec3  unit_tangent(float t)      const;
    [[nodiscard]]         Vec3  unit_normal(float t)       const;
    [[nodiscard]]         Vec3  unit_binormal(float t)     const;

    void tessellate(std::span<Vertex> out, u32 n,
                    Vec4 color = { 1.f, 1.f, 1.f, 1.f }) const;

    [[nodiscard]] u32 vertex_count(u32 n) const noexcept { return n + 1; }

protected:
    IConic() = default;
    IConic(const IConic&) = default;
    IConic& operator=(const IConic&) = default;
    IConic(IConic&&) = default;
    IConic& operator=(IConic&&) = default;
};

// ── Parabola ──────────────────────────────────────────────────────────────────
// p(t) = (t,  a*t^2 + b*t + c,  0)   t in [t_min, t_max]

class Parabola final : public IConic {
public:
    explicit Parabola(float a = 1.f, float b = 0.f, float c = 0.f,
                      float tmin = -1.f, float tmax = 1.f);

    [[nodiscard]] Vec3  evaluate(float t)          const override;
    [[nodiscard]] Vec3  derivative(float t)         const override;
    [[nodiscard]] Vec3  second_derivative(float t)  const override;
    [[nodiscard]] Vec3  third_derivative(float t)   const override;
    [[nodiscard]] float curvature(float t)           const override;

    [[nodiscard]] float t_min() const override { return m_tmin; }
    [[nodiscard]] float t_max() const override { return m_tmax; }

    [[nodiscard]] float a() const noexcept { return m_a; }
    [[nodiscard]] float b() const noexcept { return m_b; }
    [[nodiscard]] float c() const noexcept { return m_c; }
    [[nodiscard]] Vec2  vertex() const noexcept;

private:
    float m_a, m_b, m_c, m_tmin, m_tmax;
};

// ── Hyperbola ─────────────────────────────────────────────────────────────────
enum class HyperbolaAxis { Horizontal, Vertical };

class Hyperbola final : public IConic {
public:
    explicit Hyperbola(float a = 1.f, float b = 1.f,
                       float h = 0.f, float k = 0.f,
                       HyperbolaAxis axis       = HyperbolaAxis::Horizontal,
                       float         half_range = 2.f);

    [[nodiscard]] Vec3  evaluate(float t)          const override;
    [[nodiscard]] Vec3  derivative(float t)         const override;
    [[nodiscard]] Vec3  second_derivative(float t)  const override;
    [[nodiscard]] Vec3  third_derivative(float t)   const override;

    [[nodiscard]] float t_min() const override { return -m_half_range; }
    [[nodiscard]] float t_max() const override { return  m_half_range; }

    [[nodiscard]] float         a()    const noexcept { return m_a; }
    [[nodiscard]] float         b()    const noexcept { return m_b; }
    [[nodiscard]] float         h()    const noexcept { return m_h; }
    [[nodiscard]] float         k()    const noexcept { return m_k; }
    [[nodiscard]] HyperbolaAxis axis() const noexcept { return m_axis; }

    [[nodiscard]] u32 two_branch_vertex_count(u32 n) const noexcept {
        return 2u * (n + 1u) + 1u;
    }

    void tessellate_two_branch(std::span<Vertex> out, u32 n,
                               Vec4 color = { 1.f, 1.f, 1.f, 1.f }) const;

    [[nodiscard]] Vec3 eval_branch(float t, bool positive) const noexcept;

private:
    float         m_a, m_b, m_h, m_k;
    HyperbolaAxis m_axis;
    float         m_half_range;
};

// ── Helix ─────────────────────────────────────────────────────────────────────
// A circular helix in R^3, the canonical space curve:
//   p(t) = (r·cos(t),  r·sin(t),  b·t)
//
// where r is the radius and b = pitch / (2π) controls how fast it climbs.
//
// Frenet–Serret frame (analytic, exact):
//   T(t) = p'(t) / |p'(t)|
//   N(t) = principal normal = T'(t) / |T'(t)|   (points toward axis)
//   B(t) = T(t) × N(t)                           (binormal)
//
// For a circular helix these are all constant in magnitude:
//   κ = r / (r² + b²)       (curvature — constant)
//   τ = b / (r² + b²)       (torsion — constant)
//   |p'| = √(r² + b²)       (speed — constant)
//
// Formal predicate:
//   Helix(r, b, t_min, t_max) defines a regular C∞ curve in R^3
//   with constant nonzero curvature and constant torsion iff r > 0, b ≠ 0.
//   Locus theorem: A curve has constant κ > 0 and constant τ iff it is a helix.

class Helix final : public IConic {
public:
    explicit Helix(float r     = 1.f,
                   float pitch = 0.5f,
                   float tmin  = 0.f,
                   float tmax  = 4.f * ops::pi_v<float>);

    [[nodiscard]] Vec3  evaluate(float t)          const override;
    [[nodiscard]] Vec3  derivative(float t)         const override;
    [[nodiscard]] Vec3  second_derivative(float t)  const override;
    [[nodiscard]] Vec3  third_derivative(float t)   const override;
    [[nodiscard]] float curvature(float t)           const override;
    [[nodiscard]] float torsion(float t)             const override;

    [[nodiscard]] float t_min() const override { return m_tmin; }
    [[nodiscard]] float t_max() const override { return m_tmax; }

    [[nodiscard]] float radius() const noexcept { return m_r; }
    [[nodiscard]] float pitch()  const noexcept { return m_pitch; }
    [[nodiscard]] float b()      const noexcept { return m_b; }

private:
    float m_r, m_pitch, m_b, m_tmin, m_tmax;
};

// ── ParaboloidCurve ───────────────────────────────────────────────────────────
// A curve that lies on the paraboloid  z = a(x² + y²),  parameterised by
// radial distance t along a fixed azimuthal angle θ₀:
//
//   p(t) = (t·cos θ₀,  t·sin θ₀,  a·t²)     t ∈ [0, t_max]
//
// Geometric interpretation:
//   This is the intersection of the paraboloid with the vertical half-plane
//   { (x,y,z) : y/x = tan θ₀,  x ≥ 0 }.  The intersection is a parabola
//   lying exactly on the surface — a concrete example of a 1-manifold
//   embedded in a 2-manifold embedded in R³.
//
//   In a coordinate chart on the paraboloid this is a "meridional" curve —
//   the v = const curves of the UV parameterisation (u,v) → (u·cosv, u·sinv, au²).
//
// Analytic derivatives:
//   p'(t)   = (cos θ₀,  sin θ₀,  2at)
//   p''(t)  = (0,       0,        2a)
//   p'''(t) = (0,       0,        0)
//
// Analytic Frenet frame:
//   |p'| = √(1 + 4a²t²)                (speed — varies with t)
//   T = p' / |p'|
//   N = (p'' - (p''·T)T) / |...|       (points toward the paraboloid axis)
//   B = T × N
//
// Analytic curvature:
//   κ = |p' × p''| / |p'|³
//     = 2|a| / (1 + 4a²t²)^(3/2)
//
//   This is identical to κ₁(u=t) of the paraboloid — the principal curvature
//   in the meridional direction. This confirms the curve is a principal
//   curvature line of the surface.
//
// Torsion:
//   τ = 0  for all t  (the curve lies in a plane)
//   The plane is spanned by the direction (cosθ₀, sinθ₀, 0) and (0, 0, 1).
//
// Relationship to the surface normal:
//   The unit normal of the paraboloid at p(t) lies in the osculating plane
//   of this curve (since τ=0 and the curve is a geodesic of sorts).

class ParaboloidCurve final : public IConic {
public:
    /// a      = paraboloid curvature scale  (must match the Paraboloid you are rendering)
    /// theta  = azimuthal angle θ₀ (radians) — selects which meridian
    /// tmin   = minimum radial distance from the apex (usually 0)
    /// tmax   = maximum radial distance
    explicit ParaboloidCurve(float a     = 1.f,
                              float theta = 0.f,
                              float tmin  = 0.f,
                              float tmax  = 1.5f);

    [[nodiscard]] Vec3  evaluate(float t)          const override;
    [[nodiscard]] Vec3  derivative(float t)         const override;
    [[nodiscard]] Vec3  second_derivative(float t)  const override;
    [[nodiscard]] Vec3  third_derivative(float t)   const override;
    [[nodiscard]] float curvature(float t)           const override;
    [[nodiscard]] float torsion(float /*t*/)         const override { return 0.f; }

    [[nodiscard]] float t_min() const override { return m_tmin; }
    [[nodiscard]] float t_max() const override { return m_tmax; }

    [[nodiscard]] float a()     const noexcept { return m_a; }
    [[nodiscard]] float theta() const noexcept { return m_theta; }

private:
    float m_a, m_theta, m_ct, m_st;  // cosθ, sinθ precomputed
    float m_tmin, m_tmax;
};

} // namespace ndde::math
