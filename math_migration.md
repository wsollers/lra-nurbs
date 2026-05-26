# Custom Math Function Migration Plan

> **Pedagogical note:** The standard library functions like `std::sin`, `std::cos`, `std::exp` are opaque black boxes — correct and fast, but not *yours*. Building your own versions (Taylor series, CORDIC, Padé approximants, Chebyshev polynomials) is one of the most structurally illuminating exercises in numerical analysis. This document gives you a disciplined migration path: flip a global flag, then replace functions *one at a time*, measuring error as you go.

---

## 1. The Architecture: Three Layers of Control

The migration uses **two levels of flags**:

| Flag | Type | Purpose |
|------|------|---------|
| `USE_CUSTOM_MATH` | Global `constexpr bool` | Master switch — enables the custom math subsystem at all |
| `use_my_sin`, `use_my_cos`, … | Per-function `constexpr bool` | Fine-grained — swap individual functions independently |

When `USE_CUSTOM_MATH == false`, **everything routes to `std::`**, and your custom code is entirely dead. When `USE_CUSTOM_MATH == true`, each per-function flag independently selects your implementation or the stdlib fallback.

This means:
- The project always compiles correctly, regardless of which functions you've implemented.
- You can enable/disable a single function for A/B comparison without touching the call sites.
- In release builds, set all flags to `true` only when you are satisfied with the accuracy.

---

## 2. File Layout

```
src/
  math/
    MathConfig.hpp      ← Global flag + per-function flags (edit this file to migrate)
    MathFunctions.hpp   ← The unified dispatch header — include this everywhere
    MathFunctions.cpp   ← Optional: non-inline implementations
    custom/
      MySin.hpp         ← Your sin approximation
      MyCos.hpp         ← Your cos approximation
      MyExp.hpp         ← etc.
      MySqrt.hpp
      MyLog.hpp
    tests/
      MathAccuracyTest.cpp  ← Error measurement vs std::
```

> **Rule:** Every file in the project that needs math **only** includes `MathFunctions.hpp`. It never includes `<cmath>` directly. This ensures all math flows through your dispatch layer.

---

## 3. `MathConfig.hpp` — The Control Panel

```cpp
#pragma once

// ============================================================
// MASTER SWITCH
// Set to true to enable the custom math subsystem.
// When false, all math routes to std:: — project is unaffected.
// ============================================================
inline constexpr bool USE_CUSTOM_MATH = false;

// ============================================================
// PER-FUNCTION FLAGS
// Only meaningful when USE_CUSTOM_MATH == true.
// Set each to true only when your implementation is ready.
// ============================================================
namespace math_flags {
    inline constexpr bool use_my_sin  = false;
    inline constexpr bool use_my_cos  = false;
    inline constexpr bool use_my_tan  = false;
    inline constexpr bool use_my_exp  = false;
    inline constexpr bool use_my_log  = false;
    inline constexpr bool use_my_sqrt = false;
    inline constexpr bool use_my_pow  = false;
    inline constexpr bool use_my_atan2 = false;
    // Add more as needed
} // namespace math_flags
```

**Migration workflow:** When you finish implementing `MySin` and want to test it:
1. Set `USE_CUSTOM_MATH = true`.
2. Set `math_flags::use_my_sin = true`.
3. Everything else remains on `std::`.
4. Run your accuracy tests.
5. If satisfied, leave it on and move to the next function.

---

## 4. `MathFunctions.hpp` — The Dispatch Header

