#pragma once
// renderer/Renderer.hpp

#include <volk.h>
#include "platform/VulkanContext.hpp"
#include "renderer/Swapchain.hpp"
#include "renderer/Pipeline.hpp"
#include "renderer/ImGuiLayer.hpp"
#include "memory/ArenaSlice.hpp"
#include "memory/Containers.hpp"
#include "renderer/GpuTypes.hpp"
#include <glm/glm.hpp>
#include <filesystem>
#include <optional>
#include <string>

struct GLFWwindow;

namespace ndde::renderer {

struct DrawCall {
    memory::ArenaSlice slice;
    Topology           topology = Topology::LineStrip;
    DrawMode           mode     = DrawMode::VertexColor;
    Vec4               color    = { 1.f, 1.f, 1.f, 1.f };
    Mat4               mvp      = glm::mat4(1.f);
};

class Renderer {
public:
    Renderer()  = default;
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    void init(const platform::VulkanContext& ctx,
              const Swapchain&               swapchain,
              const std::string&             shader_dir,
              const std::string&             assets_dir,
              GLFWwindow*                    window);

    void destroy();

    [[nodiscard]] bool begin_frame(const Swapchain& swapchain);
    void draw(const DrawCall& dc);
    void imgui_new_frame() { m_imgui.new_frame(); }
    void imgui_build_draw_data() { m_imgui.build_draw_data(); }
    void imgui_record_draw_data() { m_imgui.record_draw_data(m_cmd); }
    void imgui_render()    { m_imgui.render(m_cmd); }
    [[nodiscard]] bool end_frame(const Swapchain& swapchain);
    void on_swapchain_recreated(const Swapchain& swapchain);
    void request_png_capture(std::filesystem::path path);

    // Call after vkDeviceWaitIdle (e.g. during scene switch) to put all
    // per-frame sync objects back into a clean known state. This destroys
    // and recreates the image_available semaphores so none are in a
    // pending-signal state from the previous scene's last acquire.
    void reset_frame_state();

    [[nodiscard]] ImFont* font_math_body()  const noexcept { return m_imgui.font_math_body();  }
    [[nodiscard]] ImFont* font_math_small() const noexcept { return m_imgui.font_math_small(); }

    // ── Stats ─────────────────────────────────────────────────────────────────
    // draw_call_count() returns the count from the *previous* completed frame.
    // The current in-progress frame count is in m_draw_calls_current.
    [[nodiscard]] u32 draw_call_count() const noexcept { return m_draw_calls_last; }

private:
    VkDevice        m_device         = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkQueue         m_graphics_queue = VK_NULL_HANDLE;
    VkQueue         m_present_queue  = VK_NULL_HANDLE;
    VkCommandPool   m_cmd_pool       = VK_NULL_HANDLE;
    VkCommandBuffer m_cmd            = VK_NULL_HANDLE;
    VkFence         m_render_fence    = VK_NULL_HANDLE;
    // Acquire semaphores are frame-slot indexed after the frame fence waits.
    // Render-finished semaphores are swapchain-image indexed because present
    // may keep using a semaphore until that same image is acquired again.
    memory::PersistentVector<VkSemaphore> m_image_available;  ///< signalled by vkAcquireNextImageKHR
    memory::PersistentVector<VkSemaphore> m_render_finished;  ///< signalled by vkQueueSubmit, waited by present
    u32             m_image_index    = 0;
    u32             m_sync_index     = 0;
    u32             m_frame_sync     = 0;
    bool            m_frame_open     = false;

    // Draw call counters: current frame accumulates; last frame is readable.
    u32 m_draw_calls_current = 0;
    u32 m_draw_calls_last    = 0;

    struct CaptureStagingBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize capacity_bytes = 0;
    };

    Pipeline   m_pipeline_line_list;
    Pipeline   m_pipeline_line_strip;
    Pipeline   m_pipeline_triangle_list;
    ImGuiLayer m_imgui;
    std::optional<std::filesystem::path> m_pending_capture;
    CaptureStagingBuffer m_capture_staging;

    void create_command_objects(u32 graphics_queue_family);
    void create_sync_objects(u32 image_count);
    void init_pipelines(VkFormat color_format, const std::string& shader_dir);
    void transition_image(VkImage image, VkImageLayout from, VkImageLayout to);
    void ensure_capture_staging_buffer(VkDeviceSize required_bytes);
    void destroy_capture_staging_buffer() noexcept;
    [[nodiscard]] u32 find_memory_type(u32 type_filter, VkMemoryPropertyFlags props) const;
    [[nodiscard]] Pipeline& pipeline_for(Topology topo);
};

} // namespace ndde::renderer
