#include "memory/MemoryService.hpp"

namespace ndde::memory {

MemoryService::MemoryService() {
    m_view.set_resource(&m_view_cpu);
    m_simulation.set_resource(&m_simulation_cpu);
    m_cache.set_resource(&m_cache_cpu);
    m_history.set_resource(&m_history_cpu);
    m_persistent.set_resource(&m_persistent_cpu);
}

void MemoryService::init_frame_gpu_arena(VkDevice device, VkPhysicalDevice physical_device, u32 size_mb) {
    m_frame_gpu.init(device, physical_device, size_mb);
}

void MemoryService::destroy() {
    m_frame_gpu.destroy();
}

void MemoryService::begin_frame() noexcept {
    m_frame.reset();
    m_frame_cpu.release();
    m_frame.set_resource(&m_frame_cpu);
    m_frame_gpu.reset();
}

void MemoryService::reset_view() noexcept {
    m_view.reset();
    m_view_cpu.release();
    m_view.set_resource(&m_view_cpu);
}

void MemoryService::reset_simulation() noexcept {
    m_simulation.reset();
    m_simulation_cpu.release();
    m_simulation.set_resource(&m_simulation_cpu);
}

void MemoryService::reset_cache() noexcept {
    m_cache.reset();
    m_cache_cpu.release();
    m_cache.set_resource(&m_cache_cpu);
}

void MemoryService::reset_history() noexcept {
    m_history.reset();
    m_history_cpu.release();
    m_history.set_resource(&m_history_cpu);
}

} // namespace ndde::memory
