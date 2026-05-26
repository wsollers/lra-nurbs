#pragma once
// engine/InteractionService.hpp
// View interaction, picking, and hover metadata service.

#include "engine/RenderService.hpp"
#include "math/Surfaces.hpp"
#include "memory/Containers.hpp"
#include "memory/MemoryService.hpp"
#include "numeric/ops.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <span>

#include <glm/gtc/matrix_inverse.hpp>

namespace ndde {

struct ViewMouseState {
    RenderViewId view = 0;
    Vec2 pixel{};
    Vec2 ndc{};
    bool enabled = false;
    f32 snap_radius_px = 22.f;
};

struct InteractionRay {
    Vec3 origin{};
    Vec3 direction{0.f, 0.f, -1.f};
    bool valid = false;
};

struct SurfaceHit {
    RenderViewId view = 0;
    Vec2 uv{};
    Vec3 world{};
    f32 distance = 0.f;
    bool hit = false;
};

struct SurfacePickRequest {
    RenderViewId view = 0;
    Vec2 fallback_uv{};
    Vec2 screen_ndc{};
    f32 amplitude = 0.35f;
    f32 radius = 0.9f;
    f32 falloff = 1.f;
    u32 seed = 0;
};

struct ViewPointPickRequest {
    RenderViewId view = 0;
    Vec2 normalized_pixel{};
    Vec2 screen_ndc{};
    u32 seed = 0;
};

struct TrailPickSample {
    u64 particle_id = 0;
    u32 particle_index = 0;
    u32 trail_index = 0;
    Vec2 uv{};
    Vec3 world{};
    f32 curvature = 0.f;
    f32 torsion = 0.f;
    f32 normal_curvature = 0.f;
    f32 geodesic_curvature = 0.f;
};

struct ParticleTrailHit {
    RenderViewId view = 0;
    u64 particle_id = 0;
    u32 particle_index = 0;
    u32 trail_index = 0;
    Vec2 uv{};
    Vec3 world{};
    f32 curvature = 0.f;
    f32 torsion = 0.f;
    f32 normal_curvature = 0.f;
    f32 geodesic_curvature = 0.f;
    f32 pixel_distance = 0.f;
    bool hit = false;
};

struct ViewPointHit {
    RenderViewId view = 0;
    Vec2 point{};
    Vec3 world{};
    bool hit = false;
};

enum class InteractionTargetKind : u8 {
    None,
    SurfacePoint,
    ViewPoint2D,
    Particle,
    TrailSample
};

struct InteractionTarget {
    InteractionTargetKind kind = InteractionTargetKind::None;
    RenderViewId view = 0;
    Vec2 uv{};
    Vec2 point2d{};
    Vec3 world{};
    u64 particle_id = 0;
    u32 particle_index = 0;
    u32 trail_index = 0;
    f32 curvature = 0.f;
    f32 torsion = 0.f;
    f32 normal_curvature = 0.f;
    f32 geodesic_curvature = 0.f;
    bool valid = false;
};

struct HoverMetadata {
    RenderViewId view = 0;
    Vec2 mouse_pixel{};
    SurfaceHit surface{};
    ParticleTrailHit particle{};
    ViewPointHit view_point{};
};

class InteractionService {
public:
    void set_memory_service(memory::MemoryService* memory) noexcept {
        m_memory = memory;
        std::pmr::memory_resource* view_resource = memory ? memory->view().resource()
                                                          : std::pmr::get_default_resource();
        if (view_resource != m_mouse.get_allocator().resource()) {
            std::destroy_at(&m_mouse);
            std::construct_at(&m_mouse, view_resource);
            std::destroy_at(&m_surface_requests);
            std::construct_at(&m_surface_requests, view_resource);
            std::destroy_at(&m_view_point_requests);
            std::construct_at(&m_view_point_requests, view_resource);
            std::destroy_at(&m_hover_targets);
            std::construct_at(&m_hover_targets, view_resource);
            std::destroy_at(&m_selected_targets);
            std::construct_at(&m_selected_targets, view_resource);
        }
    }

    void set_thread_service(ThreadManagementService* threads,
                            ThreadRole owner_role = ThreadRole::Main) noexcept {
        m_threads = threads;
        m_owner_role = owner_role;
    }

    void set_mouse(RenderViewId view, Vec2 pixel, Vec2 ndc, bool enabled, f32 snap_radius_px = 22.f) {
        if (!require_owner_thread("InteractionService::set_mouse")) return;
        ViewMouseState* state = mouse_state_mut(view);
        if (!state) {
            m_mouse.push_back(ViewMouseState{.view = view});
            state = &m_mouse.back();
        }
        state->pixel = pixel;
        state->ndc = ndc;
        state->enabled = enabled;
        state->snap_radius_px = snap_radius_px;
        m_hover.view = view;
        m_hover.mouse_pixel = pixel;
    }

