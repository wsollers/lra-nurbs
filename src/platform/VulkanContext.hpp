#pragma once
// platform/VulkanContext.hpp
// Owns Vulkan instance, debug messenger, surface, physical device,
// logical device, and queues. Read-only after init() — all accessors
// are const and safe to call from any thread.

#include <volk.h>
#include <VkBootstrap.h>
#include <cstdint>
#include <string>

struct GLFWwindow;

namespace ndde::platform {

struct QueueFamilies {
    uint32_t graphics = UINT32_MAX;
    uint32_t present  = UINT32_MAX;
};

class VulkanContext {
public:
    VulkanContext()  = default;
    ~VulkanContext();

    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&)                  noexcept;
    VulkanContext& operator=(VulkanContext&&)       noexcept;

    void init(GLFWwindow* window, const std::string& app_name = "NDDE");
    void destroy();

    [[nodiscard]] VkInstance         instance()        const noexcept { return m_instance;        }
    [[nodiscard]] VkPhysicalDevice   physical_device() const noexcept { return m_physical_device; }
    [[nodiscard]] VkDevice           device()          const noexcept { return m_device;           }
    [[nodiscard]] VkSurfaceKHR       surface()         const noexcept { return m_surface;          }
    [[nodiscard]] VkQueue            graphics_queue()  const noexcept { return m_graphics_queue;   }
    [[nodiscard]] VkQueue            present_queue()   const noexcept { return m_present_queue;    }
    [[nodiscard]] QueueFamilies      queue_families()  const noexcept { return m_queue_families;   }
    [[nodiscard]] const vkb::Device& vkb_device()      const noexcept { return m_vkb_device;       }

private:
    VkInstance               m_instance         = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger  = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface          = VK_NULL_HANDLE;
    VkPhysicalDevice         m_physical_device  = VK_NULL_HANDLE;
    VkDevice                 m_device           = VK_NULL_HANDLE;
    VkQueue                  m_graphics_queue   = VK_NULL_HANDLE;
    VkQueue                  m_present_queue    = VK_NULL_HANDLE;
    QueueFamilies            m_queue_families   = {};
    vkb::Device              m_vkb_device       = {};
    bool                     m_initialised      = false;
};

} // namespace ndde::platform
