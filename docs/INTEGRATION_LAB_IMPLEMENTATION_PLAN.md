# Integration Lab Implementation Plan

**Status:** execution plan  
**Design source:** `docs/INTEGRATION_LAB_DESIGN.md`  
**Goal:** complete a usable 2D integration workbench quickly without blocking
future 3D, surface, spline, or NURBS integration.

Focused UI contracts:

```text
docs/INTEGRATION_WORKBENCH_UI_DESIGN.md
docs/INTEGRATION_ANALYTICS_WINDOW_DESIGN.md
```

Current user-facing parity note:

```text
The engine-safe vertical slices are not the same as prototype parity.
Each phase marked complete means the underlying runnable capability exists and
is tested. The intended UI is a two-window lab:
  main window = integrated workbench
  second window = results and supporting displays
The full POC-shaped experience still requires second-window analytics polish,
richer domain/partition creation, Darboux views, and result-trace/theorem panes.
```

---

## Strategy

Build the lab in thin vertical slices. Each slice should leave the app runnable,
the current smoke-test simulation preserved, and the tests passing.

The fastest useful path is:

```text
1. Core 2D integration data model
2. Uniform-grid midpoint integration
3. Primary workbench screen in the main window
4. Renderable 2D cell/sample/heatmap snapshot
5. Second-window analytics snapshot
6. Convergence and method comparison
7. Polish interaction and diagnostics
```

Do not start with the full service matrix. Start with one strong end-to-end
2D rectangle workflow, then generalize only when the next feature demands it.

---

## Engineering Standards

Implementation must follow the project engineering direction:

```text
C++20/23
C++ Core Guidelines style
Rule of 0/3/5 as appropriate
small value types for math/config data
explicit ownership and lifetime
compact render/event/metric paths
high test coverage for numerical behavior
```

Prefer project standard types:

```text
f32, f64
u32, u64
byte
ComponentId
RuntimeNodeId
ResourceId
RenderViewId
```

Use STL/API-required types where required:

```text
std::size_t
int for ImGui/GLFW/Vulkan APIs
Vulkan/GLFW/ImGui native types at API boundaries
```

Preferred standard/library tools:

```text
std::expected for fallible numerical/config operations
std::optional for absent values
std::variant for closed sets of domain/integrand/method alternatives
std::span for non-owning array views
std::pmr and project memory arenas for owned hot-path buffers
std::jthread and std::stop_token for future background analysis jobs
```

Avoid:

```text
raw owning pointers
sentinel NaN/error values
stringly typed metric/event paths
render code recomputing math results
UI code owning numerical truth
large heap churn in frame, event, telemetry, metric, and render paths
```

Events and metrics must stay compact. Do not put formulas, paths, or long text
on compact event/metric paths. Use ids, flags, metadata records, and logger or
diagnostic presentation for human-readable text.

---

## Fluent Builder Pattern

Use fluent builders for workbench problem/config creation where they make tests
and UI update code more readable.

The builder should be explicit, value-oriented, and cheap to discard. It should
not hide expensive computation. Calling `build()` may validate and return an
`std::expected`, but it should not perform integration unless the type name makes
that clear.

Recommended builders:

```cpp
IntegrationProblem2DBuilder{}
    .rectangle({.x_min = -2.0, .x_max = 2.0, .y_min = -2.0, .y_max = 2.0})
    .integrand(IntegrandPreset2D::Gaussian)
    .measure(MeasureElement2D::Area)
    .uniform_grid({.x_cells = 32u, .y_cells = 32u})
    .method(IntegrationMethod2D::Midpoint)
    .tolerance(1.0e-4)
    .build();

IntegrationDisplayConfigBuilder{}
    .view(DiagnosticField2D::Contribution)
    .show_cells(true)
    .show_samples(true)
    .show_axes(true)
    .build();

IntegrationAnalysisRequestBuilder{}
    .problem(problem_id)
    .resolutions({8u, 16u, 32u, 64u})
    .compare_methods({IntegrationMethod2D::Midpoint,
                      IntegrationMethod2D::TensorProductTrapezoid})
    .build();
```

Builder rules:

```text
Builder methods return `Builder&` for lvalue chaining or `Builder&&` if the
local style needs move-chaining.

`build()` returns `std::expected<T, IntegrationError>` when validation can fail.

Builders validate dimensions, cell counts, domain bounds, and unsupported method
combinations.

Builders should produce plain structs consumed by the kernel. The kernel should
not depend on builder internals.

Do not use builders for tiny leaf types where aggregate initialization is clearer.

Do not store references to temporary lambdas/functions unless ownership is
spelled out. Prefer preset ids first; add callable-owning integrands deliberately.
```

