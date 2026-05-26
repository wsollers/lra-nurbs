# nurbs_dde Refactor Progress

## Roadmap

Step 1  [DONE] GaussianSurface implements ISurface
Step 2  [DONE] Thread ISurface* into AnimatedCurve
Step 3  [DONE] m_surface (unique_ptr<ISurface>) owns the surface in SurfaceSimScene
Step 3d [DONE] Torus + surface selector UI
Step 3b [DONE] float t on all ISurface methods (time-varying preparation)
Step 3c [DONE] IDeformableSurface + GaussianRipple
Step 3e [DONE] Thread m_sim_time to all geometry query call sites
Step 4  [DONE] IEquation + GradientWalker
Step 5  [DONE] IIntegrator + EulerIntegrator
Step 5b [DONE] Per-particle equation ownership (unique_ptr via with_equation)
Step 9  [DONE] BrownianMotion equation + MilsteinIntegrator
Step 10 [DONE] HistoryBuffer + DelayPursuitEquation
Step 2b [ ]    Particle state moves to parameter space (uv)
Step 2c [ ]    DomainConfinement constraint replaces hard-coded boundary reflection
Step 5c [ ]    IConstraint + pairwise constraints
Step 6  [ ]    SpawnStrategy
Step 7  [ ]    Extract GeometrySubmitter
Step 8  [ ]    Shrink SurfaceSimScene to orchestrator

---

## Category A -- DONE

### A1 / C1 -- Retire WalkState; store ParticleState directly in AnimatedCurve

- `src/app/GaussianSurface.hpp`
    - Deleted `struct WalkState { f32 x, y, phase, angle; }`
    - Replaced `WalkState m_walk` with `ndde::sim::ParticleState m_walk`
    - `head_uv()` now returns `m_walk.uv` (was `{ m_walk.x, m_walk.y }`)
    - Added `const` overload of `equation()` accessor (required for `export_session` const-correctness)

- `src/app/GaussianSurface.cpp`
    - Constructor: `m_walk = ndde::sim::ParticleState{ glm::vec2{start_x, start_y}, 0.f, 0.f }`
    - `reset()`: sets `m_walk.uv / phase / angle` individually (was `m_walk = WalkState{...}`)
    - `step()`: passes `m_walk` directly to integrator; eliminated intermediate `ps` copy-in/copy-out
    - All `m_walk.x / m_walk.y` replaced with `m_walk.uv.x / m_walk.uv.y` throughout
    - `push_history()`: `m_history->push(t, m_walk.uv)`
    - `query_history()`: fallback returns `m_walk.uv`
    - `head_world()`: `m_surface->evaluate(m_walk.uv.x, m_walk.uv.y)`

### A2 / E2 -- HistoryBuffer::to_vector()

- `src/sim/HistoryBuffer.hpp`
    - Added `[[nodiscard]] std::vector<Record> to_vector() const` after `clear()`
    - Iterates `(m_head + i) % m_capacity` when wrapped; simple copy when not
    - O(n) time and space; doc-commented as export/debug only

### A3 -- SurfaceSimScene::export_session()

- `src/app/SurfaceSimScene.hpp`
    - Declared `void export_session(const std::string& path) const` in private methods

- `src/app/SurfaceSimScene.cpp`
    - Added `#include <fstream>` and `#include <iomanip>`
    - Implemented `export_session()`: writes CSV with columns
      `particle_id, role, equation, record_type, step, a, b, c`
      Two record types: `trail` (world xyz) and `history` (sim_t, param_u, param_v)
    - Added `Export CSV` SmallButton after `Clear all` in `draw_simulation_panel()`
      Filename: `session_{N}p_{T}s.csv` in working directory
      Tooltip: "Write trail + history data to CSV in working directory"

### A4 -- Capture RNG seed in MilsteinIntegrator

- `src/sim/MilsteinIntegrator.hpp`
    - Added `static void set_global_seed(uint64_t seed)`, `global_seed()`, `seed_is_fixed()`
    - Added `inline static uint64_t s_global_seed = 0` and `inline static bool s_seed_set = false`
    - Updated `normal2()`: thread_local lambda initialises `mt19937` from `s_global_seed` when
      fixed, else from `std::random_device{}()`

- `src/app/SurfaceSimScene.cpp`
    - Added "RNG reproducibility" subsection in Brownian motion panel:
      `InputInt` for seed, `Apply` button calls `set_global_seed()`,
      green text when seed is fixed, greyed text when hardware-random

