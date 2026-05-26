# Current Architecture Diagram

This document describes the current `nurbs_dde` architecture as it exists in
the source tree. It is meant to be a refactor guide and an onboarding map:
where ownership lives, how simulations plug in, and how math data becomes
renderer-submitted geometry.

## Core Principle

```text
Engine owns lifecycle and services.
Simulation owns math state and recalculation.
RenderService owns renderer-neutral view submissions.
Renderer owns Vulkan.
MemoryService owns dynamic allocation policy and lifetime scopes.
```

## System Layers

```mermaid
flowchart TD
    AppMain["app/main.cpp"] --> Engine["Engine"]
    Engine --> Services["EngineServices"]
    Engine --> Runtime["SimulationRuntime"]
    Engine --> Renderer["Vulkan Renderer"]

    Services --> Memory["MemoryService"]
    Services --> Panels["PanelService"]
    Services --> Hotkeys["HotkeyService"]
    Services --> Interaction["InteractionService"]
    Services --> Render["RenderService"]
    Services --> Camera["CameraService"]
    Services --> CameraInput["CameraInputController"]
    Services --> Clock["SimulationClock"]

    Runtime --> Sim["ISimulation"]
    Sim --> Host["SimulationHost"]
    Host --> Panels
    Host --> Hotkeys
    Host --> Interaction
    Host --> Render
    Host --> Camera
    Host --> Clock
    Host --> Memory

    Sim --> SimState["Simulation context/state"]
    Sim --> Math["Math systems"]
    Math --> Surfaces["ISurface / IDeformableSurface"]
    Math --> Particles["ParticleSystem / behaviors / goals"]
    Math --> ODE["IDifferentialSystem / IVP / ODE solvers"]
    Math --> DDE["IDelayDifferentialSystem / DDE history / DDE solvers"]

    Sim --> RenderPackets["Render packets"]
    RenderPackets --> Render
    Render --> Renderer
```

## Runtime Lifecycle

```mermaid
sequenceDiagram
    participant Engine
    participant Registry as SimulationRegistry
    participant Runtime as SimulationRuntime
    participant Sim as ISimulation
    participant Host as SimulationHost
    participant Renderer

    Engine->>Registry: register_default_simulations()
    Registry->>Runtime: add_runtime<T>()
    Runtime->>Sim: construct with MemoryService-backed storage
    Runtime->>Sim: on_register(Host)
    Runtime->>Sim: on_start()

    loop each frame
        Engine->>Host: update services, input, camera, clock
        Engine->>Runtime: tick(TickInfo)
        Runtime->>Sim: on_tick(tick)
        Sim->>Host: submit render packets, update panels, query input
        Engine->>Renderer: draw RenderService submissions
    end

    Engine->>Runtime: stop/switch simulation
    Runtime->>Sim: on_stop()
    Runtime->>Host: scoped handles unregister panels/hotkeys/views
```

## Engine Services

`EngineServices` is the engine composition root for service objects. A
simulation does not receive a concrete `Engine`; it receives a narrowed
`SimulationHost` facade with only the services it is allowed to use.

```mermaid
flowchart LR
    EngineServices --> SimulationHost
    SimulationHost --> PanelService
    SimulationHost --> HotkeyService
    SimulationHost --> InteractionService
    SimulationHost --> RenderService
    SimulationHost --> CameraService
    SimulationHost --> SimulationClock
    SimulationHost --> MemoryService
```

Current service responsibilities:

- `PanelService`: registers global and simulation-specific ImGui panel callbacks.
- `HotkeyService`: registers named key callbacks and owns handle cleanup.
- `InteractionService`: tracks mouse/view hover, selection, surface hits, 2D hits,
  particle/trail hits, and pick queues.
- `RenderService`: owns renderer-neutral view descriptors and packet submission.
- `CameraService`: owns per-view 2D/3D camera state.
- `CameraInputController`: translates raw mouse input into camera commands.
- `SimulationClock`: produces simulation tick data.
- `MemoryService`: central allocation surface for frame, view, simulation, cache,
  history, and persistent lifetimes.

## Simulation Contract

Every first-class simulation implements `ISimulation`:

```cpp
class ISimulation {
public:
    virtual std::string_view name() const = 0;
    virtual void on_register(SimulationHost& host) = 0;
    virtual void on_start() = 0;
    virtual void on_tick(const TickInfo& tick) = 0;
    virtual void on_stop() = 0;
    virtual SceneSnapshot snapshot() const;
    virtual SimulationMetadata metadata() const;
};
```

A simulation is responsible for:

- constructing its surface, equation systems, particles, goals, and panels
- registering panels, hotkeys, render views, and interaction callbacks
- advancing math state during `on_tick`
- submitting draw data through `RenderService`
- exposing `SceneSnapshot` and `SimulationMetadata`
- releasing scoped handles in `on_stop` or destructor-owned cleanup

## Current Simulations

The default registry currently installs:

