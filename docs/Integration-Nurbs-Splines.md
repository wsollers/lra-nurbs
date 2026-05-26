# NURBS_DDE Mathematical Observatory — Architecture v3

**Document status:** v3 — corrected, completed, and extended  
**Primary build target:** Taylor Approximation Lab + 2D Integration Lab  
**Long-term direction:** Five-pillar personal mathematical observatory  
**Implementation language:** C++20/23  
**Engineering posture:** Compile-time extensibility, SOLID, C++ Core Guidelines,
high test coverage, minimal unnecessary copying, strong type safety, explicit
numerical diagnostics  

---

## Table of Contents

1. Executive Summary  
2. Project Framing  
3. Five-Pillar Mathematical Architecture  
4. Engineering Philosophy  
5. Primitive Type Aliases  
6. Math Function Aliases  
7. Capability System  
8. Theorem Connection Metadata  
9. Compile-Time Zoo Pattern  
10. Strong Type System  
11. Memory, Ownership, and Copying Policy  
12. Error Handling  
13. Diagnostic Flags  
14. Result Trace and Numerical Audit Trail  
15. Testing Requirements  
16. UI Architecture — General  
17. UI Architecture — Taylor Approximation Lab  
18. UI Architecture — 2D Integration Lab  
19. UI Architecture — Hover and Interaction Specification  
20. Phase 1 Implementation Focus  
21. Phase 2 and Beyond  
22. Avoiding Architectural Dead Ends  
23. Integration Lab — Restored Full Detail  
24. Spline/NURBS Lab — Restored Full Detail  
25. Module Layout  
26. Final Acceptance Criteria  

---

# 1. Executive Summary

`nurbs_dde` pivots from a collection of numerical visualization tools into a
**personal mathematical observatory**: a long-lived software instrument for exploring
rigorous analysis, approximation, geometry, manifolds, dynamics, control, stochastic
processes, Fourier and wavelet analysis, and PDEs.

The immediate build target is intentionally narrow:

```
Phase 1 focus:
  Taylor Approximation Laboratory
  2D Integration Laboratory
```

These two areas align with current study goals and provide the best initial vertical
slice of the architecture. Taylor approximation forces careful handling of functions,
derivatives, polynomial approximants, error bounds, convergence, and root-finding.
2D integration forces careful handling of domains, partitions, sampling, numerical
approximation, error estimation, and visualization. Both domains prepare the
architecture for splines, NURBS, manifolds, DDE pursuit, stochastic simulation,
Fourier analysis, and PDEs later.

The guiding rule:

> Build the first labs narrowly, but design their interfaces so they can grow into
> the five-pillar observatory without rewriting.

This document is high-level design and feature guidance. It defines shared
direction, vocabulary, acceptance criteria, and architectural constraints for
the observatory. Individual components, services, workbenches, and numerical
subsystems should receive focused design documents before larger implementation
work begins.

Every lab feature must produce four outputs:

```text
1. mathematical result data
2. diagnostic data
3. renderable sample data
4. metadata explaining method, domain, assumptions, units, and error
```

This applies to integration methods, derivative probes, Taylor approximations,
spline evaluation, NURBS basis analysis, frame transport, motion laws, surface
area computation, and future geometric workbenches. The purpose is to keep each
feature inspectable as data, explainable as mathematics, and renderable as a
diagnostic view.

Use separate design documents for concrete systems such as:

```text
Integration1D Lab
Darboux / Riemann Formal Lab
Partition Service
Result Trace Service
Taylor Approximation Lab
Parametric Curve Kernel
Bezier / Spline Curve Lab
NURBS Curve Lab
Frame Transport Service
Arc-Length Parameterization Service
Curve Motion Lab
Surface / NURBS Surface Lab
```

---

# 2. Project Framing: A Mathematical Observatory

`nurbs_dde` is a **personal mathematical observatory**. That means:

```
It is built for long-term learning.
It grows with the curriculum.
It privileges explainability over black-box computation.
It treats numerical results as inspectable mathematical artifacts.
It links computations to theorems and future concepts.
It favors curated compile-time examples over a fragile runtime formula editor.
It is designed to be inhabited for decades as mathematical fluency grows.
```

The observatory contains multiple laboratories. Initial labs:

```
Taylor Approximation Lab
2D Integration Lab
```

Near-future labs:

```
Darboux/Riemann Integration Lab
Root-Finding Lab
Sequences and Series of Functions Lab
Spline/NURBS Curve Lab
Frenet/Bishop Frame Motion Lab
NURBS Surface Lab
```

Long-term labs:

```
Topology and Manifold Lab
ODE Systems Lab
Control and DDE Lab
Stochastic Analysis Lab
Fourier Analysis Lab
Wavelet Analysis Lab
PDE Lab
```

---

# 3. Five-Pillar Mathematical Architecture

## 3.1 Pillar 1 — Analysis and Approximation

> What does it mean to get close to something?

```
real numbers, sequences, series, limits, continuity,
differentiation, Taylor approximation, root-finding,
Riemann/Darboux integration, 2D integration,
uniform convergence, sequences and series of functions,
approximation theory
```

Initial implementation lives here.

## 3.2 Pillar 2 — Geometry of Curves and Surfaces

> What does it mean to have a shape?

```
parametric curves, Bezier curves, B-splines, NURBS curves,
NURBS surfaces, curvature, torsion, Frenet frames,
Bishop frames, arc length, surface area, sweep geometry,
motion along curves
```

## 3.3 Pillar 3 — Topology and Manifolds

> What does it mean for two spaces to be the same, and what structure is preserved?

```
metric spaces, topological spaces, continuity via preimages,
homeomorphisms, charts and atlases, smooth manifolds,
Riemannian manifolds, geodesics, exp/log maps,
parallel transport, curvature, flat quotient torus,
round sphere, hyperbolic plane
```

## 3.4 Pillar 4 — Dynamics, Control, and Stochastics

> What does it mean for a system to evolve, be controlled, or be random?

```
ODE systems, phase portraits, stability, Lyapunov functions,
control systems, PID controllers, DDE systems, method of steps,
pursuit dynamics, Brownian motion, SDEs, geodesic random walks
```

## 3.5 Pillar 5 — Harmonic Analysis and PDEs

> What does it mean to decompose a signal or field into natural modes?

```
Fourier series, Fourier transforms, Fourier analysis on the flat torus,
wavelets, multiresolution analysis, heat equation, wave equation,
Laplace equation, spectral methods, PDEs on manifolds,
Laplace-Beltrami operator
```

---

# 4. Engineering Philosophy

## 4.1 Modern C++20/23

```
concepts
std::expected
std::optional
std::variant
std::span
std::string_view
std::unique_ptr
std::shared_ptr only when shared ownership is semantically real
std::ranges where useful
constexpr / consteval where appropriate
strongly typed wrappers via StrongScalar CRTP
RAII, move semantics
value semantics for small immutable objects
views / references / spans for large data
```

## 4.2 C++ Core Guidelines

```
Prefer RAII over manual resource management.
Prefer explicit ownership. Avoid raw owning pointers.
Avoid unnecessary heap allocation and unnecessary copying.
Prefer const correctness. Prefer narrow interfaces.
Prefer strong types over primitive obsession.
Prefer std::expected for recoverable errors.
Prefer std::optional for absent values.
Prefer std::variant for closed alternatives.
Avoid global mutable state. Avoid hidden control flow.
Use assertions for internal invariants.
Use tests for externally visible behavior.
```

## 4.3 SOLID Principles

### Single Responsibility

```
PartitionService          creates/refines partitions
IntegrationService        computes integral estimates
TaylorApproximationService builds Taylor approximants
ErrorEstimationService    computes error estimates
VisualizationService      transforms results into renderable data
ResultTraceService        records numerical audit trails
```

### Open/Closed

New functions, domains, approximation methods, zoo entries, and visualizers must be
addable without rewriting core services.

### Liskov Substitution

All implementations of shared interfaces must honor interface contracts. A
`ParametricCurve` that claims `Differentiate` capability must return a meaningful
`std::expected` on every call, never `NaN`.

### Interface Segregation

Use capability flags and concepts. A mathematical object must not be forced to
implement curvature if it only supports evaluation.

### Dependency Inversion

High-level labs depend on abstractions, not concrete implementations.

```
TaylorApproximationWorkspace depends on DifferentiableFunctionLike,
not on SineFunction or PolynomialFunction.
```

## 4.4 Virtual Dispatch vs Concept Dispatch

**Concepts** are used for compile-time dispatch in templated services and zoo entries.

**Virtual interfaces** are used at the type-erased UI/registry boundary where the
concrete type is not known at compile time.

**The pattern is a concept-constrained type-erased wrapper.** The virtual interface
is the implementation detail of the type erasure; the numerical core never sees it.

```cpp
// Numerical core — zero virtual overhead:
template<DifferentiableFunction F>
TaylorApproximationResult compute_taylor(const F& f, nd::real a, nd::i32 n);

// UI and zoo registry use the type-erased wrapper:
class AnyDifferentiableFunction {
public:
    template<DifferentiableFunction F>
    explicit AnyDifferentiableFunction(F f);   // type-erases F

    std::expected<nd::real, FunctionError> evaluate(nd::real x) const;
    std::expected<nd::real, FunctionError> derivative(nd::real x, nd::i32 order) const;
    ZooEntryMetadata     metadata()     const;
    CapabilitySet        capabilities() const;
    Domain1D             domain()       const;

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual std::expected<nd::real, FunctionError> evaluate(nd::real) const = 0;
        virtual std::expected<nd::real, FunctionError>
            derivative(nd::real, nd::i32) const = 0;
        virtual ZooEntryMetadata  metadata()     const = 0;
        virtual CapabilitySet     capabilities() const = 0;
        virtual Domain1D          domain()       const = 0;
    };
    template<DifferentiableFunction F>
    struct Model final : Concept { /* delegates to F */ };
    std::unique_ptr<Concept> impl_;
};
```

