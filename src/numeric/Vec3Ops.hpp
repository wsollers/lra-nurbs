#pragma once
// numeric/Vec3Ops.hpp
// Type-parameterised 3-vector operations on glm::vec3 / glm::dvec3.
//
// ── Geometric intuition ───────────────────────────────────────────────────────
// A vector in ℝ³ is an element of a 3-dimensional real inner-product space.
// The operations here correspond to the fundamental structure maps:
//
//   dot(u, v)   := ⟨u, v⟩  — the inner product, ℝ³ × ℝ³ → ℝ
//   length(v)   := ‖v‖     — the induced norm,   ‖v‖ = √⟨v,v⟩
//   cross(u, v) := u × v   — the exterior product, ℝ³ × ℝ³ → ℝ³
//                            (not a general inner-product-space concept —
//                             it is specific to ℝ³ and ℝ⁷)
//   normalize(v):= v/‖v‖   — the unit-sphere projection map
//
// ── Predicate logic contracts ─────────────────────────────────────────────────
//   ∀ u, v ∈ ℝ³:
//     (P1) |dot(u,v)| ≤ ‖u‖·‖v‖                      (Cauchy–Schwarz)
//     (P2) ‖cross(u,v)‖ = ‖u‖·‖v‖·|sin θ|           (area of parallelogram)
//     (P3) ‖normalize(v)‖ = 1  iff  v ≠ 0             (unit vector)
//     (P4) dot(normalize(v), normalize(v)) = 1         (idempotent norm)
//     (P5) cross(u, v) ⊥ u  and  cross(u, v) ⊥ v      (orthogonality)
//
// ── Why wrap GLM? ─────────────────────────────────────────────────────────────
// GLM is correct, battle-tested, and emits SIMD intrinsics under -O2.
// Wrapping it under ndde::numeric::vec3 achieves:
//   1. A testable seam: swap the GLM backend for a custom one in tests.
//   2. Type-parameterised dispatch: vec3<f32> uses glm::vec3,
//      vec3<f64> uses glm::dvec3 — same call site, different precision.
//   3. Safe guards: normalize_safe returns a fallback instead of NaN.
//   4. A curriculum interface: function signatures match the math notation.
//
// ── Template parameterisation ─────────────────────────────────────────────────
// The GlmVec3<T> alias maps the scalar type to the correct GLM vector type:
//   T = f32  →  glm::vec3   (GPU-native, SPIR-V compatible)
//   T = f64  →  glm::dvec3  (simulation precision)
//
// All functions are noexcept header-only inlines — GLM is header-only and
// these wrappers add exactly zero overhead after inlining at -O1 or higher.

#include "math/Scalars.hpp"
#include "numeric/MathTraits.hpp"
#include <glm/glm.hpp>

namespace ndde::numeric {

// ── GLM type selector ─────────────────────────────────────────────────────────
// GlmVec3<T> resolves to the correct GLM vector type for scalar T.
// The primary template is undefined; specialisations enforce the constraint.

template<typename T> struct GlmVec3Type;
template<> struct GlmVec3Type<f32> { using type = glm::vec3;  };
template<> struct GlmVec3Type<f64> { using type = glm::dvec3; };

template<typename T>
using GlmVec3 = typename GlmVec3Type<T>::type;

// ── Vec3Ops<T> ────────────────────────────────────────────────────────────────

template<typename T>
struct Vec3Ops {
    using V = GlmVec3<T>;

    // ── Inner product ─────────────────────────────────────────────────────────
    // dot(u, v) := u₁v₁ + u₂v₂ + u₃v₃
    // Satisfies (P1): |dot(u,v)| ≤ ‖u‖·‖v‖  (Cauchy–Schwarz inequality).
    [[nodiscard]] static T dot(const V& u, const V& v) noexcept {
        return glm::dot(u, v);
    }

    // ── Induced norm ──────────────────────────────────────────────────────────
    // length(v) := √(v₁² + v₂² + v₃²) = √⟨v,v⟩
    // The square-root function loses the Lipschitz property near 0:
    //   |‖u‖ − ‖v‖| ≤ ‖u − v‖  (reverse triangle inequality) — globally.
    //   But d/dx √x → ∞ as x → 0, so the derivative is unbounded.
    // Use near_zero() before dividing by length.
    [[nodiscard]] static T length(const V& v) noexcept {
        return glm::length(v);
    }

