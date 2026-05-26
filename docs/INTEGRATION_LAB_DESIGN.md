# Integration and Approximation Lab Design

**Status:** design draft  
**Scope:** 2D-first integration, approximation, diagnostics, and UI workbenches  
**Parent architecture:** `Integration-Nurbs-Splines.md`  
**Focused UI designs:**

```text
docs/INTEGRATION_WORKBENCH_UI_DESIGN.md
docs/INTEGRATION_ANALYTICS_WINDOW_DESIGN.md
```

**Prototype references:**

```text
docs/integration_observatory_clickable_prototype.jsx
docs/other prototype.jsx
docs/new-prototype.jsx
```

---

## Purpose

The Integration and Approximation Lab is an interactive mathematical workbench
for learning how integrals are constructed, approximated, diagnosed, and refined.

The first serious implementation target is 2D scalar integration:

```text
Integral form:  integral_D f(x, y) dA
Initial domains: rectangles, then disks, polygons, and regions under/between curves
Initial partitions: uniform grids, then adaptive quadtrees and triangular meshes
Initial methods: midpoint grid, tensor-product trapezoid/Simpson, Darboux-style bounds
Initial diagnostics: contribution map, error map, convergence trace, selected-cell inspector
```

The design must stay compatible with later 3D volume integration, surface
integration, curve arc length, NURBS surface area, and manifold-style measure
elements. The UI should start in 2D, but its vocabulary must not be trapped in
2D-only words like "rectangles under a curve."

---

## C++ Engineering Standard

Implementation should follow the project standard:

- Modern C++20/23 and C++ Core Guidelines style.
- Rule of 0/3/5 as appropriate.
- Prefer project scalar and id types such as `f32`, `f64`, `u32`, `u64`,
  `ComponentId`, `RuntimeNodeId`, and `ResourceId`.
- Use `std::expected`, `std::optional`, `std::variant`, `std::span`,
  `std::pmr`, `std::jthread`, and `std::stop_token` where they reduce
  ownership ambiguity, sentinel values, or blocking behavior.
- Keep numerical result data, diagnostic data, renderable sample data, and
  metadata as separate outputs.
- Avoid raw heap ownership in hot paths. Prefer project memory and arena
  abstractions where appropriate.
- Do not put human-readable diagnostic text on compact event buses. Use ids,
  flags, and service lookups; text belongs in logger/diagnostic presentation.

---

## Prototype Synthesis

The three JSX prototypes point at three useful design layers.

`integration_observatory_clickable_prototype.jsx` is the strongest source for
the integration-specific interaction model. It has clear workbench modes:

```text
Workbench
Darboux
Analysis
2D Preview
Arc Length
```

The useful ideas are selected-cell inspection, live estimates, Darboux upper and
lower views, convergence tables, method comparison, and explicit links from 1D
integration to arc length and later surface work.

`other prototype.jsx` is the strongest source for the overall observatory shell.
It shows how Taylor approximation and integration can share navigation, panels,
status bars, result traces, diagnostics, and theorem/context panes without
becoming unrelated mini-apps.

`new-prototype.jsx` is the strongest source for the instrument-console feel. It
is denser and more audit-oriented, with Darboux, Fubini 2D, and refinement-audit
tabs. The useful ideas are global controls, a large primary viewport, a persistent
right analysis panel, and a footer that summarizes active numerical state.

The preferred real UI should combine these:

```text
Observatory shell from other prototype
Integration-specific modes from integration_observatory_clickable_prototype
Dense audit/readout posture from new-prototype
```

---

## Core Design Rule

Every lab feature must produce four outputs:

```text
1. Mathematical result data
2. Diagnostic data
3. Renderable sample data
4. Metadata explaining method, domain, assumptions, units, and error
```

For example, a 2D midpoint integration pass should produce:

```text
Result:
  estimate
  optional reference value
  absolute/relative error when available

Diagnostics:
  convergence estimate
  local error estimates
  stability flags
  singularity/discontinuity flags

Renderable samples:
  domain boundary
  partition cells
  sample points
  contribution values
  heatmap/error-map values

Metadata:
  domain id
  integrand id
  measure element
  partition method
  approximation method
  tolerance
  evaluation count
  units
```

This prevents the lab from becoming either a black-box calculator or a picture
that cannot be inspected numerically.

---

