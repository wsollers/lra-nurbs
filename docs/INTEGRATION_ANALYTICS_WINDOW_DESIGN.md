# Integration Analytics Window Design

**Status:** design draft  
**Scope:** second-window integration analysis, result displays, and supporting graphs  
**Parent design:** `INTEGRATION_LAB_DESIGN.md`  
**Workbench companion:** `INTEGRATION_WORKBENCH_UI_DESIGN.md`

---

## Purpose

The second window is the integration lab's analysis surface. It should not be
another workbench and it should not duplicate the main spatial controls. Its job
is to answer:

```text
What did the current integration setup compute?
How trustworthy is it?
Where did the result come from?
How do methods and refinements compare?
What cell/sample/region is responsible for the current value or error?
```

When the workbench first opens with no active problem, the second window should
show an empty but structured analysis shell: empty graphs, empty tables, and
clear labels. This makes the lab feel ready without inventing fake data.

---

## C++ Engineering Standard

Implementation should follow the project standard:

- Modern C++20/23 and C++ Core Guidelines style.
- Rule of 0/3/5 as appropriate.
- Prefer project scalar and id types such as `f32`, `f64`, `u32`, `u64`,
  `ComponentId`, `RuntimeNodeId`, `ResourceId`, and `RenderViewId`.
- Use `std::expected`, `std::optional`, `std::variant`, `std::span`,
  `std::pmr`, `std::jthread`, and `std::stop_token` where they reduce
  ownership ambiguity, sentinel values, or blocking behavior.
- Keep mathematical result data, diagnostic data, renderable sample data, and
  metadata as separate outputs.
- Use project memory/arena abstractions for graph/sample buffers where
  appropriate.
- Do not put formulas, long text, paths, or UI strings on compact event paths.
  Use ids and service lookups. Human-readable text belongs in logger,
  diagnostics, metadata, or UI presentation.

---

## Role Split

```text
Main window:
  choose object/method/domain
  manipulate geometry
  inspect cells spatially

Second window:
  display result data
  display diagnostics
  display convergence and comparison graphs
  display selected-cell microscope
  display metadata and trace summaries
```

The second window consumes snapshots. It should not own the current mathematical
problem or modify the workbench state directly.

---

## Window Shape

Initial design:

```text
+----------------------------------------------------------------------------+
| Integration Analysis                                                        |
| active object | method | interval | N | status                              |
+-------------------------------+--------------------------------------------+
| Result Summary                 | Convergence Graph                          |
| estimate/reference/error       | estimate vs N / error vs N                 |
+-------------------------------+--------------------------------------------+
| Method Comparison              | Contribution / Error Distribution          |
| Left/Right/Mid/Trap/Simpson    | bars or histogram by cell                  |
+-------------------------------+--------------------------------------------+
| Selected Cell Microscope       | Metadata / Diagnostics / Trace             |
| interval, sample, f(sample),   | object id, method, measure, warnings,      |
| contribution, local flags      | evaluation count, assumptions              |
+-------------------------------+--------------------------------------------+
```

This can be implemented first as ImGui panels in the second window and later as
Vulkan text/plot overlays as the text rendering service matures.

---

## Empty Initial State

When the workbench is opened and no problem has been selected, the second window
must show stable placeholders instead of disappearing panels.

### Header

```text
Integration Analysis
Object: none
Method: none
Interval: none
N: none
Status: waiting for workbench selection
```

### Empty Result Summary

Rows:

```text
Estimate: -
Reference: -
Absolute error: -
Relative error: -
Evaluation count: -
Method order: -
```

### Empty Graphs

Each graph should render axes/frame/title with no plotted data:

```text
Convergence
Method comparison
Contribution distribution
Error estimate distribution
```

Graph empty-state labels should be short:

```text
no samples
no comparison
no selected cell
```

### Empty Selected Cell Microscope

Rows:

```text
Cell: -
Interval: -
Sample: -
f(sample): -
Width: -
Contribution: -
Flags: -
```

### Empty Diagnostics

Rows:

```text
Warnings: none
Domain status: no active domain
Method status: no active method
Trace: no computation
```

---

## Populated State

When the main workbench publishes an active snapshot, the second window fills in
the sections from snapshot data.

### Header Data

```text
object display name
object ComponentId
method name
interval/domain summary
partition count
measure element
snapshot generation
```

### Result Summary Data