The goal is to make common setup easy:

```text
tests read like mathematical setup
UI config changes are localized
future domain/method additions do not explode constructor argument lists
```

---

## Phase 0: Guard Rails

Purpose:

```text
Keep the existing sim safe and make the lab easy to enter.
```

Status:

```text
complete
```

Already present:

```text
Ctrl+1 Lab Picker
Ctrl+2 Smoke Test - Wave Predator-Prey
Ctrl+3 Integration & Derivative Lab
Ctrl+4 Taylor Expansion Lab
```

Completion gate:

```text
App starts into picker.
Smoke test sim remains available.
Integration lab remains available.
Tests pass.
```

---

## Phase 1: Core 2D Integration Kernel

Purpose:

```text
Create the non-UI model that computes a 2D integral and produces inspectable data.
```

Status:

```text
complete
```

Files:

```text
src/math/integration/Integration2D.hpp
src/math/integration/Integration2D.cpp
tests/test_integration2d_core.cpp
```

Types:

```cpp
struct RectDomain2D;
struct UniformGrid2DConfig;
struct Cell2D;
struct CellContribution2D;
struct IntegrationResult2D;
enum class IntegrationMethod2D;
enum class DiagnosticField2D;
```

Required functions:

```cpp
std::expected<UniformGrid2D, IntegrationError>
make_uniform_grid(RectDomain2D domain, UniformGrid2DConfig config);

std::expected<IntegrationResult2D, IntegrationError>
integrate_uniform_grid(RectDomain2D domain,
                       ScalarFunction2D function,
                       UniformGrid2DConfig config,
                       IntegrationMethod2D method);

std::expected<ConvergenceSeries2D, IntegrationError>
compute_convergence_series(...);
```

Initial methods:

```text
Midpoint
TensorProductTrapezoid
```

Initial integrands:

```text
Gaussian exp(-(x*x + y*y))
Wave sin(2x) cos(2y) + offset
Polynomial x*y + offset
Step/discontinuous half-plane
```

Tests:

```text
Uniform grid covers rectangle exactly.
Cell count equals nx * ny.
Cell measures sum to domain area.
Midpoint integrates constant exactly.
Midpoint integrates separable/simple polynomial within tolerance.
Contribution sum equals result estimate.
Selected cell lookup is bounds-checked.
Convergence improves for smooth Gaussian as resolution increases.
```

Completion gate:

```text
All integration2d tests pass.
No ImGui/render dependency in math kernel.
All result, diagnostic, renderable sample, and metadata fields exist.
```

---

## Phase 2: Workbench State and Snapshot Layer

Purpose:

```text
Separate UI controls from numerical computation and rendering data.
```

Status:

```text
complete
```

Files:

```text
src/app/IntegrationWorkbenchState.hpp
src/app/IntegrationWorkbenchState.cpp
tests/test_integration_workbench_state.cpp
```

Types:

```cpp
struct IntegrationProblem2DConfig;
struct IntegrationDisplayConfig;
struct IntegrationWorkbenchSnapshot;
struct IntegrationAnalysisSnapshot;
struct SelectedCellSummary2D;
struct RenderableIntegration2D;
```

Responsibilities:

```text
Own current domain/integrand/method/resolution choices.
Recompute 2D result when config changes.
Track selected/hovered cell ids.
Build immutable snapshots for primary UI and second-window analytics.
Keep UI strings out of core numerical result structures.
```

Tests:

```text
Default state produces valid rectangle/gaussian/midpoint snapshot.
Changing resolution recomputes result and cell count.
Selected cell summary matches contribution data.
Invalid selected cell clears or clamps safely.
Analysis snapshot marks old traces stale after problem change.
```

Completion gate:

```text
Simulation can own one IntegrationWorkbenchState and query snapshots.
No render packet generation occurs inside math kernel.
```

---

## Phase 3: Primary Main-Window Workbench

Purpose:

```text
Replace floating demo panels with the first usable integrated workbench.
For the current 1D-first use cases, the main window should be one top command
bar plus one primary spatial viewport.
```

Status:

```text
complete
```

Files:

```text
src/app/SimulationIntegrationDerivativeLab.hpp
src/app/SimulationIntegrationDerivativeLab.cpp
src/app/IntegrationLabPrimaryPanel.hpp       optional
src/app/IntegrationLabPrimaryPanel.cpp       optional
```

UI:

