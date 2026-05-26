#pragma once
// engine/RenderService.hpp
// Renderer-neutral render view and packet queue service.

#include "engine/CameraTypes.hpp"
#include "engine/ServiceHandle.hpp"
#include "engine/threading/ThreadManagementService.hpp"
#include "math/GeometryTypes.hpp"
#include "math/Scalars.hpp"
#include "memory/Containers.hpp"
#include "memory/MemoryService.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>

namespace ndde {

using RenderViewId = u64;
using RenderViewHandle = ServiceHandle;

enum class RenderViewKind : u8 {
    Main,
    Alternate
};

enum class AlternateViewMode : u8 {
    Contour,
    LevelCurves,
    VectorField,
    Isoclines,
    Flow
};

enum class VectorFieldMode : u8 {
    Gradient,
    NegativeGradient,
    LevelTangent,
    ParticleVelocity
};

struct ViewOverlayState {
    bool show_axes = false;
    bool show_grid = false;
    bool show_frame = false;
    bool show_labels = false;
    bool show_hover_frenet = true;
    bool show_osculating_circle = true;
    bool show_darboux_frame = true;
    bool show_diffusion_ellipse = true;
    bool show_ghost_marker = true;
    bool show_metric_ellipse = true;
};

struct AlternateViewSettings {
    f32 isocline_direction_angle = 0.f;
    f32 isocline_target_slope = 0.f;
    f32 isocline_tolerance = 0.5f;
    u32 isocline_bands = 5u;

    VectorFieldMode vector_mode = VectorFieldMode::NegativeGradient;
    u32 vector_samples = 18u;
    f32 vector_scale = 1.f;

    u32 flow_seed_count = 9u;
    u32 flow_steps = 36u;
    f32 flow_step_size = 0.14f;
};

struct ViewInteractionState {
    Vec2 hover_pixel{};
    bool hover_enabled = false;
    f32 snap_radius_px = 22.f;
};

struct RenderViewDescriptor {
    std::string title;
    RenderViewKind kind = RenderViewKind::Main;
    AlternateViewMode alternate_mode = AlternateViewMode::Contour;
    CameraProjection projection = CameraProjection::Perspective;
    CameraViewProfile camera_profile = CameraViewProfile::Auto;
    f32 viewport_aspect = 16.f / 9.f;
    Vec2 viewport_size{16.f, 9.f};
    CameraState camera{};
    ViewOverlayState overlays{};
    AlternateViewSettings alternate{};
    ViewInteractionState interaction{};
};

struct RenderViewDomain {
    f32 u_min = -1.f;
    f32 u_max = 1.f;
    f32 v_min = -1.f;
    f32 v_max = 1.f;
    f32 z_min = -1.f;
    f32 z_max = 1.f;
};

struct SurfacePerturbCommand {
    RenderViewId view = 0;
    Vec2 uv{};
    Vec2 screen_ndc{};
    f32 viewport_aspect = 16.f / 9.f;
    f32 amplitude = 0.35f;
    f32 radius = 0.9f;
    f32 falloff = 1.f;
    u32 seed = 0;
    bool use_ray_pick = false;
};

struct RenderViewSnapshot {
    RenderViewId id = 0;
    std::string title;
    RenderViewKind kind = RenderViewKind::Main;
    AlternateViewMode alternate_mode = AlternateViewMode::Contour;
    CameraProjection projection = CameraProjection::Perspective;
    CameraViewProfile camera_profile = CameraViewProfile::Auto;
    f32 viewport_aspect = 16.f / 9.f;
    Vec2 viewport_size{16.f, 9.f};
    CameraState camera{};
    ViewOverlayState overlays{};
    AlternateViewSettings alternate{};
    ViewInteractionState interaction{};
    RenderViewDomain domain{};
};

struct RenderPacket {
    RenderViewId view = 0;
    Topology topology = Topology::LineList;
    DrawMode mode = DrawMode::VertexColor;
    Vec4 color{1.f, 1.f, 1.f, 1.f};
    Mat4 mvp{1.f};
    std::pmr::vector<Vertex> vertices;
};

class RenderService {
public:
    // memory must outlive this RenderService while frame-backed packets exist.
    // EngineServices declares memory before render so destruction order is safe.
    void set_memory_service(memory::MemoryService* memory) noexcept {
        m_memory = memory;
        std::pmr::memory_resource* view_resource = memory ? memory->view().resource()
                                                          : std::pmr::get_default_resource();
        if (view_resource != m_views.get_allocator().resource()) {
            ++m_generation;
            std::destroy_at(&m_views);
            std::construct_at(&m_views, view_resource);
            std::destroy_at(&m_surface_commands);
            std::construct_at(&m_surface_commands, view_resource);
        }
    }

    void set_thread_service(ThreadManagementService* threads,
                            ThreadRole owner_role = ThreadRole::Main) noexcept {
        m_threads = threads;
        m_owner_role = owner_role;
    }

