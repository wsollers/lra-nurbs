# nurbs_dde — Master TODO

All work items in one place, ordered by priority within each category.
Sources: refactor roadmap, architecture review, session notes.
Last updated: 2026-05-01

---

## Key: status tags

```
[ ]   not started
[~]   in progress
[x]   done
[-]   deferred / won't do now
```

---

## Category A — Correctness (do these first, unblock everything else)

| # | Status | Item | Why it matters |
|---|--------|------|----------------|
| A1 | [x] | **Retire `WalkState`, store `ParticleState` directly in `AnimatedCurve`** | Same 4 fields, two names, manual copy every step. Will become a bug surface when RK4 or per-component Milstein is added. Fix: typedef or direct member. ~30 min. |
| A2 | [x] | **Add `HistoryBuffer::to_vector()`** | Ring buffer is non-contiguous when full. Export and any future iteration over all records needs a linearised view. 10 lines. |
| A3 | [x] | **Add `SurfaceSimScene::export_session()`** | Particle trail and history data exists in RAM every frame and is currently thrown away when the window closes. 50 lines CSV or JSON (nlohmann already linked). |
| A4 | [x] | **Capture RNG seed in `MilsteinIntegrator`** | Every stochastic run is unreproducible. `thread_local mt19937 rng{ std::random_device{}() }` — seed is lost on thread exit. Fix: expose a `global_seed` in config and export metadata. 2 lines. |

---

## Category B — Architecture (God Object decomposition)

The original Steps 6–8, now ordered by dependency.

| # | Status | Item | Effort |
|---|--------|------|--------|
| B1 | [x] | **Split `GaussianSurface.hpp` into three headers** | `AnimatedCurve.hpp`, `FrenetFrame.hpp`, `math/GaussianSurface.hpp`. Prerequisite for B2 and B3. ~2 hrs. |
| B2 | [x] | **Extract `ParticleRenderer`** (Step 7 — GeometrySubmitter) | Move all `submit_*` methods out of `SurfaceSimScene` into a dedicated renderer class. Takes `const vector<AnimatedCurve>&` and emits draw calls. Depends on B1. ~4 hrs. |
| B3 | [x] | **Extract `SpawnStrategy`** (Step 6) | Extract all particle-spawn logic from `handle_hotkeys()`. Different spawn patterns (random, golden-ratio, at-cursor) become interchangeable. ~2 hrs. |
| B4 | [x] | **Shrink `SurfaceSimScene` to orchestrator** (Step 8) | After B2 and B3: scene owns state, calls `advance()`, calls `renderer.submit_all()`, calls `ui.draw()`. Final line count ~520 (draw_simulation_panel is long UI code; acceptable). Depends on B2 + B3. |

---

## Category C — Simulation correctness and extensibility

Original Steps 2b, 2c, 5c plus new items.

| # | Status | Item | Effort |
|---|--------|------|--------|
| C1 | [x] | **Step 2b — `WalkState` → `ParticleState`** | See A1 above (same item). |
| C2 | [x] | **Step 2c — `DomainConfinement` constraint** | Replace 20 lines of hard-coded `if/while` boundary logic in `AnimatedCurve::step()` with an `IConstraint` interface. Enables "stay on half-torus", elastic bounce, etc. ~2 hrs. |
| C3 | [x] | **Step 5c — `IConstraint` + pairwise constraints** | Inter-particle minimum-distance constraint. Prerequisite for collision-avoidance in multi-agent pursuit. Depends on C2. ~3 hrs. |
| C4 | [x] | **Ctrl+A feature — `ExtremumSurface` + `LeaderSeekerEquation` + `BiasedBrownianLeader`** | Fully designed in `docs/ctrl_a_leader_seeker.md`. New files: `ExtremumSurface.hpp`, `ExtremumTable.hpp`, `LeaderSeekerEquation.hpp`, `BiasedBrownianLeader.hpp`, `DirectPursuitEquation.hpp`, `MomentumBearingEquation.hpp`. ~1 day. |

---

## Category D — API quality

Low urgency, no feature blocked by these.

| # | Status | Item | Effort |
|---|--------|------|--------|
| D1 | [x] | **Rename `IEquation::velocity()` → `update()`** | `velocity()` implies a pure function. The method mutates `ParticleState&` (GradientWalker persists angle). `update()` is honest. ~30 min. |
| D2 | [x] | **Replace `submit` / `submit2` with named render targets** | `submit2` is "the second window" — a leaky abstraction. `api.submit_to("contour", ...)` is cleaner and won't need `submit3` when a third window is added. ~2 hrs. |
| D3 | [x] | **Wrap `math_font_body` / `math_font_small` in helper methods** | `EngineAPI` exposes raw `ImFont*` — an ImGui type — into the app layer. Replace with `api.push_math_font(bool small)` / `api.pop_math_font()`. ~30 min. |