**Hot-loop rule:** Virtual dispatch is acceptable at service-dispatch boundaries.
It is never acceptable inside inner summation loops. `ApproximationMethod::compute`
receives the whole `Partition` and loops internally using concrete types; the virtual
call happens once, not per cell.

---

# 5. Primitive Type Aliases

**No raw primitive types (`double`, `float`, `int`, `unsigned`, etc.) shall appear
anywhere in nurbs_dde library code unless required by an external API such as ImGui
or Vulkan.** At those call sites, cast explicitly and add a comment explaining the
reason for the cast.

All numeric types must use the nurbs_dde aliases defined in `core/primitives.hpp`.
These are currently forwarded to the standard implementations but live in a single
header so that alternative backends (higher-precision arithmetic, interval arithmetic,
testing stubs) can be substituted at any time without touching call sites.

```cpp
// nurbs_dde/core/primitives.hpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace nd {

// Signed integers
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// Unsigned integers
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Size and index
using usize = std::size_t;
using isize = std::ptrdiff_t;

// Floating point
using f32 = float;
using f64 = double;
using f80 = long double;   // 80-bit on x86; platform-dependent elsewhere

// Default real type used throughout the observatory.
// Change this one alias to switch the entire numerical backend.
using real = f64;

// Bit-field types for flags
using u32flags = std::uint32_t;
using u64flags = std::uint64_t;

} // namespace nd
```

Usage rules:

```
Use nd::real for all mathematical computation unless specific precision is required.
Use nd::f64 when you need to be explicit that double precision is intended.
Use nd::i32 / nd::u32 for loop indices and counts in numerical code.
Use nd::usize for container sizes and indices.
Never write 'double', 'float', 'int', 'unsigned int', etc. in library code.
At ImGui/Vulkan call sites: static_cast<float>(nd_value)  // ImGui requires float
```

---

# 6. Math Function Aliases

All mathematical functions must use the `nd::math::` namespace, never the standard
library or `<cmath>` directly. This allows backend substitution without touching
call sites.

```cpp
// nurbs_dde/core/math_fns.hpp
#pragma once
#include "primitives.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace nd::math {

// Exponential and logarithm
inline nd::real exp  (nd::real x)           noexcept { return std::exp(x);   }
inline nd::real log  (nd::real x)           noexcept { return std::log(x);   }
inline nd::real log2 (nd::real x)           noexcept { return std::log2(x);  }
inline nd::real log10(nd::real x)           noexcept { return std::log10(x); }

// Power
inline nd::real pow (nd::real x, nd::real y) noexcept { return std::pow(x,y); }
inline nd::real sqrt(nd::real x)             noexcept { return std::sqrt(x);  }
inline nd::real cbrt(nd::real x)             noexcept { return std::cbrt(x);  }

// Trigonometric
inline nd::real sin (nd::real x) noexcept { return std::sin(x);  }
inline nd::real cos (nd::real x) noexcept { return std::cos(x);  }
inline nd::real tan (nd::real x) noexcept { return std::tan(x);  }
inline nd::real asin(nd::real x) noexcept { return std::asin(x); }
inline nd::real acos(nd::real x) noexcept { return std::acos(x); }
inline nd::real atan(nd::real x) noexcept { return std::atan(x); }
inline nd::real atan2(nd::real y, nd::real x) noexcept { return std::atan2(y,x); }

// Hyperbolic
inline nd::real sinh(nd::real x) noexcept { return std::sinh(x); }
inline nd::real cosh(nd::real x) noexcept { return std::cosh(x); }
inline nd::real tanh(nd::real x) noexcept { return std::tanh(x); }

// Rounding and absolute value
inline nd::real abs  (nd::real x)             noexcept { return std::abs(x);       }
inline nd::real floor(nd::real x)             noexcept { return std::floor(x);     }
inline nd::real ceil (nd::real x)             noexcept { return std::ceil(x);      }
inline nd::real round(nd::real x)             noexcept { return std::round(x);     }
inline nd::real fmod (nd::real x, nd::real y) noexcept { return std::fmod(x,y);   }

// Min / max / clamp
inline nd::real min  (nd::real a, nd::real b)                    noexcept { return std::min(a,b);        }
inline nd::real max  (nd::real a, nd::real b)                    noexcept { return std::max(a,b);        }
inline nd::real clamp(nd::real v, nd::real lo, nd::real hi)      noexcept { return std::clamp(v,lo,hi);  }

// Special constants and values
inline constexpr nd::real pi()      noexcept { return nd::real{3.141592653589793238462643383}; }
inline constexpr nd::real e_const() noexcept { return nd::real{2.718281828459045235360287471}; }
inline constexpr nd::real inf()     noexcept { return std::numeric_limits<nd::real>::infinity();   }
inline constexpr nd::real nan()     noexcept { return std::numeric_limits<nd::real>::quiet_NaN();  }
inline constexpr nd::real epsilon() noexcept { return std::numeric_limits<nd::real>::epsilon();    }

// Classification
inline bool is_nan   (nd::real x) noexcept { return std::isnan(x);    }
inline bool is_inf   (nd::real x) noexcept { return std::isinf(x);    }
inline bool is_finite(nd::real x) noexcept { return std::isfinite(x); }

} // namespace nd::math
```

**Rule:** Never call `std::sin`, `std::exp`, `std::abs`, etc. directly in library
code. Always call `nd::math::sin`, `nd::math::exp`, `nd::math::abs`. Violation of
this rule breaks backend substitution.

---

# 7. Capability System

## 7.1 Capability Enum

```cpp
enum class Capability : nd::usize {
    Evaluate = 0,
    Differentiate,
    Integrate,
    HasClosedFormIntegral,
    HasKnownFourierSeries,
    HasKnownSingularities,
    HasCurvature,
    HasTorsion,
    HasFrame,
    HasMetric,
    HasExpMap,
    HasLogMap,
    HasLaplacian,
    SupportsSampling,
    SupportsAdaptiveRefinement,
    Count_    // sentinel — never use as a capability value
};
```

## 7.2 CapabilitySet

```cpp
class CapabilitySet {
public:
    constexpr CapabilitySet() noexcept = default;

    // Initializer-list constructor for concise zoo declarations:
    //   constexpr CapabilitySet caps{ Capability::Evaluate, Capability::Differentiate };
    constexpr CapabilitySet(std::initializer_list<Capability> caps) noexcept {
        for (auto c : caps) add(c);
    }

    constexpr bool has(Capability c) const noexcept {
        return bits_.test(static_cast<nd::usize>(c));
    }
    constexpr void add(Capability c) noexcept {
        bits_.set(static_cast<nd::usize>(c));
    }
    constexpr void remove(Capability c) noexcept {
        bits_.reset(static_cast<nd::usize>(c));
    }
    constexpr bool operator==(const CapabilitySet&) const noexcept = default;

private:
    std::bitset<static_cast<nd::usize>(Capability::Count_)> bits_;
};
```

## 7.3 Capability Profiles

**Smooth zoo function (sin, exp, cos):**

```
Evaluate, Differentiate, Integrate, HasClosedFormIntegral, SupportsSampling
```

**Absolute value / cusp:**

```
Evaluate, Integrate, HasClosedFormIntegral, HasKnownSingularities, SupportsSampling
Diagnostic note: not differentiable at x = 0
```

**NURBS Curve:**

```
Evaluate, Differentiate, HasCurvature, HasTorsion, HasFrame,
SupportsSampling, SupportsAdaptiveRefinement
```

**Flat torus manifold:**

```
HasMetric, HasExpMap, HasLogMap, HasLaplacian, SupportsSampling
```

**Fourier zoo entry:**

```
Evaluate, Integrate, HasKnownFourierSeries, SupportsSampling
```

---

# 8. Theorem Connection Metadata

## 8.1 Type Definition

All `std::string_view` members must point to data with static storage duration
(string literals). `TheoremConnection` objects must never point into temporaries.

```cpp
struct TheoremConnection {
    std::string_view theorem_name;
    std::string_view formal_statement;       // LaTeX; empty string if omitted
    std::string_view informal_statement;
    std::string_view used_by_feature;
    std::span<const std::string_view> future_connections;  // static lifetime
};
```

## 8.2 Standard Theorem Entries (static constexpr examples)

```cpp
inline constexpr std::string_view taylor_futures[] = {
    "numerical ODE truncation error",
    "PDE discretization error",
    "uniform convergence theory",
    "analytic function theory",
};
inline constexpr TheoremConnection kTaylorsTheoremWithRemainder {
    .theorem_name       = "Taylor's Theorem with Remainder",
    .formal_statement   = "f(x) = \\sum_{k=0}^{n}\\frac{f^{(k)}(a)}{k!}(x-a)^k + R_n(x)",
    .informal_statement = "A smooth function equals a polynomial plus a controllable remainder.",
    .used_by_feature    = "Taylor Approximation Lab",
    .future_connections = taylor_futures,
};

inline constexpr std::string_view banach_futures[] = {
    "Picard iteration for ODEs",
    "DDE stability analysis",
    "numerical solver convergence proofs",
};
inline constexpr TheoremConnection kBanachFixedPoint {
    .theorem_name       = "Banach Fixed-Point Theorem",
    .formal_statement   = "T\\text{ contraction on complete }(X,d)\\Rightarrow \\exists!x^*:T(x^*)=x^*",
    .informal_statement = "A contraction on a complete metric space has a unique fixed point.",
    .used_by_feature    = "Root-Finding Lab — fixed-point iteration",
    .future_connections = banach_futures,
};
```

---

# 9. Compile-Time Zoo Pattern

## 9.1 Rationale

