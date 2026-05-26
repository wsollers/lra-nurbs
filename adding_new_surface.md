# Adding a New Surface to the NURBS DDE Project

> **Pedagogical note:** Before touching a single line of code, ask: *what is the surface, geometrically?* A surface in this project is a map $\phi: D \subset \mathbb{R}^2 \to \mathbb{R}^3$ — it takes a 2D parameter pair $(u, v)$ and produces a 3D point. Every surface you add is just a new choice of that map, plus the machinery to evaluate it, compute normals, and feed it to Vulkan. The steps below follow that logical chain exactly.

---

## 1. Understand What a "Surface" Is in the Codebase

Before adding anything, locate the existing surface abstraction. There will be (or should be) a base interface or struct resembling:

```cpp
// Conceptually — your actual class may vary
struct Surface {
    virtual glm::vec3 evaluate(float u, float v) const = 0;
    virtual glm::vec3 normal(float u, float v) const = 0;
    virtual std::string name() const = 0;
};
```

**Key questions to answer before proceeding:**

- Where is the existing surface (e.g., torus) defined?
- Is there a factory or registry that maps a string/enum to a surface instance?
- How does the renderer receive the surface — by pointer, by value, via a scene descriptor?

---

## 2. Define the New Surface Mathematically

Pick your surface. As an example, we will add a **Klein bottle immersion** (a non-orientable surface, topologically rich, great for understanding manifolds without boundary).

Its parametric map $\phi: [0, 2\pi] \times [0, 2\pi] \to \mathbb{R}^3$ (one common immersion):

$$
\phi(u, v) = \begin{cases}
x = (a + b\cos v)\cos u \\
y = (a + b\cos v)\sin u \\
z = b \sin v
\end{cases}
\quad \text{for a simpler "figure-8" immersion}
$$

Or for the full Möbius-style Klein bottle:

$$
\phi(u,v) = \begin{pmatrix}
(r + \cos(u/2)\sin v - \sin(u/2)\sin 2v)\cos u \\
(r + \cos(u/2)\sin v - \sin(u/2)\sin 2v)\sin u \\
\sin(u/2)\sin v + \cos(u/2)\sin 2v
\end{pmatrix}
$$

> **Why this matters:** The partial derivatives $\partial_u \phi$ and $\partial_v \phi$ give you two tangent vectors. Their cross product gives the surface normal. You need these for shading in Vulkan. Decide analytically or use finite differences.

---

## 3. Create the Surface File

### Directory Layout

```
src/
  surfaces/
    Torus.hpp          ← existing
    Torus.cpp
    KleinBottle.hpp    ← NEW
    KleinBottle.cpp    ← NEW
    SurfaceRegistry.hpp
    SurfaceRegistry.cpp
```

### `KleinBottle.hpp`

```cpp
#pragma once
#include "Surface.hpp"   // your base class
#include <glm/glm.hpp>
#include <string>

class KleinBottle : public Surface {
public:
    explicit KleinBottle(float r = 2.0f);

    glm::vec3 evaluate(float u, float v) const override;
    glm::vec3 normal(float u, float v) const override;
    std::string name() const override { return "KleinBottle"; }

    // Parameter domain: u in [0, 2*pi], v in [0, 2*pi]
    float uMin() const override { return 0.0f; }
    float uMax() const override { return 6.2831853f; }
    float vMin() const override { return 0.0f; }
    float vMax() const override { return 6.2831853f; }

private:
    float m_r; // radius parameter
};
```

### `KleinBottle.cpp`

