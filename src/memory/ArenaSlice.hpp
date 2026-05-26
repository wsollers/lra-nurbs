#pragma once
// memory/ArenaSlice.hpp
// The "receipt" returned by BufferManager::acquire().
// Exposes Vertex* without depending on Vulkan-specific layout helpers.

#include <volk.h>
#include "math/GeometryTypes.hpp"

namespace ndde::memory {

struct ArenaSlice {
    VkBuffer buffer       = VK_NULL_HANDLE;
    void*    data         = nullptr;
    u32      byte_offset  = 0;
    u32      vertex_count = 0;

    [[nodiscard]] Vertex* vertices() const noexcept {
        return static_cast<Vertex*>(data);
    }

    [[nodiscard]] bool valid() const noexcept {
        return buffer != VK_NULL_HANDLE && data != nullptr;
    }
};

} // namespace ndde::memory
