# Architecture Boundaries

This note records the current source ownership rules. The intent is to make
include paths explain boundaries instead of just file locations.

## Source Ownership

- `src/math/numeric` is pure math. It may depend on standard library and math
  support libraries, but it must not depend on `app`, `engine`, or
  `simulation`.
- `src/math` owns reusable mathematical geometry, surfaces, axes, scalar
  types, integration routines, and numeric utilities.
- `src/engine` owns lifecycle, services, input coordination, runtime
  orchestration, and engine-owned subsystems.
- `src/engine/renderer`, `src/engine/platform`, and `src/engine/telemetry` are
  engine-owned subsystems. They stay internally isolated even though their
  CMake targets remain named `ndde_renderer`, `ndde_platform`, and
  `ndde_telemetry`.
- `src/simulation` owns reusable simulation building blocks: contexts, object
  handles, curves, surfaces, particles, fields, events, scenarios, and spawn
  descriptors.
- `src/app/workbenches/<Name>` owns gallery/workbench assembly. A workbench
  folder may contain its registration hook, metadata, UI assembly, simulation
  class, and any local helper state needed to build that experience.

## Include Direction

- App code may include `app/...`, `engine/...`, `simulation/...`, `sim/...`,
  `math/...`, `memory/...`, and `units/...`.
- Engine code may include engine subsystems through `engine/renderer/...`,
  `engine/platform/...`, and `engine/telemetry/...`.
- Engine code must not include `app/...`; app code wires concrete workbench and
  scene registration into the engine through narrow registration callbacks.
- Simulation code may include `simulation/...`, `sim/...`, `math/...`,
  `memory/...`, and other reusable lower-level support.
- Math code must not include app, engine, or simulation UI/runtime headers.
- Simulation code must not include app headers or engine UI/runtime headers.

When a requested implementation would require crossing these boundaries, stop
and discuss the design before coding.

## Known Boundary Debt

Some reusable simulation code still includes engine headers for shared service
or runtime vocabulary:

- `simulation/scenario` uses `EventBusService` and telemetry records.
- `simulation/events` uses engine runtime IDs and app lifecycle event types.
- `simulation/particles` uses `TickInfo` and scene snapshot vocabulary.

These are not app or renderer dependencies, but they are still cross-layer
couplings. The preferred fix is to move shared POD vocabulary into a neutral
core/types layer or into `simulation` before tightening the rule further.
