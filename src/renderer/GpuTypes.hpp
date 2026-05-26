#pragma once
// renderer/GpuTypes.hpp
// Vulkan-specific GPU layout helpers and push constants.
//
// ── Why this file exists ──────────────────────────────────────────────────────
// math/Scalars.hpp is the pure-math scalar/vector header. It has zero Vulkan
// dependency and is safe to include from ndde_numeric and Python bindings.
//
// math/GeometryTypes.hpp defines Vertex without any Vulkan dependency.
// This file adds the Vulkan interpretation of that layout.
//
// ── Vertex layout ─────────────────────────────────────────────────────────────
// Binding 0, stride = sizeof(Vertex) = 28 bytes (no padding — packed f32s)
//   location 0: pos   (vec3,  VK_FORMAT_R32G32B32_SFLOAT,       offset  0)
//   location 1: color (vec4,  VK_FORMAT_R32G32B32A32_SFLOAT,    offset 12)
//
// A single vertex buffer is used for all geometry (axes, curves, surfaces,
// stochastic paths) because the per-vertex colour gives enough variety and
// avoids pipeline switches between heterogeneous draw calls.
//
// ── PushConstants layout (80 / 128 bytes) ────────────────────────────────────
//   Mat4 mvp             64 bytes — model-view-projection
//   Vec4 uniform_color   16 bytes — used when mode == UniformColor
//   i32  mode             4 bytes — 0: VertexColor, 1: UniformColor
//   [44 bytes headroom]
//
// The Vulkan spec guarantees at least 128 bytes of push constant space on
// every conformant device. The static_assert below enforces we never exceed it.

#include "math/GeometryTypes.hpp"
#include <array>
#include <volk.h>

namespace ndde {

// ── Vertex Vulkan Layout ─────────────────────────────────────────────────────

[[nodiscard]] inline VkVertexInputBindingDescription
vertex_binding_description() noexcept {
    return {
        .binding   = 0,
        .stride    = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
}

[[nodiscard]] inline std::array<VkVertexInputAttributeDescription, 2>
vertex_attribute_descriptions() noexcept {
    return {{
        { .location = 0, .binding = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, pos)   },
        { .location = 1, .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = offsetof(Vertex, color) }
    }};
}

// ── PushConstants ─────────────────────────────────────────────────────────────

struct PushConstants {
    Mat4 mvp;            ///< 64 bytes
    Vec4 uniform_color;  ///< 16 bytes  — total: 80 bytes
    i32  mode;           ///<  4 bytes  — total: 84 bytes
    // 44 bytes headroom remaining before the 128-byte spec limit
};
static_assert(sizeof(PushConstants) <= 128,
    "PushConstants exceeds the Vulkan guaranteed minimum push constant size");

} // namespace ndde
