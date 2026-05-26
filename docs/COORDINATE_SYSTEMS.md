# Coordinate Systems — NDDE Engine

**Repository:** `F:\repos\Learning-Real-Analysis\nurbs_dde`
**Last updated:** 2026-04-13

---

## Overview

The engine operates in coordinate spaces that are transformed through a
strict pipeline. Confusing any two of these is the root cause of every
cursor/snap/rendering offset bug documented in this project. This document
is the single authoritative reference.

```
  Input device         ImGui/OS           GPU              Analysis
  ───────────          ────────           ───              ────────
  Physical px   ──▶   Logical px   ──▶  World   ──▶   Snap / Geometry
  (hardware)     DPI   (MousePos)   MVP  (R² / R³) cache   (Curves)
```

In **3D mode** the same pipeline applies, but world space becomes R³ and the
snap search projects 3D cache points to screen space before measuring distance.

---

## The Three Spaces

### 1. Framebuffer Pixels (GPU)

- **Source:** `EngineAPI::viewport_size()` → `m_vp.fb_w / fb_h`
- **Used for:** Vulkan viewport dimensions, MVP aspect ratio
- **Convention:** origin at top-left, X right, Y **down** (Vulkan default),
  but corrected to Y-up by the negative-height viewport trick (see below)

### 2. Logical / Display Pixels (ImGui / OS)

- **Source:** `ImGui::GetIO().DisplaySize` → `m_vp.dp_w / dp_h`
- **Same space as:** `ImGui::GetIO().MousePos`, all drag deltas, all UI input
- **Used for:** `pixel_to_world()`, `world_to_pixel()`, pan/zoom input,
  ImGui label positioning
- **Sentinel:** when the mouse is outside the window, ImGui sets `MousePos`
  to `(-FLT_MAX, -FLT_MAX)`. Guard with `Viewport::mouse_valid(lx, ly)`
  before calling any coordinate function.

> **On a standard 100% DPI display:** `fb_w == dp_w` and `fb_h == dp_h`.
> **On HiDPI / Windows scaled displays:** `fb_w > dp_w` by the DPI scale
> factor, but the aspect ratios are identical.

### 3. World Space (R² in 2D, R³ in 3D)

- **Origin:** `(pan_x, pan_y)` is the world point at the screen centre
- **X-axis:** right is positive
- **Y-axis:** up is positive (right-handed)
- **Z-axis:** toward viewer (2D: always 0; 3D: used by the Helix)
- **Bounds (2D):**
  ```
  half_h  = base_extent / zoom
  half_w  = half_h * (fb_w / fb_h)
  left    = pan_x - half_w       right  = pan_x + half_w
  bottom  = pan_y - half_h       top    = pan_y + half_h
  ```
- All curve geometry, snap caches, epsilon balls/spheres, interval lines,
  and Frenet frame arrows live in world space.

---

## The Single Source of Truth: `app/Viewport.hpp`

`struct Viewport` owns all coordinate math. **No other file may contain
coordinate arithmetic.** Scene holds one `Viewport m_vp`. Every frame,
`Scene::sync_viewport()` is called first to populate both pixel spaces.

```cpp
void Scene::sync_viewport() {
    const Vec2 fb = m_api.viewport_size();
    m_vp.fb_w = fb.x;  m_vp.fb_h = fb.y;   // GPU pixels

    const ImGuiIO& io = ImGui::GetIO();
    m_vp.dp_w = io.DisplaySize.x;            // logical pixels
    m_vp.dp_h = io.DisplaySize.y;
    m_vp.base_extent = m_axes_cfg.extent * 1.2f;
}
```

### Key transforms

| Function | Input | Output | Notes |
|---|---|---|---|
| `pixel_to_world(lx, ly)` | logical px | world | guards mouse_valid; returns (0,0) for sentinel |
| `world_to_pixel(wx, wy)` | world | logical px | exact inverse |
| `ortho_mvp()` | — | Mat4 | 2D orthographic, uses fb_aspect |
| `perspective_mvp()` | — | Mat4 | 3D orbit camera |
| `zoom_toward(lx, ly, t)` | logical px, ticks | — | anchor-zoom; guards sentinel |
| `pan_by_pixels(dpx, dpy)` | logical px delta | — | 2D pan |
| `orbit(dpx, dpy)` | logical px delta | — | 3D yaw/pitch orbit |
| `mouse_valid(lx, ly)` | logical px | bool | `|x|, |y| < 1e15` — rejects sentinel |

### `pixel_to_world` derivation

```
nx = lx / dp_w                           // fraction [0,1] across display
ny = ly / dp_h                           // fraction [0,1] down display
wx = left()  + nx * (right()  - left())
wy = top()   + ny * (bottom() - top())   // pixel Y down → world Y up
```

`bottom() - top()` is negative (top > bottom), so increasing `ny` (cursor
moving down) gives decreasing `wy`. This is the correct Y-flip.

### `world_to_pixel` derivation (exact inverse)

