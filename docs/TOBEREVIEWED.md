# To Be Reviewed

**Status:** triage backlog
**Scope:** architectural review items, possible defects, performance work, tests, and future refactors
**Owner:** project maintainer
**Intent:** preserve review findings without treating them as accepted work until they have been checked against the current codebase

## How To Use This Document

Each item has a maintainer decision field and a work status field:

```text
Decision: undecided | yes | no | later | needs-info
Status: pending | in-progress | complete | blocked
Notes:
```

Use `yes` for accepted work, `no` for rejected findings, `later` for valid but
not current-priority work, and `needs-info` when the code needs more inspection
or measurement before deciding.

Severity is intentionally provisional. Items copied from external reviews should
be verified against the current tree before implementation.

## Highest Priority Review

### PMR Vector Rebinding Pattern

**Decision:** undecided  
**Status:** pending  
**Area:** memory, app, services  
**Files:** `src/app/ParticleSystem.hpp`, `src/memory/MemoryService.hpp`, related service bind methods  
**Reason to consider:** Some code destroys and reconstructs PMR-backed vectors in place when rebinding memory resources. This can be correct only under narrow object-lifetime assumptions and is easy to misuse.

**Current opinion:** The review's suggested simple move-assignment fix is probably not sufficient for `std::pmr::vector` with unequal allocators, because move assignment may keep the target allocator. Still, the pattern deserves a project-level helper or clearer wrapper so rebinding is not hand-written in multiple places.

**Notes:**

### SimulationRuntime Recursive Locking

**Decision:** undecided  
**Status:** pending  
**Area:** engine threading  
**Files:** `src/engine/SimulationRuntime.hpp`, `src/engine/SimulationRuntime.cpp`  
**Reason to consider:** `SimulationRuntime` currently uses `std::recursive_mutex`, and `publish()` can be called from code paths that already hold the runtime lock.

**Current opinion:** This is more of a design trap than an immediate race. Prefer a `publish_locked()` helper and, if feasible, downgrade to `std::mutex` so lock ownership is easier to reason about.

**Notes:**

### Sanitizer Build Option

**Decision:** undecided  
**Status:** pending  
**Area:** build system  
**Files:** `CMakeLists.txt`, `src/CMakeLists.txt`  
**Reason to consider:** `ENABLE_SANITIZERS` is declared but may not be wired into compile/link options for targets.

**Current opinion:** Worth fixing. ASan/UBSan are useful for exactly the memory and lifetime bugs this engine is now sophisticated enough to encounter.

**Notes:**

### Simulation Render Submit Cost

**Decision:** yes  
**Status:** complete  
**Area:** performance, renderer/simulation boundary  
**Files:** `src/engine/Engine.cpp`, `src/engine/SimulationRuntime.cpp`, app simulation render-submit code  
**Reason to consider:** Metrics show frame time dominated by `SimulationRenderSubmitMs`, while Vulkan acquire/submit/present are low. The bottleneck appears to be CPU packet preparation, cache rebuilds, packet copies, or alternate-view/hover geometry.

**Current opinion:** This is the highest-value performance path. Treat simulation state advance, geometry cache rebuild, and render packet submission as separate slices. Avoid dirtying geometry caches every tick unless inputs actually changed.

**Notes:** First pass complete. `SurfaceMeshCache` can skip fill geometry when a render path does not consume filled triangles, and the Wave Predator-Prey render-submit path now skips that unused fill build. Trail-picking samples are now built only while the mouse is active over the main view, avoiding per-frame Frenet/surface-frame work over every trail point during normal rendering.

## Correctness And Test Coverage

### HistoryBuffer Wrap-Boundary Test

**Decision:** yes 
**Status:** complete  
**Area:** simulation tests  
**Files:** `src/sim/HistoryBuffer.hpp`, `tests/`  
**Reason to consider:** Add coverage for filling the buffer to capacity, pushing across the wrap boundary, and querying/interpolating across the wrapped chronological order.

**Current opinion:** Good targeted test. This protects DDE pursuit behavior from subtle off-by-one mistakes.

**Notes:** Complete. Added `test_history_buffer.cpp` coverage for interpolation across the physical ring wrap boundary, chronological `to_vector()` export after wrap, and clamp behavior at the wrapped window extents.

