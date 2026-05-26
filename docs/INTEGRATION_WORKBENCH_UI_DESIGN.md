# Integration Workbench UI Design

**Status:** design draft  
**Scope:** main-window integration workbench UI  
**Parent design:** `INTEGRATION_LAB_DESIGN.md`  
**Analytics companion:** `INTEGRATION_ANALYTICS_WINDOW_DESIGN.md`

---

## Purpose

The integration workbench is the primary spatial viewport for constructing and
manipulating an integration problem. It is where the user chooses the
mathematical object, interval/domain, partition, approximation method, and
visual inspection target.

The first milestone is intentionally narrow:

```text
1D curve/function integration
single primary viewport
integrated top command bar
no floating workbench panels
empty second-window analytics until a problem/result exists
```

The UI should feel like a dense scientific instrument, not a landing page and
not a detached settings dialog. Controls belong in fixed workbench chrome around
the spatial viewport.

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
- Use project memory/arena abstractions for hot render/sample buffers where
  appropriate.
- Do not put formulas, long text, paths, or UI strings on compact event paths.
  Use ids and service lookups. Human-readable text belongs in logger,
  diagnostics, metadata, or UI presentation.

---

## Design Rule

Every visible workbench feature must be backed by four data products:

```text
Mathematical result data
Diagnostic data
Renderable sample data
Metadata explaining method, domain, assumptions, units, and error
```

For example, selecting `1 / (1 + 25x^2)` must produce:

```text
Result:
  current estimate
  optional reference value
  per-cell contributions

Diagnostics:
  finite-domain validation
  singularity/domain warnings
  method compatibility flags

Renderable samples:
  curve polyline
  partition cell rectangles
  sample points
  selected/hovered cell highlight

Metadata:
  ComponentId for the selected function
  display formula
  default interval
  supported measures and methods
```

---

## Main Window Shape

The main window owns the spatial act of integration.

```text
+----------------------------------------------------------------------------+
| Top Command Bar                                                             |
| brand | Equation | Method | N | Interval | optional status/action cluster    |
+----------------------------------------------------------------------------+
|                                                                            |
| Screen 1 - Primary Spatial Viewport                                         |
|                                                                            |
|  graph/domain grid                                                          |
|  curve/surface/manifold sample                                              |
|  partition cells                                                            |
|  sample points                                                              |
|  selected cell guides                                                       |
|  compact domain labels                                                      |
|                                                                            |
+----------------------------------------------------------------------------+
```

The first version should not show:

```text
left side drawer
right inspector pane
bottom trace strip
mode tabs
floating panel controls
second-window content inside the main viewport
```

Those may return later only when the workflow requires them.

---

## Top Command Bar

The top bar is the only main-window control surface for the first use cases.

### Required Controls

| Control | Type | Source | Behavior |
|---|---|---|---|
| Brand | fixed label/icon | app chrome | identifies `nurbs_dde` and `Integration Observatory` |
| Equation | dropdown | `SimMetadataService` descriptors | selects a function/curve/surface/manifold from the zoo |
| Method | dropdown | supported method enum | selects approximation method |
| N | slider/stepper | workbench config | controls partition count for 1D use case |
| Interval Min | numeric input | workbench config | lower parameter/domain bound |
| Interval Max | numeric input | workbench config | upper parameter/domain bound |

### Initial Equation Zoo

The first dropdown should include at least:

```text
y = 1
y = x
y = exp(x)
y = ln(x)
y = 1 / x
y = sin(x)
y = x^2
y = x^3 - x
y = 1 / (1 + 25x^2)
```

The list must be populated from metadata, not a UI-local string table. The UI
may map metadata ids to currently implemented evaluators, but the visible zoo is
metadata-driven.

### Initial Methods

```text
Left
Right
Midpoint
Trapezoid
Simpson
```

Method choices should be filtered or flagged by compatibility. For example,
Simpson requires a compatible partition count. The first version may clamp or
adjust `N`, but the UI should eventually explain the adjustment through
diagnostics.

### Initial Defaults

```text
Equation: 1 / (1 + 25x^2)
Method: Right
N: 16
Interval: [-2, 2]
Viewport: padded to show context outside the interval
```

---

## Primary Spatial Viewport

The viewport is the main teaching surface. For the 1D integration milestone it
shows a graph and its partition geometry.

