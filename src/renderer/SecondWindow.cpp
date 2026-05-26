// renderer/SecondWindow.cpp
#include "renderer/SecondWindow.hpp"
#include "renderer/Renderer.hpp"   // for DrawCall
#include "platform/VulkanContext.hpp"
#include "renderer/PngWriter.hpp"

#include <volk.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>
#include <format>
#include <cstdint>
#include <utility>
#include <vector>

namespace ndde::renderer {

namespace {

[[nodiscard]] std::vector<byte> convert_swapchain_pixels_to_rgba8(const byte* pixels,
                                                                  std::size_t pixel_count,
                                                                  VkFormat format) {
    std::vector<byte> rgba(pixel_count * 4u);
    const bool is_bgra = format == VK_FORMAT_B8G8R8A8_UNORM ||
                         format == VK_FORMAT_B8G8R8A8_SRGB;
    const bool is_rgba = format == VK_FORMAT_R8G8B8A8_UNORM ||
                         format == VK_FORMAT_R8G8B8A8_SRGB;
    if (!is_bgra && !is_rgba)
        throw std::runtime_error("[SecondWindow] unsupported capture swapchain format");

    for (std::size_t i = 0; i < pixel_count; ++i) {
        if (is_bgra) {
            rgba[i * 4u + 0u] = pixels[i * 4u + 2u];
            rgba[i * 4u + 1u] = pixels[i * 4u + 1u];
            rgba[i * 4u + 2u] = pixels[i * 4u + 0u];
            rgba[i * 4u + 3u] = pixels[i * 4u + 3u];
        } else {
            rgba[i * 4u + 0u] = pixels[i * 4u + 0u];
            rgba[i * 4u + 1u] = pixels[i * 4u + 1u];
            rgba[i * 4u + 2u] = pixels[i * 4u + 2u];
            rgba[i * 4u + 3u] = pixels[i * 4u + 3u];
        }
    }
    return rgba;
}

} // namespace

SecondWindow::~SecondWindow() { destroy(); }

// ── init ──────────────────────────────────────────────────────────────────────

void SecondWindow::init(const platform::VulkanContext& ctx,
                         int x, int y, u32 width, u32 height,
                         const std::string& title,
                         const std::string& shader_dir,
                         bool vsync)
{
    m_instance        = ctx.instance();
    m_physical_device = ctx.physical_device();
    m_device          = ctx.device();
    m_gfx_queue       = ctx.graphics_queue();
    m_present_queue   = ctx.present_queue();
    m_gfx_family      = ctx.queue_families().graphics;
    m_vsync           = vsync;

    // GLFW window — glfwInit already called by primary GlfwContext.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED,  GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED,  GLFW_FALSE);
    m_window = glfwCreateWindow(static_cast<int>(width),
                                static_cast<int>(height),
                                title.c_str(), nullptr, nullptr);
    if (!m_window)
        throw std::runtime_error("[SecondWindow] glfwCreateWindow failed");

    glfwSetWindowPos(m_window, x, y);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, resize_callback);

    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
        throw std::runtime_error("[SecondWindow] glfwCreateWindowSurface failed");

    int fw=0, fh=0;
    glfwGetFramebufferSize(m_window, &fw, &fh);
    build_swapchain(static_cast<u32>(fw), static_cast<u32>(fh));

    create_cmd_objects();
    create_sync_objects();
    init_pipelines(shader_dir);
    m_initialised = true;
    std::cout << "[SecondWindow] Ready: " << title << "\n";
}

// ── destroy ───────────────────────────────────────────────────────────────────

void SecondWindow::destroy() {
    if (!m_initialised) return;
    vkDeviceWaitIdle(m_device);

    m_pipeline_line_list.destroy();
    m_pipeline_line_strip.destroy();
    m_pipeline_triangle_list.destroy();
    destroy_capture_staging_buffer();

    if (m_render_fence    != VK_NULL_HANDLE) vkDestroyFence    (m_device, m_render_fence,    nullptr);
    for (auto& sem : m_image_available)
        vkDestroySemaphore(m_device, sem, nullptr);
    m_image_available.clear();
    for (auto& sem : m_render_finished)
        vkDestroySemaphore(m_device, sem, nullptr);
    m_render_finished.clear();
    if (m_cmd_pool        != VK_NULL_HANDLE) vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);

    destroy_swapchain();
    if (m_surface  != VK_NULL_HANDLE) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_window) { glfwDestroyWindow(m_window); m_window = nullptr; }

    m_render_fence    = VK_NULL_HANDLE;
    // m_image_available and m_render_finished already cleared above
    m_cmd_pool     = VK_NULL_HANDLE;
    m_surface      = VK_NULL_HANDLE;
    m_initialised  = false;
}

// ── begin_frame ───────────────────────────────────────────────────────────────

