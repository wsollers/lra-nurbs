#pragma once
// numeric/MathTraits.hpp
// Type-parameterised scalar function dispatch.
//
// ── Geometric intuition ───────────────────────────────────────────────────────
// Every function in this header is a map  f : T → T  for T ∈ {f32, f64}.
// The trait struct is the C++ way of encoding what mathematicians call a
// "function algebra over a field" — a set of operations that are closed
// under the scalar type.
//
// For stochastic analysis: the Euler–Maruyama integrator for a torus SDE
// calls sin/cos at every step. If we can guarantee our sin/cos satisfy a
// Lipschitz condition  |f(x)−f(y)| ≤ L|x−y|  with an explicit constant L,
// then we can formally bound the strong convergence error of the integrator.
// This header is where those Lipschitz constants live, as constexpr values.
//
// ── Design ────────────────────────────────────────────────────────────────────
// MathTraits<T> is a pure static trait struct. It contains only:
//   - Inline static function implementations (or std:: delegates in Release)
//   - constexpr Lipschitz bounds for each function
//   - static_asserts documenting the guarantees
//
// Two backends:
//   NDDE_USE_BUILTIN_MATH = 0 (default): ndde implementations.
//   NDDE_USE_BUILTIN_MATH = 1         : std:: delegation (validation oracle).
//
// The implementations in MathTraits.cpp use the std:: backend via the
// NDDE_USE_BUILTIN_MATH=1 path.  When you write your own Taylor/CORDIC
// implementations, they go in MathTraits.cpp and flip to the default path.
//
// ── Predicate logic contract ──────────────────────────────────────────────────
//   ∀ x ∈ ℝ:
//     |sin(x)|  ≤ 1                              (boundedness)
//     |sin(x) − sin(y)| ≤ |x − y|               (Lipschitz, L_sin  = 1)
//     |cos(x) − cos(y)| ≤ |x − y|               (Lipschitz, L_cos  = 1)
//
//   ∀ x ≥ 0:
//     |sqrt(x) − sqrt(y)| ≤ (1/(2√ε))|x − y|   (Lipschitz locally away from 0)
//     sqrt is NOT globally Lipschitz — the constant blows up near 0.
//
//   ∀ x ∈ ℝ, p > 0:
//     pow(x, p) is Lipschitz on bounded domains with L = p·(x_max)^(p−1).
//     pow is NOT globally Lipschitz for p < 1.
//
// ── Lipschitz constants (constexpr) ──────────────────────────────────────────
// These are for the global domain  x ∈ ℝ  unless noted.
// For domain-restricted bounds (needed for Euler–Maruyama step sizing),
// compute them from the formula in the comment above.

#include "math/Scalars.hpp"
#include "numeric/Constants.hpp"
#include "numeric/math_config.hpp"
#include <cmath>

namespace ndde::numeric {

namespace detail {

template<typename T>
[[nodiscard]] inline T reduce_to_half_pi(T x) noexcept {
    const T pi = Constants<T>::PI;
    const T two_pi = Constants<T>::TWO_PI;
    x = std::fmod(x, two_pi);
    if (x > pi) x -= two_pi;
    if (x < -pi) x += two_pi;
    const T half_pi = pi / T{2};
    if (x > half_pi) return pi - x;
    if (x < -half_pi) return -pi - x;
    return x;
}

template<typename T>
[[nodiscard]] inline T taylor_sin_19(T x) noexcept {
    const T y = reduce_to_half_pi(x);
    const T y2 = y * y;
    T term = y;
    T sum = y;
    for (int n = 1; n <= 9; ++n) {
        const T a = static_cast<T>(2 * n);
        const T b = static_cast<T>(2 * n + 1);
        term *= -y2 / (a * b);
        sum += term;
    }
    return sum;
}

} // namespace detail

template<typename T>
struct MathTraits {
    // ── Lipschitz constants ───────────────────────────────────────────────────
    // These are GLOBAL bounds where they exist, domain bounds otherwise.
    // See header comments for the proofs.

    /// L_sin = 1: |sin x − sin y| ≤ |x − y|  ∀ x,y ∈ ℝ
    static constexpr T L_sin  = T{1};
    /// L_cos = 1: |cos x − cos y| ≤ |x − y|  ∀ x,y ∈ ℝ
    static constexpr T L_cos  = T{1};
    /// L_exp on [0, R]: L_exp = e^R.  Stored as unit bound; scale by domain.
    static constexpr T L_exp_unit = Constants<T>::E;

    // ── Trigonometric functions ───────────────────────────────────────────────

    [[nodiscard]] static T cos(T x) noexcept { return std::cos(x); }
    [[nodiscard]] static T sin(T x) noexcept {
        if constexpr (!ndde::use_builtin_math && ndde::use_taylor_sin)
            return taylor_sin(x);
        else
            return std::sin(x);
    }
    [[nodiscard]] static T taylor_sin(T x) noexcept { return detail::taylor_sin_19(x); }
    [[nodiscard]] static T tan(T x) noexcept { return std::tan(x); }
    [[nodiscard]] static T acos(T x) noexcept { return std::acos(x); }
    [[nodiscard]] static T asin(T x) noexcept { return std::asin(x); }
    [[nodiscard]] static T atan(T x) noexcept { return std::atan(x); }
    [[nodiscard]] static T atan2(T y, T x) noexcept { return std::atan2(y, x); }

