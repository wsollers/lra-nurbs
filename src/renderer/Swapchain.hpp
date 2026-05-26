#pragma once
// renderer/Swapchain.hpp
// Manages the Vulkan swapchain. Canonical format: VK_FORMAT_B8G8R8A8_UNORM.
// Read-only after init(). Mutation requires vkDeviceWaitIdle from caller.

#include <volk.h>
#include <VkBootstrap.h>
#include "memory/Containers.hpp"
#include "platform/VulkanContext.hpp"
#include "renderer/GpuTypes.hpp"

namespace ndde::renderer {

class Swapchain {
public:
    Swapchain()  = default;
    ~Swapchain();

    Swapchain(const Swapchain&)            = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&)                 = delete;
    Swapchain& operator=(Swapchain&&)      = delete;

    void init(const platform::VulkanContext& ctx, u32 width, u32 height, bool vsync);
    void recreate(const platform::VulkanContext& ctx, u32 width, u32 height, bool vsync);
    void destroy();

    [[nodiscard]] VkSwapchainKHR                 swapchain()   const noexcept { return m_swapchain;   }
    [[nodiscard]] VkFormat                        format()      const noexcept { return m_format;      }
    [[nodiscard]] VkExtent2D                      extent()      const noexcept { return m_extent;      }
    [[nodiscard]] const memory::PersistentVector<VkImage>&     images()      const noexcept { return m_images;      }
    [[nodiscard]] const memory::PersistentVector<VkImageView>& image_views() const noexcept { return m_image_views; }
    [[nodiscard]] u32 image_count() const noexcept { return static_cast<u32>(m_images.size()); }

private:
    VkDevice               m_device    = VK_NULL_HANDLE;
    VkSwapchainKHR         m_swapchain = VK_NULL_HANDLE;
    VkFormat               m_format    = VK_FORMAT_UNDEFINED;
    VkExtent2D             m_extent    = {};
    memory::PersistentVector<VkImage>     m_images;
    memory::PersistentVector<VkImageView> m_image_views;

    void destroy_image_views();
};

} // namespace ndde::renderer
