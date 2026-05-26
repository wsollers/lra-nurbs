# Adding An Equation, Surface, And Simulation

This guide is the practical path for adding new mathematical behavior to
`nurbs_dde` without bypassing the current architecture.

## Before You Start

Use these boundaries:

- Math lives in `src/math`, `src/numeric`, or `src/sim`.
- Simulation orchestration lives in `src/app/Simulation*.hpp/.cpp`.
- Rendering goes through `engine::RenderService`, not directly to Vulkan.
- UI goes through `engine::PanelService`.
- Hotkeys go through `engine::HotkeyService`.
- Dynamic runtime memory goes through `memory::MemoryService`.

Prefer these headers as examples:

- `src/sim/DifferentialSystem.hpp`
- `src/sim/DelayDifferentialSystem.hpp`
- `src/math/Surfaces.hpp`
- `src/app/SimulationDifferential2D.cpp`
- `src/app/SimulationDelayDifferential2D.cpp`
- `src/app/SimulationSurfaceGaussian.cpp`

## Adding An Equation

There are three common equation shapes in the current system.

### Symbolic Equation Strings

Equation strings such as:

```text
x' = f(x(t), x(t - tau))
```

currently serve as metadata/display text unless they are attached to a concrete
parser-backed system. The active C++ equation path still evaluates through
`IDifferentialSystem`, `IDelayDifferentialSystem`, or particle behavior code.

If symbolic parsing is added or extended, keep the parser as an equation
factory, not as a rendering or panel concern:

```text
symbolic string + parameter table
    -> parsed expression / AST
    -> IDifferentialSystem or IDelayDifferentialSystem implementation
    -> InitialValueProblem or DelayInitialValueProblem
    -> simulation panels/render views
```

Parser-backed systems should still expose `EquationSystemMetadata`, use
`MemoryService` for AST/runtime storage, and route numeric functions through
the project numeric layer where practical.

### Particle Equation

Use this when the equation updates a particle on a surface or in a particle
system. Implement or compose around `sim::IEquation`.

Checklist:

1. Add the equation in `src/sim` or as a behavior in `src/app/ParticleBehaviors.hpp`.
2. Use project math types and numeric redirectors where practical.
3. Store dynamic state in `MemoryService`-backed containers if the equation owns
   runtime buffers.
4. Add metadata so particle inspectors can explain what is attached.
5. Add tests near the existing particle/system tests.

Typical tests:

- one step produces the expected direction
- domain constraints are respected
- history/delay queries return expected values
- metadata names the behavior clearly

### ODE System

Use `sim::IDifferentialSystem` for ordinary differential equations.

```cpp
class MySystem final : public ndde::sim::IDifferentialSystem {
public:
    [[nodiscard]] ndde::sim::EquationSystemMetadata metadata() const override {
        return {
            .name = "My system",
            .formula = "x' = ...",
            .variables = "x, y"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 2u; }

    void evaluate(ndde::f64 t,
                  std::span<const ndde::f64> state,
                  std::span<ndde::f64> derivative) const override {
        (void)t;
        derivative[0] = state[1];
        derivative[1] = -state[0];
    }
};
```

Use `InitialValueProblem` to store state, scratch, and history through
`MemoryService`.

```cpp
std::array<ndde::f64, 2> initial{1.0, 0.0};
ndde::sim::InitialValueProblem ivp(memory, system, initial);
ndde::sim::Rk4OdeSolver solver;
ivp.step(solver, 0.01);
```

Tests should live with `tests/test_differential_system.cpp` unless the system is
large enough to deserve a dedicated test file.

Recommended ODE tests:

- known analytic system, such as `y' = y`
- harmonic or damped oscillator invariants/decay
- solver scratch size
- history sample count and state contents

### DDE System

Use `sim::IDelayDifferentialSystem` for delay differential equations.