    // length_sq avoids the sqrt — useful when only comparisons are needed.
    [[nodiscard]] static T length_sq(const V& v) noexcept {
        return glm::dot(v, v);
    }

    // ── Exterior product ──────────────────────────────────────────────────────
    // cross(u, v) := (u₂v₃−u₃v₂, u₃v₁−u₁v₃, u₁v₂−u₂v₁)
    // Geometric meaning: the vector orthogonal to both u and v with
    //   ‖cross(u,v)‖ = ‖u‖·‖v‖·|sin θ|  (area of the parallelogram spanned).
    // Satisfies (P5): cross(u,v)·u = 0 and cross(u,v)·v = 0.
    [[nodiscard]] static V cross(const V& u, const V& v) noexcept {
        return glm::cross(u, v);
    }

    // ── Unit-sphere projection ────────────────────────────────────────────────
    // normalize(v) := v / ‖v‖
    // Undefined for v = 0 (division by zero → NaN in GLM).
    // PRECONDITION: ‖v‖ > 0.  Caller must check.
    [[nodiscard]] static V normalize(const V& v) noexcept {
        return glm::normalize(v);
    }

    // Safe variant: returns fallback if ‖v‖ < epsilon.
    // Formal guarantee:
    //   normalize_safe(v, fb) = v/‖v‖  if ‖v‖ ≥ ε
    //                         = fb     otherwise
    [[nodiscard]] static V normalize_safe(
        const V& v,
        const V& fallback = V{T{1}, T{0}, T{0}}) noexcept
    {
        const T len = glm::length(v);
        return MathTraits<T>::near_zero(len) ? fallback : v / len;
    }

    // ── Scalar projection ─────────────────────────────────────────────────────
    // scalar_proj(u, v) := ⟨u, v̂⟩ = dot(u, v) / ‖v‖
    // The component of u in the direction of v.
    [[nodiscard]] static T scalar_proj(const V& u, const V& v) noexcept {
        const T len = glm::length(v);
        return MathTraits<T>::safe_div(glm::dot(u, v), len);
    }

    // ── Vector projection ─────────────────────────────────────────────────────
    // proj(u, v) := (⟨u,v⟩/⟨v,v⟩) · v
    // The component of u parallel to v.
    [[nodiscard]] static V proj(const V& u, const V& v) noexcept {
        const T denom = glm::dot(v, v);
        if (MathTraits<T>::near_zero(denom)) return V{T{0}};
        return (glm::dot(u, v) / denom) * v;
    }

    // ── Gram–Schmidt rejection ────────────────────────────────────────────────
    // reject(u, v) := u − proj(u, v)
    // The component of u orthogonal to v.
    // Used in the Frenet–Serret normal: N = (p'' − (p''·T)T) / ‖…‖
    [[nodiscard]] static V reject(const V& u, const V& v) noexcept {
        return u - proj(u, v);
    }
};

// ── Free-function interface ───────────────────────────────────────────────────
// Write  numeric::dot<f64>(u, v)  instead of  Vec3Ops<f64>::dot(u, v).

template<typename T>
[[nodiscard]] inline T dot(const GlmVec3<T>& u, const GlmVec3<T>& v) noexcept {
    return Vec3Ops<T>::dot(u, v);
}
template<typename T>
[[nodiscard]] inline T length(const GlmVec3<T>& v) noexcept {
    return Vec3Ops<T>::length(v);
}
template<typename T>
[[nodiscard]] inline GlmVec3<T> cross(const GlmVec3<T>& u, const GlmVec3<T>& v) noexcept {
    return Vec3Ops<T>::cross(u, v);
}
template<typename T>
[[nodiscard]] inline GlmVec3<T> normalize(const GlmVec3<T>& v) noexcept {
    return Vec3Ops<T>::normalize(v);
}
template<typename T>
[[nodiscard]] inline GlmVec3<T> normalize_safe(
    const GlmVec3<T>& v,
    const GlmVec3<T>& fallback = GlmVec3<T>{T{1}, T{0}, T{0}}) noexcept
{
    return Vec3Ops<T>::normalize_safe(v, fallback);
}
template<typename T>
[[nodiscard]] inline GlmVec3<T> reject(const GlmVec3<T>& u, const GlmVec3<T>& v) noexcept {
    return Vec3Ops<T>::reject(u, v);
}

} // namespace ndde::numeric
