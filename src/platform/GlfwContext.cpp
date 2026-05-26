// platform/GlfwContext.cpp
#include "platform/GlfwContext.hpp"

#include <volk.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <format>
#include <stdexcept>
#include <iostream>

namespace ndde::platform {

GlfwContext::~GlfwContext() { destroy(); }

void GlfwContext::init(u32 width, u32 height, const std::string& title) {
    if (!glfwInit())
        throw std::runtime_error("[GLFW] glfwInit() failed");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED,  GLFW_TRUE);   // normal title bar + borders
    glfwWindowHint(GLFW_MAXIMIZED,  GLFW_TRUE);   // start maximised (windowed, not exclusive)

    // Use config width/height as the window's logical size.
    // GLFW will maximise on startup so the actual framebuffer will be the
    // desktop size, but the window remains an ordinary resizable window.
    m_window = glfwCreateWindow(static_cast<int>(width),
                                static_cast<int>(height),
                                title.c_str(), nullptr, nullptr);

    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("[GLFW] glfwCreateWindow() failed");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebuffer_resize_callback);

    // Read the actual framebuffer size post-maximise
    int fb_w = 0, fb_h = 0;
    glfwGetFramebufferSize(m_window, &fb_w, &fb_h);
    m_width.store(static_cast<u32>(fb_w),  std::memory_order_relaxed);
    m_height.store(static_cast<u32>(fb_h), std::memory_order_relaxed);
    m_initialised = true;

    std::cout << std::format("[GLFW] Window {}x{} (maximised): {}\n",
                             fb_w, fb_h, title);
}

void GlfwContext::destroy() {
    if (!m_initialised) return;
    if (m_window) { glfwDestroyWindow(m_window); m_window = nullptr; }
    glfwTerminate();
    m_initialised = false;
}

void GlfwContext::poll_events() { glfwPollEvents(); }

bool GlfwContext::should_close() const noexcept {
    return glfwWindowShouldClose(m_window) != 0;
}

bool GlfwContext::check_resize() noexcept {
    return m_resized.exchange(false, std::memory_order_acq_rel);
}

void GlfwContext::framebuffer_resize_callback(GLFWwindow* win, int w, int h) {
    auto* self = static_cast<GlfwContext*>(glfwGetWindowUserPointer(win));
    if (!self) return;
    self->m_width.store(static_cast<u32>(w),  std::memory_order_relaxed);
    self->m_height.store(static_cast<u32>(h), std::memory_order_relaxed);
    self->m_resized.store(true, std::memory_order_release);
}

} // namespace ndde::platform