A compile-time zoo entry represents mathematical understanding deep enough to
implement: evaluation, derivatives, known singularities, closed-form integrals, known
Taylor and Fourier series, expected convergence behavior, diagnostic notes, and theorem
connections. Runtime formula editors may be added later; they are not the foundation.

## 9.2 Base Metadata

```cpp
struct ZooEntryMetadata {
    std::string_view name;
    std::string_view pillar;
    std::string_view mathematical_context;
    std::string_view prerequisites;
    std::string_view illustrates;
    std::span<const TheoremConnection> theorem_connections;
    CapabilitySet    capabilities;
};
```

## 9.3 Concepts

```cpp
template<class T>
concept EvaluatableFunction =
    requires(const T& f, nd::real x) {
        { f.evaluate(x) }
            -> std::convertible_to<std::expected<nd::real, FunctionError>>;
        { f.domain()    }
            -> std::convertible_to<Domain1D>;
    };

template<class T>
concept DifferentiableFunction =
    EvaluatableFunction<T> &&
    requires(const T& f, nd::real x, nd::i32 order) {
        { f.derivative(x, order) }
            -> std::convertible_to<std::expected<nd::real, FunctionError>>;
    };

template<class T>
concept IntegrableFunction =
    EvaluatableFunction<T> &&
    requires(const T& f, nd::real a, nd::real b) {
        { f.closed_form_integral(a, b) }
            -> std::convertible_to<std::expected<nd::real, FunctionError>>;
    };

template<class T>
concept ZooFunction =
    EvaluatableFunction<T> &&
    requires(const T& f) {
        { f.metadata()     } -> std::convertible_to<ZooEntryMetadata>;
        { f.capabilities() } -> std::convertible_to<CapabilitySet>;
    };
```

## 9.4 Initial Zoo Entries

**Taylor Lab:**

```
PolynomialFunction     smooth; all capabilities; exact derivatives
SineFunction           smooth; HasKnownFourierSeries
CosineFunction         smooth; HasKnownFourierSeries
ExponentialFunction    smooth; radius of convergence = inf
LogOnePlusXFunction    smooth on (-1,1]; radius of convergence = 1
AbsoluteValueFunction  HasKnownSingularities; not differentiable at 0
RungeFunction          1/(1+25x^2); Runge phenomenon demo
GaussianFunction       smooth; compactly supported in practice
SineOneOverX           HasKnownSingularities; oscillatory near 0
StepFunction           jump discontinuity; piecewise constant
```

**2D Integration Lab:**

```
Constant2D             exact integral = area of domain
LinearX2D              exact integral = 0.5 on unit square
ProductXY2D            exact integral = 0.25 on unit square
IndicatorDisk2D        exact integral = pi on unit disk
GaussianBump2D         smooth; known integral
OscillatorySurface2D   high frequency; adaptive stress test
```

---

# 10. Strong Type System

## 10.1 CRTP StrongScalar

```cpp
template<typename Derived, typename T>
struct StrongScalar {
    T value{};
    constexpr explicit StrongScalar(T v) noexcept : value(v) {}
    constexpr StrongScalar() noexcept = default;

    constexpr Derived operator+(Derived rhs) const noexcept { return Derived{value + rhs.value}; }
    constexpr Derived operator-(Derived rhs) const noexcept { return Derived{value - rhs.value}; }
    constexpr Derived operator*(T s)         const noexcept { return Derived{value * s};         }
    constexpr Derived operator/(T s)         const noexcept { return Derived{value / s};         }
    constexpr Derived operator-()            const noexcept { return Derived{-value};             }
    constexpr bool operator< (Derived r) const noexcept { return value <  r.value; }
    constexpr bool operator<=(Derived r) const noexcept { return value <= r.value; }
    constexpr bool operator> (Derived r) const noexcept { return value >  r.value; }
    constexpr bool operator>=(Derived r) const noexcept { return value >= r.value; }
    constexpr bool operator==(Derived r) const noexcept { return value == r.value; }

    friend constexpr Derived operator*(T s, Derived d) noexcept { return Derived{s * d.value}; }

    // Intentionally omitted: Derived * Derived (dimensional error)
};
```

## 10.2 Scalar Strong Types

```cpp
struct Parameter1D : StrongScalar<Parameter1D, nd::real> { using StrongScalar::StrongScalar; };
struct ArcLength   : StrongScalar<ArcLength,   nd::real> { using StrongScalar::StrongScalar; };
struct Time        : StrongScalar<Time,        nd::real> { using StrongScalar::StrongScalar; };
struct Curvature   : StrongScalar<Curvature,   nd::real> { using StrongScalar::StrongScalar; };
struct Torsion     : StrongScalar<Torsion,     nd::real> { using StrongScalar::StrongScalar; };
```

`ArcLength + Time` does not compile. `ArcLength * ArcLength` does not compile.
To take a ratio of two arc lengths, extract `.value` and document the intent.

## 10.3 Point and Vector Types

```cpp
struct EuclideanVector2 {
    nd::real x{}, y{};
    constexpr EuclideanVector2 operator+(EuclideanVector2 r) const noexcept { return {x+r.x, y+r.y}; }
    constexpr EuclideanVector2 operator-(EuclideanVector2 r) const noexcept { return {x-r.x, y-r.y}; }
    constexpr EuclideanVector2 operator*(nd::real s)         const noexcept { return {x*s,   y*s};   }
    constexpr EuclideanVector2 operator-()                   const noexcept { return {-x,    -y};    }
    friend constexpr EuclideanVector2 operator*(nd::real s, EuclideanVector2 v) noexcept {
        return {s*v.x, s*v.y};
    }
};

constexpr nd::real dot(EuclideanVector2 a, EuclideanVector2 b) noexcept {
    return a.x*b.x + a.y*b.y;
}
inline nd::real norm(EuclideanVector2 v) noexcept {
    return nd::math::sqrt(dot(v, v));
}
inline EuclideanVector2 normalize(EuclideanVector2 v) noexcept {
    return v * (nd::real{1} / norm(v));
}

struct EuclideanPoint2 {
    nd::real x{}, y{};
    // Point - Point -> Vector
    constexpr EuclideanVector2 operator-(EuclideanPoint2 r) const noexcept { return {x-r.x, y-r.y}; }
    // Point + Vector -> Point
    constexpr EuclideanPoint2  operator+(EuclideanVector2 v) const noexcept { return {x+v.x, y+v.y}; }
    constexpr EuclideanPoint2  operator-(EuclideanVector2 v) const noexcept { return {x-v.x, y-v.y}; }
    // Point + Point is a category error — intentionally deleted
    EuclideanPoint2 operator+(EuclideanPoint2) const = delete;
};

// 3D analogues — same pattern
struct EuclideanVector3 {
    nd::real x{}, y{}, z{};
    constexpr EuclideanVector3 operator+(EuclideanVector3 r) const noexcept { return {x+r.x, y+r.y, z+r.z}; }
    constexpr EuclideanVector3 operator-(EuclideanVector3 r) const noexcept { return {x-r.x, y-r.y, z-r.z}; }
    constexpr EuclideanVector3 operator*(nd::real s)         const noexcept { return {x*s,   y*s,   z*s};   }
    constexpr EuclideanVector3 operator-()                   const noexcept { return {-x,    -y,    -z};    }
    friend constexpr EuclideanVector3 operator*(nd::real s, EuclideanVector3 v) noexcept {
        return {s*v.x, s*v.y, s*v.z};
    }
};

constexpr nd::real dot(EuclideanVector3 a, EuclideanVector3 b) noexcept {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
inline nd::real norm(EuclideanVector3 v) noexcept {
    return nd::math::sqrt(dot(v, v));
}
constexpr EuclideanVector3 cross(EuclideanVector3 a, EuclideanVector3 b) noexcept {
    return { a.y*b.z - a.z*b.y,
             a.z*b.x - a.x*b.z,
             a.x*b.y - a.y*b.x };
}
inline EuclideanVector3 normalize(EuclideanVector3 v) noexcept {
    return v * (nd::real{1} / norm(v));
}

struct EuclideanPoint3 {
    nd::real x{}, y{}, z{};
    constexpr EuclideanVector3 operator-(EuclideanPoint3 r)  const noexcept { return {x-r.x, y-r.y, z-r.z}; }
    constexpr EuclideanPoint3  operator+(EuclideanVector3 v) const noexcept { return {x+v.x, y+v.y, z+v.z}; }
    constexpr EuclideanPoint3  operator-(EuclideanVector3 v) const noexcept { return {x-v.x, y-v.y, z-v.z}; }
    EuclideanPoint3 operator+(EuclideanPoint3) const = delete;
};
```

## 10.4 Parameter and Screen Types

```cpp
struct Parameter2D     { nd::real u{}, v{}; };
struct ScreenPoint     { nd::real x{}, y{}; };
struct TextureCoordinate { nd::real u{}, v{}; };
```

## 10.5 Manifold-Specific Types (Future Phases)

```cpp
// Defined in manifolds/ when the manifold lab is implemented.
// Forward-declared here to prevent raw EuclideanPoint2 from being used in their place.
struct TorusPoint;    // (u,v) in [0,1)^2 — NOT a EuclideanPoint2
struct SpherePoint;   // (theta, phi) or unit 3-vector
struct TangentVector; // lives in T_pM at a specific base point
struct ManifoldPoint; // type-erased manifold point
```

`TorusPoint` must not be implicitly convertible to `EuclideanPoint2`. Wrap-around
arithmetic is the responsibility of `FlatTorusManifold`, not of raw coordinate
subtraction.

## 10.6 Category Error Examples

