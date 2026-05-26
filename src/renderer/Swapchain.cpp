// renderer/Swapchain.cpp
#include "renderer/Swapchain.hpp"
#include <format>
#include <stdexcept>
#include <iostream>

namespace ndde::renderer {

Swapchain::~Swapchain() { destroy(); }

void Swapchain::init(const platform::VulkanContext& ctx, u32 width, u32 height, bool vsync) {
    m_device = ctx.device();

    vkb::SwapchainBuilder builder{ ctx.physical_device(), ctx.device(), ctx.surface() };
    const VkPresentModeKHR present_mode =
        vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;

    auto sc_ret = builder
        .set_desired_format({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(present_mode)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        .build();

    if (!sc_ret)
        throw std::runtime_error("[Swapchain] " + sc_ret.error().message());

    vkb::Swapchain vkb_sc = sc_ret.value();
    m_swapchain   = vkb_sc.swapchain;
    m_format      = vkb_sc.image_format;
    m_extent      = vkb_sc.extent;
    const auto images = vkb_sc.get_images().value();
    const auto image_views = vkb_sc.get_image_views().value();
    m_images.assign(images.begin(), images.end());
    m_image_views.assign(image_views.begin(), image_views.end());

    std::cout << std::format("[Swapchain] {}x{} images:{} format:{} present:{}\n",
        m_extent.width, m_extent.height, m_images.size(), static_cast<int>(m_format),
        vsync ? "fifo" : "mailbox");
}

void Swapchain::recreate(const platform::VulkanContext& ctx, u32 width, u32 height, bool vsync) {
    destroy();
    init(ctx, width, height, vsync);
}

void Swapchain::destroy() {
    if (m_device == VK_NULL_HANDLE) return;
    destroy_image_views();
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
    m_device = VK_NULL_HANDLE;
}

void Swapchain::destroy_image_views() {
    for (auto& iv : m_image_views)
        vkDestroyImageView(m_device, iv, nullptr);
    m_image_views.clear();
}

} // namespace ndde::renderer
