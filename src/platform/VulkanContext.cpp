// platform/VulkanContext.cpp
#include "platform/VulkanContext.hpp"

#include <volk.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <format>
#include <stdexcept>
#include <iostream>

namespace ndde::platform {

VulkanContext::~VulkanContext() { destroy(); }

VulkanContext::VulkanContext(VulkanContext&& o) noexcept
    : m_instance       (o.m_instance)
    , m_debug_messenger(o.m_debug_messenger)
    , m_surface        (o.m_surface)
    , m_physical_device(o.m_physical_device)
    , m_device         (o.m_device)
    , m_graphics_queue (o.m_graphics_queue)
    , m_present_queue  (o.m_present_queue)
    , m_queue_families (o.m_queue_families)
    , m_vkb_device     (std::move(o.m_vkb_device))
    , m_initialised    (o.m_initialised)
{
    o.m_instance        = VK_NULL_HANDLE;
    o.m_debug_messenger = VK_NULL_HANDLE;
    o.m_surface         = VK_NULL_HANDLE;
    o.m_physical_device = VK_NULL_HANDLE;
    o.m_device          = VK_NULL_HANDLE;
    o.m_graphics_queue  = VK_NULL_HANDLE;
    o.m_present_queue   = VK_NULL_HANDLE;
    o.m_initialised     = false;
}

VulkanContext& VulkanContext::operator=(VulkanContext&& o) noexcept {
    if (this != &o) {
        destroy();
        std::swap(m_instance,        o.m_instance);
        std::swap(m_debug_messenger, o.m_debug_messenger);
        std::swap(m_surface,         o.m_surface);
        std::swap(m_physical_device, o.m_physical_device);
        std::swap(m_device,          o.m_device);
        std::swap(m_graphics_queue,  o.m_graphics_queue);
        std::swap(m_present_queue,   o.m_present_queue);
        std::swap(m_queue_families,  o.m_queue_families);
        std::swap(m_vkb_device,      o.m_vkb_device);
        std::swap(m_initialised,     o.m_initialised);
    }
    return *this;
}

void VulkanContext::init(GLFWwindow* window, const std::string& app_name) {
    vkb::InstanceBuilder inst_builder;
    auto inst_ret = inst_builder
        .set_app_name(app_name.c_str())
        .request_validation_layers(true)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();
    if (!inst_ret)
        throw std::runtime_error("[VulkanContext] Instance: " + inst_ret.error().message());

    vkb::Instance vkb_inst = inst_ret.value();
    m_instance        = vkb_inst.instance;
    m_debug_messenger = vkb_inst.debug_messenger;
    volkLoadInstance(m_instance);

    if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS)
        throw std::runtime_error("[VulkanContext] Failed to create window surface");

    VkPhysicalDeviceVulkan13Features features13{
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    vkb::PhysicalDeviceSelector phys_sel{vkb_inst};
    auto phys_ret = phys_sel
        .set_surface(m_surface)
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .select();
    if (!phys_ret)
        throw std::runtime_error("[VulkanContext] Physical device: " + phys_ret.error().message());

    m_physical_device = phys_ret.value().physical_device;
    std::cout << std::format("[Vulkan] GPU: {}\n", phys_ret.value().properties.deviceName);

    vkb::DeviceBuilder dev_builder{phys_ret.value()};
    auto dev_ret = dev_builder.build();
    if (!dev_ret)
        throw std::runtime_error("[VulkanContext] Device: " + dev_ret.error().message());

    m_vkb_device = dev_ret.value();
    m_device     = m_vkb_device.device;

    auto gq = m_vkb_device.get_queue(vkb::QueueType::graphics);
    auto pq = m_vkb_device.get_queue(vkb::QueueType::present);
    if (!gq || !pq) throw std::runtime_error("[VulkanContext] Failed to get queues");

    m_graphics_queue          = gq.value();
    m_present_queue           = pq.value();
    m_queue_families.graphics = m_vkb_device.get_queue_index(vkb::QueueType::graphics).value();
    m_queue_families.present  = m_vkb_device.get_queue_index(vkb::QueueType::present).value();
    m_initialised = true;
}

void VulkanContext::destroy() {
    if (!m_initialised) return;
    vkDestroyDevice(m_device, nullptr);
    vkb::destroy_debug_utils_messenger(m_instance, m_debug_messenger);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
    m_device          = VK_NULL_HANDLE;
    m_surface         = VK_NULL_HANDLE;
    m_debug_messenger = VK_NULL_HANDLE;
    m_instance        = VK_NULL_HANDLE;
    m_initialised     = false;
}

} // namespace ndde::platform
