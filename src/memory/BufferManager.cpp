// memory/BufferManager.cpp
#include "memory/BufferManager.hpp"
#include <stdexcept>
#include <format>
#include <iostream>

namespace ndde::memory {

BufferManager::~BufferManager() { destroy(); }

void BufferManager::init(VkDevice device, VkPhysicalDevice physical_device, u32 size_mb) {
    m_device    = device;
    m_pool_size = static_cast<u64>(size_mb) * 1024ull * 1024ull;

    VkBufferCreateInfo buf_info{
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = m_pool_size,
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(device, &buf_info, nullptr, &m_buffer) != VK_SUCCESS)
        throw std::runtime_error("[BufferManager] vkCreateBuffer failed");

    VkMemoryRequirements mem_req{};
    vkGetBufferMemoryRequirements(device, m_buffer, &mem_req);

    const u32 mem_type = find_memory_type(physical_device, mem_req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo alloc_info{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_req.size,
        .memoryTypeIndex = mem_type
    };
    if (vkAllocateMemory(device, &alloc_info, nullptr, &m_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        throw std::runtime_error("[BufferManager] vkAllocateMemory failed");
    }

    if (vkBindBufferMemory(device, m_buffer, m_memory, 0) != VK_SUCCESS) {
        vkFreeMemory(device, m_memory, nullptr);
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_memory = VK_NULL_HANDLE;
        m_buffer = VK_NULL_HANDLE;
        throw std::runtime_error("[BufferManager] vkBindBufferMemory failed");
    }

    if (vkMapMemory(device, m_memory, 0, m_pool_size, 0, &m_mapped_base) != VK_SUCCESS) {
        vkFreeMemory(device, m_memory, nullptr);
        vkDestroyBuffer(device, m_buffer, nullptr);
        m_memory = VK_NULL_HANDLE;
        m_buffer = VK_NULL_HANDLE;
        throw std::runtime_error("[BufferManager] vkMapMemory failed");
    }

    m_initialised = true;
    std::cout << std::format("[BufferManager] Arena: {} MiB\n", size_mb);
}

void BufferManager::destroy() {
    if (!m_initialised) return;
    if (m_mapped_base) { vkUnmapMemory(m_device, m_memory); m_mapped_base = nullptr; }
    if (m_buffer != VK_NULL_HANDLE) vkDestroyBuffer(m_device, m_buffer, nullptr);
    if (m_memory != VK_NULL_HANDLE) vkFreeMemory(m_device, m_memory, nullptr);
    m_buffer = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_initialised = false;
}

ArenaSlice BufferManager::acquire(u32 vertex_count) {
    const u64 raw_size     = static_cast<u64>(vertex_count) * sizeof(Vertex);
    const u64 aligned_size = (raw_size + k_alignment - 1u) & ~(k_alignment - 1u);
    const u64 start        = m_cursor.fetch_add(aligned_size, std::memory_order_relaxed);

    if (start + raw_size > m_pool_size)
        throw std::runtime_error(std::format(
            "[BufferManager] Arena overflow: requested {} bytes", raw_size));

    return ArenaSlice{
        .buffer       = m_buffer,
        .data         = static_cast<byte*>(m_mapped_base) + start,
        .byte_offset  = static_cast<u32>(start),
        .vertex_count = vertex_count
    };
}

void BufferManager::reset() noexcept {
    m_cursor.store(0, std::memory_order_release);
}

u64 BufferManager::bytes_used() const noexcept {
    return m_cursor.load(std::memory_order_relaxed);
}

f32 BufferManager::utilisation() const noexcept {
    return static_cast<f32>(bytes_used()) / static_cast<f32>(m_pool_size);
}

u32 BufferManager::find_memory_type(VkPhysicalDevice phys, u32 type_filter,
                                     VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mem_props);
    for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    throw std::runtime_error("[BufferManager] No suitable memory type found");
}

} // namespace ndde::memory
