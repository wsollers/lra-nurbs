#pragma once
// engine/CameraInputController.hpp
// Converts per-view mouse samples into camera commands.

#include "engine/CameraService.hpp"

#include <array>
#include <cstddef>

namespace ndde {

enum class CameraCommandKind : u8 {
    Orbit,
    Pan,
    Zoom,
    Reset,
    PickSurface,
    PickViewPoint,
    SelectHover
};

struct CameraCommand {
    CameraCommandKind kind = CameraCommandKind::Pan;
    RenderViewId view = 0;
    Vec2 delta{};
    Vec2 normalized_pixel{};
    Vec2 screen_ndc{};
    f32 wheel_delta = 0.f;
    CameraPreset preset = CameraPreset::Home;
    u32 seed = 0u;
};

struct CameraInputSample {
    RenderViewId view = 0;
    CameraViewProfile profile = CameraViewProfile::Auto;
    Vec2 pixel{};
    Vec2 normalized_pixel{};
    Vec2 screen_ndc{};
    Vec2 delta{};
    f32 wheel_delta = 0.f;
    bool right_drag = false;
    bool middle_drag = false;
    bool shift = false;
    bool left_click = false;
    bool left_double_click = false;
    bool enabled = true;
    u32 perturb_seed = 0u;
};

class CameraInputController {
public:
    struct Result {
        std::array<CameraCommand, 4> commands{};
        std::size_t count = 0u;

        void push(CameraCommand command) noexcept {
            if (count < commands.size())
                commands[count++] = command;
        }
    };

    [[nodiscard]] Result build_commands(const RenderService& render,
                                        const CameraInputSample& input) const noexcept {
        Result result{};
        if (!input.enabled || input.view == 0)
            return result;

        const CameraViewProfile profile = resolve_profile(render, input.view, input.profile);
        if (profile == CameraViewProfile::Locked)
            return result;

        if (input.wheel_delta != 0.f) {
            result.push(CameraCommand{
                .kind = CameraCommandKind::Zoom,
                .view = input.view,
                .wheel_delta = input.wheel_delta
            });
        }
        if (input.left_click) {
            result.push(CameraCommand{
                .kind = CameraCommandKind::SelectHover,
                .view = input.view
            });
        }

        switch (profile) {
            case CameraViewProfile::PerspectiveSurface3D:
                if (input.middle_drag || (input.right_drag && input.shift)) {
                    result.push(CameraCommand{.kind = CameraCommandKind::Pan, .view = input.view, .delta = input.delta});
                } else if (input.right_drag) {
                    result.push(CameraCommand{.kind = CameraCommandKind::Orbit, .view = input.view, .delta = input.delta});
                }
                if (input.left_double_click) {
                    result.push(CameraCommand{
                        .kind = CameraCommandKind::PickSurface,
                        .view = input.view,
                        .normalized_pixel = input.normalized_pixel,
                        .screen_ndc = input.screen_ndc,
                        .seed = input.perturb_seed
                    });
                }
                break;
            case CameraViewProfile::Orthographic2D:
                if (input.middle_drag || input.right_drag)
                    result.push(CameraCommand{.kind = CameraCommandKind::Pan, .view = input.view, .delta = input.delta});
                if (input.left_double_click) {
                    result.push(CameraCommand{
                        .kind = CameraCommandKind::PickViewPoint,
                        .view = input.view,
                        .normalized_pixel = input.normalized_pixel,
                        .screen_ndc = input.screen_ndc,
                        .seed = input.perturb_seed
                    });
                }
                break;
            case CameraViewProfile::FreeFlight:
            case CameraViewProfile::FollowParticle:
            case CameraViewProfile::Auto:
            case CameraViewProfile::Locked:
                break;
        }
        return result;
    }

    [[nodiscard]] Result dispatch(CameraService& camera,
                                  InteractionService& interaction,
                                  const RenderService& render,
                                  const CameraInputSample& input) const {
        Result result = build_commands(render, input);
        for (std::size_t i = 0; i < result.count; ++i)
            execute(camera, interaction, result.commands[i]);
        return result;
    }

    static void execute(CameraService& camera,
                        InteractionService& interaction,
                        const CameraCommand& command) {
        switch (command.kind) {
            case CameraCommandKind::Orbit:
                camera.orbit(command.view, command.delta.x, command.delta.y);
                break;
            case CameraCommandKind::Pan:
                camera.pan(command.view, command.delta.x, command.delta.y);
                break;
            case CameraCommandKind::Zoom:
                camera.zoom(command.view, command.wheel_delta);
                break;
            case CameraCommandKind::Reset:
                camera.reset(command.view, command.preset);
                break;
            case CameraCommandKind::PickSurface:
                (void)camera.queue_surface_perturbation(interaction,
                    command.view,
                    command.normalized_pixel,
                    command.screen_ndc,
                    command.seed);
                break;
            case CameraCommandKind::PickViewPoint:
                interaction.queue_view_point_pick(ViewPointPickRequest{
                    .view = command.view,
                    .normalized_pixel = command.normalized_pixel,
                    .screen_ndc = command.screen_ndc,
                    .seed = command.seed
                });
                break;
            case CameraCommandKind::SelectHover:
                interaction.select_current_hover(command.view);
                break;
        }
    }

    [[nodiscard]] static CameraViewProfile resolve_profile(const RenderService& render,
                                                           RenderViewId view,
                                                           CameraViewProfile requested) noexcept {
        if (requested != CameraViewProfile::Auto)
            return requested;
        if (const auto* descriptor = render.descriptor(view)) {
            if (descriptor->camera_profile != CameraViewProfile::Auto)
                return descriptor->camera_profile;
            if (descriptor->projection == CameraProjection::Orthographic)
                return CameraViewProfile::Orthographic2D;
        }
        return CameraViewProfile::PerspectiveSurface3D;
    }
};

} // namespace ndde