```text
estimate
reference value when available
absolute error when available
relative error when available
evaluation count
cell count
method order/expected convergence
```

### Diagnostic Data

```text
domain validity
function domain restrictions
singularity proximity
finite/infinite result status
method compatibility
partition warnings
floating-point stability hints
```

### Metadata Data

```text
ComponentId
formula display string from metadata
default domain from metadata
capabilities
units/quantity kind when available
docs path when available
```

---

## Subgraphs

The analytics window should start with empty graph shells and fill them as data
appears.

### 1. Convergence Graph

Purpose:

```text
show estimate and/or error as N changes
```

Initial data series:

```text
estimate(N)
reference value line when available
absolute error(N) when available
```

Future data series:

```text
observed order
Richardson extrapolation
adaptive refinement error
```

### 2. Method Comparison Graph

Purpose:

```text
compare the current problem across supported methods at the same N
```

Initial bars:

```text
Left
Right
Midpoint
Trapezoid
Simpson
```

Fields:

```text
estimate
absolute error when reference is available
evaluation count
method compatibility flag
```

### 3. Contribution Distribution

Purpose:

```text
show which cells contribute most to the integral
```

For 1D:

```text
bar per interval cell
x-axis = cell index or interval midpoint
y-axis = contribution
selected cell highlighted
```

For later 2D:

```text
histogram of cell contributions
or small heatmap preview
```

### 4. Error / Refinement Distribution

Purpose:

```text
show where the current approximation is least trustworthy
```

Initial version:

```text
empty until an error estimator exists
```

Future version:

```text
local error estimate per cell
refinement priority
oscillation indicator
Darboux upper-lower gap
```

### 5. Selected Cell Microscope

Purpose:

```text
explain the currently hovered or selected cell
```

Fields:

```text
cell id
interval/domain region
sample coordinate
sample value
cell width/measure
contribution
local estimate flags
```

If no cell is selected:

```text
show empty rows
keep layout stable
```

---

## Snapshot Contract

The analytics window should consume a single immutable snapshot per update.

```cpp
struct IntegrationAnalyticsSnapshot {
    IntegrationProblemMetadata problem;
    IntegrationResultSummary result;
    std::span<const CellContribution> contributions;
    std::span<const MethodComparisonEntry> method_comparison;
    std::span<const ConvergencePoint> convergence;
    std::optional<CellMicroscopeData> selected_cell;
    IntegrationDiagnostics diagnostics;
};
```

Implementation may store vectors rather than spans in the owning snapshot type.
The key rule is that render/UI code receives one coherent version of the world.

---

## Use Cases

### Use Case 1: Workbench Opened

If no problem is active:

```text
second window opens as Integration Analysis
all sections visible
all values empty
graphs show axes/frames only
status says waiting for workbench selection
no estimate is displayed
```

If the workbench auto-selects the default Runge function:

```text
second window opens populated
header shows Runge function metadata
result summary shows current estimate
convergence graph may show one point
method comparison shows available method estimates if computed
selected cell microscope remains empty until hover/click
```

### Use Case 2: Curve Selected

Trigger:

```text
main window publishes a snapshot after equation selection
```

Expected second-window result:

```text
header updates object/method/interval/N
result summary updates estimate/reference/error
convergence graph resets or appends according to policy
method comparison refreshes
contribution distribution draws one bar per cell
diagnostics show domain and method status
```

### Use Case 3: Cell Selected

Trigger:

```text
main window hover/click identifies a cell
```

Expected result:

```text
selected cell microscope fills in
contribution graph highlights the same cell
main viewport highlights the same cell
snapshot generation increments
```

### Use Case 4: Method Or N Changed

Trigger:

```text
main top bar changes method or partition count
```

Expected result:

```text
result summary updates
convergence graph appends or refreshes
method comparison refreshes
contribution distribution rebuilds
selected cell clears if invalid
diagnostics update method compatibility
```

---

## Acceptance Criteria

- The second window always has a stable analysis layout when the lab is open.
- Empty state shows empty graph frames and tables, not fake data.
- Populated state is driven by the same snapshot that drives the main viewport.
- The analytics window does not mutate the active workbench problem.
- Graph shells exist for convergence, method comparison, contribution
  distribution, and future error/refinement distribution.
- Selected-cell microscope remains visible in empty and populated states.
- Focused tests cover empty snapshot, populated snapshot, and selected-cell
  snapshot behavior.

