#!/usr/bin/env python3
"""Fail if selected hot-path files name std::vector directly.

This check is intentionally narrow. It protects the first migrated slice:
frame-local render packet/picking paths must use memory container policy
aliases so the implementation can later move to PMR/arena-backed storage.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HOT_PATH_FILES = [
    ROOT / "src" / "engine" / "RenderService.hpp",
    ROOT / "src" / "engine" / "InteractionService.hpp",
    ROOT / "src" / "sim" / "HistoryBuffer.hpp",
    ROOT / "src" / "app" / "SimulationRenderPackets.hpp",
    ROOT / "src" / "app" / "SurfaceMeshCache.hpp",
    ROOT / "src" / "app" / "ParticleTypes.hpp",
    ROOT / "src" / "app" / "AnimatedCurve.hpp",
    ROOT / "src" / "app" / "ParticleBehaviors.hpp",
    ROOT / "src" / "app" / "ParticleFactory.hpp",
    ROOT / "src" / "app" / "ParticleSystem.hpp",
    ROOT / "src" / "app" / "SimulationContext.hpp",
    ROOT / "src" / "app" / "ParticleSwarmFactory.hpp",
    ROOT / "src" / "app" / "SimulationPanelModels.hpp",
    ROOT / "src" / "app" / "SwarmRecipePanel.hpp",
    ROOT / "src" / "app" / "PanelHost.hpp",
    ROOT / "src" / "app" / "HotkeyManager.hpp",
    ROOT / "src" / "app" / "CoordDebugPanel.hpp",
    ROOT / "src" / "app" / "ProjectedParticleOverlay.hpp",
    ROOT / "src" / "app" / "ContourWindowRenderer.hpp",
    ROOT / "src" / "app" / "ParticleInspectorPanel.hpp",
    ROOT / "src" / "app" / "SpawnStrategy.hpp",
    ROOT / "src" / "engine" / "HotkeyService.hpp",
    ROOT / "src" / "engine" / "PanelService.hpp",
    ROOT / "src" / "engine" / "ScopedServiceHandles.hpp",
    ROOT / "src" / "engine" / "SimulationRuntime.hpp",
    ROOT / "src" / "engine" / "IScene.hpp",
    ROOT / "src" / "engine" / "Engine.hpp",
    ROOT / "src" / "app" / "SimulationSceneBase.hpp",
]
PATTERN = re.compile(r"\bstd::vector\s*<")


def strip_line_comment(line: str) -> str:
    return line.split("//", 1)[0]


def main() -> int:
    violations: list[str] = []
    for path in HOT_PATH_FILES:
        if not path.exists():
            continue
        for lineno, line in enumerate(path.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
            if PATTERN.search(strip_line_comment(line)):
                violations.append(f"{path.relative_to(ROOT)}:{lineno}: {line.strip()}")

    if violations:
        print("Hot-path container policy violation. Use memory::FrameVector / PersistentVector / SimVector:")
        print("\n".join(violations))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