bool SecondWindow::begin_frame() {
    if (!m_initialised) return false;

    vkWaitForFences(m_device, 1, &m_render_fence, VK_TRUE, UINT64_MAX);

    m_frame_sync = m_sync_index;
    VkSemaphore acquire_sem = m_image_available[m_frame_sync];
    VkResult acq = vkAcquireNextImageKHR(
        m_device, m_sc_raw, UINT64_MAX, acquire_sem, VK_NULL_HANDLE, &m_image_index);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) { on_resize(); return false; }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) return false;

    vkResetFences(m_device, 1, &m_render_fence);
    vkResetCommandBuffer(m_cmd, 0);

    VkCommandBufferBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    if (vkBeginCommandBuffer(m_cmd, &begin) != VK_SUCCESS)
        throw std::runtime_error("[SecondWindow] vkBeginCommandBuffer failed");

    transition_image(m_sc_images[m_image_index],
                     VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo color_attach{
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = m_sc_views[m_image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = { .color = {{ 0.04f, 0.04f, 0.06f, 1.f }} }
    };
    VkRenderingInfo render_info{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = { {0,0}, m_sc_extent },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attach
    };
    vkCmdBeginRendering(m_cmd, &render_info);

    const f32 w = static_cast<f32>(m_sc_extent.width);
    const f32 h = static_cast<f32>(m_sc_extent.height);
    VkViewport vp{ .x=0.f,.y=h,.width=w,.height=-h,.minDepth=0.f,.maxDepth=1.f };
    VkRect2D   sc{ {0,0}, m_sc_extent };
    vkCmdSetViewport(m_cmd, 0, 1, &vp);
    vkCmdSetScissor (m_cmd, 0, 1, &sc);

    m_frame_open = true;
    return true;
}

// ── draw ──────────────────────────────────────────────────────────────────────

void SecondWindow::draw(const DrawCall& dc) {
    if (!m_frame_open || !dc.slice.valid()) return;
    Pipeline& pipe = pipeline_for(dc.topology);
    if (!pipe.valid()) return;
    vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline());
    PushConstants pc{
        .mvp           = dc.mvp,
        .uniform_color = dc.color,
        .mode          = static_cast<i32>(dc.mode)
    };
    vkCmdPushConstants(m_cmd, pipe.layout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &pc);
    VkDeviceSize off = static_cast<VkDeviceSize>(dc.slice.byte_offset);
    vkCmdBindVertexBuffers(m_cmd, 0, 1, &dc.slice.buffer, &off);
    vkCmdDraw(m_cmd, dc.slice.vertex_count, 1, 0, 0);
}

void SecondWindow::request_png_capture(std::filesystem::path path) {
    m_pending_capture = std::move(path);
}

// ── end_frame ─────────────────────────────────────────────────────────────────

