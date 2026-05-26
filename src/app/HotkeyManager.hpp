#pragma once
// app/HotkeyManager.hpp
// HotkeyManager: decoupled hotkey registration and dispatch.
//
// Design
// ──────
// A hotkey is a (chord, description, callback) triple.
//
//   Chord      — modifier flags + one ImGuiKey. Built with the Chord helpers:
//                  Chord::ctrl(ImGuiKey_A)
//                  Chord::shift(ImGuiKey_S)
//                  Chord::ctrl_shift(ImGuiKey_Z)
//                  Chord{ImGuiKey_F5, Chord::None}   // bare key
//
//   Description — human-readable label shown in the hotkey reference panel.
//
//   Callback   — std::function<void()>. Accepts any callable: lambdas,
//                member pointers wrapped in [this]{...}, free functions.
//                The manager owns the callback by value; neither the
//                registrar nor the receiver needs to know about the other.
//
// Edge detection
// ──────────────
// The manager performs its own rising-edge detection. Each registered hotkey
// stores its own `prev` bool. Callers do NOT need a `bool m_ctrl_x_prev`
// member — those disappear from the owning class.
//
// Dispatch
// ────────
// Prefer handle_key_event() from the engine/GLFW key callback for snappy,
// guaranteed delivery. dispatch() is still available for pure ImGui polling
// contexts.
//
// If ImGui has captured keyboard input (io.WantCaptureKeyboard) dispatch()
// returns immediately and fires nothing — text fields get priority.
//
// Hotkey reference panel
// ──────────────────────
// draw_panel() renders a floating ImGui window listing every registered
// hotkey with its description. Pass `open` by reference so the caller can
// toggle visibility from any hotkey (including the Ctrl+H self-reference).
//
// Unregistration
// ──────────────
// register_*() returns a HotkeyID (u32). Pass it to unregister() to
// remove the binding. Useful when a panel or subsystem is destroyed.
// IDs are never reused within a session.
//
// Groups
// ──────
// Optionally tag bindings with a group string ("Spawn", "Overlays", ...).
// draw_panel() renders SeparatorText between groups, in insertion order of
// the first binding that introduces each group.
//
// Usage
// ─────
//   HotkeyManager hk;
//
//   // Toggle a bool directly:
//   hk.register_toggle(Chord::ctrl(ImGuiKey_F), "Toggle Frenet frame",
//                       m_show_frenet, "Overlays");
//
//   // Arbitrary callback (lambda capturing [this]):
//   hk.register(Chord::ctrl(ImGuiKey_L), "Spawn Leader particle",
//               [this]{ spawn_leader(); }, "Spawn");
//
//   // Each frame:
//   hk.dispatch();
//
//   // In your hotkey panel method:
//   hk.draw_panel("Hotkeys  [Ctrl+H]", m_hotkey_panel_open);

#include <imgui.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "math/Scalars.hpp"
#include "memory/Containers.hpp"
#include <functional>
#include <string>
#include <algorithm>

namespace ndde {

// ── HotkeyID ──────────────────────────────────────────────────────────────────

using HotkeyID = u32;
inline constexpr HotkeyID k_invalid_hotkey = 0u;

// ── Chord ─────────────────────────────────────────────────────────────────────
// A modifier + key combination. Modifier flags are a bitmask so future
// Alt support is a one-liner (add Alt = 4).

struct Chord {
    static constexpr u8 None  = u8(0);
    static constexpr u8 Ctrl  = u8(1);
    static constexpr u8 Shift = u8(2);

    ImGuiKey key  = ImGuiKey_None;
    u8       mods = None;

    // ── Named constructors ────────────────────────────────────────────────────
    [[nodiscard]] static constexpr Chord ctrl(ImGuiKey k) noexcept {
        return {k, Ctrl};
    }
    [[nodiscard]] static constexpr Chord shift(ImGuiKey k) noexcept {
        return {k, Shift};
    }
    [[nodiscard]] static constexpr Chord ctrl_shift(ImGuiKey k) noexcept {
        return {k, static_cast<u8>(Ctrl | Shift)};
    }
    [[nodiscard]] static constexpr Chord bare(ImGuiKey k) noexcept {
        return {k, None};
    }

