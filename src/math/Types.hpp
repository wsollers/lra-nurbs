#pragma once
// math/Types.hpp
// ── Compatibility shim ────────────────────────────────────────────────────────
// This header is DEPRECATED for new code. It is kept so that existing
// translation units that already include "math/Types.hpp" continue to compile
// without changes during the migration to the split layout.
//
// New code should include:
//   "math/Scalars.hpp"    — for scalar/vector types (f32, Vec3, Mat4, …)
//   "math/GeometryTypes.hpp" — for renderer-neutral Vertex
//
// Do NOT add new content here. Do NOT include this from ndde_numeric or from
// any translation unit that must remain Vulkan-free (Python bindings, tests).
//
// Migration status: all math/* headers now include Scalars.hpp directly.
// Remaining users of Types.hpp: renderer/*, memory/*, engine/*, app/* — all
// of which already carry an indirect Vulkan dependency and are not affected
// by the split.

#include "math/Scalars.hpp"
#include "math/GeometryTypes.hpp"