    [[nodiscard]] RenderViewHandle register_view(RenderViewDescriptor descriptor, RenderViewId* out_id = nullptr) {
        if (!require_owner_thread("RenderService::register_view")) return {};
        const RenderViewId id = m_next_id++;
        m_views.push_back(RenderViewEntry{
            .id = id,
            .descriptor = std::move(descriptor),
            .active = true
        });
        if (out_id) *out_id = id;
        return RenderViewHandle([this, id] { unregister(id); }, &m_generation);
    }

    void submit(RenderViewId view, std::span<const Vertex> vertices,
                Topology topology, DrawMode mode, Vec4 color, Mat4 mvp)
    {
        if (!require_owner_thread("RenderService::submit")) return;
        if (!is_active(view) || vertices.empty()) return;
        RenderPacket packet{
            .view = view,
            .topology = topology,
            .mode = mode,
            .color = color,
            .mvp = mvp
        };
        packet.vertices.assign(vertices.begin(), vertices.end());
        m_packets.push_back(std::move(packet));
    }

    void clear_packets() {
        if (!require_owner_thread("RenderService::clear_packets")) return;
        m_packets.clear();
    }

    void set_view_domain(RenderViewId id, RenderViewDomain domain) {
        if (!require_owner_thread("RenderService::set_view_domain")) return;
        if (auto* entry = find_active_entry(id))
            entry->domain = domain;
    }

    [[nodiscard]] RenderViewDomain view_domain(RenderViewId id) const noexcept {
        if (const auto* entry = find_active_entry(id))
            return entry->domain;
        return {};
    }

    [[nodiscard]] RenderViewDescriptor* descriptor(RenderViewId id) noexcept {
        if (auto* entry = find_active_entry(id))
            return &entry->descriptor;
        return nullptr;
    }

    [[nodiscard]] const RenderViewDescriptor* descriptor(RenderViewId id) const noexcept {
        if (const auto* entry = find_active_entry(id))
            return &entry->descriptor;
        return nullptr;
    }

    [[nodiscard]] RenderViewId first_active_main_view() const noexcept {
        for (const auto& entry : m_views) {
            if (entry.active && entry.descriptor.kind == RenderViewKind::Main)
                return entry.id;
        }
        return 0;
    }

    [[nodiscard]] RenderViewId first_active_alternate_view() const noexcept {
        for (const auto& entry : m_views) {
            if (entry.active && entry.descriptor.kind == RenderViewKind::Alternate)
                return entry.id;
        }
        return 0;
    }

    void set_axes_visible(bool visible) {
        if (!require_owner_thread("RenderService::set_axes_visible")) return;
        for (auto& entry : m_views) {
            if (entry.active)
                entry.descriptor.overlays.show_axes = visible;
        }
    }

    void set_main_view_aspect(f32 aspect) {
        if (!require_owner_thread("RenderService::set_main_view_aspect")) return;
        if (aspect <= 0.f) return;
        for (auto& entry : m_views) {
            if (entry.active && entry.descriptor.kind == RenderViewKind::Main)
                entry.descriptor.viewport_aspect = aspect;
        }
    }

    void set_viewport_size(RenderViewId id, Vec2 size) {
        if (!require_owner_thread("RenderService::set_viewport_size")) return;
        if (size.x <= 0.f || size.y <= 0.f) return;
        if (auto* entry = find_active_entry(id)) {
            entry->descriptor.viewport_size = size;
            entry->descriptor.viewport_aspect = size.x / size.y;
        }
    }

    void set_viewport_size(RenderViewKind kind, Vec2 size) {
        if (!require_owner_thread("RenderService::set_viewport_size")) return;
        if (size.x <= 0.f || size.y <= 0.f) return;
        for (auto& entry : m_views) {
            if (entry.active && entry.descriptor.kind == kind) {
                entry.descriptor.viewport_size = size;
                entry.descriptor.viewport_aspect = size.x / size.y;
            }
        }
    }

    void set_hover_cursor(RenderViewKind kind, Vec2 pixel, bool enabled) {
        if (!require_owner_thread("RenderService::set_hover_cursor")) return;
        for (auto& entry : m_views) {
            if (entry.active && entry.descriptor.kind == kind) {
                entry.descriptor.interaction.hover_pixel = pixel;
                entry.descriptor.interaction.hover_enabled = enabled;
            }
        }
    }

    [[nodiscard]] bool axes_visible() const noexcept {
        for (const auto& entry : m_views) {
            if (entry.active && entry.descriptor.overlays.show_axes)
                return true;
        }
        return false;
    }

    void queue_surface_perturbation(SurfacePerturbCommand command) {
        if (!require_owner_thread("RenderService::queue_surface_perturbation")) return;
        if (command.view == 0)
            command.view = first_active_main_view();
        if (command.view == 0) return;
        m_surface_commands.push_back(command);
    }