### Required Rendered Elements

```text
plot frame
grid lines
x-axis when visible
selected function curve
partition cell regions
sample points
active/hovered/selected cell guide lines
domain interval label
viewport padding label
```

### Empty State

If no equation has been selected yet:

```text
show grid
show neutral empty axis/frame
show no fake estimate
show subdued text: select an equation
send an empty analytics snapshot to the second window
```

The first currently implemented default may auto-select the Runge function, but
the design must support a true empty state for future picker/workspace flows.

### Selected Curve State

Once a curve/function is selected:

```text
render the function over the viewport padded interval
render partition cells over the active integration interval
render one sample point per cell according to method
highlight hover/selected cell when interaction is enabled
publish an analytics snapshot for the second window
```

### Viewport Interaction

The first interaction layer should be modest:

```text
hover cell
click cell to select
drag interval endpoints later
drag partition points later
pan/zoom later
```

The first use case can leave hover/click inactive as long as the visual state is
clear and the tests assert that the app remains runnable.

---

## Data Elements Owned By The Workbench

The workbench state should be explicit and testable.

```cpp
struct IntegrationWorkbenchUiState {
    ComponentId selected_object;
    QuadratureMethod method;
    u32 partition_count;
    f64 interval_min;
    f64 interval_max;
    std::optional<u32> hovered_cell;
    std::optional<u32> selected_cell;
};
```

The UI state should not own numerical truth. It feeds a problem builder and then
receives snapshots.

```cpp
struct IntegrationWorkbenchSnapshot {
    IntegrationProblemMetadata metadata;
    IntegrationResult1D result;
    IntegrationDiagnostics diagnostics;
    IntegrationRenderableSamples samples;
};
```

---

## Data Flow

```text
top bar change
-> update workbench UI state
-> build IntegrationProblem
-> compute or request current approximation
-> produce renderable sample packet
-> submit main-window render packets
-> publish analytics snapshot for second window
```

The main window should never recompute plots independently from the data that
feeds analytics. Both windows should consume the same result/snapshot family.

---

## Use Cases

### Use Case 1: Workbench Opened

Initial visible state:

```text
top bar visible
equation dropdown visible
method dropdown visible
N control visible
interval controls visible
primary viewport visible
no side trays or inspector panels
second window shows empty analytics shell
```

If using an auto-selected default:

```text
Runge function selected
Right method selected
N = 16
interval = [-2, 2]
curve, cells, and sample points visible
analytics receives a populated snapshot
```

If using a true empty workspace:

```text
no selected object
grid/frame visible
no cells
no result estimate
analytics receives an empty snapshot
```

### Use Case 2: Curve Selected

Trigger:

```text
user opens Equation dropdown
user selects a metadata-backed function/curve
```

Expected result:

```text
selected ComponentId changes
default interval may be applied from metadata
current method compatibility is checked
problem is rebuilt
result is recomputed
curve and partition samples are regenerated
main viewport updates
analytics window transitions from empty shell to populated result displays
```

### Use Case 3: Method Changed

Trigger:

```text
user selects Left, Right, Midpoint, Trapezoid, or Simpson
```

Expected result:

```text
sample point placement changes
cell contribution geometry updates
estimate changes
method metadata updates
analytics method badge and result fields update
```

### Use Case 4: Partition Count Changed

Trigger:

```text
user adjusts N
```

Expected result:

```text
partition cells rebuild
sample points rebuild
estimate recomputes
convergence graph in second window appends or refreshes
hover/selected cell ids are cleared if invalid
```

### Use Case 5: Interval Changed

Trigger:

```text
user edits interval min or max
```

Expected result:

```text
domain validates min < max
function domain compatibility is checked
viewport padding updates
partition rebuilds
result recomputes
analytics domain row updates
diagnostics reports invalid or singular intervals
```

---

## Acceptance Criteria

- The workbench opens without floating panels.
- The top command bar contains the required controls.
- The Equation dropdown is metadata-backed.
- Selecting a curve/function updates the rendered curve and result snapshot.
- `N`, method, and interval changes rebuild the partition.
- Main-window render data and second-window analytics data come from the same
  snapshot.
- The app remains runnable at every implementation step.
- Focused tests cover default open state, curve selection, method changes, and
  partition count changes.