```text
Top anchored bar:
  Equation object zoo selector populated from SimMetadataService
  method
  N partition count
  interval min / max

Center:
  Vulkan-rendered active integral
  graph/domain frame
  cells
  samples
  active/selected cell guides when enabled

Not shown in the first use cases:
  left tool tray
  right inspector
  bottom status strip
  mode tabs
  floating panels
```

Implementation notes:

```text
Use ImGui as anchored workbench chrome until Vulkan text/UI rendering is mature.
Do not use a movable/floating settings panel as the primary lab shape.
Do not remove the existing lab entry point.
The detailed contract lives in `INTEGRATION_WORKBENCH_UI_DESIGN.md`.
```

Tests:

```text
Existing simulation registration test still passes.
Integration lab registers zoo metadata for live scalar fields and planned
surface/manifold objects.
Snapshot after tick contains 2D result.
No render packets required for panel-only operation.
```

Completion gate:

```text
User can open Ctrl+3 and see a workbench-shaped main window:
top command bar and one primary spatial viewport.
```

---

## Phase 4: Primary Viewport Render Packets

Purpose:

```text
Make the main viewport show the domain, cells, samples, selected cell, and map.
```

Status:

```text
complete
```

Files:

```text
src/app/IntegrationLabRenderPackets.hpp
src/app/IntegrationLabRenderPackets.cpp
tests/test_integration_lab_render_packets.cpp
```

Renderable layers:

```text
Domain boundary
Grid/cell outlines
Sample points
Value heatmap
Contribution heatmap
Error heatmap
Selected cell outline
Axes/grid
```

Implementation notes:

```text
Use existing RenderService packets.
Use orthographic projection for the integration view.
Allocate vertices through MemoryService frame/simulation scopes as appropriate.
Keep generated render data compact.
Use vertex color where possible.
```

Tests:

```text
Rectangle boundary emits expected line vertices.
Grid emits stable vertex counts for nx/ny.
Heatmap emits triangle packets with one quad per cell.
Selected cell overlay emits only when selection exists.
Packet generation does not mutate workbench state.
```

Completion gate:

```text
Main window visibly shows the 2D integral geometry and selected cell.
```

---

## Phase 5: Hover and Selection

Purpose:

```text
Let the user inspect cells directly from the viewport.
```

Status:

```text
complete
```

Files:

```text
src/app/IntegrationWorkbenchState.*
src/app/SimulationIntegrationDerivativeLab.*
tests/test_integration_workbench_state.cpp
```

Behavior:

```text
Mouse hover maps screen/domain coordinate to a cell id.
Click pins selected cell.
Status strip follows hover.
Inspector follows selected cell.
```

Implementation notes:

```text
Use RenderService view domain and InteractionService.
Start with rectangle-domain hit testing.
Do not implement domain editing yet.
```

Tests:

```text
Domain coordinate maps to expected grid cell.
Coordinates outside domain return no cell.
Click selection persists across ticks.
Selection clears or clamps after resolution changes.
```

Completion gate:

```text
Hover/click updates selected-cell data in the workbench.
```

---

## Phase 6: Second-Window Analytics View

Purpose:

```text
Use the existing alternate/second Vulkan window as the results and displays
companion.
```

Status:

```text
complete initial slice
```

Files:

```text
src/app/IntegrationLabAnalyticsPanel.hpp      optional
src/app/IntegrationLabAnalyticsPanel.cpp      optional
src/app/IntegrationLabAnalyticsPackets.hpp
src/app/IntegrationLabAnalyticsPackets.cpp
tests/test_integration_lab_analytics.cpp
```

Views:

```text
Stable empty analysis shell
Result summary
Convergence trace
Method comparison
Contribution distribution
Error/refinement distribution shell
Selected-cell microscope
Metadata / diagnostics / trace rows
```

First implementation:

```text
Register one alternate view for the integration lab.
When no active problem exists, render an empty analysis shell with empty graph
frames and empty tables.
When a problem snapshot exists, populate result summary, contribution
distribution, method comparison, and convergence data from the same snapshot
that drives the main viewport.
```

The second window should not duplicate the main workbench controls. It follows
the active problem and selected cell from the main window, then presents richer
analysis, plots, comparisons, and traces.
The detailed contract lives in `INTEGRATION_ANALYTICS_WINDOW_DESIGN.md`.

Tests:

```text
Integration lab registers main and alternate views.
Analytics snapshot has convergence rows for multiple resolutions.
Method comparison contains midpoint and trapezoid rows.
Selected-cell report matches primary selected cell.
Closing second window does not break primary workbench.
```