```cpp
#include "KleinBottle.hpp"
#include <cmath>

// NOTE: std::sin / std::cos used here — see math_migration.md
// for how to swap these to your custom implementations.

KleinBottle::KleinBottle(float r) : m_r(r) {}

glm::vec3 KleinBottle::evaluate(float u, float v) const {
    float cosU  = std::cos(u);
    float sinU  = std::sin(u);
    float cosV  = std::cos(v);
    float sinV  = std::sin(v);
    float cosHU = std::cos(u * 0.5f);
    float sinHU = std::sin(u * 0.5f);

    float x = (m_r + cosHU * sinV - sinHU * std::sin(2.0f * v)) * cosU;
    float y = (m_r + cosHU * sinV - sinHU * std::sin(2.0f * v)) * sinU;
    float z = sinHU * sinV + cosHU * std::sin(2.0f * v);

    return glm::vec3(x, y, z);
}

glm::vec3 KleinBottle::normal(float u, float v) const {
    // Finite difference fallback — replace with analytic later
    const float eps = 1e-4f;
    glm::vec3 du = (evaluate(u + eps, v) - evaluate(u - eps, v)) / (2.0f * eps);
    glm::vec3 dv = (evaluate(u, v + eps) - evaluate(u, v - eps)) / (2.0f * eps);
    return glm::normalize(glm::cross(du, dv));
}
```

---

## 4. Register the Surface

Find or create a `SurfaceRegistry` (or equivalent factory). Add your new surface to it:

```cpp
// SurfaceRegistry.cpp
#include "KleinBottle.hpp"

std::unique_ptr<Surface> SurfaceRegistry::create(const std::string& name) {
    if (name == "Torus")       return std::make_unique<Torus>();
    if (name == "KleinBottle") return std::make_unique<KleinBottle>(); // ← ADD
    throw std::runtime_error("Unknown surface: " + name);
}
```

If you use an enum instead:

```cpp
enum class SurfaceType {
    Torus,
    KleinBottle,  // ← ADD
};
```

---

## 5. Wire It Into the Renderer / Scene

Find where the active surface is selected (likely a config struct or command-line argument) and add the new case:

```cpp
// In your scene setup or config parsing:
if (config.surface == "KleinBottle") {
    scene.surface = SurfaceRegistry::create("KleinBottle");
}
```

For Vulkan, the surface is typically sampled at a uniform $(u_i, v_j)$ grid and uploaded as a vertex buffer. Ensure your tessellation loop respects the new surface's parameter domain (`uMin()`, `uMax()`, etc.).

---

## 6. Test the Surface in Isolation

Before rendering, write a small unit test that:

1. Evaluates $\phi(0, 0)$ and checks against the known analytic value.
2. Checks that `normal(u, v)` is unit length for several $(u, v)$ pairs.
3. Verifies the parameter domain is correctly bounded.

```cpp
// Quick sanity check (e.g., in a test binary or main debug block)
KleinBottle kb(2.0f);
auto p = kb.evaluate(0.0f, 0.0f);
assert(std::abs(glm::length(kb.normal(1.0f, 1.0f)) - 1.0f) < 1e-5f);
```

---

## 7. Checklist

| Step | Task | Done? |
|------|------|-------|
| 1 | Understand existing surface abstraction | ☐ |
| 2 | Define parametric map mathematically | ☐ |
| 3 | Create `.hpp` and `.cpp` files | ☐ |
| 4 | Register in `SurfaceRegistry` | ☐ |
| 5 | Wire into renderer/scene config | ☐ |
| 6 | Unit-test evaluate + normal | ☐ |
| 7 | Render and visually inspect | ☐ |

---

## 8. Notes on Manifold Topology

The Klein bottle is topologically distinct from the torus: it is **non-orientable** (no globally consistent normal field), which is why the cross-product normal will flip across the self-intersection seam of the immersion. This is *mathematically correct* — it is not a bug. If Vulkan backface culling causes holes, disable it for this surface or use `abs(dot(...))` in your shader.

The torus $T^2 = S^1 \times S^1$ is orientable; the Klein bottle $K$ satisfies $\chi(K) = 0$ (same Euler characteristic as the torus) but is not embeddable in $\mathbb{R}^3$ — only immersible. You are visualizing an immersion, not an embedding.