```cpp
// These must not compile:
auto bad1 = prey_torus_point - predator_torus_point;  // TorusPoint, not EuclideanPoint2
auto bad2 = arc_length + time_val;                    // different strong scalar types
nd::real bad3 = parameter_u;                          // no implicit conversion

// Correct forms:
auto direction = flat_torus.log(predator, prey);       // uses the manifold metric
auto distance  = flat_torus.distance(predator, prey);  // uses the manifold metric
nd::real raw   = parameter_u.value;                    // explicit extraction with intent
```

---

# 11. Memory, Ownership, and Copying Policy

## 11.1 Copy Minimization Rules

```
Pass large objects by const reference.
Return small value types by value.
Use std::span for non-owning contiguous views.
Use std::string_view for non-owning string data.
Use move semantics for ownership transfer.
Use immutable snapshots for renderer handoff.
Avoid copying large ResultTrace objects — move or pass by const reference.
Avoid polymorphic heap allocation inside inner summation loops.
```

## 11.2 Ownership Rules

```
unique_ptr    exclusive ownership of polymorphic objects
shared_ptr    only when shared ownership is semantically real
weak_ptr      break ownership cycles
spans/views   non-owning; never stored beyond the lifetime of the referent
value types   preferred for small mathematical objects
```

## 11.3 Handle Types for Problem Structs

`IntegrationProblem` and similar structs use non-owning handle types:

```cpp
using DomainHandle       = const Domain*;
using IntegrandHandle    = const Integrand*;
using MeasureElementHandle = const MeasureElement*;
```

The domain and integrand must outlive the problem. If problems are stored across
frames or serialized, promote to `std::shared_ptr<const Domain>` and document.

## 11.4 Hot Loop Rules

Inner summation loops must avoid:

```
virtual dispatch (use concept-templated code or local downcast inside compute())
heap allocation
string construction
exceptions for normal numerical failures
unnecessary copying of large vectors
```

---

# 12. Error Handling

## 12.1 Error Enumerations

```cpp
enum class FunctionError : nd::u8 {
    OutsideDomain,
    NotDifferentiable,
    SingularityEncountered,
    DerivativeOrderUnsupported,
    NumericalFailure,
    ClosedFormUnavailable,
};

enum class DomainError : nd::u8 {
    InvalidBounds,
    EmptyDomain,
    UnsupportedDimensionality,
};

enum class EvaluationError : nd::u8 {
    OutsideDomain,
    NumericalFailure,
    SingularityEncountered,
};

enum class IntegrationError : nd::u8 {
    PartitionInvalid,
    ConvergenceFailed,
    ToleranceNotMet,
    IntegrandUnbounded,
    DomainError,
};

enum class CurveError : nd::u8 {
    OutsideDomain,
    DerivativeUnavailable,
    RegularityFailure,
    NumericalFailure,
};

enum class SurfaceError : nd::u8 {
    OutsideDomain,
    DerivativeUnavailable,
    DegenerateJacobian,
    NumericalFailure,
};

enum class ManifoldError : nd::u8 {
    OutsideDomain,
    ExpMapFailed,
    LogMapFailed,
    GeodesicFailed,
    NumericalFailure,
};

enum class DDEError : nd::u8 {
    HistoryInsufficient,
    InterpolationFailed,
    StepSizeTooSmall,
    NumericalFailure,
};
```

## 12.2 Usage Patterns

```
std::expected<T, Error>   for recoverable failures — the default
std::optional<T>          when absence is normal and not an error
std::variant<A, B, C>     for closed alternative types
```

Never use exceptions for routine numerical failures.
Never return sentinel `NaN` without a corresponding error path.

---

# 13. Diagnostic Flags

`DiagnosticFlags` is used in `Cell`, `CurveSample`, `MotionState`, `ResultTrace`,
`TraceStep`, and most result types. It is defined before all of those.

```cpp
enum class DiagnosticFlag : nd::u32flags {
    None                = 0,
    NearSingularity     = 1u << 0,
    HighOscillation     = 1u << 1,
    ConvergenceSlow     = 1u << 2,
    ConvergenceFailed   = 1u << 3,
    NumericallyUnstable = 1u << 4,
    FrameUndefined      = 1u << 5,
    CurvatureNearZero   = 1u << 6,
    DomainBoundary      = 1u << 7,
    UserAttention       = 1u << 8,
    NotDifferentiable   = 1u << 9,
    RadiusExceeded      = 1u << 10,  // outside Taylor radius of convergence
    GibbsPhenomenon     = 1u << 11,  // Fourier overshoot at discontinuity
    FrameFlip           = 1u << 12,  // Frenet frame sign reversal
    WrapAroundActive    = 1u << 13,  // torus geodesic crossed boundary
};

struct DiagnosticFlags {
    nd::u32flags bits{0};

    constexpr void set  (DiagnosticFlag f) noexcept { bits |=  static_cast<nd::u32flags>(f); }
    constexpr void clear(DiagnosticFlag f) noexcept { bits &= ~static_cast<nd::u32flags>(f); }
    constexpr bool has  (DiagnosticFlag f) const noexcept {
        return (bits & static_cast<nd::u32flags>(f)) != 0;
    }
    constexpr bool any()  const noexcept { return bits != 0; }
    constexpr void reset()      noexcept { bits = 0; }

    constexpr DiagnosticFlags operator|(DiagnosticFlags r) const noexcept {
        return DiagnosticFlags{bits | r.bits};
    }
};
```

---

# 14. Result Trace and Numerical Audit Trail

## 14.1 TraceId

```cpp
struct TraceId {
    nd::u64 value{};
    constexpr bool operator==(const TraceId&) const noexcept = default;
};
```

## 14.2 Metadata

```cpp
struct Metadata {
    std::string  operation_name;
    std::string  method_used;
    nd::real     tolerance{};
    nd::usize    evaluation_count{};
    std::optional<nd::real>               error_estimate;
    std::chrono::steady_clock::time_point timestamp;
    std::thread::id                        thread_id;
    std::span<const TheoremConnection>     theorem_connections;
};
```

## 14.3 TraceStep

```cpp
struct TraceStep {
    nd::usize            step_index{};
    std::string          label;
    nd::real             estimate{};
    std::optional<nd::real> error_estimate;
    std::optional<nd::real> reference_value;
    std::optional<nd::real> previous_estimate;  // enables convergence rate computation
    DiagnosticFlags      flags;
    Metadata             metadata;
};
```

The convergence order at step n is computed from the trace as:

    p_n ≈ log|e_{n+1}| / log|e_n|

where e_n = |estimate - reference_value| when reference is available, or
|estimate_{n+1} - estimate_n| otherwise.

## 14.4 ResultTrace

```cpp
struct ResultTrace {
    TraceId               id;
    std::string           operation_name;
    std::vector<TraceStep> steps;
    Metadata              metadata;
    DiagnosticFlags       flags;
};
```

---

# 15. Testing Requirements

## 15.1 Test Categories

```
unit tests           one service, one function, one mathematical fact
property tests       invariants holding for all inputs of a class
golden numerical     specific inputs -> specific outputs within tolerance
regression tests     results that must not change between builds
edge-case tests      singularities, domain boundaries, zero curvature
diagnostic tests     DiagnosticFlags are set correctly for known bad inputs
UI-independent       all numerical services tested without any UI dependency
performance sanity   no obvious algorithmic regressions
```

## 15.2 Taylor Lab Unit Tests

```
Taylor polynomial for exp at 0 has coefficients [1, 1, 1/2, 1/6, 1/24, ...].
Taylor polynomial for sin at 0 has only odd-power terms.
Taylor polynomial for cos at 0 has only even-power terms.
Taylor error for exp on [-1,1] decreases monotonically as degree increases.
AbsoluteValue at 0 returns FunctionError::NotDifferentiable and sets flag.
Log(1+x) Taylor series has radius_of_convergence = 1.
Runge function outside [-1,1] sets DiagnosticFlag::RadiusExceeded.
Remainder bound >= actual error for all zoo functions where bound is available.
```

## 15.3 Root-Finding Unit Tests

```
Newton sqrt(2): ratio of successive |errors| converges to ~2 (quadratic).
Bisection: [a_n, b_n] always contains the true root.
Secant: converges without derivative for C1 functions.
Fixed-point cos(x): contraction constant |g'| < 1 detected and reported.
Newton on function with no root in domain returns expected failure.
```

## 15.4 2D Integration Unit Tests

```
Integral of constant 1 over unit square = 1.0 (exact, within nd::math::epsilon()).
Integral of x over unit square = 0.5 (exact).
Integral of x*y over unit square = 0.25 (exact).
Integral of 1 over unit disk approximates pi within 1e-3 at 100x100 grid.
Integral of GaussianBump2D approximates known value within tolerance.
Adaptive refinement reduces estimated error by >= 50% per step for smooth functions.
Cell areas sum to domain area within nd::math::epsilon() * cell_count.
No cell has negative measure.
Invalid domain returns DomainError.
```

## 15.5 Property Tests

```
Refining a partition must not increase Darboux U-L gap for bounded functions.
Integral(f+g) ≈ Integral(f) + Integral(g)  [linearity]
Integral(c*f) ≈ c * Integral(f)            [homogeneity]
For non-negative f on positive-measure domain, estimate >= 0.
```

## 15.6 Tolerance Labeling

Each test must state its tolerance type:

```
exact           integer or rational arithmetic — exact equality
floating-point  tolerance = k * nd::math::epsilon() for small k
approximate     explicit tolerance stated (e.g. 1e-6)
statistical     Monte Carlo — tolerance + sample count stated
adaptive        requested tolerance — test checks it is achieved
```

---

# 16. UI Architecture — General

## 16.1 Two-Pane Model

Every lab uses the same layout:

```
+----------------------------------------------+--------------------------+
|                                              |                          |
|  LEFT PANE — Geometric / Visual World        |  RIGHT PANE             |
|                                              |  Analytical Debugger     |
|  The thing you see.                          |                          |
|  Interactive. Draggable. Animated.           |  Tables. Plots.          |
|                                              |  Theorem connections.    |
|                                              |  Result trace.           |
+----------------------------------------------+--------------------------+
|  STATUS BAR — active flags | cursor coords | method | tolerance        |
+---------------------------------------------------------------------------+
```

