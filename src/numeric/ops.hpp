#pragma once
// numeric/ops.hpp
// Unified math operation dispatch — the single header that Conics, Surfaces,
// and Axes should include instead of <cmath> + <glm/glm.hpp> directly.
//
// ── Purpose ───────────────────────────────────────────────────────────────────
// This header provides a stable API surface:
//   ndde::ops::cos(x)         - scalar trig
//   ndde::ops::length(v)      - vector norm
//   ndde::ops::cross(u, v)    - exterior product
//   ... etc.
//
// Under the hood it dispatches to either:
//   ndde::numeric::MathTraits<T>  (NDDE_USE_BUILTIN_MATH = 0, default)
//   platform math libraries       (NDDE_USE_BUILTIN_MATH = 1, oracle)
//
// Because both paths ultimately call the same functions right now, the
// behavioural difference is zero. The value is the seam: when MathTraits
// gains a real cos() implementation, only this file changes, and every
// caller gets the new implementation automatically.
//
// ── Migration note ────────────────────────────────────────────────────────────
// The math/*.cpp files should call this facade directly. To migrate:
//   1. Replace  #include <cmath>   with  #include "numeric/ops.hpp"
//   2. Replace raw scalar calls    with  ops::cos(x)
//   3. Replace raw vector calls    with  ops::length(v)
//   ... and so on.
// The math/*.hpp files should not change — they use the types from Scalars.hpp.

#include "numeric/math_config.hpp"
#include "numeric/MathTraits.hpp"
#include "numeric/Vec3Ops.hpp"
#include <glm/glm.hpp>