    [[nodiscard]] ViewMouseState mouse_state(RenderViewId view) const noexcept {
        if (const auto* state = mouse_state_ptr(view))
            return *state;
        return ViewMouseState{.view = view};
    }

    void queue_surface_pick(SurfacePickRequest request) {
        if (!require_owner_thread("InteractionService::queue_surface_pick")) return;
        if (request.view != 0)
            m_surface_requests.push_back(request);
    }

    [[nodiscard]] memory::FrameVector<SurfacePickRequest> consume_surface_picks(RenderViewId view) {
        if (!require_owner_thread("InteractionService::consume_surface_picks")) {
            return m_memory ? m_memory->frame().make_vector<SurfacePickRequest>()
                            : memory::FrameVector<SurfacePickRequest>{};
        }
        memory::FrameVector<SurfacePickRequest> out =
            m_memory ? m_memory->frame().make_vector<SurfacePickRequest>() : memory::FrameVector<SurfacePickRequest>{};
        auto it = m_surface_requests.begin();
        while (it != m_surface_requests.end()) {
            if (it->view == view) {
                out.push_back(*it);
                it = m_surface_requests.erase(it);
            } else {
                ++it;
            }
        }
        return out;
    }

    void queue_view_point_pick(ViewPointPickRequest request) {
        if (!require_owner_thread("InteractionService::queue_view_point_pick")) return;
        if (request.view != 0)
            m_view_point_requests.push_back(request);
    }

    [[nodiscard]] memory::FrameVector<ViewPointPickRequest> consume_view_point_picks(RenderViewId view) {
        if (!require_owner_thread("InteractionService::consume_view_point_picks")) {
            return m_memory ? m_memory->frame().make_vector<ViewPointPickRequest>()
                            : memory::FrameVector<ViewPointPickRequest>{};
        }
        memory::FrameVector<ViewPointPickRequest> out =
            m_memory ? m_memory->frame().make_vector<ViewPointPickRequest>()
                     : memory::FrameVector<ViewPointPickRequest>{};
        auto it = m_view_point_requests.begin();
        while (it != m_view_point_requests.end()) {
            if (it->view == view) {
                out.push_back(*it);
                it = m_view_point_requests.erase(it);
            } else {
                ++it;
            }
        }
        return out;
    }

    [[nodiscard]] InteractionRay make_ray(const Mat4& mvp, Vec2 screen_ndc) const noexcept {
        const Mat4 inv = glm::inverse(mvp);
        const glm::vec4 near4 = inv * glm::vec4(screen_ndc.x, screen_ndc.y, 0.f, 1.f);
        const glm::vec4 far4 = inv * glm::vec4(screen_ndc.x, screen_ndc.y, 1.f, 1.f);
        if (ops::abs(near4.w) < 1e-6f || ops::abs(far4.w) < 1e-6f)
            return {};
        const Vec3 origin = Vec3{near4.x, near4.y, near4.z} / near4.w;
        const Vec3 far_point = Vec3{far4.x, far4.y, far4.z} / far4.w;
        const Vec3 dir = far_point - origin;
        if (glm::length(dir) < 1e-6f)
            return {};
        return InteractionRay{.origin = origin, .direction = glm::normalize(dir), .valid = true};
    }

    [[nodiscard]] SurfaceHit resolve_surface_hit(RenderViewId view,
                                                 const math::ISurface& surface,
                                                 const Mat4& mvp,
                                                 Vec2 screen_ndc,
                                                 float time = 0.f) {
        SurfaceHit hit{.view = view};
        const InteractionRay ray = make_ray(mvp, screen_ndc);
        if (!ray.valid) {
            set_surface_hover(hit);
            return hit;
        }

        const float u0 = surface.u_min(time);
        const float u1 = surface.u_max(time);
        const float v0 = surface.v_min(time);
        const float v1 = surface.v_max(time);
        auto in_domain = [&](float t) {
            const Vec3 p = ray.origin + ray.direction * t;
            return p.x >= u0 && p.x <= u1 && p.y >= v0 && p.y <= v1;
        };
        auto f = [&](float t) {
            const Vec3 p = ray.origin + ray.direction * t;
            return p.z - surface.evaluate(p.x, p.y, time).z;
        };

        constexpr int samples = 256;
        constexpr float t_min = 0.05f;
        constexpr float t_max = 400.f;
        float best_t = t_min;
        float best_abs = std::numeric_limits<float>::max();
        float prev_t = t_min;
        float prev_f = f(prev_t);
        bool prev_valid = in_domain(prev_t);

        for (int i = 1; i <= samples; ++i) {
            const float t = t_min + (t_max - t_min) * static_cast<float>(i) / static_cast<float>(samples);
            if (!in_domain(t)) continue;
            const float ft = f(t);
            const float aft = ops::abs(ft);
            if (aft < best_abs) {
                best_abs = aft;
                best_t = t;
            }
            if (prev_valid && ((prev_f <= 0.f && ft >= 0.f) || (prev_f >= 0.f && ft <= 0.f))) {
                float lo = prev_t;
                float hi = t;
                float flo = prev_f;
                for (int it = 0; it < 32; ++it) {
                    const float mid = 0.5f * (lo + hi);
                    const float fm = f(mid);
                    if ((flo <= 0.f && fm >= 0.f) || (flo >= 0.f && fm <= 0.f)) {
                        hi = mid;
                    } else {
                        lo = mid;
                        flo = fm;
                    }
                }
                best_t = 0.5f * (lo + hi);
                best_abs = 0.f;
                break;
            }
            prev_t = t;
            prev_f = ft;
            prev_valid = true;
        }

        if (best_abs < 0.35f) {
            const Vec3 p = ray.origin + ray.direction * best_t;
            hit.uv = {std::clamp(p.x, u0, u1), std::clamp(p.y, v0, v1)};
            hit.world = surface.evaluate(hit.uv.x, hit.uv.y, time);
            hit.distance = best_t;
            hit.hit = true;
        }
        set_surface_hover(hit);
        return hit;
    }