```
px = ((wx - left())  / (right()  - left())) * dp_w
py = ((wy - top())   / (bottom() - top()))  * dp_h
```

Round-trip error = 0.000 px (verified by `CoordDebugPanel`).

---

## The Vulkan Y-Flip (Critical)

Vulkan's default NDC convention has **Y increasing downward** — the
opposite of OpenGL and `glm::ortho`. Without correction, `glm::ortho(L, R, B, T)`
maps world Y=+T to screen bottom and world Y=B to screen top.

**Fix:** The negative-height `VkViewport` trick (`Renderer.cpp`):

```cpp
VkViewport{
    .y      =  h,   // start at pixel row h (screen bottom)
    .height = -h,   // negative: rasterise upward, flipping Y
};
```

After this fix:
```
NDC Y = +1  →  screen top     (world top)
NDC Y = -1  →  screen bottom  (world bottom)
```

`pixel_to_world` (pixel top → world top) is now consistent with rendering.
This fix resolved the epsilon-ball tracking inversion and is the root cause
of every Y-related visual bug we encountered.

---

## Coordinate Flow Per Feature

### 2D Mouse → Snap

```
ImGui::GetIO().MousePos  (logical px)
  │  Viewport::pixel_to_world()
World (wx, wy)
  │  Distance search over snap_cache[] (x, y pairs)
Nearest cache point within snap_r_world
  │  HoverResult::world_x, world_y, snap_t
```

`snap_r_world` converts the screen-pixel threshold to world units:
```cpp
const float world_per_px = (m_vp.top() - m_vp.bottom()) / m_vp.dp_h;
const float snap_r_world = m_analysis_panel.get_snap_px_radius() * world_per_px;
```

This keeps the snap threshold constant in screen pixels regardless of zoom.

### 3D Mouse → Snap (Helix)

```
ImGui::GetIO().MousePos  (logical px)
  │  For each point in snap_cache3d (x,y,z interleaved):
  │    clip = perspective_mvp() * vec4(x, y, z, 1)
  │    screen_x = (clip.x/clip.w + 1) * 0.5 * dp_w
  │    screen_y = (1 - clip.y/clip.w) * 0.5 * dp_h
  │    d = hypot(screen_x - MousePos.x, screen_y - MousePos.y)
Nearest projected point within snap_px_radius (screen pixels, not world)
  │  HoverResult::world_x/y/z, snap_t
```

3D snap uses **screen-pixel distance** (not world distance) so the threshold
is view-independent regardless of camera distance.

### World → Screen (Vulkan Geometry)

```
Vertex  (world space, z = 0 for 2D; z ≠ 0 for 3D)
  │  ortho_mvp() or perspective_mvp()  (push constant)
NDC  [-1, +1]²
  │  VkViewport (negative height)
Screen pixels
```

All geometry submissions pass the MVP. No geometry function computes its own MVP.

### World → Screen (ImGui Overlays)

```
World point (wx, wy)
  │  Viewport::world_to_pixel()
Logical pixels (lx, ly)
  │  ImGui::SetNextWindowPos(lx + offset, ly + offset)
Floating label window
```

ImGui renders in logical pixel space matching `dp_w/dp_h`.

---

## Two Radii That Must Not Be Confused

| Name | Space | Source | Purpose |
|---|---|---|---|
| `snap_px_radius` | screen pixels | `AnalysisPanel` slider | UI interaction distance — how close the cursor must be to snap |
| `epsilon_ball_radius` | world units | `AnalysisPanel` slider | Mathematical ε neighbourhood — size of rendered ball/sphere |

These are intentionally independent. At zoom=1 on a 3840-wide display,
`0.1` world units ≈ 43 screen pixels. Conflating them either makes snapping
too aggressive or makes the ε-ball useless as a pedagogical tool.

---

## Snap Cache Layout

Each `ConicEntry` maintains a snap cache of world-space points uniformly
sampled along the curve. The layout determines the semantics of the secant
walk and branch-boundary guards.

### 2D curves — `snap_cache: vector<pair<float,float>>`

| Curve type | Cache layout | t direction | Y direction |
|---|---|---|---|
| Parabola | `[0, n]` uniform in t | left→right in x | follows curve |
| Hyperbola right branch | `[0, tessellation]` | t: -range → +range | bottom→top |
| Hyperbola left branch | `[tessellation+1, 2*tessellation+1]` | t: -range → +range | bottom→top |

**Both hyperbola branches use the same t-direction (bottom→top in world Y).**
This ensures the secant walk (`li--`, `ri++`) moves consistently.
The secant walk is bounded by `[branch_start, branch_end]` to prevent
crossing branches mid-walk.

### 3D curves — `snap_cache3d: vector<float>` (x, y, z interleaved)

| Curve type | Cache layout | Notes |
|---|---|---|
| Helix | `[x0,y0,z0, x1,y1,z1, ...]` | Uniform in t, always 3D |

Access pattern: `snap_cache3d[si*3 + 0/1/2]` for point `si`.
The 3D snap search projects each point via `perspective_mvp()` and measures
screen-pixel distance, so no world-space threshold conversion is needed.