The renderer and numerical services are completely separate. Numerical services
produce pure data structures. The UI consumes them. No ImGui, Vulkan, or Qt code
appears in any math service file.

## 16.2 Connections Panel

Every lab includes a Connections Panel in the right pane, always visible:

```
What is this?    One-sentence informal statement of the current concept.
Used by:         Other labs or computations that depend on what is shown here.
Later:           What this concept prepares for in future pillars.
```

## 16.3 Universal Hover Behavior

These hover behaviors apply in all labs:

- **Any numerical value** → full-precision tooltip: formula, active DiagnosticFlags,
  theorem connection if any.
- **Any flag icon** → flag name, definition, and remediation hint.
- **Any theorem name** → formal LaTeX statement (rendered), informal statement,
  list of future connections.
- **Any plotted curve** → function name, current (x, y), derivative if available,
  active DiagnosticFlags.
- **Any error bar or band** → error formula, whether this is a bound or estimate,
  the theorem that justifies the bound.

## 16.4 Global Controls (All Labs)

```
Method selector        dropdown / radio buttons
Tolerance input        text field with validation; accepts scientific notation
Zoo selector           function / domain / object picker
Precision display      toggle summary / full precision
Result trace viewer    collapsible panel; full audit trail
Theorem browser        searchable list of TheoremConnections in this lab
Export button          saves ResultTrace to JSON
Reset button           all sliders / selections to defaults
```

---

# 17. UI Architecture — Taylor Approximation Lab

## 17.1 Left Pane — Graph Canvas

**Always visible:**

```
f(x)           — solid curve, primary color, full domain
T_n(x)         — dashed curve, secondary color
|f-T_n| band   — shaded region, tertiary color, clamped to visible range
Expansion line — vertical dashed line labeled "a = [value]"
Radius markers — vertical lines at a ± R with shaded divergence zones (if R finite)
```

**For n = 1:** draw tangent line explicitly as a geometric object with label.
**For n = 2:** draw osculating parabola explicitly with label.

**Interactive elements:**

- **Drag expansion point `a`** — click and drag the vertical dashed line. All
  quantities update live: polynomial recomputes, error band redraws, right pane
  updates.
- **Drag evaluation cursor** — a vertical hairline draggable across the domain.
  Tooltip (see Section 19.3) appears while dragging. Releases pin a marker.
- **Click a point on f(x)** — pins a marker; right pane shows full breakdown.
- **Degree slider** — along the bottom of the left pane, controls `n`. As it
  slides the dashed Taylor polynomial animates toward f(x) (or diverges outside
  the radius). A small animation eases between degrees.
- **Scroll to zoom** — rescales both axes.

## 17.2 Right Pane — Taylor Analytical Panel

**Section A — Function Info**

```
Name:            sin(x)
Domain:          (-inf, +inf)
Expansion point: a = 0.00000
Degree:          n = 5
Capabilities:    Evaluate  Differentiate  Integrate  SupportsSampling
Flags:           [none]
```

**Section B — Polynomial Display**

Explicit formula for the current n and a. For sin at a=0, n=5:

```
T_5(x) = x  -  x^3/6  +  x^5/120
```

Coefficient table:

```
k    f^(k)(a)      k!       c_k = f^(k)(a)/k!
0    0.000000      1        0.000000
1    1.000000      1        1.000000
2    0.000000      2        0.000000
3   -1.000000      6       -0.166667
4    0.000000      24       0.000000
5    1.000000      120      0.008333
```

**Section C — Error Analysis (at cursor position)**

```
f(x)               = 1.000000000000000
T_n(x)             = 0.999942
Actual error       = |f - T_n| = 5.84e-5
Remainder bound    = |R_n| <= M * |x-a|^(n+1) / (n+1)!
                     M = max|f^(n+1)| on [a,x] = 1.0000
                     Bound = 5.31e-5
Bound / actual     = 0.91   [tight]
```

If the remainder bound is unavailable (e.g. AbsoluteValue):

```
Remainder bound:   unavailable
Reason:            function is not (n+1)-times differentiable at x = 0
DiagnosticFlag:    NotDifferentiable
```

**Section D — Convergence Plot**

Inline plot: x-axis = degree n (0 to max_degree), y-axis = log|error| at cursor.
Shows decay rate. Stagnation and divergence are visually distinct from convergence.

**Section E — Radius of Convergence**

```
Radius R:       +inf    (exp, sin, cos)
                1.0     (log(1+x))
                unknown (abs, step)

|x - a|:        [value]
Inside radius:  YES / NO / UNKNOWN
```

When outside the radius, the divergence zone shading in the left pane highlights
the current cursor position in red.

**Section F — Theorem Connections**

```
Taylor's Theorem with Remainder
  "A smooth function equals a polynomial plus a controllable remainder."
  -> future: ODE truncation error, PDE discretization, Fourier convergence

Lagrange Remainder
  "R_n(x) = f^(n+1)(ξ)/(n+1)! * (x-a)^(n+1) for some ξ in (a,x)"
  -> future: numerical approximation theory, convergence radius
```

**Section G — Result Trace** (collapsible)

```
n    T_n(x)      error        bound        flags
0    0.000000    0.479426     0.500000
1    1.000000    0.520574     0.500000
3    0.833333    0.353093     ...
5    0.841667    0.002241     0.000026     [bound tight]
```

## 17.3 Root-Finding Sub-Lab

Switching to Root-Finding mode in the Taylor lab:

**Left pane:**

```
f(x) graph
x_n marker (draggable initial guess)
Tangent line at x_n (Newton: the geometric construction)
x_{n+1} marker (tangent-line / x-axis intersection)
Bracket [a,b] shaded green (bisection)
```

**Right pane:**

```
Method: Newton-Raphson / Bisection / Secant / Fixed-point

Iteration table:
n    x_n           f(x_n)        |x_n - x*|    order
0    2.000000      2.000000      0.585786       —
1    1.500000      0.250000      0.085786       —
2    1.416667      0.006945      0.002453       1.93
3    1.414216      0.000006      0.000002       1.99

Log-error plot: expected slope -2 for Newton (quadratic), -1 for bisection.

Contraction (fixed-point only):
  g'(x*) = [value]
  |g'| < 1?  YES -> Banach fixed-point applies
  Convergence rate ≈ |g'| = [value]
```

**Hover on any x_n marker:** full precision x_n, f(x_n), f'(x_n), tangent equation.

---

# 18. UI Architecture — 2D Integration Lab

## 18.1 Left Pane — Domain Canvas (2D top-down)

**Always visible:**

```
Domain boundary     — solid outline
Partition cells     — grid lines or quadtree subdivisions
Sample points       — dots at sample location within each cell
Cell coloring       — cool-to-warm colormap by integrand value
```

**Toggle overlays:**

```
Error heatmap       — recolor cells by estimated local error
Refinement overlay  — border-highlight cells flagged for refinement
Singularity markers — marked where HasKnownSingularities is set
```

**Interactive elements:**

- **Hover over any cell** → cell tooltip (Section 19.2).
- **Click a cell** → pin it; right pane shows full breakdown for that cell.
- **Right-click a cell** → context menu: Refine / Mark for attention / Show trace.
- **Resolution slider** → changes base grid resolution live.
- **Toggle heatmap mode** → integrand-value vs error-estimate coloring.
- **Drag domain control points** → reshapes parametric domains live.

## 18.2 Left Pane — 3D Height Surface (Toggle)

Second view mode: integrand as 3D height surface.

```
f(x,y) surface      — mesh or smooth shaded
Domain floor        — xy-plane with domain boundary projected down
Sample pillars      — vertical lines (x,y,0) -> (x,y,f(x,y))
```

Toggle between 2D and 3D with a button. Both views stay synchronized — hovering
a cell in either view highlights it in both.

## 18.3 Right Pane — 2D Integration Analytical Panel

**Section A — Problem Statement**

```
Domain:         Rectangle [-1,1] x [-1,1]
Integrand:      GaussianBump2D  exp(-x^2 - y^2)
Method:         Midpoint Rule 2D
Grid:           50 x 50 = 2500 cells
Total measure:  4.0000
```

**Section B — Running Estimate**

```
Estimate:       3.141593
Reference:      pi = 3.14159265...   (exact, zoo metadata)
Absolute error: 2.65e-7
Relative error: 8.43e-8
Stability score: 0.0012   (low — good)
```

**Section C — Method Comparison Table** (toggleable)

```
Method                Estimate    Abs error   Evals   Order
Midpoint 50x50        3.141593    2.65e-7     2500    —
Trapezoid 50x50       3.141592    3.11e-7     2601    —
Simpson 50x50         3.141593    1.02e-8     2601    —
Adaptive quadtree     3.141593    4.4e-9      1847    —
```

**Section D — Cell Contribution Table** (sorted by |contribution|, top 20)

```
Cell      Region              Sample       f(s)        Measure      Contribution
(24,25)   [-0.02,0.02]^2      (0,0)        1.000000    0.001600     0.001600
(24,26)   [-0.02,0.02]x...    (0,.04)      0.998400    0.001600     0.001597
...
```

**Section E — Error Analysis**

```
Estimated error per cell:   shown in table
Total estimated error:      2.65e-7
Darboux gap (Darboux mode): U(f,P) - L(f,P) = [value]
Cells above threshold:      [count]  — click to highlight in canvas
```

**Section F — Convergence Plot**

X-axis: log(1/h). Y-axis: log|error|. Expected slope: 2 (midpoint/trapezoid),
4 (Simpson). Slope deviation triggers a diagnostic flag.

**Section G — Theorem Connections**