---

## Category E — Housekeeping

| # | Status | Item | Effort |
|---|--------|------|--------|
| E1 | [x] | **Move `Scene.cpp` / `AnalysisPanel.cpp` to `legacy/`** | Both are compiled but `m_scene->on_frame()` is commented out in `Engine::run_frame()`. Dead weight in the build. ~15 min. |
| E2 | [x] | **Add `HistoryBuffer::to_vector()` accessor** | See A2. Also needed for display / debugging independent of export. |
| E3 | [x] | **Add `equation()` accessor note to `AnimatedCurve` comment block** | `equation()` was added for live Brownian tuning but the class comment doesn't mention it. ~5 min. |

---

## Category F — Threading (only when particle count makes it necessary)

| # | Status | Item | Effort |
|---|--------|------|--------|
| F1 | [-] | **Thread pool for particle integration** | Each `AnimatedCurve::advance()` is independent — embarrassingly parallel. Add when particle count exceeds ~200 and frame time degrades. Requires double-buffering `m_trail`. ~4 hrs when needed. |
| F2 | [-] | **Double-buffer surface geometry caches** | Prerequisite for F1 if sim thread writes cache while render thread reads it. Not needed while single-threaded. |

---

## Recommended order of execution

```
Session 1 (data first):    A2, A3, A4            — unlock offline analysis
Session 2 (correctness):   A1 / C1               — retire WalkState
Session 3 (header split):  B1                    — prerequisite for B2/B3
Session 4 (constraints):   C2, C3                — enable multi-agent collision
Session 5 (Ctrl+A):        C4                    — the big new feature
Session 6 (God Object):    B2, B3, B4            — decompose SurfaceSimScene
Session 7 (API polish):    D1, D2, D3            — when the architecture is clean
Session 8 (housekeeping):  E1, E2, E3            — anytime
F1, F2: revisit when profiling shows sim is the bottleneck
```

---

## Completed items (from refactor_progress.md)

```
[x] Step 1   GaussianSurface implements ISurface
[x] Step 2   Thread ISurface* into AnimatedCurve
[x] Step 3   m_surface as unique_ptr<ISurface>
[x] Step 3b  float t on all ISurface methods
[x] Step 3c  IDeformableSurface + GaussianRipple
[x] Step 3d  Torus + surface selector UI
[x] Step 3e  Thread m_sim_time to all geometry call sites
[x] Step 4   IEquation + GradientWalker
[x] Step 5   IIntegrator + EulerIntegrator
[x] Step 5b  Per-particle equation ownership (with_equation factory)
[x] Step 9   BrownianMotion + MilsteinIntegrator
[x] Step 10  HistoryBuffer + DelayPursuitEquation

[x] Wireframe geometry cache (CPU-side, invalidated by dirty flag)
[x] Filled surface cache (triangle mesh, curvature-coloured)
[x] Surface display mode toggle (Wireframe / Filled / Both)
[x] Curvature colour map (K > 0 warm, K < 0 cool, K = 0 grey)
[x] Grid resolution slider (8–256, live vertex count readout)
[x] Pause / unpause (Ctrl+P, PAUSED overlay, hover still active)
[x] Hotkey reference panel (Ctrl+H, floating, auto-resize)
[x] Live Brownian param tuning (dynamic_cast to BrownianMotion*, per-particle sliders)
[x] head_uv() accessor on AnimatedCurve
[x] equation() accessor on AnimatedCurve
```

---

## Supporting documents

```
docs/architecture_review.md          full architectural assessment
docs/refactor_progress.md            step-by-step log of completed steps
docs/ctrl_a_leader_seeker.md         Ctrl+A feature design (LeaderSeeker + BiasedBrownianLeader)
docs/how_to_add_a_surface_prompt.md  AI prompt for adding a new surface
docs/ADDING_A_MATH_COMPONENT.md      existing guide for math components
docs/ENGINE_ARCHITECTURE.md          engine layer documentation
docs/COORDINATE_SYSTEMS.md           coordinate system reference
docs/TIME_AND_SIMULATION.md          simulation time reference
```