---

## Step 10 -- DONE

### Files created

- src/sim/HistoryBuffer.hpp
    class HistoryBuffer
    Params: capacity (ring buffer size), dt_min (rate-limiter)
    push(t, uv): append record; overwrites oldest when full
    query(t_past) -> glm::vec2: binary search + linear interpolation
    oldest_t(), newest_t(), size(), empty(), clear()
    Invariant: records stored in chronological order via ring buffer with m_head
    Predicate: query(t) = lerp(r[lo].uv, r[hi].uv, alpha) where r[lo].t <= t <= r[hi].t

- src/sim/DelayPursuitEquation.hpp
    class DelayPursuitEquation : public IEquation
    Params: tau (delay s), pursuit_speed (param-units/s), noise_sigma
    Constructor: takes const HistoryBuffer* (non-owning) + const ISurface*
    velocity(state, surface, t):
        target = history.query(t - tau)
        delta  = target - state.uv                 [naive difference]
        if periodic: wrap delta to shortest path    [torus-correct]
        return (delta / |delta|) * pursuit_speed
    noise_coefficient(): {noise_sigma, noise_sigma}
    Geometric note: periodic shortest-path wrapping implemented per axis

### Files changed

- src/app/GaussianSurface.hpp
    Added #include "sim/HistoryBuffer.hpp"
    AnimatedCurve public interface:
        enable_history(capacity, dt_min): creates m_history
        push_history(t): pushes (t, {x,y}) into m_history if set
        query_history(t_past) -> glm::vec2: delegates to m_history
        history() const -> const HistoryBuffer*: accessor for scene
    AnimatedCurve private:
        Added std::unique_ptr<HistoryBuffer> m_history (null by default)

- src/app/GaussianSurface.cpp
    Added implementations: enable_history, push_history, query_history
    push_history uses m_walk.x, m_walk.y (parameter-space position, not trail)

- src/app/SurfaceSimScene.hpp
    Added includes: HistoryBuffer.hpp, DelayPursuitEquation.hpp
    Added m_dp_params: DelayPursuitEquation::Params (UI state)
    Added m_dp_count: u32 (spawn counter for angle offsets)
    Added m_ctrl_r_prev: bool (edge detection for Ctrl+R)

- src/app/SurfaceSimScene.cpp
    advance_simulation(): after particle advance loop, calls push_history(m_sim_time)
      on all curves (push_history is a no-op when m_history == nullptr)
    handle_hotkeys(): Ctrl+R spawn logic:
        ensures m_curves[0].history() != null (calls enable_history if needed)
        spawns AnimatedCurve::with_equation(DelayPursuitEquation(...), &m_milstein)
        capacity = ceil(tau * 120 * 1.5) + 256
    draw_simulation_panel(): new "Delay Pursuit [Ctrl+R]" section
        Tau slider (0.1 - 10s), Speed slider, Noise sigma slider
        "Spawn Pursuer" button
        History window readout: "N.Ns window  N records"
        Warm-up indicator: yellow text when window < tau

### Pointer safety after vector reallocation
    DelayPursuitEquation holds: const HistoryBuffer* -> m_curves[0].m_history.get()
    When m_curves.push_back() reallocates, AnimatedCurves are MOVED.
    unique_ptr<HistoryBuffer> move does NOT change the heap address of the
    HistoryBuffer object -- only the unique_ptr's internal raw pointer is moved.
    Therefore the DelayPursuitEquation's stored pointer remains valid. CORRECT.

### Usage protocol
    1. Ctrl+L: ensure leader (curve 0) exists
    2. Let it run for at least tau seconds (warm-up period)
    3. Ctrl+R: spawn delay-pursuit chaser
       - Panel shows "Warming up: N.Ns / tau s" until history window >= tau
       - Once warm, the chaser traces the leader's path from tau seconds ago
    4. Multiple chasers can target the same leader (each with independent DDE)
    5. On torus: shortest-path wrapping ensures correct pursuit across seams

### Mathematical notes
    This is a Delay Differential Equation (DDE).
    The solution depends on the entire history X_l(s), s in [t-tau, t].
    The initial condition is a FUNCTION on [-tau, 0], not a point.
    In simulation: HistoryBuffer provides this function via linear interpolation.
    The cold-start (window < tau): query clamps to oldest record (constant init condition).
    Stability: for pursuit speed v and delay tau, the system is stable when
      v * tau < pi/2 (Lotka-Volterra type stability bound).
      At v*tau = pi/2 the chaser enters a stable limit cycle (orbit around leader).
      Above this threshold: divergence / oscillation amplification.