## UI Shape

The lab should be a dense, restrained scientific instrument. The main window is
the integration workbench itself. Controls are part of the workbench chrome
around the rendered integral, not a free-floating settings dialog.

```text
+-----------------------------------------------------------------------+
| Top Control Strip                                                     |
| Domain | Integrand | Measure | Partition | Method | Tolerance | Views |
+-------------------------------+---------------------------------------+
| Primary Geometry Viewport      | Analysis / Inspector Pane             |
|                               |                                       |
| Domain, cells, samples,       | Estimate, error, selected cell,       |
| value map, contribution map,  | method comparison, diagnostics,       |
| error map, boundaries         | theorem context, result trace         |
|                               |                                       |
+-------------------------------+---------------------------------------+
| Status / Trace Strip                                                  |
| estimate | error | cells | flags | active method | hover selection     |
+-----------------------------------------------------------------------+
```

The first screen should be the usable workbench, not a landing page. The UI
should default to a working 2D integral with a visible domain, visible partition,
and a live estimate.

The two-window mental model is:

```text
Main window:
  Workbench
  construct/manipulate the active integral
  integrated toolbar, tool tray, viewport, inspector, and status strip

Second window:
  Results and displays
  convergence, comparison, diagnostic plots, selected-cell microscope,
  result traces, and supporting graphs
```

The main window may use ImGui while the renderer/text systems mature, but those
widgets should be anchored as workbench chrome. They should not appear as
movable floating panels over an unrelated viewport.

---

## Primary Screen: Integration Workbench

The main app window should be the first screen of the integration lab. It is the
place where the user constructs and manipulates the integral. It should answer:

```text
What am I integrating?
Over what domain?
With what measure?
How is the domain partitioned?
Where are the samples?
What is each cell contributing?
```

The primary screen should feel like a working bench with one large visual target
and a compact inspector. It should not try to show every convergence/audit panel
at once; those belong in the second analytics window.

### Primary Screen Layout

```text
+--------------------------------------------------------------------------------+
| Integration Lab                                                                |
| Domain [Rectangle]  Integrand [Gaussian]  Measure [dA]  Method [Midpoint]       |
| Partition [Uniform Grid]  Cells [32 x 32]  View [Contribution]  Tolerance [...] |
+----------------------------------------------+---------------------------------+
|                                              | Problem                         |
| Primary Domain Viewport                      |   integral_D f(x,y) dA          |
|                                              |   D = [-2,2] x [-2,2]           |
|  - domain boundary                           |                                 |
|  - partition cells                           | Result                          |
|  - sample points                             |   estimate                      |
|  - selected cell                             |   reference/error if known      |
|  - value/contribution/error map              |                                 |
|                                              | Selected Cell                   |
|                                              |   id, region, sample, measure   |
|                                              |   value, contribution, flags    |
+----------------------------------------------+---------------------------------+
| estimate | error | cells | selected cell | active map | flags                    |
+--------------------------------------------------------------------------------+
```

Recommended split:

```text
Main viewport: 65-75% of width
Inspector pane: 25-35% of width
Status strip: one line at bottom
```

The top strip is for fast switching, not deep configuration. When a setting
needs multiple controls, open a small focused section in the right inspector.

### Main Window Workbench Chrome

The main window should be composed as anchored UI around the Vulkan workbench
view:

```text
Top bar:
  lab title
  object zoo selection populated from metadata
  active method
  partition resolution
  display field
  workbench mode selector

Left tool tray:
  mode-specific tools for domain, partition, samples, contribution, error,
  bounds, or analysis setup

Center:
  Vulkan-rendered domain, cells, samples, heatmaps, selected cell, and later
  Vulkan text labels/legends

Right inspector:
  problem summary
  live result
  selected-cell details
  compact metadata and flags

Bottom strip:
  estimate
  error
  cell count
  selected/hovered cell
  active mode
  diagnostic flags
```

This keeps the user’s attention on the integral while still making the controls
and readouts feel like part of the instrument.

### Object Zoo Selector

The first top-bar control should select the mathematical object being studied.
This is broader than an integrand picker.

```text
Object Zoo:
  functions
  scalar fields
  surfaces
  manifolds / charts
  later: NURBS curves and surfaces
```

The selector must be populated from `SimMetadataService` descriptors, not from a
hardcoded UI-only list. The metadata entry should describe:

