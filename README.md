# lra-nurbs

NURBS / DDE simulation and visualization engine (C++23 / Vulkan / CMake).

Extracted from `Learning-Real-Analysis/nurbs_dde/`.

## Overview

A C++23 / Vulkan graphics project for rendering parametric curves in R³.

**Current milestone:** draw a parabola in a full-screen Vulkan window.  
**Long-term goals:** NURBS curves, delay-differential equation trajectories in R³.

## Dependencies

### System packages (Ubuntu)

```bash
sudo apt update
sudo apt install -y \
    clang-17 cmake ninja-build \
    libvulkan-dev vulkan-tools vulkan-validationlayers \
    libglfw3-dev glslang-tools pkg-config
```

## Build

```bash
cmake -B build -G Ninja \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

## Run

```bash
./build/src/nurbs_dde
```

## Tests

```bash
ctest --test-dir build --output-on-failure
```

## Relationship to monorepo

This repo was extracted from `Learning-Real-Analysis`. The analysis notes that mathematically motivated this code live in the LaTeX volumes.

See also: `nurbs_dde_code_review.md` and `nurbs_dde_particle_recommendations.md` in the monorepo for design documentation.
