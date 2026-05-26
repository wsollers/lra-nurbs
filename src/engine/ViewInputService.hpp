#pragma once
// engine/ViewInputService.hpp
// Per-render-view pointer ownership and camera-input sampling.

#include "engine/RenderService.hpp"
#include "engine/threading/ThreadManagementService.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

namespace ndde {

struct ViewInputRect {
    Vec2 origin{};
    Vec2 size{};
};

struct ViewPointerButtons {
    bool left_click = false;
    bool left_double_click = false;
    bool right_down = false;
    bool middle_down = false;
    bool shift_down = false;
};

struct ViewInputSample {
    RenderViewId view = 0;
    Vec2 pixel{};
    Vec2 normalized_pixel{};
    Vec2 screen_ndc{};
    Vec2 delta{};
    f32 wheel_delta = f32(0);
    bool left_click = false;
    bool left_double_click = false;
    bool right_drag = false;
    bool middle_drag = false;
    bool shift = false;
    bool hovered = false;
    bool captured = false;
    bool enabled = false;
};

struct ViewInputUpdate {
    RenderViewId view = 0;
    ViewInputRect rect{};
    Vec2 cursor{};
    ViewPointerButtons buttons{};
    f32 wheel_delta = f32(0);
    bool ui_blocked = false;
};

class ViewInputService {
public:
    void set_thread_service(ThreadManagementService* threads,
                            ThreadRole owner_role = ThreadRole::Main) noexcept {
        m_threads = threads;
        m_owner_role = owner_role;
    }

    [[nodiscard]] ViewInputSample update(const ViewInputUpdate& input) {
        if (!require_owner_thread("ViewInputService::update")) {
            return ViewInputSample{.view = input.view};
        }
        ViewState& state = state_for(input.view);
        const bool inside = contains(input.rect, input.cursor);
        const bool drag_down = input.buttons.right_down || input.buttons.middle_down;
        const bool hover = inside && !input.ui_blocked;

        if (!drag_down) {
            state.captured = false;
        } else if (!state.captured && hover) {
            state.captured = true;
        }

        const Vec2 delta = state.prev_valid ? input.cursor - state.prev_pixel : Vec2{};
        state.prev_pixel = input.cursor;
        state.prev_valid = true;

        const Vec2 normalized = normalize(input.rect, input.cursor);
        const bool enabled = hover || state.captured;
        state.last = ViewInputSample{
            .view = input.view,
            .pixel = input.cursor,
            .normalized_pixel = normalized,
            .screen_ndc = {normalized.x * f32(2) - f32(1), f32(1) - normalized.y * f32(2)},
            .delta = delta,
            .wheel_delta = enabled ? input.wheel_delta : f32(0),
            .left_click = hover && input.buttons.left_click,
            .left_double_click = hover && input.buttons.left_double_click,
            .right_drag = state.captured && input.buttons.right_down,
            .middle_drag = state.captured && input.buttons.middle_down,
            .shift = input.buttons.shift_down,
            .hovered = hover,
            .captured = state.captured,
            .enabled = enabled
        };
        return state.last;
    }

    [[nodiscard]] ViewInputSample sample(RenderViewId view) const noexcept {
        if (const ViewState* state = state_ptr(view))
            return state->last;
        return ViewInputSample{.view = view};
    }

    void clear_view(RenderViewId view) {
        if (!require_owner_thread("ViewInputService::clear_view")) return;
        m_views.erase(std::remove_if(m_views.begin(), m_views.end(),
            [view](const ViewState& state) { return state.view == view; }),
            m_views.end());
    }

private:
    struct ViewState {
        RenderViewId view = 0;
        Vec2 prev_pixel{};
        bool prev_valid = false;
        bool captured = false;
        ViewInputSample last{};
    };

    std::vector<ViewState> m_views;
    ThreadManagementService* m_threads = nullptr;
    ThreadRole m_owner_role = ThreadRole::Main;

    [[nodiscard]] bool require_owner_thread(std::string_view api_name) {
        return !m_threads || m_threads->require_thread_role(m_owner_role, api_name);
    }

    [[nodiscard]] ViewState& state_for(RenderViewId view) {
        if (ViewState* state = state_mut(view))
            return *state;
        m_views.push_back(ViewState{.view = view, .last = ViewInputSample{.view = view}});
        return m_views.back();
    }

    [[nodiscard]] ViewState* state_mut(RenderViewId view) noexcept {
        for (ViewState& state : m_views)
            if (state.view == view)
                return &state;
        return nullptr;
    }

    [[nodiscard]] const ViewState* state_ptr(RenderViewId view) const noexcept {
        for (const ViewState& state : m_views)
            if (state.view == view)
                return &state;
        return nullptr;
    }

    [[nodiscard]] static bool contains(ViewInputRect rect, Vec2 p) noexcept {
        return rect.size.x > f32(0) && rect.size.y > f32(0)
            && p.x >= rect.origin.x && p.y >= rect.origin.y
            && p.x <= rect.origin.x + rect.size.x
            && p.y <= rect.origin.y + rect.size.y;
    }

    [[nodiscard]] static Vec2 normalize(ViewInputRect rect, Vec2 p) noexcept {
        if (rect.size.x <= f32(0) || rect.size.y <= f32(0))
            return {};
        return {
            std::clamp((p.x - rect.origin.x) / rect.size.x, f32(0), f32(1)),
            std::clamp((p.y - rect.origin.y) / rect.size.y, f32(0), f32(1))
        };
    }
};

} // namespace ndde
