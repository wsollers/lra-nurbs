#!/usr/bin/env python3
"""Fail if selected hot-path files name std::vector directly.

This check is intentionally narrow. It protects the first migrated slice:
frame-local render packet/picking paths must use memory container policy
aliases so the implementation can later move to PMR/arena-backed storage.
"""

from __future__ import annotations

import argparse
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
    ROOT / "src" / "simulation" / "particles" / "ParticleTypes.hpp",
    ROOT / "src" / "simulation" / "curves" / "AnimatedCurve.hpp",
    ROOT / "src" / "simulation" / "particles" / "ParticleBehaviors.hpp",
    ROOT / "src" / "simulation" / "particles" / "ParticleFactory.hpp",
    ROOT / "src" / "simulation" / "particles" / "ParticleSystem.hpp",
    ROOT / "src" / "simulation" / "particles" / "ParticleSimulationContext.hpp",
    ROOT / "src" / "simulation" / "particles" / "ParticleSwarmFactory.hpp",
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


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Fail if selected hot-path files name std::vector directly instead "
            "of project container policy aliases."
        )
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    parse_args(argv)
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