### Milstein Statistical Variance Test

**Decision:** yes  
**Status:** complete  
**Area:** numerical tests  
**Files:** `src/sim/MilsteinIntegrator.hpp`, `tests/`  
**Reason to consider:** For pure diffusion, the expected variance is known. A statistical test can catch bad `sqrt(dt)` scaling, RNG wiring, or Milstein correction mistakes.

**Current opinion:** Useful, but keep tolerances broad enough to avoid flaky CI. Prefer deterministic seed and enough particles/samples for stability.

**Notes:** Complete. Added deterministic pure-Brownian variance coverage for `MilsteinIntegrator`. The test checks both parameter axes against the analytic variance `sigma^2 * T`. `MilsteinIntegrator::set_global_seed()` now bumps a seed generation so thread-local RNGs can be reseeded deterministically even if the current thread initialized the generator earlier.

### DelayPursuitEquation Surface Ownership

**Decision:** undecided  
**Status:** pending  
**Area:** simulation correctness  
**Files:** `src/sim/DelayPursuitEquation.hpp`  
**Reason to consider:** The equation stores a surface pointer and comments that it can become stale after a surface swap. `update()` already receives the current `ISurface&`, which may be enough to remove the stored pointer.

**Current opinion:** Worth inspecting. If the member pointer is avoidable, using the parameter would make the contract stronger.

**Notes:**

### Integrator Phase Semantics

**Decision:** undecided  
**Status:** pending  
**Area:** simulation semantics  
**Files:** `src/sim/EulerIntegrator.hpp`, `src/sim/MilsteinIntegrator.hpp`  
**Reason to consider:** Euler and Milstein may accumulate `state.phase` differently: full velocity versus drift-only.

**Current opinion:** Decide what `phase` means before changing code. If it tracks visual steering phase, drift-only may be intentional. If it tracks path length, stochastic displacement should contribute.

**Notes:**

### Milstein Thread-Local Seed Documentation

**Decision:** undecided  
**Status:** pending  
**Area:** numerical reproducibility  
**Files:** `src/sim/MilsteinIntegrator.hpp`  
**Reason to consider:** Thread-local RNG state is initialized once per thread. Changing a global seed after the first use may not reseed existing thread-local generators.

**Current opinion:** Document the contract or provide an explicit reseed mechanism if runtime reseeding becomes a real feature.

**Notes:**

## Renderer And Vulkan

### Frames In Flight

**Decision:** undecided  
**Status:** pending  
**Area:** renderer performance  
**Files:** `src/renderer/Renderer.hpp`, `src/renderer/Renderer.cpp`, `src/renderer/SecondWindow.*`  
**Reason to consider:** A single command buffer/fence model can serialize CPU and GPU work. Two frames in flight usually improves throughput and frame pacing.

**Current opinion:** Valid renderer evolution, but not necessarily the first FPS fix while `SimulationRenderSubmitMs` dominates CPU frame time.

**Notes:**

### Capture Staging Buffer Reuse

**Decision:** yes  
**Status:** complete  
**Area:** renderer capture  
**Files:** `src/renderer/Renderer.cpp`  
**Reason to consider:** PNG capture allocates Vulkan buffer/memory for capture work. Reusing a staging buffer would reduce driver allocation churn and make ownership cleaner.

**Current opinion:** Good cleanup for capture reliability. Lower priority than frame-path work unless capture becomes frequent.

**Notes:** Complete. `Renderer` and `SecondWindow` now own reusable host-visible capture staging buffers that grow on demand instead of allocating/freeing Vulkan buffer memory for every PNG capture. Build and tests pass; live PNG capture should still be smoke-tested in the Vulkan app.

### Capture Format Conversion

**Decision:** yes 
**Status:** complete  
**Area:** renderer capture portability  
**Files:** `src/renderer/Renderer.cpp`  
**Reason to consider:** Capture conversion should branch on swapchain format instead of assuming BGRA byte order.

**Current opinion:** Worth doing when capture code is touched next.

**Notes:** Complete. Primary and second-window PNG capture now convert readback pixels according to the actual swapchain format, handling BGRA and RGBA UNORM/SRGB explicitly and failing loudly on unsupported capture formats. Live Vulkan PNG capture should still be smoke-tested.