```
Darboux Integrability Criterion
  "f integrable iff U(f,P)-L(f,P) -> 0"
  -> future: Lebesgue criterion, measure theory

Fubini's Theorem
  "Integral over rectangle = iterated 1D integrals (when f integrable)"
  -> future: surface integrals, manifold integration

Change of Variables / Jacobian
  "Integral under coordinate change acquires |det DΦ| factor"
  -> future: surface integrals, arc length, manifold integration
```

**Section H — Result Trace** (collapsible)

Shows refinement history with convergence order.

---

# 19. UI Architecture — Hover and Interaction Specification

## 19.1 Universal Tooltip Format

```
[Bold title: what this thing is]
────────────────────────────────
Value:     [full precision — all significant digits]
Formula:   [expression that produced this value]
Theorem:   [name if applicable]
Flags:     [active DiagnosticFlags, or "none"]
Future:    [one-line forward pointer if applicable]
────────────────────────────────
[Hint: "Click to pin.  Right-click for options."]
```

## 19.2 Cell Hover (2D Integration)

```
Cell (24, 25)
────────────────────────────────
Region:           x in [-0.02, 0.02], y in [-0.02, 0.02]
Sample:           (0.000000, 0.000000)
f(sample):        1.000000000000000
Measure:          0.001600000000000
Contribution:     0.001600000000000   [= f × measure]
Est. local error: 3.2e-10
Flags:            none
────────────────────────────────
Right-click: Refine | Pin | Show trace
```

Near a singularity:

```
Cell (0, 0)
────────────────────────────────
Region:    x in [-0.01, 0.01], y in [-0.01, 0.01]
f(sample): [evaluation failed — singularity encountered]
Flags:     NearSingularity  HighOscillation
Theorem:   Integrability Microscope
  -> Region may be integrable but difficult.
  -> Adaptive quadtree refinement recommended.
────────────────────────────────
Right-click: Refine | Inspect singularity | Flag for attention
```

## 19.3 Taylor Curve Hover

Normal case:

```
sin(x)  at  x = 1.5708
────────────────────────────────
f(x):            1.000000000000000
T_5(x):          0.999942
|error|:         5.84e-5
Bound:           5.31e-5   [|R_5| <= 1 * |x|^6/720]
Bound / actual:  0.91  [tight]
f'(x):           0.000000   (cos(pi/2) = 0)
Flags:           none
────────────────────────────────
Drag to move.  Click to pin.
```

Outside radius of convergence (log(1+x) at x=2):

```
log(1+x)  at  x = 2.0000
────────────────────────────────
f(x):     1.098612
T_10(x):  -0.712...   [diverging]
|error|:  1.810...
Flags:    RadiusExceeded
  -> x = 2.0 is outside the radius of convergence R = 1.
  -> The series diverges here. Expected behavior.
Theorem:  Radius of Convergence
  -> "Power series converges absolutely inside R, diverges outside R."
  -> future: complex analysis, analytic functions
```

## 19.4 Expansion Point Drag

While dragging `a`:

- Left pane: all elements update live.
- Right pane: coefficient table and error analysis update live.
- Status bar: `a = [value]` updating continuously.
- If `a` lands on a singularity: `NotDifferentiable` flag appears; warning banner
  replaces the coefficient table with an explanation and suggested action.

## 19.5 Degree Slider

While sliding `n`:

- Dashed Taylor polynomial animates smoothly in left pane.
- Convergence plot adds a new data point at each integer n.
- Coefficient table adds or removes rows with animation.
- If error is increasing with degree: warning appears explaining divergence.

## 19.6 Theorem Name Hover

```
Taylor's Theorem with Remainder
────────────────────────────────
Formal:   f(x) = Σ_{k=0}^{n} f^(k)(a)/k! * (x-a)^k + R_n(x)
Informal: A smooth function equals a polynomial plus a controllable remainder.
Used by:  Taylor Approximation Lab
Later:    ODE truncation error, PDE discretization, Fourier convergence
```

## 19.7 DiagnosticFlag Hover

```
Flag: RadiusExceeded
────────────────────────────────
Definition: The evaluation point x lies outside the radius of convergence
            of the Taylor series centered at a.
Effect:     The series may diverge; the polynomial is unreliable here.
Action:     Move x inside [a - R, a + R], or choose a different a.
Theorem:    Radius of Convergence
  "Power series Σ c_n (x-a)^n converges absolutely for |x-a| < R."
```

## 19.8 Result Trace Row Hover

```
TraceStep n = 5
────────────────────────────────
Estimate:           0.999942
Actual error:       5.84e-5
Previous estimate:  0.998334   (n = 3)
Conv. ratio:        error_5 / error_3 ≈ 0.0039   (~factorial decay)
Flags:              none
Theorem:            Taylor remainder shrinks as (n+1)! grows
```

## 19.9 Method Selector Hover

```
Simpson's Rule (2D tensor product)
────────────────────────────────
Formula:  (h/3)(f_{00} + 4f_{10} + f_{20} + 4f_{01} + ... + f_{22})
Order:    4   [error = O(h^4)]
Cost:     (n+1)^2 evaluations for n-panel grid
Best for: smooth, non-oscillatory integrands on rectangles
Avoid:    functions with singularities or jump discontinuities
Theorem:  Simpson's Rule error bound
  "Error <= (b-a)^5/180 * h^4 * max|f^(4)|"
  -> future: Gaussian quadrature as the generalization
```

---

# 20. Phase 1 Implementation Focus

## 20.1 Phase 1A — Core Infrastructure

```
nd:: primitive type aliases                    (primitives.hpp)
nd::math:: function aliases                    (math_fns.hpp)
DiagnosticFlag, DiagnosticFlags               (diagnostics.hpp)
FunctionError, DomainError, EvaluationError,
IntegrationError, CurveError, SurfaceError,
ManifoldError, DDEError                        (errors.hpp)
CapabilitySet with initializer-list ctor       (capability.hpp)
TheoremConnection with static string_view      (theorem_connection.hpp)
TraceId, Metadata, TraceStep, ResultTrace      (result_trace.hpp)
StrongScalar CRTP, all scalar strong types     (strong_types.hpp)
EuclideanPoint2/3, EuclideanVector2/3          (strong_types.hpp)
Domain1D (Interval), Domain2D (Rectangle,Disk) (domain.hpp)
ZooEntryMetadata                               (zoo_registry.hpp)
Concepts: EvaluatableFunction, DifferentiableFunction,
          IntegrableFunction, ZooFunction      (zoo_registry.hpp)
Unit test harness
```

Deliverable: can register functions, query capabilities, evaluate with typed returns,
produce DiagnosticFlags, and build ResultTrace objects.

## 20.2 Phase 1B — Taylor Approximation Lab

```
AnyDifferentiableFunction (type-erased wrapper)
TaylorPolynomialBuilder
DerivativeProvider
TaylorApproximationService (concept-templated)
RemainderBoundService
TaylorApproximationResult (struct below)
Taylor zoo entries: exp, sin, cos, log(1+x), poly, abs, Runge
PlotCurve, ErrorBand data types (pure data, no renderer)
CoefficientTable, ErrorAnalysis data types
Unit tests per Section 15.2
```

```cpp
struct TaylorApproximationResult {
    Polynomial            polynomial;
    nd::real              expansion_point{};
    nd::i32               degree{};
    std::optional<nd::real>    radius_of_convergence;
    std::optional<ErrorBound>  remainder_bound;
    std::vector<SampleError>   sampled_errors;
    ResultTrace           trace;
    DiagnosticFlags       flags;
};
```

## 20.3 Phase 1C — Root-Finding Mini-Lab

```
NewtonRaphson
Bisection
SecantMethod
FixedPointIteration
RootFindingResult with full iteration trace
Log-error convergence data
Unit tests per Section 15.3
```

## 20.4 Phase 1D — 2D Integration Core

```cpp
// Integration problem and result
struct IntegrationProblem {
    DomainHandle           domain{};
    IntegrandHandle        integrand{};
    MeasureElementHandle   measure{};
    PartitionConfig        partition_config;
    ApproximationConfig    approximation_config;
    ErrorConfig            error_config;
    VisualizationConfig    visualization_config;
    Metadata               metadata;
};

struct CellContribution {
    nd::u64  cell_id{};
    nd::real sample_value{};
    nd::real cell_measure{};
    nd::real contribution{};           // = sample_value * cell_measure
    nd::real estimated_local_error{};
    DiagnosticFlags flags;
};

struct ConvergenceTrace {
    std::vector<TraceStep>   steps;
    std::optional<nd::real>  observed_order;
};

struct StabilityReport {
    nd::real    stability_score{};
    bool        appears_converged{};
    bool        is_stable{};
    std::string diagnostic_message;
};

struct IntegrationResult {
    nd::real estimate{};
    std::optional<nd::real>           reference_value;
    std::optional<nd::real>           absolute_error;
    std::optional<nd::real>           relative_error;
    std::vector<CellContribution>     contributions;
    ConvergenceTrace                  convergence;
    StabilityReport                   stability;
    DiagnosticFlags                   flags;
};

// Partition
struct Cell {
    nd::u64        id{};
    DomainRegion   region;
    nd::real       measure{};
    SamplePoint2D  default_sample;
    DiagnosticFlags flags;
};

struct RefinementNode {
    nd::u64 cell_id{};
    std::optional<std::array<nd::u64, 4>> children;  // nullopt = leaf
    nd::usize depth{};
};

struct RefinementTree {
    std::vector<RefinementNode> nodes;
    nd::u64  root{};
    bool     is_leaf(nd::u64 id) const noexcept;
    std::span<const nd::u64> leaves()  const noexcept;
};

struct Partition {
    std::vector<Cell> cells;
    nd::real          mesh_size{};
    PartitionMethod   method{};
    RefinementTree    refinement_tree;
};
```