```mermaid
flowchart TD
    Registry["register_default_simulations"] --> Sim1["Surface Simulation"]
    Registry --> Sim2["Sine-Rational Analysis"]
    Registry --> Sim3["Multi-Well Centroid"]
    Registry --> Sim4["Wave Predator-Prey"]
    Registry --> Sim5["Differential Systems"]
    Registry --> Sim6["Delay Differential Systems"]

    Sim1 --> SurfaceGaussian["GaussianRipple / deformable surface"]
    Sim2 --> SineRational["Sine-rational surface"]
    Sim3 --> MultiWell["Multi-well wave surface"]
    Sim4 --> WaveSurface["Wave predator-prey surface"]
    Sim5 --> ODESystems["ODE systems and phase space"]
    Sim6 --> DDESystems["Bounded delayed feedback DDE"]
```

## Render Flow

Simulations build renderer-neutral geometry and submit it to `RenderService`.
The Vulkan renderer receives only the packet stream and view descriptors.

```mermaid
flowchart LR
    Sim["ISimulation::on_tick"] --> Build["Build geometry/overlays"]
    Build --> Packets["Render packets"]
    Packets --> Views["RenderViewDescriptor"]
    Views --> RenderService
    RenderService --> PrimitiveRenderer
    PrimitiveRenderer --> VulkanRenderer["Renderer / SecondWindow"]
```

Typical submitted content:

- surface wireframes and axes
- 2D curve trajectories
- 2D phase-space vector fields
- contour/alternate view packets
- particle markers and trails
- Frenet, normal, binormal, tangent, and osculating overlays
- debug/selection hover markers

## Input, Hover, And Selection

```mermaid
flowchart TD
    RawInput["GLFW mouse/key input"] --> Engine
    Engine --> CameraInput["CameraInputController"]
    Engine --> Interaction["InteractionService"]

    CameraInput --> Camera["CameraService"]
    Interaction --> Hover["Hover targets"]
    Interaction --> Selection["Selected target"]
    Interaction --> Picks["Surface/2D/particle pick queues"]

    Hover --> GlobalDebug["Debug coordinates panel"]
    Selection --> GlobalDebug
    Picks --> Sim["Active simulation"]
    Camera --> Render["RenderService view matrices/domains"]
```

Supported target kinds include:

- surface point
- 2D view point
- particle
- trail sample
- none

Surface simulations can use surface picks for perturbations. Differential
systems can use 2D picks to reset initial conditions.

## Math Stack

```mermaid
flowchart TD
    Math["Math domain"] --> Surfaces["ISurface"]
    Surfaces --> StaticSurface["Static analytic/finite-difference surfaces"]
    Surfaces --> Deformable["IDeformableSurface"]

    Math --> ParticleMath["Particle system"]
    ParticleMath --> Behaviors["Behavior stack"]
    ParticleMath --> Goals["Goals / win conditions"]
    ParticleMath --> Constraints["Constraints"]
    ParticleMath --> History["Trails/history"]

    Math --> ODE["IDifferentialSystem"]
    ODE --> IVP["InitialValueProblem"]
    IVP --> Euler["EulerOdeSolver"]
    IVP --> RK4["Rk4OdeSolver"]

    Math --> DDE["IDelayDifferentialSystem"]
    DDE --> DDEIVP["DelayInitialValueProblem"]
    DDEIVP --> DelayHistory["DelayHistoryView"]
    DDEIVP --> EulerDDE["EulerDdeSolver"]
```

All simulation math should route through the project math/numeric layer where
possible. GPU-specific types should stay behind renderer-facing aliases or
conversion boundaries.

## Memory Lifetimes

The engine allocation policy is centralized under `memory::MemoryService`.
Code should request memory by intended lifetime.

```mermaid
flowchart LR
    MemoryService --> Frame["Frame: per-frame scratch/render packets"]
    MemoryService --> View["View: registered views and view-owned data"]
    MemoryService --> Simulation["Simulation: active sim objects/state"]
    MemoryService --> Cache["Cache: mesh/extremum/reusable derived data"]
    MemoryService --> History["History: trails, ODE/DDE samples, replay data"]
    MemoryService --> Persistent["Persistent: registry/app/runtime data"]
```

Current public surfaces include:

- `memory.frame().make_vector<T>()`
- `memory.view().make_vector<T>()`
- `memory.simulation().make_vector<T>()`
- `memory.cache().make_vector<T>()`
- `memory.history().make_vector<T>()`
- `memory.persistent().make_vector<T>()`
- `memory.<scope>().make_unique<T>()`
- `memory.<scope>().make_unique_as<Base, Derived>()`

See `ALLOCATION_POLICY.md` for the exact rules.

## Current Architectural Notes

- `ISimulation` and `SimulationHost` are now the primary boundary between app
  simulations and engine services.
- `RenderService` is renderer-neutral, but simulation-specific packet builders
  still live in `src/app`.
- Surface, ODE, and DDE math are first-class enough to extend without touching
  Vulkan.
- Global panels and simulation panels both register through `PanelService`, but
  their content is still intentionally simulation-owned.
- Memory ownership is centralized, but policy enforcement is still strongest in
  migrated hot-path/runtime code.
- The current DDE foundation has a fixed-step Euler DDE solver; ODE has Euler
  and RK4.
- New simulations should be registry-driven through `SceneFactories.cpp`.