```cpp
class MyDelaySystem final : public ndde::sim::IDelayDifferentialSystem {
public:
    [[nodiscard]] ndde::sim::EquationSystemMetadata metadata() const override {
        return {
            .name = "My delay system",
            .formula = "x' = f(x(t), x(t - tau))",
            .variables = "x"
        };
    }

    [[nodiscard]] std::size_t dimension() const override { return 1u; }
    [[nodiscard]] ndde::f64 max_delay() const noexcept override { return m_tau; }

    void evaluate(ndde::f64 t,
                  std::span<const ndde::f64> state,
                  const ndde::sim::DelayHistoryView& history,
                  std::span<ndde::f64> derivative) const override {
        ndde::f64 delayed = 0.0;
        history.query(t - m_tau, std::span<ndde::f64>{&delayed, 1u});
        derivative[0] = -state[0] + delayed;
    }

private:
    ndde::f64 m_tau = 1.0;
};
```

Use `DelayInitialValueProblem` so delay history is stored in the engine history
lifetime.

Recommended DDE tests:

- initial history is sampled over `[-tau, 0]`
- history interpolation returns bracketed values
- bounded systems stay within expected limits for reasonable step sizes
- `max_delay()` controls required history range

## Adding A Surface

Surfaces implement `math::ISurface`.

```cpp
class MySurface final : public ndde::math::ISurface {
public:
    [[nodiscard]] ndde::Vec3 evaluate(float u, float v, float t = 0.f) const override;
    [[nodiscard]] float u_min(float t = 0.f) const override;
    [[nodiscard]] float u_max(float t = 0.f) const override;
    [[nodiscard]] float v_min(float t = 0.f) const override;
    [[nodiscard]] float v_max(float t = 0.f) const override;
    [[nodiscard]] ndde::math::SurfaceMetadata metadata(float t = 0.f) const override;
};
```

Override analytic derivatives when possible:

```cpp
[[nodiscard]] ndde::Vec3 du(float u, float v, float t = 0.f) const override;
[[nodiscard]] ndde::Vec3 dv(float u, float v, float t = 0.f) const override;
```

If the surface changes over time or supports perturbations, derive from
`math::IDeformableSurface`.

```cpp
class MyDeformableSurface final : public ndde::math::IDeformableSurface {
public:
    void advance(float dt) override;
    void reset() override;
};
```

Surface checklist:

1. Add the surface class in `src/math` for generic math surfaces, or `src/app`
   if it is simulation-specific.
2. Fill out `SurfaceMetadata`: name, formula, domain, derivative support,
   deformable/static, time-varying, and parameters.
3. Register a factory in `src/app/SurfaceRegistry.hpp` when the surface should
   be selectable or reusable.
4. Use `MemoryService` for factory-created ownership:

```cpp
auto surface = memory.simulation().make_unique<MySurface>(args...);
```

5. Add tests in `tests/test_surface_metadata.cpp` or a new focused test file.

Recommended surface tests:

- metadata name/formula/domain are correct
- `evaluate` matches known points
- analytic `du`/`dv` match finite differences
- deformable surfaces change after `advance`
- domain and periodic flags match the intended chart

## Adding A Simulation

New first-class simulations derive from `ndde::ISimulation`.

```cpp
class SimulationMyFeature final : public ndde::ISimulation {
public:
    explicit SimulationMyFeature(ndde::memory::MemoryService* memory = nullptr);

    [[nodiscard]] std::string_view name() const override;
    void on_register(ndde::SimulationHost& host) override;
    void on_start() override;
    void on_tick(const ndde::TickInfo& tick) override;
    void on_stop() override;
    [[nodiscard]] ndde::SceneSnapshot snapshot() const override;
    [[nodiscard]] ndde::SimulationMetadata metadata() const override;
};
```

Simulation responsibilities:

- own or reference its math systems
- build surfaces, particles, ODE/DDE problems, and goals
- register panels and hotkeys through scoped service handles
- register render views through `RenderService`
- query hover/selection/pick results through `InteractionService`
- advance state from `TickInfo`
- submit draw packets through `RenderService`
- expose useful snapshot and metadata

### Registration

