#pragma once
// engine/CanvasInput.hpp
// Generic ImGui canvas input capture. Scenes decide how to apply the input.

#include <imgui.h>

namespace ndde {

struct CanvasInputFrame {
    ImVec2 pos{};
    ImVec2 size{};
    bool hovered = false;
    float mouse_wheel = 0.f;
    ImVec2 mouse_delta{};
    bool orbit_drag = false;
};

inline CanvasInputFrame begin_canvas_input(const char* id, ImVec2 size) {
    CanvasInputFrame frame;
    frame.pos = ImGui::GetCursorScreenPos();
    frame.size = size;

    ImGui::InvisibleButton(id, size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

    frame.hovered = ImGui::IsItemHovered();
    if (frame.hovered) {
        const ImGuiIO& io = ImGui::GetIO();
        frame.mouse_wheel = io.MouseWheel;
        frame.mouse_delta = io.MouseDelta;
        frame.orbit_drag = ImGui::IsMouseDragging(ImGuiMouseButton_Right)
                         || ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
    }
    return frame;
}

} // namespace ndde
