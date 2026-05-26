#pragma once
// numeric/Constants.hpp
// Compile-time mathematical constants at f32 and f64 precision.
//
// ── Design ────────────────────────────────────────────────────────────────────
// All constants are constexpr and templated on the scalar type T.
// The primary template is left undefined; explicit specialisations for f32
// and f64 provide the actual values. This forces a hard compile error on
// unsupported types rather than a silent truncation.
//
// ── Why not just use std::numbers? ───────────────────────────────────────────
// std::numbers (C++20) gives double-typed constants only. We want:
//   1. Type-parameterised constants so MathTraits<f32> and MathTraits<f64>
//      each get the correctly-rounded value for their own precision.
//   2. Domain extras (TWO_PI, SQRT_TWO_PI, PHI, epsilon) not in std::numbers.
//   3. Comments tying each constant to its role in stochastic analysis and
//      Lipschitz bound work — these are curriculum notes, not just numbers.
//
// ── Formal definitions ────────────────────────────────────────────────────────
//   ∀ T ∈ {f32, f64}:
//     π    := lim_{n→∞} n·sin(π/n)           (Archimedes polygon limit)
//     e    := lim_{n→∞} (1 + 1/n)^n           (Euler's number)
//     φ    := (1 + √5)/2                       (golden ratio)
//     √2   := sup { x ∈ ℚ : x² < 2 }          (Dedekind cut)
//     ln 2 := ∫₁² (1/t) dt                    (natural log of 2)
//
// ── Lipschitz relevance ───────────────────────────────────────────────────────
//   sin and cos have Lipschitz constant L=1 on ℝ (|sin x − sin y| ≤ |x−y|).
//   TWO_PI is therefore the worst-case phase accumulation period for
//   trigonometric drift terms in torus Brownian motion SDEs.
//   The Euler–Maruyama step bound h < 2L/σ² uses L=1, σ = diffusion coeff.

#include "math/Scalars.hpp"

namespace ndde::numeric {

// Primary template — undefined. Attempting Constants<int> is a compile error.
template<typename T> struct Constants;

// ── f32 specialisation ────────────────────────────────────────────────────────
template<>
struct Constants<f32> {
    static constexpr f32 PI          = 3.14159265f;
    static constexpr f32 TWO_PI      = 6.28318530f;
    static constexpr f32 HALF_PI     = 1.57079632f;
    static constexpr f32 INV_PI      = 0.31830988f;
    static constexpr f32 INV_TWO_PI  = 0.15915494f;

    static constexpr f32 E           = 2.71828182f;
    static constexpr f32 LOG2_E      = 1.44269504f;
    static constexpr f32 LN_2        = 0.69314718f;
    static constexpr f32 LN_10       = 2.30258509f;

    static constexpr f32 SQRT_2      = 1.41421356f;
    static constexpr f32 SQRT_3      = 1.73205080f;
    static constexpr f32 SQRT_HALF   = 0.70710678f;
    static constexpr f32 SQRT_TWO_PI = 2.50662827f;   ///< √(2π) — Gaussian normalisation

    static constexpr f32 PHI         = 1.61803398f;   ///< golden ratio (1+√5)/2

    // Numerical thresholds:
    //   EPSILON — smallest positive x s.t. 1 + x ≠ 1 in f32 arithmetic.
    //   Use for near-zero guards in: division, normalize(), curvature().
    static constexpr f32 EPSILON     = 1.0e-6f;
    static constexpr f32 SQRT_EPSILON= 1.0e-3f;       ///< √EPSILON — step-size bound
};

// ── f64 specialisation ────────────────────────────────────────────────────────
template<>
struct Constants<f64> {
    static constexpr f64 PI          = 3.14159265358979323846;
    static constexpr f64 TWO_PI      = 6.28318530717958647692;
    static constexpr f64 HALF_PI     = 1.57079632679489661923;
    static constexpr f64 INV_PI      = 0.31830988618379067154;
    static constexpr f64 INV_TWO_PI  = 0.15915494309189533577;

    static constexpr f64 E           = 2.71828182845904523536;
    static constexpr f64 LOG2_E      = 1.44269504088896340736;
    static constexpr f64 LN_2        = 0.69314718055994530942;
    static constexpr f64 LN_10       = 2.30258509299404568402;

    static constexpr f64 SQRT_2      = 1.41421356237309504880;
    static constexpr f64 SQRT_3      = 1.73205080756887729353;
    static constexpr f64 SQRT_HALF   = 0.70710678118654752440;
    static constexpr f64 SQRT_TWO_PI = 2.50662827463100050242;

    static constexpr f64 PHI         = 1.61803398874989484820;

    static constexpr f64 EPSILON     = 1.0e-12;
    static constexpr f64 SQRT_EPSILON= 1.0e-6;
};

// ── Convenience aliases ───────────────────────────────────────────────────────
// Write  numeric::pi<f64>  instead of  Constants<f64>::PI.

template<typename T> inline constexpr T pi          = Constants<T>::PI;
template<typename T> inline constexpr T two_pi      = Constants<T>::TWO_PI;
template<typename T> inline constexpr T half_pi     = Constants<T>::HALF_PI;
template<typename T> inline constexpr T e_const     = Constants<T>::E;
template<typename T> inline constexpr T ln2         = Constants<T>::LN_2;
template<typename T> inline constexpr T sqrt2       = Constants<T>::SQRT_2;
template<typename T> inline constexpr T sqrt_two_pi = Constants<T>::SQRT_TWO_PI;
template<typename T> inline constexpr T epsilon     = Constants<T>::EPSILON;

} // namespace ndde::numeric
