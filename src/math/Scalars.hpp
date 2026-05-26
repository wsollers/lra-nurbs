#pragma once
// math/Scalars.hpp
// Canonical scalar and vector type aliases for the NDDE engine.
//
// ── Design contract ───────────────────────────────────────────────────────────
// This header is the ONLY math header that the pure-math and numeric layers
// include. It has zero external dependencies beyond GLM (header-only, no
// linker cost) and the C++ standard library.
//
// Specifically, it MUST NOT include:
//   - <volk.h>, <vulkan/vulkan.h>, or any Vulkan header
//   - Any platform header (GLFW, Win32, etc.)
//   - Any renderer or memory header
//
// Renderer-neutral geometry payloads live in math/GeometryTypes.hpp.
// Vulkan-specific push constants and VkVertexInput helpers live in
// renderer/GpuTypes.hpp.
//
// ── Scalar aliases ────────────────────────────────────────────────────────────
//   f32 / f64  — IEEE-754 single/double, native on every x64 target we care
//                about. f32 is the GPU-native scalar; f64 is used for
//                simulation and stochastic integration.
//   f128       — long double, extended precision for stochastic integrators
//                only. Not supported by GLSL/SPIR-V.
//
// ── Vector aliases ────────────────────────────────────────────────────────────
//   Vec2/3/4 and Mat4 alias the corresponding GLM types.  GLM's memory layout
//   is natively compatible with SPIR-V/Vulkan push constants and vertex
//   attributes — sizeof(glm::vec3) == 12, packed, no padding — so the aliases
//   carry this guarantee forward without a reinterpret_cast.

#include <cstdint>
#include <glm/glm.hpp>

namespace ndde {

// ── Floating-point scalars ────────────────────────────────────────────────────

using f32  = float;           ///< IEEE-754 single  — GPU native
using f64  = double;          ///< IEEE-754 double  — simulation / analysis
using f128 = long double;     ///< Extended         — stochastic integration only

// ── Integer scalars ───────────────────────────────────────────────────────────

using byte = std::uint8_t; ///< Raw byte storage. Use char only for text/API boundaries.

using i8   = std::int8_t;
using i16  = std::int16_t;
using i32  = std::int32_t;
using i64  = std::int64_t;

using u8   = std::uint8_t;
using u16  = std::uint16_t;
using u32  = std::uint32_t;
using u64  = std::uint64_t;

// ── Linear algebra (GLM aliased) ──────────────────────────────────────────────
// Memory layout is identical to SPIR-V vec2/3/4/mat4. Safe to memcpy into
// Vulkan push constants and vertex buffers without any reinterpretation.

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat4 = glm::mat4;

// ── Draw / topology enums ─────────────────────────────────────────────────────
// Defined here (not in GpuTypes.hpp) so the math layer can use them in
// EngineAPI callbacks without pulling in Vulkan headers.

/// Selects which fragment colour source the pipeline uses.
enum class DrawMode : i32 {
    VertexColor  = 0,   ///< per-vertex colour attribute
    UniformColor = 1,   ///< PushConstants::uniform_color for every fragment
};

/// Topology hint passed alongside geometry slices so the renderer can select
/// the correct Vulkan pipeline without the math layer knowing about
/// VkPrimitiveTopology.
enum class Topology {
    LineList,       ///< axes, grids   — pairs of vertices form segments
    LineStrip,      ///< curves        — contiguous strip
    TriangleList,   ///< surfaces      — future use
};

} // namespace ndde
