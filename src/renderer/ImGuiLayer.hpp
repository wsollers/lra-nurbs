#pragma once
// renderer/ImGuiLayer.hpp
// Owns ImGui's descriptor pool and GLFW+Vulkan backends.
// GLFW/ImGui frame construction stays on the GUI/Main thread. Vulkan recording
// of finished ImGui draw data may run on the renderer thread once the engine
// has synchronised the frame boundary.

#include <volk.h>
#include <imgui.h>
#include "renderer/GpuTypes.hpp"
#include <string>

struct GLFWwindow;
namespace ndde::platform { class VulkanContext; }
namespace ndde::renderer { class Swapchain; }

namespace ndde::renderer {

class ImGuiLayer {
public:
    ImGuiLayer()  = default;
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&)            = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&)                 = delete;
    ImGuiLayer& operator=(ImGuiLayer&&)      = delete;

    void init(GLFWwindow*                    window,
              const platform::VulkanContext& ctx,
              const Swapchain&               swapchain,
              const std::string&             assets_dir);

    void destroy();
    void new_frame();
    void build_draw_data();
    void record_draw_data(VkCommandBuffer cmd);
    void render(VkCommandBuffer cmd);
    void on_swapchain_recreated(const Swapchain& swapchain);

    [[nodiscard]] ImFont* font_math_body()  const noexcept { return m_font_math_body;  }
    [[nodiscard]] ImFont* font_math_small() const noexcept { return m_font_math_small; }
    [[nodiscard]] bool    valid()           const noexcept { return m_initialised;      }

private:
    VkDevice         m_device      = VK_NULL_HANDLE;
    VkDescriptorPool m_pool        = VK_NULL_HANDLE;
    bool             m_initialised = false;
    ImFont*          m_font_math_body  = nullptr;
    ImFont*          m_font_math_small = nullptr;

    void load_fonts(const std::string& assets_dir);
};

} // namespace ndde::renderer
