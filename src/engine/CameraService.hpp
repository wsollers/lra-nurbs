#pragma once
// engine/CameraService.hpp
// Engine-owned camera movement, presets, and view/projection construction.

#include "engine/CameraTypes.hpp"
#include "engine/InteractionService.hpp"
#include "engine/RenderService.hpp"
#include "numeric/ops.hpp"

#include <algorithm>
#include <string_view>
#include <glm/gtc/matrix_transform.hpp>

namespace ndde {

class CameraService {
public:
    void set_render_service(RenderService* render) noexcept {
        m_render = render;
    }

    void set_thread_service(ThreadManagementService* threads,
                            ThreadRole owner_role = ThreadRole::Main) noexcept {
        m_threads = threads;
        m_owner_role = owner_role;
    }

    void orbit_main(f32 dx, f32 dy) {
        if (!require_owner_thread("CameraService::orbit_main")) return;
        if (!m_render) return;
        for (const RenderViewSnapshot& view : m_render->active_view_snapshots()) {
            if (view.kind != RenderViewKind::Main) continue;
            if (view.projection == CameraProjection::Orthographic)
                continue;
            orbit(view.id, dx, dy);
        }
    }

    void orbit(RenderViewId view, f32 dx, f32 dy) {
        if (!require_owner_thread("CameraService::orbit")) return;
        if (!m_render) return;
        const auto* descriptor = m_render->descriptor(view);
        if (!descriptor || descriptor->projection == CameraProjection::Orthographic)
            return;
        if (auto* mutable_descriptor = m_render->descriptor(view)) {
            mutable_descriptor->camera.yaw += dx * k_orbit_speed;
            mutable_descriptor->camera.pitch = std::clamp(mutable_descriptor->camera.pitch + dy * k_orbit_speed,
                                                          -1.35f, 1.35f);
        }
    }

    void pan_main(f32 dx, f32 dy) {
        if (!require_owner_thread("CameraService::pan_main")) return;
        if (!m_render) return;
        for (const RenderViewSnapshot& view : m_render->active_view_snapshots()) {
            if (view.kind == RenderViewKind::Main)
                pan(view.id, dx, dy);
        }
    }

    void zoom_main(f32 wheel_delta) {
        if (!require_owner_thread("CameraService::zoom_main")) return;
        if (!m_render) return;
        for (const RenderViewSnapshot& view : m_render->active_view_snapshots()) {
            if (view.kind == RenderViewKind::Main)
                zoom(view.id, wheel_delta);
        }
    }

    void pan(RenderViewId view, f32 dx, f32 dy) {
        if (!require_owner_thread("CameraService::pan")) return;
        if (!m_render) return;
        auto* descriptor = m_render->descriptor(view);
        if (!descriptor) return;
        const RenderViewDomain domain = m_render->view_domain(view);
        const f32 scale = std::max(domain.u_max - domain.u_min, domain.v_max - domain.v_min)
            / std::max(descriptor->camera.zoom, 0.05f);
        descriptor->camera.target.x -= dx * scale * 0.0012f;
        descriptor->camera.target.y += dy * scale * 0.0012f;
    }

    void zoom(RenderViewId view, f32 wheel_delta) {
        if (!require_owner_thread("CameraService::zoom")) return;
        if (!m_render || wheel_delta == 0.f) return;
        if (auto* descriptor = m_render->descriptor(view)) {
            descriptor->camera.zoom = std::clamp(descriptor->camera.zoom * (1.f + wheel_delta * k_zoom_step),
                                                 0.08f, 30.f);
        }
    }

    void reset_main(CameraPreset preset = CameraPreset::Home) {
        if (!require_owner_thread("CameraService::reset_main")) return;
        if (!m_render) return;
        for (const RenderViewSnapshot& view : m_render->active_view_snapshots()) {
            if (view.kind == RenderViewKind::Main)
                reset(view.id, preset);
        }
    }

    void reset(RenderViewId view, CameraPreset preset = CameraPreset::Home) {
        if (!require_owner_thread("CameraService::reset")) return;
        if (!m_render) return;
        if (auto* descriptor = m_render->descriptor(view))
            descriptor->camera = preset_camera(preset, m_render->view_domain(view));
    }

    [[nodiscard]] bool frame_selection(const InteractionService& interaction,
                                       RenderViewId view = 0) {
        if (!require_owner_thread("CameraService::frame_selection")) return false;
        if (!m_render) return false;
        const InteractionTarget target = interaction.selected_target(view);
        if (!target.valid || target.view == 0)
            return false;
        auto* descriptor = m_render->descriptor(target.view);
        if (!descriptor)
            return false;
        if (target.kind == InteractionTargetKind::ViewPoint2D) {
            descriptor->camera.target.x = target.point2d.x;
            descriptor->camera.target.y = target.point2d.y;
            return true;
        }
        descriptor->camera.target = target.world;
        return true;
    }

    [[nodiscard]] CameraState camera(RenderViewId view) const noexcept {
        if (m_render) {
            if (const auto* descriptor = m_render->descriptor(view))
                return descriptor->camera;
        }
        return {};
    }