Completion gate:

```text
Second window displays analytics derived from the same active integration problem.
```

---

## Phase 7: Diagnostics and Polish

Purpose:

```text
Make the lab trustworthy and pleasant for study.
```

Features:

```text
Contribution leaderboard
Error-map legend
Observed convergence order
Sampled Darboux bounds
Function/domain metadata
Result trace rows
Stable color palette and labels
```

Tests:

```text
Darboux sampled upper >= lower.
Bounds gap shrinks for smooth functions under refinement.
Result trace appends recomputation rows.
Diagnostic flags trigger for discontinuous preset.
```

Completion gate:

```text
The 2D lab can teach value, contribution, error, and convergence clearly.
```

---

## Phase 7A: Expanded POC-Shaped Workbench Shell

Purpose:

```text
Restore the broader prototype-shaped workbench only after the first use cases
are stable. The current first-screen contract remains one top command bar plus
one primary spatial viewport.
```

Status:

```text
pending after initial use cases
```

Later target shell:

```text
Main window:
  anchored top bar
  anchored left tool tray
  Vulkan-rendered center workbench
  anchored right inspector
  anchored bottom status strip

Mode strip:
  Domain
  Partition
  Samples
  Contribution
  Error
  Bounds
  Analysis

Live controls:
  rectangle domains
  metadata-backed object zoo selector
  1D equation objects: y=1, y=x, y=exp(x), y=ln(x), y=1/x
  live scalar-field presets
  uniform grids
  samples
  contribution maps
  local-error maps
  analysis tables

Inspector:
  problem summary
  live result
  selected-cell microscope
  status and viewport notes
```

Rules:

```text
Controls backed by current engine behavior are marked live.
Controls that express the design but lack backend support are visible but
disabled or marked planned.
The UI must not claim a method/domain/partition works until the math kernel,
snapshots, render packets, and tests exist for it.
```

Next fill-in items:

```text
Darboux sampled bounds backend for the Bounds mode
draggable/nonuniform tag support for the Samples mode
adaptive refinement queue for the Error mode
second-window labels through TextOverlayService
domain creation for disk/annulus/triangle/polygon
```

Completion gate:

```text
The user can navigate the full first-screen workbench shape, use all live
features, and clearly see which future tools are planned. This is not required
for Use Case 1 or Use Case 2.
```

---

## Phase 8: Stretch Features After MVP

Only start these after the first 2D workbench and analytics path are usable.

```text
Disk and polygon domains
Boundary-clipped cells
Adaptive quadtree refinement
Monte Carlo and quasi-Monte Carlo
Fubini iterated integral view
Change-of-variables/Jacobian view
Surface area bridge
Arc length bridge using Integration1D
3D volume slices
```

---

## Fastest Implementation Order

Use this order unless a blocker appears:

```text
1. Integration2D.hpp/.cpp + tests
2. IntegrationWorkbenchState + tests
3. Primary ImGui workbench controls/inspector
4. Main viewport render packets
5. Hover/select mapping
6. Convergence series + method comparison
7. Alternate analytics view registration/rendering
8. Diagnostics polish
```

---

## Do Not Do Yet

To keep momentum:

```text
Do not build a generic service registry for all future math labs yet.
Do not implement arbitrary domains before rectangle domains are excellent.
Do not add 3D rendering before 2D selection/inspection works.
Do not make theorem/context metadata a full service until two labs need it.
Do not optimize SIMD/cache layout until the cell/result model is stable.
Do not remove the current 1D integration/derivative slice until its useful
parts are represented in the new workbench.
```

---

## Verification Cadence

After each phase:

```text
cmake --build cmake-build-debug --parallel 4
ctest --test-dir cmake-build-debug --output-on-failure
```

After major UI/rendering changes:

```text
Manual app smoke test:
  Ctrl+1 picker
  Ctrl+2 smoke test sim
  Ctrl+3 integration lab
  Ctrl+4 Taylor lab
  verify second window remains optional
```

After larger refactors:

```text
cmake --build cmake-build-tidy --target nurbs_dde --parallel 1
```

---

## MVP Definition

The MVP is complete when:

```text
Ctrl+3 opens the integration lab.
The primary screen shows a rectangle-domain 2D integral.
The user can choose integrand, method, and resolution.
The viewport shows cells, samples, and value/contribution/error map.
The user can hover/click cells.
The inspector shows selected-cell contribution data.
The result is computed by Integration2D core, not UI/render code.
The analytics path shows convergence and method comparison.
All tests pass.
```