namespace ndde::ops {

template<typename T> inline constexpr T pi_v     = numeric::Constants<T>::PI;
template<typename T> inline constexpr T two_pi_v = numeric::Constants<T>::TWO_PI;

// ── Scalar ops ────────────────────────────────────────────────────────────────
// All templated on T; specialise for f32 or f64.

template<typename T> [[nodiscard]] inline T cos(T x)  noexcept { return numeric::MathTraits<T>::cos(x);  }
template<typename T> [[nodiscard]] inline T sin(T x)  noexcept { return numeric::MathTraits<T>::sin(x);  }
template<typename T> [[nodiscard]] inline T tan(T x)  noexcept { return numeric::MathTraits<T>::tan(x);  }
template<typename T> [[nodiscard]] inline T acos(T x) noexcept { return numeric::MathTraits<T>::acos(x); }
template<typename T> [[nodiscard]] inline T asin(T x) noexcept { return numeric::MathTraits<T>::asin(x); }
template<typename T> [[nodiscard]] inline T atan(T x) noexcept { return numeric::MathTraits<T>::atan(x); }
template<typename T> [[nodiscard]] inline T atan2(T y, T x) noexcept { return numeric::MathTraits<T>::atan2(y, x); }
template<typename T> [[nodiscard]] inline T cosh(T x) noexcept { return numeric::MathTraits<T>::cosh(x); }
template<typename T> [[nodiscard]] inline T sinh(T x) noexcept { return numeric::MathTraits<T>::sinh(x); }
template<typename T> [[nodiscard]] inline T exp(T x)  noexcept { return numeric::MathTraits<T>::exp(x);  }
template<typename T> [[nodiscard]] inline T log(T x)  noexcept { return numeric::MathTraits<T>::log(x);  }
template<typename T> [[nodiscard]] inline T sqrt(T x) noexcept { return numeric::MathTraits<T>::sqrt(x); }
template<typename T> [[nodiscard]] inline T pow(T b, T e) noexcept { return numeric::MathTraits<T>::pow(b, e); }
template<typename T> [[nodiscard]] inline T pow_1_5(T x) noexcept { return numeric::MathTraits<T>::pow_1_5(x); }
template<typename T> [[nodiscard]] inline T abs(T x)  noexcept { return numeric::MathTraits<T>::abs(x);  }
template<typename T> [[nodiscard]] inline T floor(T x)noexcept { return numeric::MathTraits<T>::floor(x);}
template<typename T> [[nodiscard]] inline T ceil(T x) noexcept { return numeric::MathTraits<T>::ceil(x); }
template<typename T> [[nodiscard]] inline T fmod(T x, T y) noexcept { return numeric::MathTraits<T>::fmod(x, y); }
template<typename T> [[nodiscard]] inline T clamp(T x, T lo, T hi) noexcept {
    return numeric::MathTraits<T>::clamp(x, lo, hi);
}
template<typename T> [[nodiscard]] inline T safe_sqrt(T x) noexcept { return numeric::MathTraits<T>::safe_sqrt(x); }
template<typename T> [[nodiscard]] inline T safe_div(T n, T d, T fb = T{0}) noexcept {
    return numeric::MathTraits<T>::safe_div(n, d, fb);
}
template<typename T> [[nodiscard]] inline bool near_zero(T x) noexcept {
    return numeric::MathTraits<T>::near_zero(x);
}

// ── Vec3 ops ──────────────────────────────────────────────────────────────────

template<typename T>
[[nodiscard]] inline T dot(
    const numeric::GlmVec3<T>& u,
    const numeric::GlmVec3<T>& v) noexcept
{ return numeric::Vec3Ops<T>::dot(u, v); }

template<typename T>
[[nodiscard]] inline T length(const numeric::GlmVec3<T>& v) noexcept
{ return numeric::Vec3Ops<T>::length(v); }

template<glm::length_t L, typename T, glm::qualifier Q>
[[nodiscard]] inline T length(const glm::vec<L, T, Q>& v) noexcept
{ return glm::length(v); }

template<typename T>
[[nodiscard]] inline T length_sq(const numeric::GlmVec3<T>& v) noexcept
{ return numeric::Vec3Ops<T>::length_sq(v); }

template<typename T>
[[nodiscard]] inline numeric::GlmVec3<T> cross(
    const numeric::GlmVec3<T>& u,
    const numeric::GlmVec3<T>& v) noexcept
{ return numeric::Vec3Ops<T>::cross(u, v); }

template<typename T>
[[nodiscard]] inline numeric::GlmVec3<T> normalize(
    const numeric::GlmVec3<T>& v) noexcept
{ return numeric::Vec3Ops<T>::normalize(v); }

template<typename T>
[[nodiscard]] inline numeric::GlmVec3<T> normalize_safe(
    const numeric::GlmVec3<T>& v,
    const numeric::GlmVec3<T>& fallback = numeric::GlmVec3<T>{T{1},T{0},T{0}}) noexcept
{ return numeric::Vec3Ops<T>::normalize_safe(v, fallback); }

template<typename T>
[[nodiscard]] inline numeric::GlmVec3<T> reject(
    const numeric::GlmVec3<T>& u,
    const numeric::GlmVec3<T>& v) noexcept
{ return numeric::Vec3Ops<T>::reject(u, v); }

// ── f32 convenience overloads (drop-in for existing Conics/Surfaces code) ─────
// These let you write  ops::cos(some_float)  without the explicit <f32>.

[[nodiscard]] inline f32 cos(f32 x)  noexcept { return numeric::MathTraits<f32>::cos(x);  }
[[nodiscard]] inline f32 sin(f32 x)  noexcept { return numeric::MathTraits<f32>::sin(x);  }
[[nodiscard]] inline f32 cosh(f32 x) noexcept { return numeric::MathTraits<f32>::cosh(x); }
[[nodiscard]] inline f32 sinh(f32 x) noexcept { return numeric::MathTraits<f32>::sinh(x); }
[[nodiscard]] inline f32 sqrt(f32 x) noexcept { return numeric::MathTraits<f32>::sqrt(x); }
[[nodiscard]] inline f32 abs(f32 x)  noexcept { return numeric::MathTraits<f32>::abs(x);  }
[[nodiscard]] inline f32 floor(f32 x)noexcept { return numeric::MathTraits<f32>::floor(x);}
[[nodiscard]] inline f32 fmod(f32 x, f32 y)noexcept { return numeric::MathTraits<f32>::fmod(x,y); }
[[nodiscard]] inline f32 pow(f32 b, f32 e)noexcept { return numeric::MathTraits<f32>::pow(b,e); }
[[nodiscard]] inline f32 pow_1_5(f32 x)   noexcept { return numeric::MathTraits<f32>::pow_1_5(x);}

[[nodiscard]] inline f32 length(const Vec3& v)  noexcept { return numeric::Vec3Ops<f32>::length(v);    }
[[nodiscard]] inline Vec3 cross(const Vec3& u, const Vec3& v) noexcept { return numeric::Vec3Ops<f32>::cross(u,v); }
[[nodiscard]] inline f32 dot(const Vec3& u, const Vec3& v)    noexcept { return numeric::Vec3Ops<f32>::dot(u,v);   }
[[nodiscard]] inline Vec3 normalize(const Vec3& v)            noexcept { return numeric::Vec3Ops<f32>::normalize(v);}
[[nodiscard]] inline Vec3 reject(const Vec3& u, const Vec3& v)noexcept { return numeric::Vec3Ops<f32>::reject(u,v);}

} // namespace ndde::ops
