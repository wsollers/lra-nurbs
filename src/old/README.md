# src/old — Archived Simulations

These simulation classes were the original set of demo simulations bundled
with the engine. They have been preserved here intact for reference during
the ongoing refactor to the `simulation/` + `ui/` layered architecture.

**They are NOT compiled.** They have been removed from `src/CMakeLists.txt`.

## Why preserved, not deleted

The particle spawner logic, surface definitions, and panel wiring patterns
in these files serve as reference implementations for the new `SwarmFactory`,
`ScenarioBuilder`, and `SimulationUI` classes. When porting behaviour to the
new system, compare against these files.

## Archived files

| File | Original purpose |
|---|---|
| `SimulationAnalysis.*` | Sine-Rational surface analysis view |
| `SimulationDelayDifferential2D.*` | 2D delay differential systems |
| `SimulationDifferential2D.*` | 2D ordinary differential systems |
| `SimulationMultiWell.*` | Multi-well centroid pursuit |
| `SimulationSurfaceGaussian.*` | Gaussian surface demo |
| `AnalysisSpawner.*` | Legacy spawner for Analysis sim |
| `MultiWellSpawner.*` | Legacy spawner for MultiWell sim |
| `SurfaceSimSpawner.*` | Legacy spawner for Gaussian sim |
| `WavePredatorPreySpawner.*` | Original spawner (logic now in SwarmFactory) |
| `SimulationSceneBase.hpp` | Base class (logic now in SimulationUI + SimContext) |
| `GaussianSurface.*` | Gaussian height-field surface |

## Active simulation

The single active simulation is `SimulationWavePredatorPrey` in `src/app/`.
It has been refactored to use the new `simulation/` + `ui/` layer.