```cpp
#pragma once

#include "MathConfig.hpp"
#include <cmath>  // always available as fallback

// ---- Forward declarations of your custom functions ----
// Only compiled/linked when USE_CUSTOM_MATH is true.
#if defined(USE_CUSTOM_MATH) && USE_CUSTOM_MATH
  #include "custom/MySin.hpp"
  #include "custom/MyCos.hpp"
  #include "custom/MyExp.hpp"
  #include "custom/MySqrt.hpp"
  #include "custom/MyLog.hpp"
  // ... add more as you build them
#endif

namespace mymath {

// ================================================================
// sin
// ================================================================
[[nodiscard]] inline float sin(float x) noexcept {
    if constexpr (USE_CUSTOM_MATH && math_flags::use_my_sin) {
        return custom::my_sin(x);   // your implementation
    } else {
        return std::sin(x);
    }
}

// ================================================================
// cos
// ================================================================
[[nodiscard]] inline float cos(float x) noexcept {
    if constexpr (USE_CUSTOM_MATH && math_flags::use_my_cos) {
        return custom::my_cos(x);
    } else {
        return std::cos(x);
    }
}

// ================================================================
// tan
// ================================================================
[[nodiscard]] inline float tan(float x) noexcept {
    if constexpr (USE_CUSTOM_MATH && math_flags::use_my_tan) {
        return custom::my_tan(x);
    } else {
        return std::tan(x);
    }
}

// ================================================================
// exp
// ================================================================
[[nodiscard]] inline float exp(float x) noexcept {
    if constexpr (USE_CUSTOM_MATH && math_flags::use_my_exp) {
        return custom::my_exp(x);
    } else {
        return std::exp(x);
    }
}

// ================================================================
// log (natural)
// ================================================================
[[nodiscard]] inline float log(float x) noexcept {
    if constexpr (USE_CUSTOM_MATH && math_flags::use_my_log) {
        return custom::my_log(x);
    } else {
        return std::log(x);
    }
}

// ================================================================
// sqrt
// ================================================================
[[nodiscard]] inline float sqrt(float x) noexcept {
    if constexpr (USE_CUSTOM_MATH && math_flags::use_my_sqrt) {
        return custom::my_sqrt(x);
    } else {
        return std::sqrt(x);
    }
}

// ================================================================
// atan2
// ================================================================
[[nodiscard]] inline float atan2(float y, float x) noexcept {
    if constexpr (USE_CUSTOM_MATH && math_flags::use_my_atan2) {
        return custom::my_atan2(y, x);
    } else {
        return std::atan2(y, x);
    }
}

} // namespace mymath
```

> **Key design:** `if constexpr` evaluates at **compile time**. When `USE_CUSTOM_MATH == false`, the compiler sees only `std::sin(x)` — there is zero overhead and no dead code in the binary. This is not a runtime branch.

---

## 5. A First Custom Implementation: `MySin.hpp`

The 5th-order Taylor series for $\sin(x)$ around $x = 0$:

$$\sin(x) \approx x - \frac{x^3}{3!} + \frac{x^5}{5!} = x - \frac{x^3}{6} + \frac{x^5}{120}$$

**Error bound:** $|R_7(x)| \leq \frac{|x|^7}{7!}$. For $x \in [-\pi, \pi]$, the maximum error is around $4.6 \times 10^{-4}$. That is fine for early testing; you will want range reduction and higher order for production.

```cpp
// custom/MySin.hpp
#pragma once
#include <cmath>  // for M_PI in range reduction

namespace custom {

// Range-reduce x into [-pi, pi] using the identity sin(x + 2*pi*k) = sin(x)
// Then apply a degree-7 minimax polynomial (better than Taylor for uniform error)
[[nodiscard]] inline float my_sin(float x) noexcept {
    // Range reduction: fold into [-pi, pi]
    // Using std::fmod here — replace with custom when you have it
    constexpr float TWO_PI = 6.28318530718f;
    constexpr float PI     = 3.14159265359f;

    x = std::fmod(x, TWO_PI);
    if (x >  PI) x -= TWO_PI;
    if (x < -PI) x += TWO_PI;

    // Degree-5 Taylor approximation
    // Horner's method: x*(1 + x^2*(-1/6 + x^2*(1/120)))
    float x2 = x * x;
    return x * (1.0f + x2 * (-0.16666667f + x2 * 0.00833333f));
}

} // namespace custom
```

**Planned upgrades (in order of complexity):**

1. Degree-5 Taylor (above) — start here.
2. Degree-9 Taylor — extend the Horner chain.
3. Chebyshev minimax polynomial — minimizes the *maximum* error over the interval, unlike Taylor which minimizes error near $x=0$.
4. CORDIC — iterative, hardware-friendly, no multiplications needed. Relevant for GPU compute shaders.

---

## 6. Migrating Call Sites

Search the project for every `std::sin`, `std::cos`, etc. and replace with `mymath::sin`, `mymath::cos`.