---

## Remaining steps

### Original roadmap (plumbing)

Step 2b  [ ]  Particle state moves to parameter space — retire WalkState, store ParticleState directly
Step 2c  [ ]  DomainConfinement IConstraint replaces hard-coded boundary reflection
Step 5c  [ ]  IConstraint + pairwise minimum-distance constraints
Step 6   [ ]  SpawnStrategy — extract spawn logic from handle_hotkeys()
Step 7   [ ]  Extract GeometrySubmitter (ParticleRenderer + SurfaceRenderer)
Step 8   [ ]  Shrink SurfaceSimScene to thin orchestrator (~200 lines)

### New items from architecture review

A2  [x]  HistoryBuffer::to_vector() -- linearise ring buffer for export and iteration (10 lines)
A3  [x]  SurfaceSimScene::export_session() -- CSV/JSON export of trail + history + metadata (50 lines)
A4  [x]  RNG seed capture in MilsteinIntegrator -- expose global_seed in config + export metadata
B1  [ ]  Split GaussianSurface.hpp into AnimatedCurve.hpp, FrenetFrame.hpp, math/GaussianSurface.hpp
C4  [ ]  Ctrl+A feature — ExtremumSurface, ExtremumTable, LeaderSeekerEquation, BiasedBrownianLeader,
             DirectPursuitEquation, MomentumBearingEquation (designed in docs/ctrl_a_leader_seeker.md)
D1  [x]  Rename IEquation::velocity() -> update() (signals mutation, not pure computation)
D2  [DONE]  Replace submit/submit2 with named render targets
E1  [ ]  Move Scene.cpp / AnalysisPanel.cpp to legacy/ (dead code, m_scene->on_frame() is commented out)

### New items from architecture review

A2  [DONE]  HistoryBuffer::to_vector()
A3  [DONE]  SurfaceSimScene::export_session()
A4  [DONE]  RNG seed capture in MilsteinIntegrator
B1  [DONE]  Split GaussianSurface.hpp -> FrenetFrame.hpp, AnimatedCurve.hpp, GaussianSurface.hpp
B2  [DONE]  Extract ParticleRenderer (all submit_* methods + submit_all loop)
B3  [DONE]  Extract SpawnStrategy (ndde::spawn namespace, header-only)
B4  [DONE]  SurfaceSimScene shrunk to orchestrator (~520 lines, panel UI is inherently long)
C2  [DONE]  IConstraint + DomainConfinement
C3  [DONE]  IPairConstraint + MinDistConstraint
C4  [DONE]  Ctrl+A feature
D1  [DONE]  Rename IEquation::velocity() -> update()
D2  [DONE]  Replace submit/submit2 with named render targets
E1  [ ]  Move Scene.cpp / AnalysisPanel.cpp to legacy/

---

## Category C -- DONE

### New files created

```
src/sim/IConstraint.hpp          IConstraint (per-particle) + IPairConstraint (pairwise) interfaces
src/sim/DomainConfinement.hpp    Concrete IConstraint: reflect/wrap existing boundary logic
src/sim/MinDistConstraint.hpp    Concrete IPairConstraint: elastic midpoint push in parameter space
src/math/ExtremumTable.hpp       ExtremumTable struct declaration
src/math/ExtremumTable.cpp       ExtremumTable::build() -- grid search + gradient refinement
src/math/ExtremumSurface.hpp     Bimodal Gaussian height field; unique global max + min; analytic grad
src/sim/LeaderSeekerEquation.hpp Deterministic 2-state (SeekMax/SeekMin) leader equation
src/sim/BiasedBrownianLeader.hpp Stochastic SDE leader: goal-directed drift + Wiener noise
src/sim/DirectPursuitEquation.hpp  Strategy A: pursuer steers toward leader's current uv
src/sim/MomentumBearingEquation.hpp Strategy C: pursuer infers leader goal from velocity history window
```

### Existing files modified