    // ── Equality ──────────────────────────────────────────────────────────────
    [[nodiscard]] constexpr bool operator==(const Chord& o) const noexcept {
        return key == o.key && mods == o.mods;
    }

    // ── Human-readable label for the panel ("Ctrl+F", "Ctrl+Shift+Z") ────────
    [[nodiscard]] std::string label() const {
        std::string s;
        if (mods & Ctrl)  s += "Ctrl+";
        if (mods & Shift) s += "Shift+";
        // Map common keys to their character; fall back to ImGui's name.
        if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
            s += static_cast<char>('A' + (key - ImGuiKey_A));
        } else if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12) {
            s += 'F';
            s += std::to_string(1 + (key - ImGuiKey_F1));
        } else {
            s += ImGui::GetKeyName(key);
        }
        return s;
    }

    // ── Test against current ImGuiIO state ────────────────────────────────────
    [[nodiscard]] bool pressed(const ImGuiIO& io) const noexcept {
        if (key == ImGuiKey_None) return false;
        const bool ctrl_ok  = !(mods & Ctrl)  || io.KeyCtrl;
        const bool shift_ok = !(mods & Shift) || io.KeyShift;
        // Reject if an unrequested modifier is held (prevents Ctrl+Shift+F
        // from also firing the Ctrl+F binding).
        const bool no_extra_ctrl  = (mods & Ctrl)  || !io.KeyCtrl;
        const bool no_extra_shift = (mods & Shift) || !io.KeyShift;
        return ctrl_ok && shift_ok && no_extra_ctrl && no_extra_shift
               && ImGui::IsKeyDown(key);
    }
};

// ── HotkeyManager ─────────────────────────────────────────────────────────────

class HotkeyManager {
public:
    HotkeyManager() = default;
    ~HotkeyManager() = default;

    // Non-copyable (owns callbacks by value; copy is rarely correct).
    // Moveable so it can live as a class member and be default-constructed.
    HotkeyManager(const HotkeyManager&)            = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;
    HotkeyManager(HotkeyManager&&)                 = default;
    HotkeyManager& operator=(HotkeyManager&&)      = default;

    // ── Registration ──────────────────────────────────────────────────────────

    // Register an arbitrary callback fired on the rising edge of chord.
    // group:  optional section label for draw_panel() ("Spawn", "Overlays").
    //         Pass "" to place in a default ungrouped section.
    // Returns a HotkeyID that can be passed to unregister().
    HotkeyID register_action(Chord              chord,
                              std::string        description,
                              std::function<void()> callback,
                              std::string        group = "")
    {
        const HotkeyID id = ++m_next_id;
        m_entries.push_back(Entry{
            .id          = id,
            .chord       = chord,
            .description = std::move(description),
            .group       = std::move(group),
            .callback    = std::move(callback),
            .prev        = false,
        });
        return id;
    }

    // Convenience: toggle a bool flag on each rising edge.
    // Equivalent to register_action(chord, desc, [&flag]{ flag = !flag; }, group).
    HotkeyID register_toggle(Chord       chord,
                              std::string description,
                              bool&       flag,
                              std::string group = "")
    {
        return register_action(chord, std::move(description),
                               [&flag]{ flag = !flag; },
                               std::move(group));
    }

