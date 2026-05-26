// renderer/Renderer.cpp
#include "renderer/Renderer.hpp"
#include "renderer/PngWriter.hpp"
#include <stdexcept>
#include <iostream>
#include <format>
#include <cstring>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

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
        throw std::runtime_error("[Renderer] unsupported capture swapchain format");

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

Renderer::~Renderer() { destroy(); }

void Renderer::init(const platform::VulkanContext& ctx,
                    const Swapchain&               swapchain,
                    const std::string&             shader_dir,
                    const std::string&             assets_dir,
                    GLFWwindow*                    window)
{
    m_device         = ctx.device();
    m_physical_device = ctx.physical_device();
    m_graphics_queue = ctx.graphics_queue();
    m_present_queue  = ctx.present_queue();

    create_command_objects(ctx.queue_families().graphics);
    create_sync_objects(swapchain.image_count());
    init_pipelines(swapchain.format(), shader_dir);
    m_imgui.init(window, ctx, swapchain, assets_dir);

    std::cout << "[Renderer] Initialised\n";
}

void Renderer::destroy() {
    if (m_device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(m_device);

    m_imgui.destroy();
    m_pipeline_line_list.destroy();
    m_pipeline_line_strip.destroy();
    m_pipeline_triangle_list.destroy();
    destroy_capture_staging_buffer();

    if (m_render_fence    != VK_NULL_HANDLE) vkDestroyFence(m_device, m_render_fence, nullptr);
    for (auto& sem : m_image_available)
        vkDestroySemaphore(m_device, sem, nullptr);
    m_image_available.clear();
    for (auto& sem : m_render_finished)
        vkDestroySemaphore(m_device, sem, nullptr);
    m_render_finished.clear();
    if (m_cmd_pool        != VK_NULL_HANDLE) vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);

    m_render_fence    = VK_NULL_HANDLE;
    m_cmd_pool        = VK_NULL_HANDLE;
    m_cmd             = VK_NULL_HANDLE;
    m_physical_device = VK_NULL_HANDLE;
    m_device          = VK_NULL_HANDLE;
}

void Renderer::request_png_capture(std::filesystem::path path) {
    m_pending_capture = std::move(path);
}

bool Renderer::begin_frame(const Swapchain& swapchain) {
    vkWaitForFences(m_device, 1, &m_render_fence, VK_TRUE, UINT64_MAX);

    m_frame_sync = m_sync_index;
    VkSemaphore acquire_sem = m_image_available[m_frame_sync];
    VkResult acquire = vkAcquireNextImageKHR(
        m_device, swapchain.swapchain(), UINT64_MAX,
        acquire_sem, VK_NULL_HANDLE, &m_image_index);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) return false;
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("[Renderer] vkAcquireNextImageKHR failed");

    vkResetFences(m_device, 1, &m_render_fence);
    vkResetCommandBuffer(m_cmd, 0);

    // Reset per-frame draw call counter
    m_draw_calls_current = 0;

    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    if (vkBeginCommandBuffer(m_cmd, &begin_info) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] vkBeginCommandBuffer failed");

    transition_image(swapchain.images()[m_image_index],
                     VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo color_attach{
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = swapchain.image_views()[m_image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = { .color = {{ 0.05f, 0.05f, 0.08f, 1.0f }} }
    };

    VkExtent2D ext = swapchain.extent();
    VkRenderingInfo render_info{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = { {0, 0}, ext },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attach
    };
    vkCmdBeginRendering(m_cmd, &render_info);

    // Negative-height VkViewport: aligns Vulkan NDC Y with glm::ortho convention.
    // NDC Y=+1 → screen top,  NDC Y=-1 → screen bottom.
    // This makes world +Y render at the screen top and fixes the epsilon-ball
    // tracking inversion described in docs/COORDINATE_SYSTEMS.md.
    const float w = static_cast<f32>(ext.width);
    const float h = static_cast<f32>(ext.height);
    VkViewport viewport{
        .x = 0.f, .y = h, .width = w, .height = -h,
        .minDepth = 0.f, .maxDepth = 1.f
    };
    vkCmdSetViewport(m_cmd, 0, 1, &viewport);

    VkRect2D scissor{ {0, 0}, ext };
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);

    m_frame_open = true;
    return true;
}