```
src/sim/IConstraint.hpp          (new -- both interfaces in one header)
src/app/AnimatedCurve.hpp        + #include IConstraint.hpp
                                  + m_constraints vector<unique_ptr<IConstraint>>
                                  + add_constraint() public method
                                  + walk_state() mutable accessor for pairwise constraints
src/app/GaussianSurface.cpp      + #include DomainConfinement.hpp
                                  + constructor body: push DomainConfinement into m_constraints
                                  + with_equation factory: same
                                  + step(): replaced 20-line if/while block with constraint loop
src/app/SurfaceSimScene.hpp      + #include IConstraint.hpp, MinDistConstraint.hpp,
                                    ExtremumTable.hpp, ExtremumSurface.hpp,
                                    LeaderSeekerEquation.hpp, BiasedBrownianLeader.hpp,
                                    DirectPursuitEquation.hpp, MomentumBearingEquation.hpp
                                  + SurfaceType::Extremum = 3
                                  + m_pair_constraints, m_pair_collision, m_min_dist
                                  + m_extremum_table (value member, stable address)
                                  + m_extremum_rebuild_countdown
                                  + LeaderMode enum + m_leader_mode
                                  + m_ls_params, m_bbl_params
                                  + PursuitMode enum + m_pursuit_mode
                                  + m_pursuit_tau, m_pursuit_window
                                  + m_spawning_pursuer, m_ctrl_a_prev
                                  + apply_pairwise_constraints(), spawn_leader_seeker(),
                                    spawn_pursuit_particle(), rebuild_extremum_table_if_needed(),
                                    draw_leader_seeker_panel() declarations
src/app/SurfaceSimScene.cpp      + swap_surface(): Extremum case + m_spawning_pursuer reset
                                  + advance_simulation(): rebuild_extremum_table_if_needed() call
                                  + advance_simulation(): apply_pairwise_constraints() call
                                  + apply_pairwise_constraints() implementation
                                  + handle_hotkeys(): Ctrl+A two-phase edge logic
                                  + draw_simulation_panel(): Extremum radio button
                                  + draw_simulation_panel(): Collision avoidance UI section
                                  + draw_simulation_panel(): draw_leader_seeker_panel() call
                                  + draw_simulation_panel(): m_spawning_pursuer reset in Clear all
                                  + rebuild_extremum_table_if_needed() implementation
                                  + spawn_leader_seeker() implementation
                                  + spawn_pursuit_particle() implementation
                                  + draw_leader_seeker_panel() implementation
src/CMakeLists.txt               + math/ExtremumTable.cpp added to ndde_math STATIC sources
```

### Key invariants

- **DomainConfinement is always-on.** Every AnimatedCurve (both constructor paths)
  pushes a DomainConfinement into m_constraints at construction time. No particle
  can ever escape the domain regardless of which equation or integrator is used.

- **ExtremumTable address is invariant.** m_extremum_table is a value member of
  SurfaceSimScene, not a unique_ptr. Pointers held by LeaderSeekerEquation and
  BiasedBrownianLeader survive swap_surface() and m_curves vector reallocation.

- **m_goal is mutable in both leader equations.** The equations are const-correct
  at the IEquation interface level (velocity() is const), but the goal-switching
  state must persist across velocity() calls. The pattern mirrors GradientWalker's
  state.angle update.

- **Leader must exist before pursuers.** spawn_pursuit_particle() checks
  m_curves.empty() and returns immediately if there is no leader.

- **Ctrl+A is two-phase.** First press: spawn leader (m_spawning_pursuer = true).
  All subsequent presses: spawn pursuers. m_spawning_pursuer resets to false on
  swap_surface() and on the Clear all button.

### New files created in Category B

```
src/app/FrenetFrame.hpp       FrenetFrame, SurfaceFrame, make_surface_frame
src/app/AnimatedCurve.hpp     AnimatedCurve class (full declaration + inline methods)
src/app/ParticleRenderer.hpp  ParticleRenderer class declaration
src/app/ParticleRenderer.cpp  All submit_* implementations + submit_all()
src/app/SpawnStrategy.hpp     ndde::spawn namespace: SpawnContext, reference_uv,
                               offset_spawn, spawn_shared, spawn_owned
```

### Key invariants preserved

- EngineAPI is a value type (std::function members) -- ParticleRenderer holds its own copy safely
- submit_to("3d", ...) -> primary 3D window, submit_to("contour", ...) -> second window (ParticleRenderer uses "3d" only)
- Delay-pursuit chasers spawned with prewarm=false (must wait for leader history)
- AnimatedCurve.hpp does NOT include GaussianSurface.hpp (no circular dependency)
- GaussianSurface.hpp includes AnimatedCurve.hpp transitively via the split headers