### Auto-extend (Parabola)

The parabola's t-range is `[t_min, t_max]` and `t = x`, so the cache only
covers the x range at build time. If the user pans or zooms to reveal curve
outside this range, `on_frame()` detects it and triggers a rebuild:

```cpp
if (m_vp.left()  < e.par_tmin + margin ||
    m_vp.right() > e.par_tmax - margin) {
    e.par_tmin = m_vp.left()  - 1.0f;
    e.par_tmax = m_vp.right() + 1.0f;
    e.mark_dirty();
}
```

---

## HoverResult — What the Snap Produces

`HoverResult` is populated once per frame by `update_hover()` or
`update_hover_3d()` and consumed by all overlay submissions and the
Analysis Readout panel:

```
HoverResult {
    hit, world_x, world_y, world_z   — snap point in world space
    curve_idx, snap_idx, snap_t       — which curve and where on it

    has_secant, secant_x0/y0/x1/y1   — ε-walk endpoints
    slope, intercept                  — secant line y = mx + b

    has_tangent, tangent_slope        — 2D slope dy/dx
    T[3], N[3], B[3]                  — Frenet–Serret unit vectors
    kappa, tau                        — curvature and torsion
    speed                             — |p'(t)|
    osc_radius                        — 1/κ (0 if κ = 0)
}
```

The Frenet frame (T, N, B, κ, τ) is computed in `update_hover()` for all
curve types. For the `Helix`, κ and τ are analytic constants independent of
t — the panel displays them as a live theorem verification.

---

## Component Responsibility Summary

| Component | Coordinate responsibility |
|---|---|
| `Viewport` | Single source of truth. All transforms live here. |
| `Scene::sync_viewport()` | Populates `m_vp.fb_w/h` and `m_vp.dp_w/h` each frame |
| `Scene::update_camera()` | Calls `m_vp.zoom_toward / pan_by_pixels / orbit` |
| `Scene::update_hover()` | Calls `m_vp.pixel_to_world(MousePos)`; 2D world-distance snap |
| `Scene::update_hover_3d()` | Projects 3D points via `perspective_mvp()`; screen-pixel snap |
| `Scene::submit_*()` | All pass `m_vp.ortho_mvp()` or `perspective_mvp()` |
| `Scene::submit_interval_labels()` | Calls `m_vp.world_to_pixel()` to position ImGui windows |
| `Renderer::begin_frame()` | Sets negative-height `VkViewport` for Y-flip correction |
| `CoordDebugPanel` | Reads all values; guards `mouse_valid()` before transforms |
| `PerformancePanel` | Display only; no coordinate math |

---

## Diagnostic Tools

### CoordDebugPanel

Toggle with **"Coord Debug"** in the Scene panel. Shows live + 120-frame
latched snapshots of every coordinate value.

**Key checks:**
- `dp matches DisplaySize: OK` — `sync_viewport` is correct
- `vp.fb_w/h vs fb_w/h: OK` — framebuffer size matches
- `round-trip error: 0.000 px` (green) — coordinate math is exact
- `mouse-snap px offset` — should be `≤ snap_px_radius` when snapped to a curve
- `Curve Snap Cache Sizes` — Hyperbola shows `514` (both branches); Helix shows `771` (3D)
- `OUT OF WINDOW` in red — ImGui sentinel detected, no coordinate math run

### PerformancePanel

Toggle with **"Perf Stats"** in the Scene panel. Shows 120-frame sparklines
for FPS, frame-ms, arena %, vertex count, and draw-call count with min/avg/max
tooltips. Arena utilisation bar at a glance. Does not affect coordinate math.

---

## Historical Bugs and Root Causes

| Bug | Root cause | Fix |
|---|---|---|
| Epsilon ball offset from cursor | Framebuffer px used instead of logical px | Use `dp_w/dp_h` (DisplaySize) not `fb_w/fb_h` |
| Ball tracking inverted (up/down) | Vulkan Y-down vs OpenGL Y-up: `glm::ortho` Y-up rendered at screen bottom | Negative-height `VkViewport` in `Renderer.cpp` |
| Hyperbola left-branch snap inverted | Left branch stored in reversed t order → secant walk gave wrong direction | Both branches now stored bottom→top (same t-direction) |
| Snap only near origin | Parabola `t_max=2` — no cache points beyond x=±2 | Auto-extend `par_tmin/tmax` to viewport bounds each frame |
| Snap missing left branch entirely | Snap cache only held right branch | Two-branch cache: `[0..n]` right, `[n+1..2n+1]` left |
| ImGui sentinel → NaN/∞ display | `(-FLT_MAX) / dp_w` overflowed; `CoordDebugPanel` printed garbage | `Viewport::mouse_valid()` guard; panel shows `OUT OF WINDOW` in red |
| Y-axis labels overlapping axis | Label offset was 6px — too small to clear the green axis line | PAD increased to 10px; labels horizontally centred on tick position |
