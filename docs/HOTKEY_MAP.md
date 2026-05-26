# Hotkey Map

**Status:** maintained manually for now
**Scope:** engine-global and active simulation hotkeys
**Intent:** keep keyboard controls discoverable while the hotkey system evolves toward a generated reference.

## Engineering Notes

- Prefer registering new simulation hotkeys through `HotkeyService` so the
  binding lives with the owning simulation.
- Keep global engine hotkeys in `Engine::on_key_event` until there is a shared
  engine hotkey registry that can express built-in commands.
- Keep this document synchronized when hotkeys change. A later CMake/tooling
  step can extract registered descriptors from C++ metadata and regenerate this
  file.
- Avoid hard-coded per-simulation number branches. Simulation switching uses
  range arithmetic: `Ctrl+1` selects index `0`, `Ctrl+2` selects index `1`, and
  so on through `Ctrl+9`.

## Engine Global

| Hotkey | Action | Source |
|---|---|---|
| `Ctrl+1` ... `Ctrl+9` | Switch to simulation slot 1 through 9 when present. | `Engine::on_key_event` |
| `F12` | Request still PNG capture for both windows. | `Engine::on_key_event` |
| `Ctrl+Shift+P` | Pause the active simulation, then request still PNG capture. | `Engine::on_key_event` |
| `Left Arrow` | Orbit the main camera left. | `Engine::on_key_event` |
| `Right Arrow` | Orbit the main camera right. | `Engine::on_key_event` |

## Lab Picker

Current slot: `Ctrl+1`

| Hotkey | Action | Source |
|---|---|---|
| none | Use the picker panel to open the smoke test, integration workbench, or Taylor workbench. | `SimulationLabPicker` |

## Smoke Test - Wave Predator-Prey

Current slot: `Ctrl+2`

| Hotkey | Action | Source |
|---|---|---|
| `Ctrl+R` | Reset predator/prey showcase. | `SimulationWavePredatorPrey::on_start` |
| `Ctrl+B` | Spawn Brownian cloud. | `SimulationWavePredatorPrey::on_start` |
| `Ctrl+L` | Spawn contour band. | `SimulationWavePredatorPrey::on_start` |

## Integration & Derivative Lab

Current slot: `Ctrl+3`

| Hotkey | Action | Source |
|---|---|---|
| none yet | Use the lab panel to select function, method, cells, and derivative probe. | `SimulationIntegrationDerivativeLab` |

## Taylor Expansion Lab

Current slot: `Ctrl+4`

| Hotkey | Action | Source |
|---|---|---|
| none yet | Use the lab panel to select Taylor degree, center, and probe point. | `SimulationTaylorExpansionLab` |

## UI Labels Without Registered Hotkeys

Some visible labels currently mention keyboard chords before they are backed by
`Engine::on_key_event` or `HotkeyService` registration. Treat these as UI debt
until the binding is either implemented or the label is removed.

| Label | Location |
|---|---|
| `Ctrl+P` pause label | `SwarmRecipePanel` / projected surface canvas |
| `Ctrl+F` hover Frenet label | `SwarmRecipePanel` |
| `Ctrl+O` osculating circle label | `SwarmRecipePanel` |
