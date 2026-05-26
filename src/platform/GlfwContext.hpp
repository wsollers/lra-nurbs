#pragma once
// platform/GlfwContext.hpp
// Owns GLFW init, the window, and the resize flag.
// All methods must be called from the main thread (GLFW requirement).
// check_resize() is the one exception — safe from any thread.

#include "math/Scalars.hpp"
#include <atomic>
#include <string>

struct GLFWwindow;

namespace ndde::platform {

class GlfwContext {
public:
    GlfwContext() = default;
    ~GlfwContext();

    GlfwContext(const GlfwContext&)            = delete;
    GlfwContext& operator=(const GlfwContext&) = delete;
    GlfwContext(GlfwContext&&)                  = delete;
    GlfwContext& operator=(GlfwContext&&)       = delete;

    void init(u32 width, u32 height, const std::string& title);
    void destroy();

    void poll_events();
    [[nodiscard]] bool should_close() const noexcept;

    /// Returns true once per resize event, then resets atomically.
    [[nodiscard]] bool check_resize() noexcept;

    [[nodiscard]] u32        width()  const noexcept { return m_width.load(std::memory_order_relaxed);  }
    [[nodiscard]] u32        height() const noexcept { return m_height.load(std::memory_order_relaxed); }
    [[nodiscard]] GLFWwindow* window() const noexcept { return m_window; }
    void set_key_callback_user(void* user) noexcept { m_key_callback_user = user; }
    [[nodiscard]] void* key_callback_user() const noexcept { return m_key_callback_user; }

private:
    GLFWwindow*       m_window      = nullptr;
    void*             m_key_callback_user = nullptr;
    std::atomic<u32>  m_width{0};
    std::atomic<u32>  m_height{0};
    std::atomic<bool> m_resized{false};
    bool              m_initialised = false;

    static void framebuffer_resize_callback(GLFWwindow* win, int w, int h);
};

} // namespace ndde::platform