    // ── Hyperbolic functions ──────────────────────────────────────────────────

    [[nodiscard]] static T cosh(T x) noexcept { return std::cosh(x); }
    [[nodiscard]] static T sinh(T x) noexcept { return std::sinh(x); }
    [[nodiscard]] static T tanh(T x) noexcept { return std::tanh(x); }

    // ── Exponential and logarithm ─────────────────────────────────────────────

    [[nodiscard]] static T exp(T x)  noexcept { return std::exp(x);  }
    [[nodiscard]] static T log(T x)  noexcept { return std::log(x);  }
    [[nodiscard]] static T log2(T x) noexcept { return std::log2(x); }

    // ── Power and root ────────────────────────────────────────────────────────

    [[nodiscard]] static T sqrt(T x) noexcept { return std::sqrt(x); }
    [[nodiscard]] static T cbrt(T x) noexcept { return std::cbrt(x); }
    [[nodiscard]] static T pow(T base, T exp) noexcept {
        return std::pow(base, exp);
    }
    // pow(x, 1.5) = x * sqrt(x), avoiding a general pow call.
    // Used in curvature denominators: (1 + 4a²u²)^(3/2).
    [[nodiscard]] static T pow_1_5(T x) noexcept {
        return x * std::sqrt(x);
    }
    // pow(x, 2) = x*x, zero overhead.
    [[nodiscard]] static T pow2(T x) noexcept { return x * x; }
    // pow(x, 3) = x*x*x.
    [[nodiscard]] static T pow3(T x) noexcept { return x * x * x; }

    // ── Rounding and remainder ────────────────────────────────────────────────

    [[nodiscard]] static T abs(T x)           noexcept { return std::abs(x);           }
    [[nodiscard]] static T floor(T x)         noexcept { return std::floor(x);         }
    [[nodiscard]] static T ceil(T x)          noexcept { return std::ceil(x);          }
    [[nodiscard]] static T round(T x)         noexcept { return std::round(x);         }
    [[nodiscard]] static T fmod(T x, T y)     noexcept { return std::fmod(x, y);       }
    [[nodiscard]] static T min(T a, T b)      noexcept { return a < b ? a : b;         }
    [[nodiscard]] static T max(T a, T b)      noexcept { return a > b ? a : b;         }
    [[nodiscard]] static T clamp(T x, T lo, T hi) noexcept {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    // ── Near-zero predicate ───────────────────────────────────────────────────
    // Returns true iff |x| < epsilon<T>.
    // Used to guard divisions and normalisation operations.
    //
    // Formal predicate:
    //   near_zero(x) := |x| < ε  where ε := Constants<T>::EPSILON
    [[nodiscard]] static bool near_zero(T x) noexcept {
        return std::abs(x) < Constants<T>::EPSILON;
    }

    // ── Safe sqrt ─────────────────────────────────────────────────────────────
    // sqrt is undefined for x < 0. In geometric code (lengths, determinants)
    // floating-point noise can produce tiny negatives. This clamps to 0 first.
    [[nodiscard]] static T safe_sqrt(T x) noexcept {
        return std::sqrt(x < T{0} ? T{0} : x);
    }

    // ── Safe division ─────────────────────────────────────────────────────────
    // Returns numerator/denominator, or fallback if |denom| < epsilon.
    // Avoids NaN/Inf in curvature and normalisation code.
    [[nodiscard]] static T safe_div(T num, T denom, T fallback = T{0}) noexcept {
        return near_zero(denom) ? fallback : num / denom;
    }
};

// Convenience aliases — same ergonomics as std:: but namespaced.
// Write  numeric::cos<f64>(x)  instead of  MathTraits<f64>::cos(x).

template<typename T> [[nodiscard]] inline T cos(T x)  noexcept { return MathTraits<T>::cos(x);  }
template<typename T> [[nodiscard]] inline T sin(T x)  noexcept { return MathTraits<T>::sin(x);  }
template<typename T> [[nodiscard]] inline T sqrt(T x) noexcept { return MathTraits<T>::sqrt(x); }
template<typename T> [[nodiscard]] inline T pow(T b, T e) noexcept { return MathTraits<T>::pow(b, e); }
template<typename T> [[nodiscard]] inline T abs(T x)  noexcept { return MathTraits<T>::abs(x);  }
template<typename T> [[nodiscard]] inline T floor(T x)noexcept { return MathTraits<T>::floor(x);}
template<typename T> [[nodiscard]] inline T fmod(T x, T y) noexcept { return MathTraits<T>::fmod(x, y); }
template<typename T> [[nodiscard]] inline T pow_1_5(T x) noexcept { return MathTraits<T>::pow_1_5(x); }
template<typename T> [[nodiscard]] inline bool near_zero(T x) noexcept { return MathTraits<T>::near_zero(x); }
template<typename T> [[nodiscard]] inline T safe_sqrt(T x) noexcept { return MathTraits<T>::safe_sqrt(x); }
template<typename T> [[nodiscard]] inline T safe_div(T n, T d, T fb = T{0}) noexcept {
    return MathTraits<T>::safe_div(n, d, fb);
}

} // namespace ndde::numeric
