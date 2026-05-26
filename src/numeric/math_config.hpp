#pragma once
// numeric/math_config.hpp
// Compile-time backend selection for ndde_numeric.
//
// ── How to switch backends ────────────────────────────────────────────────────
// In CMake (preferred):
//   option(NDDE_USE_BUILTIN_MATH "Use std/GLM backend" OFF)
//   # OFF = ndde::numeric implementations  (default, educational)
//   # ON  = std:: / GLM delegation         (validation oracle)
//
// The CMakeLists.txt translates this into:
//   target_compile_definitions(ndde_numeric PUBLIC NDDE_USE_BUILTIN_MATH=1)
// when ON.
//
// ── Why constexpr bool, not #ifdef? ──────────────────────────────────────────
// A preprocessor #ifdef evaporates before template instantiation. It cannot:
//   - Be used in if constexpr branches inside template bodies
//   - Be tested in static_assert
//   - Be inspected at runtime for diagnostics
//
// A constexpr bool is a first-class C++ value. It can be used in:
//   - if constexpr (ndde::use_builtin_math) { … }
//   - static_assert(!ndde::use_builtin_math, "expected ndde backend");
//   - Runtime logging: fmt::print("math backend: {}", ndde::use_builtin_math);
//
// We still read a preprocessor macro here (the only place), and immediately
// convert it to a constexpr bool. Everywhere else in the codebase, use the
// constexpr bool — never the macro directly.
//
// ── Approximation switches ───────────────────────────────────────────────────
// NDDE_USE_TAYLOR_SIN=1 routes MathTraits<T>::sin through the in-house Taylor
// implementation when NDDE_USE_BUILTIN_MATH is OFF. The approximation remains
// callable directly either way as MathTraits<T>::taylor_sin(x).

namespace ndde {

/// True  → std:: / GLM oracle backend (for validation).
/// False → ndde::numeric in-house backend (default, curriculum target).
inline constexpr bool use_builtin_math =
#if defined(NDDE_USE_BUILTIN_MATH) && NDDE_USE_BUILTIN_MATH
    true;
#else
    false;
#endif

/// True → route numeric sine through the in-house Taylor approximation.
inline constexpr bool use_taylor_sin =
#if defined(NDDE_USE_TAYLOR_SIN) && NDDE_USE_TAYLOR_SIN
    true;
#else
    false;
#endif

} // namespace ndde