### Renderer Pipeline Fallback

**Decision:** undecided  
**Status:** pending  
**Area:** renderer cleanup  
**Files:** `src/renderer/Renderer.cpp`, `src/renderer/SecondWindow.cpp`  
**Reason to consider:** `pipeline_for()` currently throws on unknown topology. Some reviews suggest `std::unreachable()`.

**Current opinion:** Low priority. Throwing is acceptable for diagnosability unless this becomes a measured hot-path problem.

**Notes:**

### Vulkan Barrier Audit

**Decision:** undecided  
**Status:** pending  
**Area:** renderer synchronization  
**Files:** `src/renderer/Renderer.cpp`, `src/renderer/SecondWindow.cpp`  
**Reason to consider:** External review flagged broad barriers. Current `Renderer::transition_image()` appears to use specific stage masks, but `SecondWindow` and future paths should stay consistent.

**Current opinion:** Verify rather than assume. If barriers are already specific, mark this `no` or convert it to a regression note.

**Notes:**

## Architecture And Layering

### Remove Vulkan Dependency From Simulation Library

**Decision:** yes  
**Status:** complete  
**Area:** layering, CMake  
**Files:** `src/CMakeLists.txt`, `src/app/AnimatedCurve.*`, memory/render headers  
**Reason to consider:** `ndde_simulation` links `volk` because a transitive include pulls in a Vulkan-facing type. Pure simulation should not depend on Vulkan concepts.

**Current opinion:** Good architectural cleanup. Simulation should expose data/math; engine or renderer should translate to GPU resources.

**Notes:** Complete for the Vulkan dependency. `ndde_simulation` no longer links `volk`; alert rules now consume compact `AlertParticleView` records instead of concrete `AnimatedCurve` objects, and the app adapts particles before evaluation. Broader vocabulary cleanup remains possible later because `ParticleRole`/particle metadata still live under `app/ParticleTypes.hpp`.

### Engine Responsibility Split

**Decision:** Yes, implement a global PanelService that handles the panels.   
**Status:** complete  
**Area:** engine architecture  
**Files:** `src/engine/Engine.hpp`, `src/engine/Engine.cpp`  
**Reason to consider:** `Engine` owns frame loop, simulation switching, input, global panels, capture, telemetry, and renderer coordination.

**Current opinion:** Extracting all of this at once would be churn. A `GlobalPanelSet` or panel service consolidation is the most natural first extraction if the class keeps growing.

**Notes:** Complete for the first extraction. `PanelService` now supports service-owned ImGui window framing via `draw_body`, including first-use position, size, and background alpha. Engine global panels register body callbacks, so the service owns global panel Begin/End/window placement while older full-window callbacks remain supported. A later split can still move panel body implementations out of `Engine.cpp` if the class keeps growing.

### GLFW Hotkey Dispatch

**Decision:** yes  
**Status:** complete  
**Area:** platform/input cleanup  
**Files:** `src/engine/Engine.cpp`  
**Reason to consider:** A global `GLFWwindow* -> Engine*` map can be replaced with `glfwSetWindowUserPointer()` / `glfwGetWindowUserPointer()`.

**Current opinion:** Small, good cleanup.

**Notes:** Complete. The primary GLFW user pointer remains owned by `GlfwContext` for platform callbacks, and `GlfwContext` now carries an opaque key-callback user slot for the engine. `Engine` no longer needs the global `GLFWwindow* -> Engine*` map for hotkey dispatch.

### EngineAPI Hot-Path Function Objects

**Decision:** undecided  
**Status:** pending  
**Area:** render/simulation API boundary  
**Files:** `src/engine/EngineAPI.hpp`, call sites  
**Reason to consider:** `std::function` callbacks may add overhead for per-frame or per-packet operations.

**Current opinion:** Do not change until measured or until the API boundary is refactored for other reasons. A virtual interface or non-owning function pointer table are both possible replacements.

**Notes:**

### Lifetime Vector Alias Type Safety

**Decision:** undecided  
**Status:** pending  
**Area:** memory API design  
**Files:** `src/memory/Containers.hpp`  
**Reason to consider:** `FrameVector`, `SimVector`, `PersistentVector`, `HistoryVector`, and `CacheVector` are semantic aliases over the same type, so the compiler does not distinguish lifetimes.

