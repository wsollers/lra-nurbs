#pragma once
// numeric/Differentiator.hpp
// Type-parameterised finite-difference differentiation engine.
//
// ── Geometric intuition ───────────────────────────────────────────────────────
// The derivative of a curve p : ℝ → ℝ³ at t measures the instantaneous
// velocity vector — the direction and speed of travel along the curve.
// Finite differences approximate this by sampling p at two nearby points
// and measuring the chord between them.
//
// Visually: if you zoom in on any smooth curve until it looks like a straight
// line, the slope of that line IS the derivative. Finite differences automate
// this zoom-and-measure process numerically.
//
// ── Three standard schemes ────────────────────────────────────────────────────
//
//   Forward difference:
//     f'(t) ≈ [f(t+h) − f(t)] / h
//     Error: O(h)  — first-order accurate. Fast but one-sided.
//
//   Backward difference:
//     f'(t) ≈ [f(t) − f(t−h)] / h
//     Error: O(h)  — first-order accurate.
//
//   Central difference:
//     f'(t) ≈ [f(t+h) − f(t−h)] / (2h)
//     Error: O(h²) — second-order accurate. Preferred for smooth curves.
//     Derivation via Taylor expansion:
//       f(t+h) = f(t) + h·f'(t) + h²/2·f''(t) + O(h³)
//       f(t−h) = f(t) − h·f'(t) + h²/2·f''(t) + O(h³)
//       ∴ f(t+h)−f(t−h) = 2h·f'(t) + O(h³)
//       ∴ [f(t+h)−f(t−h)]/(2h) = f'(t) + O(h²)
//     The h² error terms cancel — that's why central differences are better.
//
//   Second derivative (central):
//     f''(t) ≈ [f(t+h) − 2f(t) + f(t−h)] / h²
//     Error: O(h²)
//     Derivation: add the two Taylor expansions above:
//       f(t+h)+f(t−h) = 2f(t) + h²·f''(t) + O(h⁴)
//       ∴ f''(t) ≈ [f(t+h) − 2f(t) + f(t−h)] / h²
//
// ── Step size h — the fundamental tradeoff ────────────────────────────────────
// Truncation error decreases as h → 0: O(h²) for central differences.
// Roundoff error (IEEE-754 catastrophic cancellation) INCREASES as h → 0.
// The optimal h balances these:
//   For central FD:  h_opt ≈ ε^(1/3) where ε = machine epsilon.
//   f32: ε ≈ 1.2e-7  →  h_opt ≈ 5e-3
//   f64: ε ≈ 2.2e-16 →  h_opt ≈ 6e-6
//
// Differentiator uses a relative step:  h = span * h_rel
// where span = t_max − t_min normalises h to the curve's parameter domain.
// Default h_rel values are tuned to the above optima.
//
// ── Predicate logic ───────────────────────────────────────────────────────────
//   ∀ smooth f : ℝ → ℝ³, ∀ t ∈ [t_min, t_max]:
//     |D1_central(f, t, h) − f'(t)| ≤ C₁·h²
//     |D2_central(f, t, h) − f''(t)| ≤ C₂·h²
//   where C₁, C₂ depend on the fourth derivative of f (bounded for C⁴ curves).

#include "math/Scalars.hpp"
#include "numeric/MathTraits.hpp"
#include <functional>

namespace ndde::numeric {

// ── Differentiator<T, V> ──────────────────────────────────────────────────────
// T = scalar type (f32 or f64)
// V = vector type (glm::vec3 for f32, glm::dvec3 for f64, or plain T)

template<typename T, typename V>
struct Differentiator {
    using Func = std::function<V(T)>;

    // Default relative step sizes, chosen to balance truncation and roundoff.
    static constexpr T k_h1_rel = (sizeof(T) == 4) ? T{5e-3f} : T{6e-6};  ///< for 1st deriv
    static constexpr T k_h2_rel = (sizeof(T) == 4) ? T{1e-2f} : T{1e-4};  ///< for 2nd deriv
    static constexpr T k_h3_rel = (sizeof(T) == 4) ? T{2e-2f} : T{6e-5};  ///< for 3rd deriv

    // ── First derivative — five-point central stencil, O(h⁴) ────────────────
    // f'(t) ≈ [-f(t+2h) + 8f(t+h) − 8f(t−h) + f(t−2h)] / (12h)
    // t is clamped to [t_min, t_max] at both probe points.
    [[nodiscard]] static V d1(
        const Func& f,
        T t, T t_min, T t_max,
        T h_rel = k_h1_rel) noexcept
    {
        const T span = t_max - t_min;
        const T h    = span * h_rel;
        const T tm2  = t - T{2} * h;
        const T tm1  = t - h;
        const T tp1  = t + h;
        const T tp2  = t + T{2} * h;
        if (tm2 >= t_min && tp2 <= t_max) {
            return (-f(tp2) + T{8} * f(tp1) - T{8} * f(tm1) + f(tm2)) /
                   (T{12} * h);
        }
        const T tl   = MathTraits<T>::max(t - h, t_min);
        const T tr   = MathTraits<T>::min(t + h, t_max);
        return (f(tr) - f(tl)) / static_cast<T>(tr - tl);
    }