bool SecondWindow::end_frame() {
    if (!m_frame_open) return false;
    vkCmdEndRendering(m_cmd);

    std::filesystem::path capture_path;

    const bool do_capture = m_pending_capture.has_value();
    if (do_capture) {
        const VkDeviceSize bytes =
            static_cast<VkDeviceSize>(m_sc_extent.width) * m_sc_extent.height * 4u;
        capture_path = std::move(*m_pending_capture);
        m_pending_capture.reset();
        ensure_capture_staging_buffer(bytes);

        transition_image(m_sc_images[m_image_index],
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {m_sc_extent.width, m_sc_extent.height, 1}
        };
        vkCmdCopyImageToBuffer(m_cmd, m_sc_images[m_image_index],
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               m_capture_staging.buffer, 1, &region);
        transition_image(m_sc_images[m_image_index],
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    } else {
        transition_image(m_sc_images[m_image_index],
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    if (vkEndCommandBuffer(m_cmd) != VK_SUCCESS)
        throw std::runtime_error("[SecondWindow] vkEndCommandBuffer failed");

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphore wait_sem   = m_image_available[m_frame_sync];
    VkSemaphore signal_sem = m_render_finished[m_image_index];
    VkSubmitInfo submit{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &wait_sem,
        .pWaitDstStageMask    = &wait_stage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &m_cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &signal_sem
    };
    if (vkQueueSubmit(m_gfx_queue, 1, &submit, m_render_fence) != VK_SUCCESS)
        throw std::runtime_error("[SecondWindow] vkQueueSubmit failed");

    if (do_capture) {
        vkWaitForFences(m_device, 1, &m_render_fence, VK_TRUE, UINT64_MAX);
        const std::size_t pixel_count =
            static_cast<std::size_t>(m_sc_extent.width) * m_sc_extent.height;
        const std::size_t byte_count = pixel_count * 4u;
        void* mapped = nullptr;
        if (vkMapMemory(m_device, m_capture_staging.memory, 0,
                        static_cast<VkDeviceSize>(byte_count), 0, &mapped) != VK_SUCCESS)
            throw std::runtime_error("[SecondWindow] capture vkMapMemory failed");

        const auto* pixels = static_cast<const byte*>(mapped);
        std::vector<byte> rgba = convert_swapchain_pixels_to_rgba8(pixels, pixel_count, m_sc_format);
        vkUnmapMemory(m_device, m_capture_staging.memory);
        write_png_rgba8(capture_path, m_sc_extent.width, m_sc_extent.height, rgba);
        std::cout << "[SecondWindow] Wrote capture: " << capture_path.string() << "\n";
    }

    VkPresentInfoKHR present{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &signal_sem,
        .swapchainCount     = 1,
        .pSwapchains        = &m_sc_raw,
        .pImageIndices      = &m_image_index
    };
    VkResult pr = vkQueuePresentKHR(m_present_queue, &present);
    m_frame_open = false;
    m_sync_index = (m_sync_index + 1u) % static_cast<u32>(m_image_available.size());
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) { on_resize(); return false; }
    return pr == VK_SUCCESS;
}

// ── on_resize ─────────────────────────────────────────────────────────────────

void SecondWindow::on_resize() {
    int fw=0, fh=0;
    glfwGetFramebufferSize(m_window, &fw, &fh);
    if (fw==0 || fh==0) return;
    vkDeviceWaitIdle(m_device);
    if (m_render_fence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device, m_render_fence, nullptr);
        m_render_fence = VK_NULL_HANDLE;
    }
    for (auto& sem : m_image_available)
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(m_device, sem, nullptr);
    m_image_available.clear();
    for (auto& sem : m_render_finished)
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(m_device, sem, nullptr);
    m_render_finished.clear();
    destroy_swapchain();
    build_swapchain(static_cast<u32>(fw), static_cast<u32>(fh));
    create_sync_objects();
    m_image_index = 0;
    m_sync_index  = 0;
    m_frame_sync  = 0;
    m_frame_open  = false;
}

bool SecondWindow::should_close() const noexcept {
    return m_window && glfwWindowShouldClose(m_window);
}

bool SecondWindow::hovered() const noexcept {
    return m_window && glfwGetWindowAttrib(m_window, GLFW_HOVERED) == GLFW_TRUE;
}

bool SecondWindow::mouse_button_down(int button) const noexcept {
    return m_window && glfwGetMouseButton(m_window, button) == GLFW_PRESS;
}

ndde::Vec2 SecondWindow::cursor_position() const noexcept {
    if (!m_window) return {};
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(m_window, &x, &y);
    return {static_cast<f32>(x), static_cast<f32>(y)};
}

// ── Private: build_swapchain ──────────────────────────────────────────────────

void SecondWindow::build_swapchain(u32 w, u32 h) {
    vkb::SwapchainBuilder builder{ m_physical_device, m_device, m_surface };
    auto sc_ret = builder
        .set_desired_format({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(m_vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR)
        .set_desired_extent(w, h)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build();
    if (!sc_ret)
        throw std::runtime_error("[SecondWindow] build_swapchain: " + sc_ret.error().message());

    vkb::Swapchain vkb_sc = sc_ret.value();
    m_sc_raw    = vkb_sc.swapchain;
    m_sc_format = vkb_sc.image_format;
    m_sc_extent = vkb_sc.extent;
    const auto images = vkb_sc.get_images().value();
    const auto views = vkb_sc.get_image_views().value();
    m_sc_images.assign(images.begin(), images.end());
    m_sc_views.assign(views.begin(), views.end());
    std::cout << std::format("[SecondWindow] Swapchain {}x{} images:{} present:{}\n",
        m_sc_extent.width, m_sc_extent.height, m_sc_images.size(),
        m_vsync ? "fifo" : "mailbox");
}

void SecondWindow::destroy_swapchain() {
    for (auto iv : m_sc_views)
        vkDestroyImageView(m_device, iv, nullptr);
    m_sc_views.clear();
    if (m_sc_raw != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_sc_raw, nullptr);
        m_sc_raw = VK_NULL_HANDLE;
    }
}

void SecondWindow::create_cmd_objects() {
    VkCommandPoolCreateInfo ci{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_gfx_family
    };
    if (vkCreateCommandPool(m_device, &ci, nullptr, &m_cmd_pool) != VK_SUCCESS)
        throw std::runtime_error("[SecondWindow] vkCreateCommandPool failed");
    VkCommandBufferAllocateInfo ai{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = m_cmd_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    if (vkAllocateCommandBuffers(m_device, &ai, &m_cmd) != VK_SUCCESS)
        throw std::runtime_error("[SecondWindow] vkAllocateCommandBuffers failed");
}

void SecondWindow::create_sync_objects() {
    VkFenceCreateInfo fi{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    if (vkCreateFence(m_device, &fi, nullptr, &m_render_fence) != VK_SUCCESS)
        throw std::runtime_error("[SecondWindow] vkCreateFence failed");
    VkSemaphoreCreateInfo si{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    // One semaphore per swapchain image — m_sc_images is already populated.
    m_image_available.resize(m_sc_images.size(), VK_NULL_HANDLE);
    for (auto& sem : m_image_available)
        if (vkCreateSemaphore(m_device, &si, nullptr, &sem) != VK_SUCCESS)
            throw std::runtime_error("[SecondWindow] vkCreateSemaphore (image_available) failed");
    m_render_finished.resize(m_sc_images.size(), VK_NULL_HANDLE);
    for (auto& sem : m_render_finished)
        if (vkCreateSemaphore(m_device, &si, nullptr, &sem) != VK_SUCCESS)
            throw std::runtime_error("[SecondWindow] vkCreateSemaphore (render_finished) failed");
}

void SecondWindow::init_pipelines(const std::string& shader_dir) {
    const std::string vert = shader_dir + "/curve.vert.spv";
    const std::string frag = shader_dir + "/curve.frag.spv";
    m_pipeline_line_list.init    (m_device, m_sc_format, vert, frag, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    m_pipeline_line_strip.init   (m_device, m_sc_format, vert, frag, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
    m_pipeline_triangle_list.init(m_device, m_sc_format, vert, frag, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
}

void SecondWindow::transition_image(VkImage image, VkImageLayout from, VkImageLayout to) {
    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkAccessFlags src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkAccessFlags dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (from == VK_IMAGE_LAYOUT_UNDEFINED && to == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        src_access = 0;
        dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    } else if (from == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && to == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dst_access = 0;
    } else if (from == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && to == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dst_access = VK_ACCESS_TRANSFER_READ_BIT;
    } else if (from == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && to == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        src_access = VK_ACCESS_TRANSFER_READ_BIT;
        dst_access = 0;
    }

    VkImageMemoryBarrier b{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = src_access,
        .dstAccessMask       = dst_access,
        .oldLayout           = from, .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1 }
    };
    vkCmdPipelineBarrier(m_cmd, src_stage, dst_stage,
        0, 0, nullptr, 0, nullptr, 1, &b);
}

void SecondWindow::ensure_capture_staging_buffer(VkDeviceSize required_bytes) {
    if (m_capture_staging.buffer != VK_NULL_HANDLE &&
        m_capture_staging.memory != VK_NULL_HANDLE &&
        m_capture_staging.capacity_bytes >= required_bytes) {
        return;
    }

    destroy_capture_staging_buffer();

    VkBufferCreateInfo bi{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = required_bytes,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(m_device, &bi, nullptr, &m_capture_staging.buffer) != VK_SUCCESS)
        throw std::runtime_error("[SecondWindow] capture vkCreateBuffer failed");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(m_device, m_capture_staging.buffer, &req);
    VkMemoryAllocateInfo ai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = find_memory_type(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    if (vkAllocateMemory(m_device, &ai, nullptr, &m_capture_staging.memory) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, m_capture_staging.buffer, nullptr);
        m_capture_staging.buffer = VK_NULL_HANDLE;
        throw std::runtime_error("[SecondWindow] capture vkAllocateMemory failed");
    }
    if (vkBindBufferMemory(m_device, m_capture_staging.buffer, m_capture_staging.memory, 0) != VK_SUCCESS) {
        destroy_capture_staging_buffer();
        throw std::runtime_error("[SecondWindow] capture vkBindBufferMemory failed");
    }
    m_capture_staging.capacity_bytes = required_bytes;
}

void SecondWindow::destroy_capture_staging_buffer() noexcept {
    if (m_device == VK_NULL_HANDLE) return;
    if (m_capture_staging.memory != VK_NULL_HANDLE)
        vkFreeMemory(m_device, m_capture_staging.memory, nullptr);
    if (m_capture_staging.buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_device, m_capture_staging.buffer, nullptr);
    m_capture_staging = {};
}

u32 SecondWindow::find_memory_type(u32 type_filter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_props);
    for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    throw std::runtime_error("[SecondWindow] No suitable capture memory type found");
}

Pipeline& SecondWindow::pipeline_for(Topology topo) {
    switch (topo) {
        case Topology::LineList:     return m_pipeline_line_list;
        case Topology::LineStrip:    return m_pipeline_line_strip;
        case Topology::TriangleList: return m_pipeline_triangle_list;
    }
    throw std::runtime_error("[SecondWindow] Unknown topology");
}

void SecondWindow::resize_callback(GLFWwindow* win, int, int) {
    auto* self = static_cast<SecondWindow*>(glfwGetWindowUserPointer(win));
    if (self) self->on_resize();
}

} // namespace ndde::renderer