    [[nodiscard]] memory::FrameVector<SurfacePerturbCommand> consume_surface_perturbations(RenderViewId view) {
        if (!require_owner_thread("RenderService::consume_surface_perturbations")) {
            return m_memory ? m_memory->frame().make_vector<SurfacePerturbCommand>()
                            : memory::FrameVector<SurfacePerturbCommand>{};
        }
        memory::FrameVector<SurfacePerturbCommand> out =
            m_memory ? m_memory->frame().make_vector<SurfacePerturbCommand>()
                     : memory::FrameVector<SurfacePerturbCommand>{};
        auto it = m_surface_commands.begin();
        while (it != m_surface_commands.end()) {
            if (it->view == view) {
                out.push_back(*it);
                it = m_surface_commands.erase(it);
            } else {
                ++it;
            }
        }
        return out;
    }

    [[nodiscard]] std::size_t active_view_count() const noexcept {
        return static_cast<std::size_t>(std::count_if(m_views.begin(), m_views.end(),
            [](const RenderViewEntry& entry) { return entry.active; }));
    }

    [[nodiscard]] std::size_t packet_count(RenderViewId view) const noexcept {
        return static_cast<std::size_t>(std::count_if(m_packets.begin(), m_packets.end(),
            [view](const RenderPacket& packet) { return packet.view == view; }));
    }

    [[nodiscard]] bool contains_view(std::string_view title) const {
        return std::any_of(m_views.begin(), m_views.end(),
            [title](const RenderViewEntry& entry) {
                return entry.active && entry.descriptor.title == title;
            });
    }

    [[nodiscard]] RenderViewKind view_kind(RenderViewId id) const noexcept {
        for (const auto& entry : m_views) {
            if (entry.active && entry.id == id)
                return entry.descriptor.kind;
        }
        return RenderViewKind::Main;
    }

    [[nodiscard]] const std::pmr::vector<RenderPacket>& packets() const noexcept { return m_packets; }

    [[nodiscard]] memory::FrameVector<RenderViewSnapshot> active_view_snapshots() const {
        memory::FrameVector<RenderViewSnapshot> out =
            m_memory ? m_memory->frame().make_vector<RenderViewSnapshot>() : memory::FrameVector<RenderViewSnapshot>{};
        out.reserve(m_views.size());
        for (const auto& entry : m_views) {
            if (!entry.active) continue;
            out.push_back(RenderViewSnapshot{
                .id = entry.id,
                .title = entry.descriptor.title,
                .kind = entry.descriptor.kind,
                .alternate_mode = entry.descriptor.alternate_mode,
                .projection = entry.descriptor.projection,
                .camera_profile = entry.descriptor.camera_profile,
                .viewport_aspect = entry.descriptor.viewport_aspect,
                .viewport_size = entry.descriptor.viewport_size,
                .camera = entry.descriptor.camera,
                .overlays = entry.descriptor.overlays,
                .alternate = entry.descriptor.alternate,
                .interaction = entry.descriptor.interaction,
                .domain = entry.domain
            });
        }
        return out;
    }

private:
    struct RenderViewEntry {
        RenderViewId id = 0;
        RenderViewDescriptor descriptor;
        RenderViewDomain domain;
        bool active = false;
    };

    RenderViewId m_next_id = 1;
    memory::ViewVector<RenderViewEntry> m_views;
    std::pmr::vector<RenderPacket> m_packets;
    memory::ViewVector<SurfacePerturbCommand> m_surface_commands;
    u64 m_generation = 0;
    memory::MemoryService* m_memory = nullptr;
    ThreadManagementService* m_threads = nullptr;
    ThreadRole m_owner_role = ThreadRole::Main;

    [[nodiscard]] bool require_owner_thread(std::string_view api_name) {
        return !m_threads || m_threads->require_thread_role(m_owner_role, api_name);
    }

    [[nodiscard]] bool is_active(RenderViewId id) const noexcept {
        return std::any_of(m_views.begin(), m_views.end(),
            [id](const RenderViewEntry& entry) { return entry.active && entry.id == id; });
    }

    [[nodiscard]] RenderViewEntry* find_active_entry(RenderViewId id) noexcept {
        for (auto& entry : m_views) {
            if (entry.active && entry.id == id)
                return &entry;
        }
        return nullptr;
    }

    [[nodiscard]] const RenderViewEntry* find_active_entry(RenderViewId id) const noexcept {
        for (const auto& entry : m_views) {
            if (entry.active && entry.id == id)
                return &entry;
        }
        return nullptr;
    }

    void unregister(RenderViewId id) noexcept {
        for (auto& entry : m_views) {
            if (entry.id == id) {
                entry.active = false;
                return;
            }
        }
    }
};

} // namespace ndde