Core interfaces (dispatch boundaries — not hot-loop virtual):

```cpp
struct Domain {
    virtual ~Domain() = default;
    virtual Dimension   dimension() const = 0;
    virtual BoundingBox bounds()    const = 0;
    virtual DomainType  type()      const = 0;
};

struct Integrand {
    virtual ~Integrand() = default;
    virtual std::expected<nd::real, EvaluationError>
    evaluate(const SamplePoint2D& p) const = 0;
};

struct MeasureElement {
    virtual ~MeasureElement() = default;
    virtual std::expected<nd::real, EvaluationError>
    evaluate_cell_measure(const Cell& cell) const = 0;
};

struct ApproximationMethod {
    virtual ~ApproximationMethod() = default;
    virtual std::expected<IntegrationResult, IntegrationError>
    compute(const IntegrationProblem& problem,
            const Partition& partition) const = 0;
    // Implementations loop internally using concrete types.
    // This virtual call occurs once per compute(), not per cell.
};

struct ErrorEstimator {
    virtual ~ErrorEstimator() = default;
    virtual std::expected<ErrorReport, IntegrationError>
    estimate(const IntegrationProblem& problem,
             const IntegrationResult& result) const = 0;
};
```

## 20.5 Phase 1E — 2D Integration Diagnostics

```
Adaptive quadtree partition (QuadTreePartition)
Per-cell contribution heatmap data
Estimated error heatmap data
StabilityReport with score = variation / max(1, |estimate|)
Method comparison table data
Oscillation heatmap data (sup-inf per cell)
Unit tests per Section 15.4/15.5
```

---

# 21. Phase 2 and Beyond

```
Phase 2:   Rigorous Darboux/Riemann Lab
Phase 3:   Sequences and Series of Functions Lab
Phase 4:   Spline/NURBS Curve Lab
Phase 5:   Frenet/Bishop Frame Motion Lab
Phase 6:   NURBS Surface and Surface Integration
Phase 7:   Topology and Manifold Lab
Phase 8:   ODE Systems Lab
Phase 9:   Control Theory Lab
Phase 10:  DDE and Pursuit Lab
Phase 11:  Stochastic Analysis Lab
Phase 12:  Fourier Analysis Lab
Phase 13:  Wavelet Analysis Lab
Phase 14:  PDE Lab
Phase 15:  Full Workspace Unification
```

See Section 3 for the five-pillar mathematical content of each phase.

---

# 22. Avoiding Architectural Dead Ends

```
Do not build a runtime formula editor first.
Do not couple visualization to numerical core.
Do not treat all spaces as Euclidean.
Do not treat NURBS as merely graphics objects.
Do not hide parameterization (u, t, s, speed are distinct).
Do not assume closed forms are available.
Do not overbuild future pillars now.
Do not use raw double, float, int, unsigned in library code.
Do not call std::sin, std::exp, std::abs directly — use nd::math::.
Do not conflate the embedded NURBS torus with the flat quotient torus T^2.
Do not use Euclidean subtraction for directions on the torus.
Do not allow virtual dispatch inside inner summation loops.
Do not allow TorusPoint to be implicitly converted to EuclideanPoint2.
```

---

# 23. Integration Lab — Full Detail

## 23.1 General Model

```
Integral Problem = Domain × Integrand × Measure × Partition × Method × Error × Viz

∫_a^b f(x) dx           1D interval
∫_γ f ds                arc length / line integral
∬_D f(x,y) dA           2D area integral
∬_S f dS                surface integral
∭_V f dV                3D volume integral
∫_a^b f(x) dg(x)        Riemann-Stieltjes
```

## 23.2 Differential Elements

```
dx                       1D Lebesgue measure element
dA                       2D area element (Jacobian-weighted in general)
dV                       3D volume element
ds = ||γ'(u)|| du        arc length element
dS = ||S_u × S_v|| du dv surface area element
dg                       Stieltjes measure
ρ ds                     weighted arc length (linear density × ds)
|det DΦ| du dv           change-of-variables Jacobian factor
```

## 23.3 Integration Modes

```
Tagged Riemann:          general tagged partitions, left/right/midpoint/random/draggable tags
Darboux:                 upper and lower sums, oscillation per cell, U-L gap
Riemann-Stieltjes:       integrator g, jump functions, probability expectation mode
Improper:                infinite intervals, singularity exclusion, principal value, divergence detector
```

## 23.4 Domain Types

```
1D: interval, open interval, half-open, union of intervals,
    piecewise collection, improper interval

2D: rectangle, triangle, disk, annulus, polygon,
    region under graph, region between curves,
    parametric patch, trimmed NURBS domain, level-set domain

3D: box, sphere, cylinder, cone, solid of revolution,
    volume under/between surfaces, tetrahedral mesh, swept volume
```

## 23.5 Approximation Methods

1D: Left/Right/Midpoint Riemann, Darboux upper/lower, Trapezoid,
    Simpson, Boole, Romberg, Gaussian, Gauss-Kronrod, Adaptive Simpson,
    Adaptive Gaussian, Monte Carlo, Quasi-Monte Carlo

2D: Rectangle, Midpoint Grid, Tensor-Product Trapezoid/Simpson/Gaussian,
    Triangular Mesh, Adaptive Quadtree, Monte Carlo, Surface Patch Quadrature

3D: Voxel Sum, Box Rule, Tensor-Product Gaussian, Tetrahedral,
    Adaptive Octree, Monte Carlo, Washer/Shell/Disk Methods

## 23.6 Integrability Microscope

User selects a point, interval, cell, patch, or region. System displays:

```
local oscillation (sup - inf on cell)
upper/lower contribution
refinement effectiveness
local discontinuity type
local singularity behavior
cell contribution to total error
nearby sample values
whether refinement is helping
```

Classification labels:

```
smooth | corner/cusp | jump discontinuity | removable discontinuity
infinite discontinuity | highly oscillatory | dense discontinuities
numerically unstable | integrable but difficult | probably divergent
```

## 23.7 Stability Score

```
StabilityScore = variation across partition perturbations / max(1, |estimated integral|)
```

Green (< 0.01), amber (0.01–0.1), red (> 0.1). Converged-but-unstable results
display a warning banner.

---

# 24. Spline/NURBS Lab — Full Detail

## 24.1 Supported Curve Types

```
Polyline, LineSegment, CircularArc, BezierCurve, CompositeBezierSpline,
HermiteSpline, CatmullRomSpline, CubicBSpline, NonuniformBSpline,
RationalBSpline, NURBSCurve, PeriodicSpline, ClosedSplineLoop,
InterpolatingSpline, ApproximatingSpline, Clothoid/EulerSpiral,
ArcLengthReparameterizedCurve
```

## 24.2 Supported Surface Types

```
PlanePatch, BilinearPatch, BezierSurface, BSplineSurface, NURBSSurface,
TensorProductSurface, TrimmedNURBSSurface, HeightfieldSurface,
SurfaceOfRevolution, SweepSurface, TubeSurface, RuledSurface, TriangleMeshSurface
```

## 24.3 Parametric Curve Interface

```cpp
struct ParametricCurve {
    virtual ~ParametricCurve() = default;
    virtual std::expected<EuclideanPoint3, CurveError>
        evaluate(Parameter1D u) const = 0;
    virtual std::expected<EuclideanVector3, CurveError>
        derivative(Parameter1D u, nd::i32 order) const = 0;
    virtual Domain1D parameter_domain() const = 0;
    virtual std::expected<CurveRegularity, CurveError>
        regularity(Parameter1D u) const = 0;
};
```

## 24.4 Parametric Surface Interface

```cpp
struct ParametricSurface {
    virtual ~ParametricSurface() = default;
    virtual std::expected<EuclideanPoint3,  SurfaceError> evaluate     (Parameter2D uv) const = 0;
    virtual std::expected<EuclideanVector3, SurfaceError> derivative_u (Parameter2D uv) const = 0;
    virtual std::expected<EuclideanVector3, SurfaceError> derivative_v (Parameter2D uv) const = 0;
    virtual std::expected<EuclideanVector3, SurfaceError> derivative_uu(Parameter2D uv) const = 0;
    virtual std::expected<EuclideanVector3, SurfaceError> derivative_uv(Parameter2D uv) const = 0;
    virtual std::expected<EuclideanVector3, SurfaceError> derivative_vv(Parameter2D uv) const = 0;
    virtual Domain2D  parameter_domain() const = 0;
    virtual std::expected<SurfaceRegularity, SurfaceError> regularity(Parameter2D uv) const = 0;
};
// Note: ParametricSurface != RiemannianManifold.
// Embedded geometry and intrinsic manifold structure are separate abstractions.
```

## 24.5 Curve Differential Quantities

```
position    γ(u)
d1          γ'(u)
d2          γ''(u)
d3          γ'''(u)
speed       ||γ'(u)||
unit tangent T = γ'/||γ'||
curvature   κ = ||γ' × γ''|| / ||γ'||^3
torsion     τ = ((γ' × γ'') · γ''') / ||γ' × γ''||^2
```

## 24.6 Frenet and Bishop Frames

```
Frenet: T = γ'/||γ'||, N = T'/||T'||, B = T × N
Failure modes: ||γ'|| ≈ 0, κ ≈ 0, inflection point, nearly straight segment
Bishop: rotation-minimizing; stable near zero curvature; required for tube generation
```

## 24.7 Key Data Structures

