# Time and Simulation in NDDE

**Last updated:** 2026-04-13

---

## How the Engine Handles Time

The engine owns the clock. Frame timing is measured in `Engine::run_frame()`
using `glfwGetTime()` and distributed to `Scene` via `EngineAPI::debug_stats()`.

```cpp
// In Engine::run_frame():
const double now     = glfwGetTime();         // wall clock in seconds
const double delta_s = now - m_last_frame_time;
m_last_frame_time    = now;

m_debug_stats.frame_ms = static_cast<f32>(delta_s * 1000.0);
m_debug_stats.fps      = (frame_ms > 0.f) ? 1000.f / frame_ms : 0.f;
```

`DebugStats` is populated before `Scene::on_frame()` is called and is readable
via `m_api.debug_stats()`. The `PerformancePanel` uses it to drive the 120-frame
sparklines.

### If you need `dt` inside `Scene`

`DebugStats::frame_ms` is your `dt`:

```cpp
// In Scene::on_frame():
const float dt_ms = m_api.debug_stats().frame_ms;   // milliseconds
const float dt_s  = dt_ms * 0.001f;                 // seconds
```

Do not use `ImGui::GetTime()` — the engine clock is sourced from `glfwGetTime()`
which is lower latency and does not depend on ImGui initialisation order.
`ImGui::GetTime()` is still available and accurate, but `DebugStats::frame_ms`
is pre-calculated and the canonical source.

---

## Time for a DDE Simulation

A delay-differential equation of the form

    x'(t) = f(x(t), x(t - τ))

requires access to the state at a past time `t - τ`. The engine provides no
history buffer — that is the simulation object's responsibility.

The standard architecture is a **ring buffer of (time, state) pairs**:

```cpp
struct StateRecord { double t; Vec3 pos; };

class DDEParticle {
    std::vector<StateRecord> m_history;   // ring buffer
    std::size_t              m_head = 0;
    double                   m_tau  = 0.5;  // delay in seconds
};
```

Each integration step:
1. Write `{ current_t, current_pos }` into `m_history[m_head]`
2. Advance `m_head = (m_head + 1) % m_history.size()`
3. To evaluate `x(t - τ)`, walk backwards from `m_head` to find records
   bracketing `current_t - tau`, then linearly interpolate

Buffer size needed: at least `ceil(tau / dt_min) + 2` entries. With `tau = 0.5s`
and `dt_min ≈ 1/60 s` ≈ 16.7ms, that is about 32 entries. Allocate 4× headroom
for safety and variable frame rate.

---

## Simulation Speed and Pausing

Add to `Scene.hpp`:

```cpp
double m_sim_time  = 0.0;   // simulation clock (seconds)
float  m_speed     = 1.0f;  // 1.0 = real time, 2.0 = double speed
bool   m_paused    = false;
```

Expose in `draw_main_panel()`:

```cpp
ImGui::SliderFloat("Speed", &m_speed, 0.1f, 10.f, "%.2fx");
ImGui::Checkbox("Pause", &m_paused);
```

Advance the clock in `on_frame()`:

```cpp
const float dt = m_api.debug_stats().frame_ms * 0.001f;
if (!m_paused) m_sim_time += static_cast<double>(dt * m_speed);
```

Pausing freezes the simulation clock; the render loop continues drawing
the last computed state at vsync rate.

---

## Threading Model

The engine is currently single-threaded. The render loop and simulation
advance both run on the main thread inside `on_frame()`.

For a computationally heavy DDE integrator (e.g. RK4 with many particles
on a torus), the recommended upgrade is a double-buffered thread split:

```
Main thread (render)            Worker thread (simulation)
─────────────────────           ──────────────────────────
on_frame() reads front_buf      integrator writes back_buf
at end of frame:                at fixed dt:
  swap front/back                 step integrator
  (atomic int flip)               write trajectory points
```

The render thread reads whatever trajectory data is in the front buffer.
The worker advances at whatever rate the integrator allows, independent of
vsync. The swap is a single `std::atomic<int>` (0 or 1) or a `std::mutex`-
guarded pointer swap — the render thread blocks for no more than a mutex lock.

---

## The Torus Use Case

For Brownian motion on a torus with coordinates `(θ, φ) ∈ [0, 2π)²`,
each particle needs:

- A `Vec2` state `(theta, phi)` updated each frame via:
  - Drift from the DDE pursuit term `x(t - τ)`
  - Diffusion from a Wiener increment `dW = N(0,1) * sqrt(dt)` per component
  - Wrapping via `std::fmod(theta + 2π, 2π)` after each step
- Conversion to `Vec3` for rendering:
  ```cpp
  // R = major radius, r = tube radius
  const float x = (R + r * std::cos(phi)) * std::cos(theta);
  const float y = (R + r * std::cos(phi)) * std::sin(theta);
  const float z = r * std::sin(phi);
  ```

### Memory for particle state

Particle positions belong in a dedicated **`SimulationBuffer`** (device-local
SSBO, written by compute shader, persistent across frames) — **not** in the
per-frame geometry arena. The arena resets every frame and is for display
geometry only. See `docs/ENGINE_ARCHITECTURE.md §Planned: SimulationBuffer`.

The history ring buffer for the DDE delay term is CPU-side in the simulation
object. It does not go into the Vulkan arena at all.

### Spatial indexing for pursuit

At N=M=100 particles, O(N²) nearest-neighbour search is ≈10,000 comparisons
per frame — negligible. At N=M=10,000, a **toroidal spatial hash** becomes
necessary. This will live in `math/TorusGrid` as a pure data structure over
`(theta, phi)` coordinates. It is completely separate from the rendering arena.
The decision to not add spatial structure to `BufferManager` was deliberate —
the arena is a flat linear allocator and has no meaningful semantics for
spatial queries over simulation particles.

---

## Stochastic Integration Note

For the Brownian motion term, draw Wiener increments as:

```cpp
// In your simulation object (e.g. uses std at header level):
std::mt19937_64                      m_rng{ std::random_device{}() };
std::normal_distribution<double>     m_normal{ 0.0, 1.0 };

// Each step, for each coordinate:
const double dW_theta = m_normal(m_rng) * std::sqrt(dt);
const double dW_phi   = m_normal(m_rng) * std::sqrt(dt);
```

Use `double` precision for the integration step; convert to `float` only
when writing to the GPU vertex buffer. Accumulation over thousands of steps
with `float` precision introduces visible drift on the torus.