**Current opinion:** Acceptable for now if documented clearly. Strong wrapper types may be useful later, but they will add friction and migration cost.

**Notes:**

## Build And Packaging

### ImGui Target Naming

**Decision:** yes  
**Status:** complete  
**Area:** CMake hygiene  
**Files:** `CMakeLists.txt`  
**Reason to consider:** A plain `imgui` target name can collide with external target names.

**Current opinion:** Low-risk cleanup. Rename to `ndde_imgui` or add an alias when convenient.

**Notes:** Complete. The manually defined Dear ImGui target is now `ndde_imgui`, with an `ndde::imgui` alias used by project targets and tests.

### ImGui Shallow Clone

**Decision:** yes  
**Status:** complete  
**Area:** CMake dependency fetch  
**Files:** `CMakeLists.txt`  
**Reason to consider:** `GIT_SHALLOW OFF` fetches full ImGui history.

**Current opinion:** Set shallow clone on unless there is a workflow reason to preserve full history.

**Notes:** Complete. Dear ImGui FetchContent now uses `GIT_SHALLOW ON` so clean dependency fetches avoid cloning the full repository history.

### Runtime Path Relocatability

**Decision:** yes  
**Status:** complete  
**Area:** packaging/runtime config  
**Files:** `src/CMakeLists.txt`, config loading code  
**Reason to consider:** Absolute `SHADER_DIR` and `NDDE_PROJECT_DIR` compile definitions make developer builds convenient but non-relocatable.

**Current opinion:** Fine for local development. Revisit when packaging/distribution matters.

**Notes:** Complete for the developer runtime path. Engine startup now resolves config, shaders, assets, telemetry, and capture output from the executable directory when possible. The build copies `assets`, `shaders`, and `engine_config.json` beside `nurbs_dde.exe`, and the engine no longer depends on `SHADER_DIR`, `ASSETS_DIR`, or `NDDE_PROJECT_DIR` compile definitions.

### Ctrl+Number Simulation Switching

**Decision:** yes. Maintain a HOTKEY_MAP.MD that we can use / reference or better add a post build cmake step to extract data from the c++ to autocreate the doc.  
**Status:** complete  
**Area:** input cleanup  
**Files:** `src/engine/Engine.cpp`  
**Reason to consider:** Repeated `Ctrl+1` through `Ctrl+5` conditionals can become range arithmetic.

**Current opinion:** Tiny cleanup. Do when touching hotkeys.

**Notes:** Complete. `Engine::on_key_event` already uses range arithmetic for `Ctrl+1` through `Ctrl+9`. Added `docs/HOTKEY_MAP.md` as the maintained hotkey reference and noted a few visible UI labels that should either become registered hotkeys or lose the chord label.

### Capture Timestamp Portability

**Decision:** yes, prefer std:: to any thing compiler / platform specifc. Add that to all engineering blurbs in design documents and use it for all new design documents as well.  
**Status:** complete  
**Area:** portability  
**Files:** `src/engine/Engine.cpp`  
**Reason to consider:** `localtime_s` is MSVC-specific. `std::chrono`/`std::format` could be more portable if compiler support is sufficient.

**Current opinion:** Low priority while Windows/MSVC is the active development environment.

**Notes:** Complete. Capture and telemetry timestamp formatting now use `std::chrono` plus `std::format`, removing `localtime_s`, `localtime_r`, `gmtime_s`, `gmtime_r`, `std::tm`, and `std::put_time` from the active source tree.

## Already Checked Or Likely Stale

### BrownianMotion Constant Noise

**Decision:** no  
**Status:** complete  
**Area:** simulation  
**Files:** `src/sim/BrownianMotion.hpp`  
**Reason:** Current code already overrides `has_constant_noise()` to return `true`.

**Notes:**

### Renderer ALL_COMMANDS Barrier Claim

**Decision:** needs-info  
**Status:** pending  
**Area:** renderer synchronization  
**Files:** `src/renderer/Renderer.cpp`, `src/renderer/SecondWindow.cpp`  
**Reason:** Current `Renderer::transition_image()` uses specific stage masks, not literal `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT`. `SecondWindow` should still be checked before closing this fully.

**Notes:**
