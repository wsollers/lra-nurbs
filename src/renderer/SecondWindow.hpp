#pragma once
// renderer/SecondWindow.hpp
// A second OS window with its own Vulkan surface + swapchain,
// sharing the VkDevice/queues/pipelines of the primary window.

#include "memory/Containers.hpp"
#include "renderer/GpuTypes.hpp"
#include "renderer/Pipeline.hpp"
#include <volk.h>
#include <VkBootstrap.h>
#include <filesystem>
#include <optional>
#include <string>

struct GLFWwindow;
namespace ndde::platform { class VulkanContext; }

namespace ndde::renderer {

struct DrawCall;

class SecondWindow {
public:
    SecondWindow()  = default;
    ~SecondWindow();

    SecondWindow(const SecondWindow&)            = delete;
    SecondWindow& operator=(const SecondWindow&) = delete;
    SecondWindow(SecondWindow&&)                 = delete;
    SecondWindow& operator=(SecondWindow&&)      = delete;

    // x,y: OS screen position for top-left corner.
    void init(const platform::VulkanContext& ctx,
              int x, int y, u32 width, u32 height,
              const std::string& title,
              const std::string& shader_dir,
              bool vsync);

    void destroy();

    [[nodiscard]] bool begin_frame();
    void draw(const DrawCall& dc);
    [[nodiscard]] bool end_frame();
    void request_png_capture(std::filesystem::path path);

    void on_resize();

    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] bool valid()        const noexcept { return m_initialised; }
    [[nodiscard]] u32  width()        const noexcept { return m_sc_extent.width;  }
    [[nodiscard]] u32  height()       const noexcept { return m_sc_extent.height; }
    [[nodiscard]] GLFWwindow* window()const noexcept { return m_window; }
    [[nodiscard]] bool hovered() const noexcept;
    [[nodiscard]] bool mouse_button_down(int button) const noexcept;
    [[nodiscard]] ndde::Vec2 cursor_position() const noexcept;

private:
    GLFWwindow*      m_window          = nullptr;
    VkInstance       m_instance        = VK_NULL_HANDLE;  // borrowed
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;  // borrowed
    VkDevice         m_device          = VK_NULL_HANDLE;  // borrowed
    VkQueue          m_gfx_queue       = VK_NULL_HANDLE;  // borrowed
    VkQueue          m_present_queue   = VK_NULL_HANDLE;  // borrowed
    u32              m_gfx_family      = 0;
    VkSurfaceKHR     m_surface         = VK_NULL_HANDLE;  // owned

    // Raw swapchain state (bypass Swapchain class — it uses VulkanContext::surface())
    VkSwapchainKHR           m_sc_raw    = VK_NULL_HANDLE;
    VkFormat                 m_sc_format = VK_FORMAT_UNDEFINED;
    VkExtent2D               m_sc_extent = {};
    memory::PersistentVector<VkImage>     m_sc_images;
    memory::PersistentVector<VkImageView> m_sc_views;

    VkCommandPool   m_cmd_pool        = VK_NULL_HANDLE;
    VkCommandBuffer m_cmd             = VK_NULL_HANDLE;
    VkFence         m_render_fence    = VK_NULL_HANDLE;
    // Acquire semaphores are frame-slot indexed after the frame fence waits.
    // Render-finished semaphores are swapchain-image indexed because present
    // may keep using a semaphore until that same image is acquired again.
    memory::PersistentVector<VkSemaphore> m_image_available;
    memory::PersistentVector<VkSemaphore> m_render_finished;
    u32             m_image_index     = 0;
    u32             m_sync_index      = 0;
    u32             m_frame_sync      = 0;
    bool            m_frame_open      = false;
    bool            m_initialised     = false;
    bool            m_vsync           = true;

    Pipeline m_pipeline_line_list;
    Pipeline m_pipeline_line_strip;
    Pipeline m_pipeline_triangle_list;
    std::optional<std::filesystem::path> m_pending_capture;

    struct CaptureStagingBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize capacity_bytes = 0;
    };
    CaptureStagingBuffer m_capture_staging;

    void build_swapchain(u32 w, u32 h);
    void destroy_swapchain();
    void create_cmd_objects();
    void create_sync_objects();
    void init_pipelines(const std::string& shader_dir);
    void transition_image(VkImage image, VkImageLayout from, VkImageLayout to);
    void ensure_capture_staging_buffer(VkDeviceSize required_bytes);
    void destroy_capture_staging_buffer() noexcept;
    [[nodiscard]] u32 find_memory_type(u32 type_filter, VkMemoryPropertyFlags props) const;
    [[nodiscard]] Pipeline& pipeline_for(Topology topo);

    static void resize_callback(GLFWwindow* win, int w, int h);
};

} // namespace ndde::renderer