Add the simulation to `src/app/SceneFactories.cpp`:

```cpp
registry.add_runtime<SimulationMyFeature>("My Feature");
```

Also add the new `.cpp` to `src/CMakeLists.txt`.

### Panels

Register panels in `on_register` through `PanelService`.

```cpp
m_panel_handles.add(host.panels().register_panel({
    .title = "Sim - Controls",
    .draw = [this]() { draw_controls_panel(); }
}));
```

Use the current panel naming pattern:

- `Sim - Controls`
- `Sim - Swarms` or recipe-specific panel
- `Sim - Particles`
- `Sim - Goals`
- `Sim - Differential Eq` or `Sim - DDE State` for equation systems

Global debug/log/metadata panels should stay engine-owned.

### Hotkeys

Register simulation-specific hotkeys through `HotkeyService`, not through raw
GLFW checks.

```cpp
m_hotkey_handles.add(host.hotkeys().register_hotkey({
    .name = "Reset my simulation",
    .key = ...,
    .callback = [this]() { reset(); }
}));
```

### Rendering

Do not call Vulkan directly. Build renderer-neutral packets and submit them:

```cpp
host.render().submit(view_id, packet);
```

Use existing helpers where they fit:

- `SimulationRenderPackets.hpp`
- `ProjectedSurfaceCanvas.hpp`
- `ContourWindowRenderer.hpp`
- `Curve2DOverlay.hpp`
- `PrimitiveRenderer.hpp`

### Interaction

Use `InteractionService` for input meaning:

- surface hover/hit for 3D surfaces
- 2D hover/hit for phase-space or graph simulations
- particle and trail sample hits
- selection state
- double-click command queues

Do not duplicate mouse-to-domain mapping inside a simulation unless it is a
temporary local experiment.

### Memory

Choose the narrowest correct lifetime:

- frame scratch/render packets: `memory.frame()`
- view descriptors/view-owned data: `memory.view()`
- active simulation objects: `memory.simulation()`
- reusable computed data: `memory.cache()`
- trails and equation histories: `memory.history()`
- app/session registry data: `memory.persistent()`

Examples:

```cpp
auto values = memory.history().make_vector<ndde::f64>();
auto surface = memory.simulation().make_unique<MySurface>();
auto sim = memory.persistent().make_unique_as<ndde::ISimulation, SimulationMyFeature>();
```

Raw `new`, `delete`, `malloc`, `free`, direct `std::unique_ptr` ownership, and
`std::make_unique` are banned outside the memory package.

## Tests To Add

At minimum, add or update:

- `tests/test_all_simulations.cpp`: simulation constructs/registers cleanly
- equation-specific tests: ODE, DDE, or particle behavior correctness
- surface metadata/evaluation tests if a surface is added
- render packet builder tests if a new view mode is added
- interaction tests if the simulation uses custom picking semantics

Good smoke-test expectations:

- simulation starts and stops without stale panels/hotkeys/views
- metadata is non-empty and names the surface/equation
- one tick advances time/state when running
- pause prevents state advance
- reset restores initial state

## Documentation To Update

When adding a new math component, update whichever docs apply:

- `CURRENT_ARCHITECTURE_DIAGRAM.md` if the component changes boundaries
- `TIME_AND_SIMULATION.md` for time/history/integration behavior
- `COORDINATE_SYSTEMS.md` for new view/domain conventions
- `ALLOCATION_POLICY.md` if new lifetime patterns are introduced

## Common Mistakes

- Calling Vulkan or renderer internals directly from a simulation.
- Allocating dynamic state with raw STL ownership instead of `MemoryService`.
- Putting a panel callback in global engine UI when it is simulation-specific.
- Keeping a stale panel/hotkey/view handle after `on_stop`.
- Using `std::sin`/`std::cos` directly in simulation math where the numeric
  redirect layer should own math behavior.
- Duplicating picking logic instead of using `InteractionService`.
- Creating a config-driven abstraction before two or three concrete simulations
  prove the shared shape.
