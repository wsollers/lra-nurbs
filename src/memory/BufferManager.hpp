#pragma once
// memory/BufferManager.hpp
// Per-frame linear arena allocator backed by a single host-visible,
// host-coherent Vulkan buffer.
//
// Thread safety: acquire() is lock-free (atomic fetch_add on the cursor).
// reset() must be called only from the render thread between frames.

#include <atomic>
#include <volk.h>
#include "math/Scalars.hpp"
#include "memory/ArenaSlice.hpp"

namespace ndde::memory {

class BufferManager {
public:
    BufferManager() = default;
    ~BufferManager();

    BufferManager(const BufferManager&)            = delete;
    BufferManager& operator=(const BufferManager&) = delete;
    BufferManager(BufferManager&&)                  = delete;
    BufferManager& operator=(BufferManager&&)       = delete;

    void init(VkDevice device, VkPhysicalDevice physical_device, u32 size_mb = 128);
    void destroy();

    [[nodiscard]] ArenaSlice acquire(u32 vertex_count);
    void reset() noexcept;

    [[nodiscard]] u64 bytes_used()  const noexcept;
    [[nodiscard]] u64 bytes_total() const noexcept { return m_pool_size; }
    [[nodiscard]] f32 utilisation() const noexcept;

private:
    VkDevice         m_device       = VK_NULL_HANDLE;
    VkBuffer         m_buffer       = VK_NULL_HANDLE;
    VkDeviceMemory   m_memory       = VK_NULL_HANDLE;
    void*            m_mapped_base  = nullptr;
    u64              m_pool_size    = 0;
    std::atomic<u64> m_cursor{0};
    bool             m_initialised  = false;

    static constexpr u64 k_alignment = 64;

    [[nodiscard]] u32 find_memory_type(VkPhysicalDevice phys,
                                        u32 type_filter,
                                        VkMemoryPropertyFlags props) const;
};

} // namespace ndde::memory