    // ── Second derivative — five-point central stencil, O(h⁴) ───────────────
    // f''(t) ≈ [-f(t+2h) + 16f(t+h) − 30f(t) + 16f(t−h) − f(t−2h)] / (12h²)
    [[nodiscard]] static V d2(
        const Func& f,
        T t, T t_min, T t_max,
        T h_rel = k_h2_rel) noexcept
    {
        const T span = t_max - t_min;
        const T h    = span * h_rel;
        const T tm2  = t - T{2} * h;
        const T tm1  = t - h;
        const T tp1  = t + h;
        const T tp2  = t + T{2} * h;
        if (tm2 >= t_min && tp2 <= t_max) {
            return (-f(tp2) + T{16} * f(tp1) - T{30} * f(t) +
                    T{16} * f(tm1) - f(tm2)) / (T{12} * h * h);
        }
        const T tl   = MathTraits<T>::max(t - h, t_min);
        const T tr   = MathTraits<T>::min(t + h, t_max);
        // Use h as denominator even when clamped — consistent with IConic base
        return (f(tr) - f(t) * T{2} + f(tl)) / (h * h);
    }

    // ── Third derivative — five-point central stencil, O(h²) ────────────────
    // f'''(t) ≈ [f(t+2h) − 2f(t+h) + 2f(t−h) − f(t−2h)] / (2h³)
    // This avoids differentiating a finite-difference second derivative, which
    // amplifies roundoff enough to bias torsion estimates.
    [[nodiscard]] static V d3(
        const Func& f,
        T t, T t_min, T t_max,
        T h_rel = k_h3_rel) noexcept
    {
        const T span = t_max - t_min;
        const T h    = span * h_rel;
        const T tm2  = t - T{2} * h;
        const T tm1  = t - h;
        const T tp1  = t + h;
        const T tp2  = t + T{2} * h;
        if (tm2 < t_min || tp2 > t_max) {
            const T tl = MathTraits<T>::max(t - h, t_min);
            const T tr = MathTraits<T>::min(t + h, t_max);
            const auto d2_at = [&](T s){ return d2(f, s, t_min, t_max, h_rel); };
            return (d2_at(tr) - d2_at(tl)) / static_cast<T>(tr - tl);
        }
        return (f(tp2) - T{2} * f(tp1) + T{2} * f(tm1) - f(tm2)) /
               (T{2} * h * h * h);
    }

    // ── Curvature — scalar, from first and second derivatives ─────────────────
    // κ(t) = ‖f'(t) × f''(t)‖ / ‖f'(t)‖³
    // For planar curves (f' × f'' = (0,0,z)):  κ = |z| / ‖f'‖³
    // Returns 0 if ‖f'‖ < epsilon (degenerate / cusps).
    [[nodiscard]] static T curvature(
        const V& d1v, const V& d2v) noexcept
    {
        // This overload works for V = glm::vec3 / glm::dvec3.
        // For plain T, use the scalar overload below.
        const T len  = glm::length(d1v);
        if (MathTraits<T>::near_zero(len)) return T{0};
        return glm::length(glm::cross(d1v, d2v)) / (len * len * len);
    }

    // ── Torsion — scalar, from first three derivatives ────────────────────────
    // τ(t) = (f'(t) × f''(t)) · f'''(t)  /  ‖f'(t) × f''(t)‖²
    // Returns 0 if ‖f' × f''‖ < epsilon (zero curvature → torsion undefined).
    [[nodiscard]] static T torsion(
        const V& d1v, const V& d2v, const V& d3v) noexcept
    {
        const V cross   = glm::cross(d1v, d2v);
        const T denom   = glm::dot(cross, cross);
        if (MathTraits<T>::near_zero(denom)) return T{0};
        return glm::dot(cross, d3v) / denom;
    }
};

// ── Convenience aliases for the common cases ──────────────────────────────────
using Diff3f  = Differentiator<f32, glm::vec3>;   ///< f32 curve in ℝ³
using Diff3d  = Differentiator<f64, glm::dvec3>;  ///< f64 curve in ℝ³
using Diff1f  = Differentiator<f32, f32>;          ///< f32 scalar function
using Diff1d  = Differentiator<f64, f64>;          ///< f64 scalar function

} // namespace ndde::numeric