**Sed one-liner (run from project root):**
```bash
# Dry run first — see what will change
grep -rn "std::sin\|std::cos\|std::exp\|std::sqrt\|std::log" src/ --include="*.cpp" --include="*.hpp"

# Replace (after confirming the above looks right)
find src/ \( -name "*.cpp" -o -name "*.hpp" \) \
  -not -path "*/math/custom/*" \
  -not -path "*/math/MathFunctions.hpp" \
  -exec sed -i \
    -e 's/std::sin/mymath::sin/g' \
    -e 's/std::cos/mymath::cos/g' \
    -e 's/std::exp/mymath::exp/g' \
    -e 's/std::sqrt/mymath::sqrt/g' \
    -e 's/std::log/mymath::log/g' \
    -e 's/std::tan/mymath::tan/g' \
    -e 's/std::atan2/mymath::atan2/g' \
    {} +
```

> **Exclude** the `math/custom/` directory from replacement — those files call `std::` internally and that is intentional (they are the implementations, not the call sites).

---

## 7. Accuracy Testing

For each custom function, measure the maximum absolute error and maximum relative error against `std::`:

```cpp
// tests/MathAccuracyTest.cpp
#include "math/MathFunctions.hpp"
#include <cmath>
#include <iostream>
#include <algorithm>

void test_sin_accuracy(int N = 10000) {
    float max_abs_err = 0.0f;
    float max_rel_err = 0.0f;

    for (int i = 0; i < N; ++i) {
        float x = -3.14159f + 6.28318f * (float(i) / float(N));
        float ref  = std::sin(x);
        float mine = custom::my_sin(x);  // test the raw impl directly
        float abs_err = std::abs(ref - mine);
        float rel_err = (std::abs(ref) > 1e-10f) ? abs_err / std::abs(ref) : abs_err;
        max_abs_err = std::max(max_abs_err, abs_err);
        max_rel_err = std::max(max_rel_err, rel_err);
    }

    std::cout << "sin: max_abs_err = " << max_abs_err
              << ", max_rel_err = "    << max_rel_err << "\n";
}
```

**Target thresholds for the Brownian/DDE simulation:**

| Function | Acceptable max abs error | Notes |
|----------|--------------------------|-------|
| `sin`, `cos` | `< 1e-5` | Geometry and curvature calculations |
| `exp` | `< 1e-4` | Heat kernel, diffusion weights |
| `sqrt` | `< 1e-6` | Distance, norm computations |
| `log` | `< 1e-5` | Entropy, log-likelihood |

---

## 8. Migration Checklist

| Step | Task | Status |
|------|------|--------|
| 1 | Create `MathConfig.hpp` with `USE_CUSTOM_MATH = false` | ☐ |
| 2 | Create `MathFunctions.hpp` dispatch layer | ☐ |
| 3 | Replace all `std::` calls in project with `mymath::` | ☐ |
| 4 | Verify project compiles and runs identically (`USE_CUSTOM_MATH = false`) | ☐ |
| 5 | Implement `custom::my_sin` in `MySin.hpp` | ☐ |
| 6 | Set `use_my_sin = true`, run accuracy test | ☐ |
| 7 | If passing: leave on. Implement next function. | ☐ |
| 8 | Repeat for `cos`, `exp`, `sqrt`, `log`, `atan2` | ☐ |
| 9 | Final: set `USE_CUSTOM_MATH = true`, all flags true, run full simulation | ☐ |

---

## 9. Connection to the Broader Project

When you eventually move to GLSL/SPIR-V shaders for Vulkan, the GPU has its own built-in `sin`, `cos` etc. which are often lower precision than IEEE 754 double. Having implemented your own approximations in C++, you will understand *exactly* what polynomial approximation the GPU is using under the hood — and you can make an informed decision about whether to rely on the hardware intrinsics or implement your own in the compute shader using the same Chebyshev/Horner techniques.

The Lipschitz constants of your custom functions are also directly computable from the polynomial coefficients, which feeds back into your GPU optimization work: the Lipschitz constant of `my_sin` bounds the rate of change of surface positions per unit parameter change, informing your tessellation granularity.