```cpp
struct CurveSample {
    Parameter1D      u{};
    ArcLength        s{};
    EuclideanPoint3  position;
    EuclideanVector3 d1, d2, d3;
    nd::real         speed_du{};
    Curvature        curvature{};
    Torsion          torsion{};
    Frame3           frenet, bishop, user_frame;
    DiagnosticFlags  flags;
};

struct MotionState {
    Time             time{};
    Parameter1D      u{};
    ArcLength        s{};
    EuclideanPoint3  position;
    EuclideanVector3 velocity, acceleration;
    EuclideanVector3 tangential_acceleration, normal_acceleration;
    nd::real         speed{};
    Curvature        curvature{};
    Torsion          torsion{};
    Frame3           active_frame;
    DiagnosticFlags  flags;
};

struct ArcLengthTable {
    std::vector<Parameter1D> u_values;
    std::vector<ArcLength>   s_values;
    ArcLength  total_length{};
    nd::real   estimated_error{};
};

struct ContinuityReport {
    Parameter1D u_join{};
    bool c0{}, c1{}, c2{};
    bool g0{}, g1{}, g2{};
    nd::real  d1_jump{}, d2_jump{}, tangent_angle{};
    Curvature curvature_jump{};
    Torsion   torsion_jump{};
    DiagnosticSeverity severity{};
};
```

## 24.8 Motion Laws

```
UniformParameterMotion         u(t) = u_0 + c*t
ConstantArcLengthSpeed         s(t) = s_0 + v*t, u = s^{-1}(s)
CurvatureLimitedSpeed          v_max(s) = sqrt(a_max / κ(s))
AccelerationLimitedMotion
JerkLimitedMotion / QuinticProfile
DDEDrivenMotion                (Phase 10 — requires DDEHistory)
StochasticPerturbedMotion      (Phase 11)
```

## 24.9 NURBS Design Formulae

```
Curve:   C(u) = [Σ_i N_{i,p}(u) w_i P_i] / [Σ_i N_{i,p}(u) w_i]
Surface: S(u,v) = [Σ_i Σ_j N_{i,p}(u) M_{j,q}(v) w_{ij} P_{ij}]
                 / [Σ_i Σ_j N_{i,p}(u) M_{j,q}(v) w_{ij}]

Continuity at internal knot with multiplicity m in degree-p curve: C^(p-m)
```

## 24.10 Spline UI Hotkey Layer

```
S1   Curve / Control Polygon
S2   Basis Functions and Knot Vector
S3   Continuity Inspector
S4   Particle Motion Along Curve
S5   Frenet / Bishop / User Frame Comparison
S6   Curvature and Torsion Comb
S7   Arc-Length Parameterization
S8   NURBS Weights and Rational Pull
S9   Surface Patch / Control Net
S10  Sweep / Tube / Frame Transport
```

---

# 25. Module Layout

```
src/
  nurbs_dde/
    core/
      primitives.hpp
      math_fns.hpp
      capability.hpp
      theorem_connection.hpp
      metadata.hpp
      result_trace.hpp
      diagnostics.hpp
      strong_types.hpp
      errors.hpp
      domain.hpp
    zoo/
      zoo_registry.hpp
      function_zoo.hpp
      taylor_zoo.hpp
      integration_zoo.hpp
    functions/
      function_1d.hpp
      function_2d.hpp
      polynomial.hpp
      elementary_functions.hpp
      singular_functions.hpp
    approximation/
      taylor_polynomial.hpp
      taylor_approximation_service.hpp
      remainder_bound_service.hpp
      root_finding.hpp
    sampling/
      sampling_service.hpp
      sample_grid.hpp
    partitioning/
      partition_1d.hpp
      partition_2d.hpp
      grid_partition_2d.hpp
      quadtree_partition.hpp
      refinement_tree.hpp
    integration/
      integration_problem.hpp
      integration_result.hpp
      cell_contribution.hpp
      convergence_trace.hpp
      stability_report.hpp
      integration_service.hpp
      approximation_method_2d.hpp
      error_estimation.hpp
    geometry/
      parametric_curve.hpp
      parametric_surface.hpp
    splines/
      bezier_curve.hpp
      bspline_curve.hpp
      nurbs_curve.hpp
    frames/
      frame3.hpp
      frenet_frame.hpp
      bishop_frame.hpp
      arc_length_table.hpp
    motion/
      motion_state.hpp
      motion_laws.hpp
      continuity_report.hpp
    manifolds/
      riemannian_manifold.hpp
      flat_torus.hpp
      round_sphere.hpp
      hyperbolic_plane.hpp
    topology/
      metric_space.hpp
      epsilon_ball.hpp
      continuity_checker.hpp
    ode_systems/
      phase_portrait.hpp
      equilibrium_classifier.hpp
      ode_solver.hpp
    control/
      pid_controller.hpp
      dde_characteristic_roots.hpp
    dde/
      dde_history.hpp
      method_of_steps.hpp
      dde_motion_state.hpp
      pursuit_system.hpp
    stochastic/
      wiener_process.hpp
      geodesic_random_walk.hpp
      sde_solver.hpp
    fourier/
      fourier_series.hpp
      fourier_transform.hpp
      fourier_on_torus.hpp
    wavelets/
      multiresolution_analysis.hpp
      haar_wavelet.hpp
      daubechies_wavelet.hpp
    pde/
      heat_equation.hpp
      wave_equation.hpp
      laplace_equation.hpp
      spectral_solver.hpp
    units/
      units.hpp

  services/
    taylor_approximation_service.hpp/.cpp
    root_finding_service.hpp/.cpp
    integration_service.hpp/.cpp
    partition_service.hpp/.cpp
    sampling_service.hpp/.cpp
    error_estimation_service.hpp/.cpp
    stability_analysis_service.hpp/.cpp
    result_trace_service.hpp/.cpp
    zoo_service.hpp/.cpp
    curve_geometry_service.hpp/.cpp
    spline_evaluation_service.hpp/.cpp
    nurbs_evaluation_service.hpp/.cpp
    continuity_analysis_service.hpp/.cpp
    frame_transport_service.hpp/.cpp
    arc_length_service.hpp/.cpp
    curve_motion_service.hpp/.cpp
    surface_geometry_service.hpp/.cpp
    sweep_geometry_service.hpp/.cpp
    metadata_service.hpp/.cpp
    threading_service.hpp/.cpp

  ui/
    workspaces/
      taylor_approximation_workspace.hpp/.cpp
      integration_2d_workspace.hpp/.cpp
    panes/
      geometry_pane.hpp/.cpp
      analysis_pane.hpp/.cpp
      plot_pane.hpp/.cpp
      diagnostics_pane.hpp/.cpp
      connections_pane.hpp/.cpp
      spectrum_pane.hpp/.cpp
      phase_portrait_pane.hpp/.cpp
      manifold_pane.hpp/.cpp
      epsilon_ball_pane.hpp/.cpp

  tests/
    core/
    functions/
    approximation/
    integration/
    partitioning/
    diagnostics/
    geometry/
    splines/
    frames/
    motion/
    manifolds/
    ode/
    fourier/
    pde/
```

---

# 26. Final Acceptance Criteria

## 26.1 Engineering

```
No raw double, float, int, unsigned anywhere in library code.
nd::real used for all mathematical computation.
nd::math:: used for all math functions (sin, exp, abs, etc.).
std::expected<T, E> for all recoverable errors.
std::optional<T> for all absent values. No sentinel NaN.
No virtual dispatch in inner summation loops.
AnyDifferentiableFunction wrapper used at UI/registry boundary.
All strong scalar types defined with correct arithmetic.
EuclideanPoint2 - EuclideanPoint2 -> EuclideanVector2 compiles.
EuclideanPoint2 + EuclideanPoint2 does not compile (deleted).
ArcLength + Time does not compile (different StrongScalar types).
DiagnosticFlags defined before all types that use it.
FunctionError defined before all concepts that reference it.
CapabilitySet constructible from std::initializer_list<Capability>.
TheoremConnection uses span for future_connections (plural).
RefinementTree defines is_leaf() and leaves().
IntegrationProblem, IntegrationResult, CellContribution all defined.
ConvergenceTrace and StabilityReport defined.
TraceStep includes step_index and previous_estimate.
```

## 26.2 Taylor Lab

```
Select exp, sin, cos, log(1+x), poly, abs, Runge from zoo.
Drag expansion point a in left pane; all quantities update live.
Drag degree slider n; polynomial animates live.
Coefficient table shows all terms.
Actual error and remainder bound shown at cursor.
Convergence plot shows log|error| vs n.
Radius of convergence shown with divergence zone shading.
Failure diagnostics for NotDifferentiable and RadiusExceeded.
Theorem connections visible in connections panel.
Result trace exportable.
```

## 26.3 Root-Finding

```
Newton, bisection, secant, fixed-point on zoo functions.
Tangent-line construction visible in left pane for Newton.
Iteration table with convergence order column.
Log-error plot with expected slope labeled.
Contraction constant and Banach status for fixed-point.
```

## 26.4 2D Integration

```
Rectangle and disk domains selectable.
2D integrand from zoo selectable.
Grid resolution slider.
Midpoint / rectangle / Simpson estimates.
2D canvas shows cells colored by integrand value.
Toggle to error heatmap.
Cell contribution table (sorted by |contribution|).
Exact value comparison when zoo metadata provides it.
Adaptive quadtree refinement.
Method comparison table.
Convergence plot with expected slope.
Result trace exportable.
```

## 26.5 UI and Hover

```
Every numerical value shows full-precision tooltip on hover.
Every DiagnosticFlag shows explanatory tooltip.
Every theorem name shows formal+informal tooltip.
Every plotted curve has a draggable cursor with live readout.
Every cell has a hover tooltip with region/sample/f/measure/contribution/flags.
Expansion point a is draggable in the Taylor left pane.
Degree slider animates Taylor polynomial live.
Connections panel is visible in every lab.
Result trace is collapsible and shows convergence order per step.
Method selector shows method specification on hover.
```

---

*This document is the authoritative architecture reference for nurbs_dde v3.*  
*All implementation decisions should be checked against it.*  
*Update this document when a new phase begins or a significant architectural decision is made.*
