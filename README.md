# nurbs_dde

A C++23 / Vulkan graphics project for rendering parametric curves in R³.

**Current milestone:** draw a parabola in a full-screen Vulkan window.  
**Long-term goals:** NURBS curves, delay-differential equation trajectories in R³.

---

## Dependencies

All fetched automatically by CMake FetchContent except the system packages below.

### System packages (Ubuntu)

```bash
sudo apt update
sudo apt install -y \
    clang-17 \
    cmake \
    ninja-build \
    libvulkan-dev \
    vulkan-tools \
    vulkan-validationlayers \
    libglfw3-dev \
    glslang-tools \
    pkg-config
```

Set Clang 17 as default if needed:
```bash
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-17 100
sudo update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-17   100
```

---

## Build

```bash
# Configure (Debug — includes ASan + UBSan)
cmake -B build -G Ninja \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)
```

For a release build:
```bash
cmake -B build-rel -G Ninja \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-rel -j$(nproc)
```

---

## Run

```bash
./build/src/nurbs_dde
# ESC to quit
```

---

## Tests

```bash
ctest --test-dir build --output-on-failure
# or directly:
./build/tests/nurbs_dde_tests
```

---

## Project structure

```
nurbs_dde/
├── CMakeLists.txt
├── cmake/
│   └── CompileShaders.cmake       Shader GLSL → SPIR-V helper
├── shaders/
│   ├── curve.vert                 Line-strip vertex shader (push-constant MVP)
│   └── curve.frag                 Colour passthrough fragment shader
├── src/
│   ├── math/
│   │   └── Vec3.hpp               R³ types (thin glm wrapper)
│   ├── geometry/
│   │   ├── Curve.hpp/.cpp         Abstract parametric curve p:[t0,t1]→R³
│   │   └── Parabola.hpp/.cpp      y = at²+bt+c on XY plane
│   ├── renderer/
│   │   ├── VulkanContext.hpp/.cpp Instance / device / queues (vk-bootstrap)
│   │   ├── Swapchain.hpp/.cpp     Swapchain management
│   │   ├── Pipeline.hpp/.cpp      Line-strip graphics pipeline
│   │   └── Renderer.hpp/.cpp      Frame loop, vertex buffer, sync
│   └── app/
│       ├── Application.hpp/.cpp   Top-level orchestrator
│       └── main.cpp
└── tests/
    ├── CMakeLists.txt
    ├── test_vec3.cpp
    ├── test_parabola.cpp
    └── test_curve.cpp
```

---

## Roadmap

| Milestone | Status |
|---|---|
| Full-screen Vulkan window | ✅ |
| Draw parabola (line-strip) | ✅ |
| GTest suite (geometry + math) | ✅ |
| NURBS curve (de Boor algorithm) | ⏳ |
| DDE trajectory integration (RK4 / history buffer) | ⏳ |
| Camera / orbit control in R³ | ⏳ |
| Depth buffer (3-D curves) | ⏳ |

---

## Design notes

- **`Curve` abstract base** — `tessellate(n)` gives uniform parameter samples;
  `vertex_buffer(n)` gives `vec4` data ready for a Vulkan vertex buffer.
  NURBS will override with knot-adaptive sampling.
- **Homogeneous coordinates** (`w = 1`) in vertex buffers are intentional —
  rational NURBS weights will live in `w`.
- **Push-constant MVP** (64 bytes) — sufficient for 2-D curves today;
  a UBO will be added when camera + multiple objects are needed.
- **Dynamic rendering** (VK 1.3) — no `VkRenderPass` objects; cleaner
  for a geometry-exploration tool that changes frame structure frequently.
- **No staging buffer yet** — vertex data is uploaded via host-visible memory.
  A proper staging pipeline will be introduced before adding large DDE history meshes.