```text
display name
category
capabilities
assumptions
trust level
documentation reference
whether a live backend/factory is available
```

The first live objects may still map onto current `IntegrandPreset2D` values,
but the UI vocabulary should remain object-oriented:

```text
1D equation objects:
  y = 1
  y = x
  y = exp(x)
  y = ln(x)
  y = 1 / x

2D scalar-field objects:
Gaussian scalar field
Wave scalar field
Polynomial scalar field
Step discontinuity field

Planned geometric/manifold objects:
Plane surface patch       planned
Torus manifold chart      planned
```

Planned zoo entries may be visible but disabled or marked planned. They should
not pretend to run until the math adapter, renderable sample data, diagnostics,
metadata, and tests exist.

### Primary Screen Modes

The first screen should use a small mode selector. These are not separate apps;
they are view/interaction modes over the same `IntegrationProblem`.

```text
Domain
Partition
Samples
Contribution
Error
Bounds
```

Initial mode behavior:

```text
Domain:
  Shows domain boundary, axes, coordinate readout, and domain parameters.

Partition:
  Emphasizes cells, mesh size, selected-cell details, and boundary clipping.

Samples:
  Emphasizes sample points/tags and the value f(sample).

Contribution:
  Colors cells by signed or absolute contribution f(sample) * measure(cell).

Error:
  Colors cells by local error estimate or refinement indicator.

Bounds:
  Shows Darboux-style lower/upper estimates where available. For 2D MVP this
  can be sampled local min/max, clearly labeled as sampled bounds rather than
  exact supremum/infimum.
```

### Primary Viewport Interaction

The viewport should support:

```text
Hover cell:
  update status strip and preview cell details

Click cell:
  pin selected cell in the inspector

Drag domain handles:
  later, edit rectangle/disk/polygon boundaries

Mouse wheel:
  zoom within the 2D orthographic domain

Right/middle drag:
  pan/orbit according to current projection mode

Double click:
  refine selected cell or center probe, depending on active mode
```

For the first implementation, hover/select is enough. Domain editing and
interactive refinement can come after the data model is stable.

### Primary Inspector Sections

The right inspector should be compact and stable. Prefer collapsible sections
over opening many floating panels.

Initial sections:

```text
Problem
  domain
  integrand
  measure
  partition
  approximation method

Live Result
  estimate
  reference value when known
  absolute error
  relative error
  evaluation count

Selected Cell
  cell id
  region/bounds
  sample coordinate
  measure
  f(sample)
  contribution
  local error estimate
  diagnostic flags

Display Layers
  domain boundary
  cells
  sample points
  grid axes
  value map
  contribution map
  error map
```

Later sections:

```text
Adaptive Refinement
Boundary Approximation
Coordinate Transform / Jacobian
Method Parameters
Units and Metadata
```

### Primary Screen Data Contract

The primary screen consumes one immutable snapshot per update:

```text
IntegrationWorkbenchSnapshot
  IntegrationProblemSummary
  IntegrationResultSummary
  SelectedCellSummary
  RenderableIntegrationSampleSet
  DiagnosticFieldSummary
  MetadataSummary
```

The viewport should not recompute the integral. It renders sample/cell data
already produced by the integration pipeline. ImGui controls may request a new
problem/config, but the math work should live behind a workbench/service layer.

### First-Screen Acceptance Criteria

The first screen is acceptable when:

```text
The lab opens directly into a visible 2D integral.
The user can choose a simple integrand and grid resolution.
The user can switch value/contribution/error display modes.
The user can hover or click a cell.
The right inspector updates from the selected cell.
The bottom strip shows estimate, error, cell count, and flags.
The integration result is not computed inside rendering code.
The screen can run without the second analytics window.
```

---

## Second Window: Analytics View

The second window should be the analytical companion to the main workbench. The
main screen is for constructing and inspecting one active integral; the second
window is for asking whether the computation is trustworthy.

The second window should not duplicate the main workbench controls. Its job is
to show results and supporting displays. Think of it as the analysis wall beside
the workbench.

Current engine terminology maps naturally:

```text
Primary window:
  RenderViewKind::Main
  active integration workbench viewport

Second window:
  RenderViewKind::Alternate
  analytics / convergence / diagnostic display
```