    [[nodiscard]] Mat4 perspective_mvp(RenderViewId view) const noexcept {
        if (!m_render) return Mat4{1.f};
        const auto* descriptor = m_render->descriptor(view);
        const RenderViewDomain domain = m_render->view_domain(view);
        const CameraState cam = descriptor ? descriptor->camera : CameraState{};
        const Vec3 center = normalized_target(cam, domain);
        const float span_u = std::max(domain.u_max - domain.u_min, 0.01f);
        const float span_v = std::max(domain.v_max - domain.v_min, 0.01f);
        const float radius = std::max(span_u, span_v) * 0.62f;
        const float zoom_value = std::max(cam.zoom, 0.05f);
        const float dist = std::max(radius * 2.35f / zoom_value, 4.f);
        const Vec3 eye{
            center.x + dist * ops::cos(cam.pitch) * ops::cos(cam.yaw),
            center.y + dist * ops::cos(cam.pitch) * ops::sin(cam.yaw),
            center.z + dist * ops::sin(cam.pitch)
        };
        const float aspect = descriptor ? std::max(descriptor->viewport_aspect, 0.1f) : 16.f / 9.f;
        return glm::perspective(glm::radians(43.f), aspect, 0.05f, 400.f)
            * glm::lookAt(eye, center, Vec3{0.f, 0.f, 1.f});
    }

    [[nodiscard]] Mat4 orthographic_mvp(RenderViewId view, f32 pad_fraction = 0.04f) const noexcept {
        if (!m_render) return Mat4{1.f};
        const auto* descriptor = m_render->descriptor(view);
        const RenderViewDomain domain = m_render->view_domain(view);
        const CameraState cam = descriptor ? descriptor->camera : CameraState{};
        const Vec3 center3 = normalized_target(cam, domain);
        const float du = std::max(domain.u_max - domain.u_min, 0.01f);
        const float dv = std::max(domain.v_max - domain.v_min, 0.01f);
        const float zoom_value = std::max(cam.zoom, 0.05f);
        const float half_u = (0.5f + pad_fraction) * du / zoom_value;
        const float half_v = (0.5f + pad_fraction) * dv / zoom_value;
        return glm::ortho(center3.x - half_u, center3.x + half_u,
                          center3.y - half_v, center3.y + half_v,
                          -400.f, 400.f);
    }

    [[nodiscard]] Mat4 view_mvp(RenderViewId view) const noexcept {
        if (!m_render) return Mat4{1.f};
        if (const auto* descriptor = m_render->descriptor(view)) {
            if (descriptor->projection == CameraProjection::Orthographic)
                return orthographic_mvp(view);
        }
        return perspective_mvp(view);
    }

    [[nodiscard]] bool queue_surface_perturbation(InteractionService& interaction,
                                                  RenderViewId view,
                                                  Vec2 normalized_pixel,
                                                  Vec2 screen_ndc,
                                                  u32 seed,
                                                  f32 amplitude = 0.25f,
                                                  f32 radius = 1.0f,
                                                  f32 falloff = 1.f) const {
        if (!require_owner_thread("CameraService::queue_surface_perturbation")) return false;
        if (!m_render || view == 0) return false;
        const RenderViewDomain domain = m_render->view_domain(view);
        interaction.queue_surface_pick(SurfacePickRequest{
            .view = view,
            .fallback_uv = {
                domain.u_min + normalized_pixel.x * (domain.u_max - domain.u_min),
                domain.v_max - normalized_pixel.y * (domain.v_max - domain.v_min)
            },
            .screen_ndc = screen_ndc,
            .amplitude = amplitude,
            .radius = radius,
            .falloff = falloff,
            .seed = seed
        });
        return true;
    }

    [[nodiscard]] static CameraState preset_camera(CameraPreset preset, RenderViewDomain domain) noexcept {
        const Vec3 target{
            (domain.u_min + domain.u_max) * 0.5f,
            (domain.v_min + domain.v_max) * 0.5f,
            (domain.z_min + domain.z_max) * 0.5f
        };
        switch (preset) {
            case CameraPreset::Top:
                return CameraState{.target = target, .yaw = 0.f, .pitch = 1.35f, .zoom = 1.f};
            case CameraPreset::Front:
                return CameraState{.target = target, .yaw = 1.5707963f, .pitch = 0.10f, .zoom = 1.f};
            case CameraPreset::Side:
                return CameraState{.target = target, .yaw = 0.f, .pitch = 0.10f, .zoom = 1.f};
            case CameraPreset::Home:
            default:
                return CameraState{.target = target, .yaw = 0.72f, .pitch = 0.55f, .zoom = 1.f};
        }
    }

private:
    static constexpr f32 k_orbit_speed = 0.0065f;
    static constexpr f32 k_zoom_step = 0.12f;

    RenderService* m_render = nullptr;
    ThreadManagementService* m_threads = nullptr;
    ThreadRole m_owner_role = ThreadRole::Main;

    [[nodiscard]] bool require_owner_thread(std::string_view api_name) const {
        return !m_threads || m_threads->require_thread_role(m_owner_role, api_name);
    }

    [[nodiscard]] static Vec3 normalized_target(CameraState camera, RenderViewDomain domain) noexcept {
        const Vec3 domain_center{
            (domain.u_min + domain.u_max) * 0.5f,
            (domain.v_min + domain.v_max) * 0.5f,
            (domain.z_min + domain.z_max) * 0.5f
        };
        // Default-constructed cameras mean "use the view domain center" unless
        // the domain itself is centered at the origin.
        const bool camera_at_origin = ops::abs(camera.target.x) < 1e-6f
            && ops::abs(camera.target.y) < 1e-6f
            && ops::abs(camera.target.z) < 1e-6f;
        const bool domain_at_origin = ops::abs(domain_center.x) < 1e-6f
            && ops::abs(domain_center.y) < 1e-6f
            && ops::abs(domain_center.z) < 1e-6f;
        if (camera_at_origin && !domain_at_origin)
            return domain_center;
        return camera.target;
    }
};

} // namespace ndde