    // Remove a previously registered binding.
    // Safe to call with k_invalid_hotkey (no-op).
    void unregister(HotkeyID id) {
        if (id == k_invalid_hotkey) return;
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(),
                           [id](const Entry& e){ return e.id == id; }),
            m_entries.end());
    }

    // ── Dispatch ──────────────────────────────────────────────────────────────

    // Call once per frame after ImGui::NewFrame().
    // Fires callbacks for any chord whose key transitions from up to down.
    // Respects io.WantCaptureKeyboard: if ImGui owns the keyboard (text
    // input is active) no callbacks fire and all prev states reset to the
    // current key state (prevents stale rising edges after focus returns).
    void dispatch() {
        const ImGuiIO& io = ImGui::GetIO();
        for (auto& e : m_entries) {
            const bool cur = e.chord.pressed(io);
            if (!io.WantCaptureKeyboard && cur && !e.prev)
                e.callback();
            e.prev = cur;
        }
    }

    // Handle a GLFW-style key callback event. Returns true when a registered
    // hotkey consumed the event. Repeat/release events are ignored.
    bool handle_key_event(int key, int action, int mods) {
        if (action != GLFW_PRESS) return false;

        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput) return false;

        const ImGuiKey imgui_key = glfw_key_to_imgui(key);
        if (imgui_key == ImGuiKey_None) return false;

        const u8 chord_mods = mods_from_glfw(mods);
        for (auto& e : m_entries) {
            if (e.chord.key == imgui_key && e.chord.mods == chord_mods) {
                e.callback();
                e.prev = true;
                return true;
            }
        }
        return false;
    }

    // ── Hotkey reference panel ─────────────────────────────────────────────────

    // Render a floating ImGui window listing all registered hotkeys.
    // `open` is read and written: the window sets it to false when closed.
    // The window is not rendered when `open` is false.
    void draw_panel(const char* title, bool& open) const {
        if (!open) return;

        ImGui::SetNextWindowBgAlpha(0.88f);
        ImGui::SetNextWindowSize(ImVec2(340.f, 0.f), ImGuiCond_FirstUseEver);

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse   |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;

        if (!ImGui::Begin(title, &open, flags)) {
            ImGui::End();
            return;
        }

        // Collect group order (insertion order of first appearance).
        memory::FrameVector<std::string> group_order;
        for (const auto& e : m_entries) {
            const bool seen = std::any_of(group_order.begin(), group_order.end(),
                                          [&](const std::string& g){ return g == e.group; });
            if (!seen) group_order.push_back(e.group);
        }

        for (const auto& group : group_order) {
            if (!group.empty())
                ImGui::SeparatorText(group.c_str());
            else
                ImGui::Separator();

            for (const auto& e : m_entries) {
                if (e.group != group) continue;
                // Two-column layout: key label left-aligned, description after gap.
                const std::string lbl = e.chord.label();
                ImGui::TextDisabled("%s", lbl.c_str());
                ImGui::SameLine(100.f);
                ImGui::TextUnformatted(e.description.c_str());
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Press the hotkey or close this window to dismiss.");
        ImGui::End();
    }

    // ── Inspection ────────────────────────────────────────────────────────────

    [[nodiscard]] std::size_t size()  const noexcept { return m_entries.size(); }
    [[nodiscard]] bool        empty() const noexcept { return m_entries.empty(); }

private:
    struct Entry {
        HotkeyID              id;
        Chord                 chord;
        std::string           description;
        std::string           group;
        std::function<void()> callback;
        bool                  prev = false;  ///< previous-frame key state (edge detection)
    };

    memory::SimVector<Entry> m_entries;
    HotkeyID           m_next_id = 0u;

    [[nodiscard]] static u8 mods_from_glfw(int mods) noexcept {
        u8 out = Chord::None;
        if ((mods & GLFW_MOD_CONTROL) != 0) out |= Chord::Ctrl;
        if ((mods & GLFW_MOD_SHIFT) != 0) out |= Chord::Shift;
        return out;
    }

    [[nodiscard]] static ImGuiKey glfw_key_to_imgui(int key) noexcept {
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
            return static_cast<ImGuiKey>(ImGuiKey_A + (key - GLFW_KEY_A));
        if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12)
            return static_cast<ImGuiKey>(ImGuiKey_F1 + (key - GLFW_KEY_F1));
        switch (key) {
            case GLFW_KEY_SPACE: return ImGuiKey_Space;
            case GLFW_KEY_ENTER: return ImGuiKey_Enter;
            case GLFW_KEY_TAB: return ImGuiKey_Tab;
            case GLFW_KEY_ESCAPE: return ImGuiKey_Escape;
            default: return ImGuiKey_None;
        }
    }
};

} // namespace ndde
