<!--
GENERATED FILE — DO NOT EDIT BY HAND.

Source repo: wsollers/lra-governance
Source commit: a55609adaf2be4bd68941a4cb78336e56d92a60b
Generated from:
- docs/governance/...
- docs/architecture/...
- docs/governance/repo-overlays/lra-nurbs.md

Regenerate from lra-governance.
Emergency downstream edits must be ported upstream before the next sync.
-->

# Gemini Instructions

## Global Agent Rules

- Treat generated instruction files as derived artifacts.
- Follow the owning repository boundary for every task.
- Do not include secrets, credentials, tokens, or machine-local private values.
- Do not modify mathematical content during governance or wrapper-generation tasks.
- Do not touch `Learning-Real-Analysis/scripts/`.
- Port emergency downstream instruction repairs back to `lra-governance`.

## Repo Overlay

# lra-nurbs Overlay

Stub overlay for C++ / Vulkan / geometry / simulation work.

Owned concerns:

- C++ and CMake build rules,
- Vulkan rendering rules,
- geometry and NURBS implementation,
- simulation and DDE implementation,
- local validators and CI expectations.

## Agent Scope

C++ / Vulkan / geometry / simulation rules apply only to `lra-nurbs` and the
monorepo `nurbs_dde/` mirror. They must not be injected into volume content
instructions.

Use local CMake, tests, and repo validators for implementation changes.

## Provider Notes

Gemini should follow this wrapper and the generated repo overlay.
