#pragma once
// engine/PanelService.hpp
// Engine-owned registration surface for UI panels.

#include "engine/ServiceHandle.hpp"
#include "engine/threading/ThreadManagementService.hpp"
#include "memory/Containers.hpp"
#include "memory/MemoryService.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>

#include <imgui.h>

namespace ndde {

using PanelId = u64;
using PanelHandle = ServiceHandle;

enum class PanelScope : u8 {
    Global,
    Simulation
};

struct PanelDescriptor {
    std::string title;
    std::string category = "Simulation";
    PanelScope scope = PanelScope::Simulation;
    bool initially_open = true;
    ImVec2 first_use_pos{0.f, 0.f};
    ImVec2 first_use_size{0.f, 0.f};
    f32 background_alpha = 0.86f;
    std::function<void()> draw;
    std::function<void()> draw_body;
};

class PanelService {
public:
    void set_memory_service(memory::MemoryService* memory) {
        std::pmr::memory_resource* resource = memory ? memory->persistent().resource()
                                                     : std::pmr::get_default_resource();
        if (resource == m_panels.get_allocator().resource())
            return;
        ++m_generation;
        std::destroy_at(&m_panels);
        std::construct_at(&m_panels, resource);
    }

    void set_thread_service(ThreadManagementService* threads,
                            ThreadRole owner_role = ThreadRole::Main) noexcept {
        m_threads = threads;
        m_owner_role = owner_role;
    }

    [[nodiscard]] PanelHandle register_panel(PanelDescriptor descriptor) {
        if (!require_owner_thread("PanelService::register_panel")) return {};
        const PanelId id = m_next_id++;
        m_panels.push_back(PanelEntry{
            .id = id,
            .descriptor = std::move(descriptor),
            .active = true
        });
        return PanelHandle([this, id] { unregister(id); }, &m_generation);
    }

    void draw_registered_panels(PanelScope scope) {
        if (!require_owner_thread("PanelService::draw_registered_panels")) return;
        for (auto& entry : m_panels) {
            if (!entry.active) continue;
            if (entry.descriptor.scope != scope) continue;
            if (entry.descriptor.draw) {
                entry.descriptor.draw();
                continue;
            }
            if (!entry.descriptor.draw_body) continue;
            draw_window_panel(entry.descriptor);
        }
    }

    void draw_registered_panels() {
        draw_registered_panels(PanelScope::Global);
        draw_registered_panels(PanelScope::Simulation);
    }

    [[nodiscard]] std::size_t active_count() const noexcept {
        return static_cast<std::size_t>(std::count_if(m_panels.begin(), m_panels.end(),
            [](const PanelEntry& entry) { return entry.active; }));
    }

    [[nodiscard]] std::size_t active_count(PanelScope scope) const noexcept {
        return static_cast<std::size_t>(std::count_if(m_panels.begin(), m_panels.end(),
            [scope](const PanelEntry& entry) {
                return entry.active && entry.descriptor.scope == scope;
            }));
    }

    [[nodiscard]] bool contains(std::string_view title) const {
        return std::any_of(m_panels.begin(), m_panels.end(),
            [title](const PanelEntry& entry) {
                return entry.active && entry.descriptor.title == title;
            });
    }

private:
    struct PanelEntry {
        PanelId id = 0;
        PanelDescriptor descriptor;
        bool active = false;
    };

    PanelId m_next_id = 1;
    u64 m_generation = 0;
    memory::PersistentVector<PanelEntry> m_panels;
    ThreadManagementService* m_threads = nullptr;
    ThreadRole m_owner_role = ThreadRole::Main;

    [[nodiscard]] bool require_owner_thread(std::string_view api_name) {
        return !m_threads || m_threads->require_thread_role(m_owner_role, api_name);
    }

    void unregister(PanelId id) noexcept {
        for (auto& entry : m_panels) {
            if (entry.id == id) {
                entry.active = false;
                entry.descriptor.draw = {};
                entry.descriptor.draw_body = {};
                return;
            }
        }
    }

    static void draw_window_panel(const PanelDescriptor& descriptor) {
        if (descriptor.first_use_pos.x != 0.f || descriptor.first_use_pos.y != 0.f)
            ImGui::SetNextWindowPos(descriptor.first_use_pos, ImGuiCond_FirstUseEver);
        if (descriptor.first_use_size.x > 0.f && descriptor.first_use_size.y > 0.f)
            ImGui::SetNextWindowSize(descriptor.first_use_size, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(descriptor.background_alpha);
        if (!ImGui::Begin(descriptor.title.c_str())) {
            ImGui::End();
            return;
        }
        descriptor.draw_body();
        ImGui::End();
    }
};

} // namespace ndde
