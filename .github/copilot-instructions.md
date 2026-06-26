<!--
GENERATED FILE — DO NOT EDIT BY HAND.

Source repo: wsollers/lra-governance
Source commit: 6b4b1beb46a83f1c6ebb32cdd0866aa7954c0244
Generated from:
- docs/governance/...
- docs/architecture/...
- docs/governance/repo-overlays/lra-nurbs.md

Regenerate from lra-governance.
Emergency downstream edits must be ported upstream before regeneration.
-->

# Copilot Instructions

Keep this file concise. Point to canonical docs and generated repo instructions
rather than embedding large governance manuals.

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