    [[nodiscard]] ParticleTrailHit resolve_particle_trail_hit(RenderViewId view,
                                                              std::span<const TrailPickSample> samples,
                                                              const Mat4& mvp,
                                                              Vec2 viewport_size) {
        ParticleTrailHit hit{.view = view};
        const ViewMouseState mouse = mouse_state(view);
        if (!mouse.enabled || viewport_size.x <= 0.f || viewport_size.y <= 0.f) {
            set_particle_hover(hit);
            return hit;
        }

        float best = std::max(mouse.snap_radius_px, 1.f);
        for (const TrailPickSample& sample : samples) {
            const auto pixel = project_world_to_pixel(sample.world, mvp, viewport_size);
            if (!pixel) continue;
            const float dx = pixel->x - mouse.pixel.x;
            const float dy = pixel->y - mouse.pixel.y;
            const float distance = ops::sqrt(dx * dx + dy * dy);
            if (distance < best) {
                best = distance;
                hit.particle_id = sample.particle_id;
                hit.particle_index = sample.particle_index;
                hit.trail_index = sample.trail_index;
                hit.uv = sample.uv;
                hit.world = sample.world;
                hit.curvature = sample.curvature;
                hit.torsion = sample.torsion;
                hit.normal_curvature = sample.normal_curvature;
                hit.geodesic_curvature = sample.geodesic_curvature;
                hit.pixel_distance = distance;
                hit.hit = true;
            }
        }
        set_particle_hover(hit);
        return hit;
    }

    [[nodiscard]] const HoverMetadata& hover_metadata() const noexcept { return m_hover; }
    [[nodiscard]] InteractionTarget hover_target(RenderViewId view = 0) const noexcept {
        if (view != 0) {
            if (const InteractionTarget* target = target_for_view(m_hover_targets, view))
                return *target;
            return InteractionTarget{.view = view};
        }
        return m_hover_target;
    }

    [[nodiscard]] InteractionTarget selected_target(RenderViewId view = 0) const noexcept {
        if (view != 0) {
            if (const InteractionTarget* target = target_for_view(m_selected_targets, view))
                return *target;
            return InteractionTarget{.view = view};
        }
        return m_selected_target;
    }

    void set_hover_hits(SurfaceHit surface, ParticleTrailHit particle = {}) {
        if (!require_owner_thread("InteractionService::set_hover_hits")) return;
        set_surface_hover(surface);
        set_particle_hover(particle);
    }

    void set_hover_view_point(RenderViewId view, Vec2 point, Vec3 world = {}) {
        if (!require_owner_thread("InteractionService::set_hover_view_point")) return;
        if (view == 0) return;
        m_hover.view = view;
        m_hover.view_point = ViewPointHit{
            .view = view,
            .point = point,
            .world = world,
            .hit = true
        };
        set_hover_target(InteractionTarget{
            .kind = InteractionTargetKind::ViewPoint2D,
            .view = view,
            .uv = point,
            .point2d = point,
            .world = world,
            .valid = true
        });
    }

    void select_current_hover(RenderViewId view = 0) {
        if (!require_owner_thread("InteractionService::select_current_hover")) return;
        InteractionTarget target = hover_target(view);
        if (target.valid)
            set_selected_target(target);
    }

    void select_surface(SurfaceHit hit) {
        if (!require_owner_thread("InteractionService::select_surface")) return;
        if (!hit.hit) return;
        set_selected_target(InteractionTarget{
            .kind = InteractionTargetKind::SurfacePoint,
            .view = hit.view,
            .uv = hit.uv,
            .world = hit.world,
            .valid = true
        });
    }