The second window should never be required for correctness. If it is closed, the
main screen remains usable. When present, it shows richer traces and comparison
views.

### Analytics Window Layout

```text
+--------------------------------------------------------------------------------+
| Integration Analytics                                                          |
| Active problem | Method | Refinement family | Sync: selected cell / resolution  |
+--------------------------------------+-----------------------------------------+
| Convergence Plot                     | Method Comparison                       |
|  log(error) vs log(h)                 |  midpoint / trapezoid / Simpson / ...   |
|  observed order                       |  estimate, error, evals, order          |
+--------------------------------------+-----------------------------------------+
| Error / Contribution Diagnostic Map   | Selected Cell Microscope                |
|  same domain, analytics coloring      |  local samples, bounds, flags           |
+--------------------------------------+-----------------------------------------+
| Result Trace / Audit Trail                                                     |
|  step, method, cells, estimate, error, elapsed, message                         |
+--------------------------------------------------------------------------------+
```

The second window can use dense tables and plots because it is not competing
with the manipulation viewport.

### Analytics Window Panes

Initial panes:

```text
Convergence Trace
  rows for n, h, estimate, error, observed order
  supports several grid resolutions

Log Error Plot
  log|error| vs log(h)
  expected slope reference line when method order is known

Method Comparison
  method
  estimate
  absolute error
  relative error
  evaluation count
  expected order

Diagnostic Map
  same selected domain
  colors by contribution, local error, oscillation, or refinement priority

Selected Cell Microscope
  selected cell inherited from primary screen
  local sample values
  local min/max estimate
  local contribution
  local diagnostic flags

Result Trace
  one row per recomputation/refinement step
  method/domain/partition metadata
  elapsed CPU time once metrics are wired in
```

Later panes:

```text
Stability Perturbation Study
Adaptive Refinement Tree
Monte Carlo Variance Plot
Fubini Iterated Integral Comparison
Change-of-Variables Jacobian View
Surface/Volume Slice Diagnostics
```

### Analytics Synchronization Rules

The analytics window follows the main workbench.

```text
Main changes integrand/domain/method:
  analytics recomputes or marks previous traces stale

Main changes selected cell:
  selected-cell microscope updates

Main changes display map:
  analytics may mirror it, but does not have to

Analytics changes refinement resolution:
  may request a new active resolution only through explicit user action
```

Avoid hidden feedback loops. The analytics window should not silently mutate the
main workbench just because the user is browsing a comparison row.

### Analytics Data Contract

The analytics window consumes:

```text
IntegrationAnalysisSnapshot
  active_problem_id
  active_result_id
  convergence_series
  method_comparison_rows
  diagnostic_fields
  selected_cell_report
  result_trace_rows
  metadata
```

This is intentionally separate from the main workbench snapshot. The main screen
needs low-latency interaction data; the analytics screen can show slower,
heavier numerical studies that may be computed on a worker thread later.

### Second-Window Acceptance Criteria

The analytics view is acceptable when:

```text
It opens in the existing second Vulkan window path.
It shows convergence over at least four grid resolutions.
It compares at least two methods.
It shows a diagnostic map derived from the same active problem.
It follows the selected cell from the main screen.
It has a result trace table.
Closing the second window does not break the primary workbench.
```

---

## Main Workbenches

### Domain Workbench

Purpose:

```text
Choose and inspect D.
```

Initial domain types:

```text
Rectangle
Disk
Annulus
Triangle
Polygon
Region under curve
Region between curves
```

Later domain types:

```text
Surface patch parameter domain
Trimmed NURBS domain
Voxel region
Implicit level-set region
Volume between surfaces
```

The viewport should show domain boundary, active coordinate system, and whether
the selected partition respects the boundary exactly or approximates it.

### Partition Workbench

Purpose:

```text
Make the approximation geometry inspectable.
```

Initial partition types:

```text
Uniform 2D grid
Nonuniform grid
Adaptive quadtree
Simple triangular mesh
Monte Carlo samples
```

Each cell should be selectable. A selected cell should expose:

```text
cell id
domain region
sample point
cell measure
f(sample)
contribution
local error estimate
diagnostic flags
```

### Approximation Workbench

Purpose:

```text
Compare methods and make convergence visible.
```

Initial methods:

```text
Midpoint grid
Tensor-product trapezoid
Tensor-product Simpson
Darboux-style lower/upper estimates
Monte Carlo area integration
```