### Full priority order -- see TODO.md at repo root

---

## Category D — D1 DONE

### Files touched

```
src/sim/IEquation.hpp              virtual update() replaces velocity() (pure virtual declaration)
                                    Block comment: velocity() -> update() (3 occurrences)
                                    noise_coefficient doc: "dX = update()*dt + g*dW"
src/sim/IIntegrator.hpp            Doc comment: "equation.velocity" -> "equation.update" (1 occurrence)
src/sim/EulerIntegrator.hpp        Call site: equation.velocity() -> equation.update()
                                    Inline comment updated to match
src/sim/MilsteinIntegrator.hpp     Call site (drift): equation.velocity() -> equation.update()
                                    (sigma_gradient uses noise_coefficient only -- no change needed)
src/sim/GradientWalker.hpp         Override declaration + section banner comment
src/sim/BrownianMotion.hpp         Override declaration
src/sim/DelayPursuitEquation.hpp   Override declaration + section banner comment
src/sim/LeaderSeekerEquation.hpp   Override declaration
src/sim/BiasedBrownianLeader.hpp   Override declaration + method doc comment
src/sim/DirectPursuitEquation.hpp  Override declaration
src/sim/MomentumBearingEquation.hpp Override declaration
```

### Invariant

`IEquation::update()` is the single point of mutation for `ParticleState` per
integrator sub-step.  The integrator is the only caller; it owns the
`ParticleState` and passes it as a mutable reference, allowing stateful equations
(GradientWalker's turn-rate limiter, LeaderSeekerEquation's goal flip,
BiasedBrownianLeader's goal flip) to persist state without `const_cast`.

### What was NOT changed

- `noise_coefficient()` -- name unchanged
- `phase_rate()` -- name unchanged
- Local variables named `vel`, `velocity`, `mu` inside method bodies -- left alone
- Prose comments using "velocity" as a mathematical concept (e.g. "velocity field",
  "velocity history window" in MomentumBearingEquation) -- left alone; the word
  is mathematically correct, only the C++ method name changed

### Verification

Grep of `src/` for `\.velocity(` returned zero matches after all edits.

---

## Category D — D2 DONE

### Files touched

```
src/engine/EngineAPI.hpp         Added #include <string_view>
                                  Removed: std::function<void(slice, topology, mode, color, mvp)> submit
                                  Removed: std::function<void(slice, topology, mode, color, mvp)> submit2
                                  Added:   std::function<void(string_view target, slice, topology,
                                                              mode, color, mvp)> submit_to
                                  Doc comment: explains "3d" and "contour" as known targets;
                                  unknown target or closed window is a no-op
src/engine/Engine.cpp            make_api(): replaced two separate lambda assignments
                                  (api.submit = ..., api.submit2 = ...) with a single
                                  api.submit_to = ... lambda dispatching on target string:
                                    "3d"      -> m_renderer.draw(...)
                                    "contour" -> m_second_win.draw(...) (guarded by valid())
                                    else      -> no-op
src/app/ParticleRenderer.hpp     Updated doc comment: submit() -> submit_to("3d", ...),
                                  submit2() -> submit_to("contour", ...)
src/app/ParticleRenderer.cpp     9 call sites migrated to submit_to("3d", ...):
                                    submit_arrow (1), submit_trail_3d (1),
                                    submit_head_dot_3d (1), submit_osc_circle_3d (1),
                                    submit_normal_plane_3d (2), submit_torsion_3d (2)
src/app/SurfaceSimScene.cpp      9 call sites migrated:
                                    submit_wireframe_3d -> submit_to("3d", ...) x1
                                    submit_filled_3d    -> submit_to("3d", ...) x1
                                    submit_contour_second_window -> submit_to("contour", ...) x7
                                      (background triangles, contour lines, per-curve trails,
                                       inline arr lambda for 2D Frenet arrows,
                                       osculating circle, torsion tip dot)
src/app/Scene.cpp                13 call sites migrated to submit_to("3d", ...):
                                    submit_surfaces (1), submit_epsilon_ball (1),
                                    submit_epsilon_sphere (1), submit_frenet_frame (1),
                                    submit_osc_circle (1), submit_interval_lines (1),
                                    submit_secant_line (1), submit_tangent_line (1),
                                    submit_grid (2), submit_axes (1), submit_conics (2)
```

### Invariant

`submit_to()` dispatches by string name; unknown targets are silent no-ops;
window count is not encoded in the API. Adding a third render window requires
only a new `else if (target == "new_name")` branch in `Engine::make_api()` —
no `EngineAPI` field additions, no call-site changes anywhere in scene code.

### Verification

Grep of all `.cpp` files in `src/` for `\.submit(` and `\.submit2(` (excluding
`submit_to`) returned zero matches after all edits.

---

## Category D — D3 DONE

### Files touched

```
src/engine/EngineAPI.hpp         Removed struct ImFont; forward-declaration (no longer needed
                                  in the API header)
                                  Removed: std::function<ImFont*()> math_font_body
                                  Removed: std::function<ImFont*()> math_font_small
                                  Added:   std::function<void(bool small)> push_math_font
                                  Added:   std::function<void()>           pop_math_font
                                  Doc comment explains push/pop contract and null-safety
src/engine/Engine.cpp            make_api(): replaced two raw-pointer lambdas with:
                                    push_math_font: selects body vs small font via bool,
                                      guards on nullptr before PushFont (safe no-op if
                                      font asset failed to load)
                                    pop_math_font: unconditional ImGui::PopFont()
                                  ImFont* stays entirely inside the engine layer
src/app/AnalysisPanel.hpp        Replaced #include <imgui.h> with #include "engine/EngineAPI.hpp"
                                  draw(hover, ImFont*)          -> draw(hover, EngineAPI&)
                                  draw_readout_panel(hover, ImFont*) -> draw_readout_panel(hover, EngineAPI&)
src/app/AnalysisPanel.cpp        draw() and draw_readout_panel() signatures updated
                                  4 if (font) ImGui::PushFont/PopFont patterns replaced with
                                    api.push_math_font(false) / api.pop_math_font()
src/app/Scene.cpp                m_analysis_panel.draw(m_hover, m_api.math_font_body())
                                    -> m_analysis_panel.draw(m_hover, m_api)
                                  submit_interval_labels(): ImFont* font local variable removed;
                                    3x if(font)PushFont/PopFont pairs replaced with
                                    m_api.push_math_font(true) / m_api.pop_math_font()
```

### Invariant

`ImFont*` does not appear in any `EngineAPI` field. The app layer (Scene,
AnalysisPanel) calls `push_math_font` / `pop_math_font` and never handles a
raw ImGui font pointer. The engine layer selects the correct font internally.

### Verification

Grep of `src/` for `math_font_body` and `math_font_small` returns zero matches.
The only surviving `ImFont*` is the local variable inside `Engine::make_api()`'s
`push_math_font` lambda — correct and intentional.

---

## Category E — E1, E3 DONE

### E1 — Move Scene.cpp / AnalysisPanel.cpp to legacy/

```
src/app/legacy/                  Created (new directory)
src/app/legacy/README.md         Explains what is here, why, and how to re-enable
src/app/legacy/Scene.cpp         Full Scene implementation (fully updated: submit_to, push_math_font)
src/app/legacy/AnalysisPanel.cpp Full AnalysisPanel implementation (fully updated: EngineAPI& api)
src/app/legacy/Scene_on_frame_patch.bak  Historical patch fragment, preserved
src/app/Scene.cpp                Replaced with #error tombstone (compiler guard)
src/app/AnalysisPanel.cpp        Replaced with #error tombstone (compiler guard)
src/app/Scene_on_frame_patch.bak Replaced with single redirect comment
src/CMakeLists.txt               Removed app/Scene.cpp and app/AnalysisPanel.cpp
                                  from add_executable(nurbs_dde ...) source list
```

**Why tombstones instead of deletion:** The Filesystem tools don't expose a
delete operation. The `#error` directive in the stub files means that if
anyone accidentally re-adds the files to CMakeLists, the build fails loudly
with a clear message rather than silently compiling stale code.

**Headers remain in place:** `Scene.hpp` and `AnalysisPanel.hpp` stay at
`src/app/` because `Engine.cpp` includes `Scene.hpp` for the `Scene` type
definition needed by `std::unique_ptr<Scene> m_scene` in `Engine.hpp`.

### E3 — Add equation() accessor note to AnimatedCurve comment block

```
src/app/AnimatedCurve.hpp        Added "Live equation access" subsection to the file-level
                                  block comment, immediately after the "History recording"
                                  subsection. Documents:
                                    - what equation() returns
                                    - how to dynamic_cast to a concrete type
                                    - example using BrownianMotion
                                    - that this is the live Brownian tuning mechanism
```
