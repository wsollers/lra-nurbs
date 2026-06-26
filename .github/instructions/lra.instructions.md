<!--
GENERATED FILE — DO NOT EDIT BY HAND.

Source repo: wsollers/lra-governance
Source commit: 24580f56384be0e7d70066be9a19799f9d1c3e0c
Generated from:
- docs/governance/...
- docs/architecture/...
- docs/governance/repo-overlays/lra-nurbs.md

Regenerate from lra-governance.
Emergency downstream edits must be ported upstream before regeneration.
-->

# LRA Repository Instructions

This file is intended for `.github/instructions/lra.instructions.md`. Keep it
concise and refer to canonical governance docs rather than copying large docs.

## Global Agent Rules

- Treat generated instruction files as derived artifacts.
- Follow the owning repository boundary for every task.
- Do not include secrets, credentials, tokens, or machine-local private values.
- Do not modify mathematical content during governance or wrapper-generation tasks.
- Do not touch the retired `Learning-Real-Analysis` monorepo.
- Keep context small: use governance docs as targeted references, not preload material.
- Open only the workflow, standard, schema, or overlay needed for the current task.
- Port emergency downstream instruction repairs back to `lra-governance`.

## Repo Overlay

# lra-nurbs Overlay

Stub overlay for C++ / Vulkan / geometry / simulation work.

Owned concerns:

- C++ and CMake build rules,
- Vulkan rendering rules,
- geometry and NURBS implementation,
- simulation and DDE implementation,
- local validators and CI expectations.

## Agent Scope

C++ / Vulkan / geometry / simulation rules apply only to `lra-nurbs`. They must
not be injected into volume content instructions.

Use local CMake, tests, and repo validators for implementation changes.

## Implementation Standards

`lra-nurbs` is a C++23 / Vulkan / geometry / simulation codebase. Keep new C++
modern, typed, and explicit: prefer scoped enums, value types, RAII ownership,
`std::span`, `std::optional`, `std::expected`, `std::string_view`, and
`[[nodiscard]]` where they clarify contracts. Avoid raw ownership, hidden global
state, and stringly typed protocols unless a boundary API requires them.

Follow the architecture visible in the source tree: domain code belongs under
the appropriate `ndde` namespace and subnamespace, rendering/platform concerns
stay separated from math and simulation kernels, and reusable abstractions
should have a clear caller before they are promoted. Names should describe the
domain concept or service responsibility rather than implementation mechanics.

Keep the C++ Core Guidelines in mind, especially around ownership, lifetime,
copy/move behavior, slicing, and explicit contracts. The local clang-tidy
configuration already checks selected `cppcoreguidelines-*` rules; do not work
around those warnings without documenting the reason in code or tests.

## Build And Validation

CI uses a build matrix:

- Windows x64 with MSVC, Vulkan SDK, CMake, and CTest.
- Linux x64 with Clang inside the checked-in Docker image
  `docker/linux-clang.Dockerfile`.

The Linux container build is the portable path for environments without MSVC:

```bash
docker build -t lra-nurbs-linux -f docker/linux-clang.Dockerfile .
docker run --rm -v "$PWD:/workspace" -w /workspace lra-nurbs-linux bash -lc \
  'cmake -S . -B build -G Ninja \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_C_COMPILER=clang-18 \
     -DCMAKE_CXX_COMPILER=clang++-18 \
     -DCMAKE_CXX_FLAGS="-stdlib=libc++ -fexperimental-library" \
     -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++ -fexperimental-library" \
     -DENABLE_SANITIZERS=OFF \
     -DNDDE_ENABLE_CLANG_TIDY=OFF &&
   cmake --build build --parallel &&
   ctest --test-dir build --output-on-failure --parallel 4'
```

CodeQL remains a separate native Linux job so GitHub can trace the C++ build.

## Provider Notes

Keep provider-specific guidance concise and defer durable policy to governance docs.