    void select_view_point(RenderViewId view, Vec2 point, Vec3 world = {}) {
        if (!require_owner_thread("InteractionService::select_view_point")) return;
        if (view == 0) return;
        set_selected_target(InteractionTarget{
            .kind = InteractionTargetKind::ViewPoint2D,
            .view = view,
            .uv = point,
            .point2d = point,
            .world = world,
            .valid = true
        });
    }

private:
    memory::ViewVector<ViewMouseState> m_mouse;
    memory::ViewVector<SurfacePickRequest> m_surface_requests;
    memory::ViewVector<ViewPointPickRequest> m_view_point_requests;
    memory::ViewVector<InteractionTarget> m_hover_targets;
    memory::ViewVector<InteractionTarget> m_selected_targets;
    HoverMetadata m_hover{};
    InteractionTarget m_hover_target{};
    InteractionTarget m_selected_target{};
    memory::MemoryService* m_memory = nullptr;
    ThreadManagementService* m_threads = nullptr;
    ThreadRole m_owner_role = ThreadRole::Main;

    [[nodiscard]] bool require_owner_thread(std::string_view api_name) {
        return !m_threads || m_threads->require_thread_role(m_owner_role, api_name);
    }

    [[nodiscard]] static std::optional<Vec2> project_world_to_pixel(Vec3 world,
                                                                    const Mat4& mvp,
                                                                    Vec2 viewport_size) {
        const glm::vec4 clip = mvp * glm::vec4(world.x, world.y, world.z, 1.f);
        if (clip.w <= 1e-6f || viewport_size.x <= 0.f || viewport_size.y <= 0.f)
            return std::nullopt;
        const float ndc_x = clip.x / clip.w;
        const float ndc_y = clip.y / clip.w;
        if (ndc_x < -1.2f || ndc_x > 1.2f || ndc_y < -1.2f || ndc_y > 1.2f)
            return std::nullopt;
        return Vec2{
            (ndc_x + 1.f) * 0.5f * viewport_size.x,
            (1.f - ndc_y) * 0.5f * viewport_size.y
        };
    }

    [[nodiscard]] ViewMouseState* mouse_state_mut(RenderViewId view) noexcept {
        for (auto& state : m_mouse) {
            if (state.view == view)
                return &state;
        }
        return nullptr;
    }

    [[nodiscard]] const ViewMouseState* mouse_state_ptr(RenderViewId view) const noexcept {
        for (const auto& state : m_mouse) {
            if (state.view == view)
                return &state;
        }
        return nullptr;
    }

    void set_surface_hover(SurfaceHit hit) {
        m_hover.view = hit.view != 0 ? hit.view : m_hover.view;
        m_hover.surface = hit;
        if (hit.view != 0)
            m_hover.view_point = ViewPointHit{.view = hit.view};
        if (hit.hit) {
            set_hover_target(InteractionTarget{
                .kind = InteractionTargetKind::SurfacePoint,
                .view = hit.view,
                .uv = hit.uv,
                .world = hit.world,
                .valid = true
            });
        } else if (hit.view != 0) {
            set_hover_target(InteractionTarget{.view = hit.view});
        }
    }

    void set_particle_hover(ParticleTrailHit hit) {
        m_hover.view = hit.view != 0 ? hit.view : m_hover.view;
        m_hover.particle = hit;
        if (hit.hit) {
            set_hover_target(InteractionTarget{
                .kind = InteractionTargetKind::TrailSample,
                .view = hit.view,
                .uv = hit.uv,
                .world = hit.world,
                .particle_id = hit.particle_id,
                .particle_index = hit.particle_index,
                .trail_index = hit.trail_index,
                .curvature = hit.curvature,
                .torsion = hit.torsion,
                .normal_curvature = hit.normal_curvature,
                .geodesic_curvature = hit.geodesic_curvature,
                .valid = true
            });
        }
    }

    void set_hover_target(InteractionTarget target) {
        m_hover_target = target;
        set_target_for_view(m_hover_targets, target);
    }

    void set_selected_target(InteractionTarget target) {
        m_selected_target = target;
        set_target_for_view(m_selected_targets, target);
    }

    [[nodiscard]] static const InteractionTarget* target_for_view(const memory::ViewVector<InteractionTarget>& targets,
                                                                  RenderViewId view) noexcept {
        for (const InteractionTarget& target : targets) {
            if (target.view == view)
                return &target;
        }
        return nullptr;
    }

    static void set_target_for_view(memory::ViewVector<InteractionTarget>& targets, InteractionTarget target) {
        if (target.view == 0) return;
        for (InteractionTarget& existing : targets) {
            if (existing.view == target.view) {
                existing = target;
                return;
            }
        }
        targets.push_back(target);
    }
};

} // namespace ndde