void Renderer::draw(const DrawCall& dc) {
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

    VkDeviceSize offset = static_cast<VkDeviceSize>(dc.slice.byte_offset);
    vkCmdBindVertexBuffers(m_cmd, 0, 1, &dc.slice.buffer, &offset);
    vkCmdDraw(m_cmd, dc.slice.vertex_count, 1, 0, 0);

    ++m_draw_calls_current;
}

bool Renderer::end_frame(const Swapchain& swapchain) {
    // Latch completed frame's draw call count before presenting
    m_draw_calls_last = m_draw_calls_current;

    vkCmdEndRendering(m_cmd);

    std::filesystem::path capture_path;

    const bool do_capture = m_pending_capture.has_value();
    if (do_capture) {
        const VkExtent2D ext = swapchain.extent();
        const VkDeviceSize bytes = static_cast<VkDeviceSize>(ext.width) * ext.height * 4u;
        capture_path = std::move(*m_pending_capture);
        m_pending_capture.reset();
        ensure_capture_staging_buffer(bytes);

        transition_image(swapchain.images()[m_image_index],
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
            .imageExtent = {ext.width, ext.height, 1}
        };
        vkCmdCopyImageToBuffer(m_cmd, swapchain.images()[m_image_index],
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               m_capture_staging.buffer, 1, &region);
        transition_image(swapchain.images()[m_image_index],
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    } else {
        transition_image(swapchain.images()[m_image_index],
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    if (vkEndCommandBuffer(m_cmd) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] vkEndCommandBuffer failed");

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
    if (vkQueueSubmit(m_graphics_queue, 1, &submit, m_render_fence) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] vkQueueSubmit failed");

    if (do_capture) {
        vkWaitForFences(m_device, 1, &m_render_fence, VK_TRUE, UINT64_MAX);
        const VkExtent2D ext = swapchain.extent();
        const std::size_t pixel_count = static_cast<std::size_t>(ext.width) * ext.height;
        const std::size_t byte_count = pixel_count * 4u;
        void* mapped = nullptr;
        if (vkMapMemory(m_device, m_capture_staging.memory, 0, static_cast<VkDeviceSize>(byte_count), 0, &mapped) != VK_SUCCESS)
            throw std::runtime_error("[Renderer] capture vkMapMemory failed");

        const auto* pixels = static_cast<const byte*>(mapped);
        std::vector<byte> rgba = convert_swapchain_pixels_to_rgba8(pixels, pixel_count, swapchain.format());
        vkUnmapMemory(m_device, m_capture_staging.memory);
        write_png_rgba8(capture_path, ext.width, ext.height, rgba);
        std::cout << "[Renderer] Wrote capture: " << capture_path.string() << "\n";
    }

    VkSwapchainKHR sc = swapchain.swapchain();
    VkPresentInfoKHR present{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &signal_sem,
        .swapchainCount     = 1,
        .pSwapchains        = &sc,
        .pImageIndices      = &m_image_index
    };

    VkResult pr = vkQueuePresentKHR(m_present_queue, &present);
    m_frame_open = false;
    m_sync_index = (m_sync_index + 1u) % static_cast<u32>(m_image_available.size());

    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) return false;
    if (pr != VK_SUCCESS) throw std::runtime_error("[Renderer] vkQueuePresentKHR failed");
    return true;
}

void Renderer::on_swapchain_recreated(const Swapchain& swapchain) {
    m_imgui.on_swapchain_recreated(swapchain);
}

void Renderer::reset_frame_state() {
    // Called after vkDeviceWaitIdle when the active simulation changes.
    // Destroy and recreate both semaphore vectors so every slot is
    // cleanly unsignaled before the new scene's first acquire.
    const VkSemaphoreCreateInfo si{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (auto& sem : m_image_available) {
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(m_device, sem, nullptr);
        if (vkCreateSemaphore(m_device, &si, nullptr, &sem) != VK_SUCCESS)
            throw std::runtime_error("[Renderer] vkCreateSemaphore (image_available reset) failed");
    }
    for (auto& sem : m_render_finished) {
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(m_device, sem, nullptr);
        if (vkCreateSemaphore(m_device, &si, nullptr, &sem) != VK_SUCCESS)
            throw std::runtime_error("[Renderer] vkCreateSemaphore (render_finished reset) failed");
    }
    // Recreate the fence in signaled state so begin_frame's WaitForFences
    // returns immediately on the first frame of the new scene.
    const VkFenceCreateInfo fi{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    vkDestroyFence(m_device, m_render_fence, nullptr);
    if (vkCreateFence(m_device, &fi, nullptr, &m_render_fence) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] vkCreateFence (reset) failed");
    m_image_index = 0;
    m_sync_index  = 0;
    m_frame_sync  = 0;
    m_frame_open  = false;
}

void Renderer::create_command_objects(u32 graphics_queue_family) {
    VkCommandPoolCreateInfo pool_info{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_queue_family
    };
    if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_cmd_pool) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] vkCreateCommandPool failed");

    VkCommandBufferAllocateInfo alloc_info{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = m_cmd_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    if (vkAllocateCommandBuffers(m_device, &alloc_info, &m_cmd) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] vkAllocateCommandBuffers failed");
}

void Renderer::create_sync_objects(u32 image_count) {
    VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    if (vkCreateFence(m_device, &fence_info, nullptr, &m_render_fence) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] vkCreateFence failed");

    VkSemaphoreCreateInfo sem_info{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    // One acquire semaphore per swapchain image so each image slot has its
    // own semaphore. This prevents the validation error where the same
    // semaphore is re-signalled by vkAcquireNextImageKHR before presentation
    // has finished consuming it from the previous frame.
    m_image_available.resize(image_count, VK_NULL_HANDLE);
    for (auto& sem : m_image_available)
        if (vkCreateSemaphore(m_device, &sem_info, nullptr, &sem) != VK_SUCCESS)
            throw std::runtime_error("[Renderer] vkCreateSemaphore (image_available) failed");

    m_render_finished.resize(image_count, VK_NULL_HANDLE);
    for (auto& sem : m_render_finished)
        if (vkCreateSemaphore(m_device, &sem_info, nullptr, &sem) != VK_SUCCESS)
            throw std::runtime_error("[Renderer] vkCreateSemaphore (render_finished) failed");
}

void Renderer::init_pipelines(VkFormat color_format, const std::string& shader_dir) {
    const std::string vert = shader_dir + "/curve.vert.spv";
    const std::string frag = shader_dir + "/curve.frag.spv";

    m_pipeline_line_list.init(m_device, color_format, vert, frag,
                               VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    m_pipeline_line_strip.init(m_device, color_format, vert, frag,
                                VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
    m_pipeline_triangle_list.init(m_device, color_format, vert, frag,
                                   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    std::cout << "[Renderer] Pipelines: LineList, LineStrip, TriangleList\n";
}

void Renderer::transition_image(VkImage image, VkImageLayout from, VkImageLayout to) {
    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkAccessFlags src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkAccessFlags dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (from == VK_IMAGE_LAYOUT_UNDEFINED && to == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        src_access = 0;
        dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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
    } else if (from == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && to == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dst_access = 0;
    }

    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = src_access,
        .dstAccessMask       = dst_access,
        .oldLayout           = from,
        .newLayout           = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };
    vkCmdPipelineBarrier(m_cmd, src_stage, dst_stage,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void Renderer::ensure_capture_staging_buffer(VkDeviceSize required_bytes) {
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
        throw std::runtime_error("[Renderer] capture vkCreateBuffer failed");

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
        throw std::runtime_error("[Renderer] capture vkAllocateMemory failed");
    }
    if (vkBindBufferMemory(m_device, m_capture_staging.buffer, m_capture_staging.memory, 0) != VK_SUCCESS) {
        destroy_capture_staging_buffer();
        throw std::runtime_error("[Renderer] capture vkBindBufferMemory failed");
    }
    m_capture_staging.capacity_bytes = required_bytes;
}

void Renderer::destroy_capture_staging_buffer() noexcept {
    if (m_device == VK_NULL_HANDLE) return;
    if (m_capture_staging.memory != VK_NULL_HANDLE)
        vkFreeMemory(m_device, m_capture_staging.memory, nullptr);
    if (m_capture_staging.buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_device, m_capture_staging.buffer, nullptr);
    m_capture_staging = {};
}

u32 Renderer::find_memory_type(u32 type_filter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_props);
    for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    throw std::runtime_error("[Renderer] No suitable capture memory type found");
}

Pipeline& Renderer::pipeline_for(Topology topo) {
    switch (topo) {
        case Topology::LineList:     return m_pipeline_line_list;
        case Topology::LineStrip:    return m_pipeline_line_strip;
        case Topology::TriangleList: return m_pipeline_triangle_list;
    }
    throw std::runtime_error("[Renderer] Unknown topology");
}

} // namespace ndde::renderer