Later methods:

```text
Gaussian quadrature
Adaptive Simpson
Adaptive quadtree quadrature
Triangular mesh quadrature
Quasi-Monte Carlo
Surface patch quadrature
Volume integration
```

### Analysis Workbench Summary

Purpose:

```text
Explain whether the answer is trustworthy.
```

Most of this belongs in the second analytics window. The primary screen should
show only the compact live result, selected-cell inspector, and immediate flags.

Analysis panes should include:

```text
Convergence trace
Method comparison
Error map
Contribution leaderboard
Selected-cell microscope
Stability score
Result trace / audit trail
Theorem/context pane
```

The theorem/context pane should react to the active mode. It should explain why
the current numerical situation matters, not act as a detached textbook page.

Examples:

```text
Uniform rectangle domain -> Fubini theorem
Coordinate transform -> change of variables / Jacobian
Darboux mode -> Darboux integrability criterion
Adaptive mode -> local oscillation and refinement
Surface mode -> dS = ||S_u x S_v|| du dv
```

---

## 2D-First, 3D-Compatible Vocabulary

Use dimension-generic names in the model and UI:

```text
Domain
Cell
Sample
Measure
Contribution
Partition
Approximation method
Diagnostic field
Renderable sample set
```

Avoid baking these terms into core APIs:

```text
rectangle
pixel
square
heightmap-only
x-y-only
```

Concrete 2D implementations can use rectangles and grids, but the service
interfaces should allow future mappings:

```text
2D:
  Cell -> rectangle, triangle, polygon fragment
  Measure -> dA

3D:
  Cell -> voxel, tetrahedron, clipped volume cell
  Measure -> dV

Surface:
  Cell -> parameter patch
  Measure -> dS

Curve:
  Cell -> parameter interval
  Measure -> ds
```

---

## View Layers

The primary viewport should support composable layers:

```text
Domain boundary
Partition cells
Sample points
Scalar value heatmap
Contribution heatmap
Error heatmap
Refinement tree
Selected cell outline
Coordinate axes
Boundary approximation marks
Singularity/discontinuity warnings
```

For 3D later, these become:

```text
Volume slices
Isosurfaces
Voxel/tetrahedral cell outlines
Surface patches
Section planes
Sample clouds
Contribution/error color fields
```

The design should not require a separate "3D integration app." It should require
a different viewport mode and additional domain/cell/render adapters.

---

## Initial MVP

The first complete 2D lab slice should support:

```text
Rectangle domain [-a,a] x [-b,b]
Scalar integrand library:
  gaussian
  sin/cos wave
  polynomial
  step or discontinuous region
Uniform grid partition
Midpoint 2D integration
Tensor-product trapezoid comparison
Value map
Contribution map
Basic local error map
Hover/select cell inspector
Convergence trace over multiple resolutions
Result metadata panel
```

Acceptance criteria:

```text
User can select an integrand and resolution.
User can see the domain and all cells.
User can hover/select a cell and inspect its contribution.
User can toggle value, contribution, and error views.
User can compare at least two methods.
User can view convergence across refinement levels.
Result data, diagnostic data, renderable sample data, and metadata are available separately.
The model uses dimension-generic domain/cell/sample/measure vocabulary.
```

---

## Open Design Decisions

- Should Darboux 1D remain a separate mode, or should it be a formal-analysis
  overlay inside the broader integration workbench?
- Should the first 2D Darboux-style mode use sampled local min/max per cell, or
  wait until we have stronger interval/optimization support?
- Should theorem/context panes live in the Integration Lab implementation, or
  should they be a shared `ConceptConnection`/metadata service used by Taylor,
  integration, splines, and NURBS?
- Should Monte Carlo appear in the MVP, or wait until deterministic methods and
  convergence views are solid?
- Should the first render path for 2D integration use ImGui draw lists, engine
  render packets, or both?

---

## Recommended Next Step

Start implementation with the following thin vertical slice:

```text
Integration2D core model
UniformGrid2D partition
Midpoint2D approximation
Integration2DResult package
RenderableIntegration2DSnapshot package
ImGui Analysis pane
2D viewport render packet adapter
Focused tests for partitioning, contribution sums, and convergence
```

This gives the learning lab a real spine without requiring the whole future
observatory to exist at once.
