#include "engine/HotkeyService.hpp"
#include "engine/CameraInputController.hpp"
#include "engine/CameraService.hpp"
#include "engine/ISimulation.hpp"
#include "engine/PanelService.hpp"
#include "engine/RenderService.hpp"
#include "engine/SimulationHost.hpp"
#include "memory/MemoryService.hpp"

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

namespace {

using namespace ndde;

class InteractionPlaneSurface final : public ndde::math::ISurface {
public:
    [[nodiscard]] Vec3 evaluate(float u, float v, float = 0.f) const override {
        return {u, v, 0.f};
    }
    [[nodiscard]] float u_min(float = 0.f) const override { return -2.f; }
    [[nodiscard]] float u_max(float = 0.f) const override { return 2.f; }
    [[nodiscard]] float v_min(float = 0.f) const override { return -2.f; }
    [[nodiscard]] float v_max(float = 0.f) const override { return 2.f; }
};

class ArenaOwnedBase {
public:
    virtual ~ArenaOwnedBase() = default;
};

class ArenaOwnedDerived final : public ArenaOwnedBase {
public:
    explicit ArenaOwnedDerived(int* destroyed) : m_destroyed(destroyed) {}
    ~ArenaOwnedDerived() override {
        if (m_destroyed) ++(*m_destroyed);
    }

private:
    int* m_destroyed = nullptr;
};

class ArenaValue final {
public:
    explicit ArenaValue(int value) : m_value(value) {}
    [[nodiscard]] int plus_one() const noexcept { return m_value + 1; }

private:
    int m_value = 0;
};

TEST(PanelService, RegisterUnregisterPanel) {
    PanelService panels;
    int draws = 0;

    auto handle = panels.register_panel(PanelDescriptor{
        .title = "Particles",
        .category = "Simulation",
        .draw = [&draws] { ++draws; }
    });

    EXPECT_EQ(panels.active_count(), 1u);
    EXPECT_EQ(panels.active_count(PanelScope::Simulation), 1u);
    EXPECT_EQ(panels.active_count(PanelScope::Global), 0u);
    EXPECT_TRUE(panels.contains("Particles"));

    panels.draw_registered_panels();
    EXPECT_EQ(draws, 1);

    handle.reset();
    EXPECT_EQ(panels.active_count(), 0u);
    EXPECT_FALSE(panels.contains("Particles"));

    panels.draw_registered_panels();
    EXPECT_EQ(draws, 1);
}

TEST(PanelService, DrawsPanelsByScope) {
    PanelService panels;
    int global_draws = 0;
    int simulation_draws = 0;

    auto global = panels.register_panel(PanelDescriptor{
        .title = "Global",
        .scope = PanelScope::Global,
        .draw = [&global_draws] { ++global_draws; }
    });
    auto simulation = panels.register_panel(PanelDescriptor{
        .title = "Simulation",
        .scope = PanelScope::Simulation,
        .draw = [&simulation_draws] { ++simulation_draws; }
    });

    panels.draw_registered_panels(PanelScope::Global);
    EXPECT_EQ(global_draws, 1);
    EXPECT_EQ(simulation_draws, 0);

    panels.draw_registered_panels(PanelScope::Simulation);
    EXPECT_EQ(global_draws, 1);
    EXPECT_EQ(simulation_draws, 1);
}

TEST(PanelService, HandleUnregistersOnDestruction) {
    PanelService panels;
    {
        auto handle = panels.register_panel(PanelDescriptor{
            .title = "Temporary",
            .draw = [] {}
        });
        EXPECT_TRUE(handle);
        EXPECT_EQ(panels.active_count(), 1u);
    }
    EXPECT_EQ(panels.active_count(), 0u);
}

TEST(HotkeyService, DispatchesMatchingChord) {
    HotkeyService hotkeys;
    int calls = 0;

    auto handle = hotkeys.register_action(HotkeyDescriptor{
        .chord = {.key = 66, .mods = 2},
        .label = "Spawn Brownian",
        .callback = [&calls] { ++calls; }
    });

    EXPECT_EQ(hotkeys.active_count(), 1u);
    EXPECT_TRUE(hotkeys.contains("Spawn Brownian"));
    EXPECT_FALSE(hotkeys.dispatch({.key = 67, .mods = 2}));
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(hotkeys.dispatch({.key = 66, .mods = 2}));
    EXPECT_EQ(calls, 1);

    handle.reset();
    EXPECT_EQ(hotkeys.active_count(), 0u);
    EXPECT_FALSE(hotkeys.dispatch({.key = 66, .mods = 2}));
    EXPECT_EQ(calls, 1);
}

TEST(InteractionService, TracksMouseAndSurfacePickRequests) {
    ndde::memory::MemoryService memory;
    memory.begin_frame();
    InteractionService interaction;
    interaction.set_memory_service(&memory);
    interaction.set_mouse(7u, {320.f, 240.f}, {0.f, 0.f}, true, 18.f);
    const ViewMouseState mouse = interaction.mouse_state(7u);
    EXPECT_TRUE(mouse.enabled);
    EXPECT_FLOAT_EQ(mouse.pixel.x, 320.f);
    EXPECT_FLOAT_EQ(mouse.snap_radius_px, 18.f);

    interaction.queue_surface_pick(SurfacePickRequest{
        .view = 7u,
        .fallback_uv = {0.25f, -0.5f},
        .screen_ndc = {0.f, 0.f},
        .seed = 42u
    });
    const auto requests = interaction.consume_surface_picks(7u);
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_EQ(requests.get_allocator().resource(), memory.frame().resource());
    EXPECT_FLOAT_EQ(requests.front().fallback_uv.y, -0.5f);
    EXPECT_TRUE(interaction.consume_surface_picks(7u).empty());
}

TEST(InteractionService, ResolvesSurfaceAndTrailHits) {
    InteractionService interaction;
    InteractionPlaneSurface surface;
    const Mat4 mvp = glm::perspective(glm::radians(45.f), 1.f, 0.05f, 20.f)
        * glm::lookAt(Vec3{0.f, 0.f, 4.f}, Vec3{0.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f});

    const SurfaceHit surface_hit = interaction.resolve_surface_hit(5u, surface, mvp, {0.f, 0.f});
    EXPECT_TRUE(surface_hit.hit);
    EXPECT_NEAR(surface_hit.uv.x, 0.f, 1e-4f);
    EXPECT_NEAR(surface_hit.uv.y, 0.f, 1e-4f);

    interaction.set_mouse(5u, {100.f, 100.f}, {0.f, 0.f}, true, 16.f);
    const TrailPickSample samples[] = {
        {.particle_id = 11u, .particle_index = 0u, .trail_index = 3u, .uv = {-1.f, 0.f}, .world = {-1.f, 0.f, 0.f}, .curvature = 0.1f},
        {.particle_id = 12u, .particle_index = 1u, .trail_index = 4u, .uv = {2.f, 3.f}, .world = {0.f, 0.f, 0.f}, .curvature = 0.2f, .torsion = 0.3f}
    };
    const ParticleTrailHit trail_hit = interaction.resolve_particle_trail_hit(
        5u, samples, mvp, {200.f, 200.f});
    EXPECT_TRUE(trail_hit.hit);
    EXPECT_EQ(trail_hit.particle_id, 12u);
    EXPECT_EQ(trail_hit.trail_index, 4u);
    EXPECT_FLOAT_EQ(trail_hit.uv.x, 2.f);
    EXPECT_FLOAT_EQ(trail_hit.uv.y, 3.f);
    EXPECT_FLOAT_EQ(trail_hit.curvature, 0.2f);

    const HoverMetadata& hover = interaction.hover_metadata();
    EXPECT_TRUE(hover.surface.hit);
    EXPECT_TRUE(hover.particle.hit);

    const InteractionTarget hover_target = interaction.hover_target(5u);
    EXPECT_EQ(hover_target.kind, InteractionTargetKind::TrailSample);
    EXPECT_EQ(hover_target.particle_id, 12u);
    interaction.select_current_hover(5u);
    const InteractionTarget selected = interaction.selected_target(5u);
    EXPECT_TRUE(selected.valid);
    EXPECT_EQ(selected.kind, InteractionTargetKind::TrailSample);
    EXPECT_EQ(selected.trail_index, 4u);

    interaction.select_surface(surface_hit);
    const InteractionTarget selected_surface = interaction.selected_target(5u);
    EXPECT_EQ(selected_surface.kind, InteractionTargetKind::SurfacePoint);
    EXPECT_NEAR(selected_surface.uv.x, 0.f, 1e-4f);
}

TEST(InteractionService, TracksViewPointHoverTargets) {
    InteractionService interaction;
    interaction.set_hover_view_point(9u, {1.25f, -0.5f}, {1.25f, -0.5f, 0.f});

    const HoverMetadata& hover = interaction.hover_metadata();
    EXPECT_EQ(hover.view, 9u);
    EXPECT_TRUE(hover.view_point.hit);
    EXPECT_FLOAT_EQ(hover.view_point.point.x, 1.25f);
    EXPECT_FLOAT_EQ(hover.view_point.point.y, -0.5f);

    const InteractionTarget target = interaction.hover_target(9u);
    EXPECT_TRUE(target.valid);
    EXPECT_EQ(target.kind, InteractionTargetKind::ViewPoint2D);
    EXPECT_FLOAT_EQ(target.point2d.x, 1.25f);
    EXPECT_FLOAT_EQ(target.point2d.y, -0.5f);

    interaction.select_current_hover(9u);
    const InteractionTarget selected = interaction.selected_target(9u);
    EXPECT_TRUE(selected.valid);
    EXPECT_EQ(selected.kind, InteractionTargetKind::ViewPoint2D);
    EXPECT_FLOAT_EQ(selected.point2d.x, 1.25f);
}

TEST(RenderService, RegistersMainAndAlternateViewsAndQueuesPackets) {
    ndde::memory::MemoryService memory;
    memory.begin_frame();
    RenderService render;
    render.set_memory_service(&memory);
    RenderViewId main_id = 0;
    RenderViewId alt_id = 0;

    auto main = render.register_view(RenderViewDescriptor{
        .title = "Surface 3D",
        .kind = RenderViewKind::Main
    }, &main_id);
    auto alt = render.register_view(RenderViewDescriptor{
        .title = "Alternate View",
        .kind = RenderViewKind::Alternate,
        .alternate_mode = AlternateViewMode::VectorField,
        .projection = CameraProjection::Orthographic
    }, &alt_id);

    ASSERT_NE(main_id, 0u);
    ASSERT_NE(alt_id, 0u);
    EXPECT_EQ(render.active_view_count(), 2u);
    EXPECT_TRUE(render.contains_view("Surface 3D"));
    EXPECT_TRUE(render.contains_view("Alternate View"));
    ASSERT_NE(render.descriptor(alt_id), nullptr);
    EXPECT_EQ(render.descriptor(alt_id)->alternate_mode, AlternateViewMode::VectorField);
    EXPECT_EQ(render.descriptor(alt_id)->projection, CameraProjection::Orthographic);

    render.set_view_domain(main_id, RenderViewDomain{.u_min = -2.f, .u_max = 2.f, .v_min = -3.f, .v_max = 3.f});
    EXPECT_FLOAT_EQ(render.view_domain(main_id).u_min, -2.f);
    render.set_viewport_size(main_id, Vec2{1920.f, 1080.f});
    EXPECT_FLOAT_EQ(render.descriptor(main_id)->viewport_aspect, 1920.f / 1080.f);
    EXPECT_FLOAT_EQ(render.descriptor(main_id)->viewport_size.x, 1920.f);
    CameraService camera;
    camera.set_render_service(&render);
    const float yaw_before = render.descriptor(main_id)->camera.yaw;
    camera.orbit_main(10.f, -5.f);
    EXPECT_NE(render.descriptor(main_id)->camera.yaw, yaw_before);
    camera.zoom_main(1.f);
    EXPECT_GT(render.descriptor(main_id)->camera.zoom, 1.f);
    camera.reset_main(CameraPreset::Top);
    EXPECT_NEAR(render.descriptor(main_id)->camera.pitch, 1.35f, 1e-5f);
    render.queue_surface_perturbation(SurfacePerturbCommand{.view = main_id, .uv = {0.25f, -0.5f}, .seed = 42u});
    {
        const auto commands = render.consume_surface_perturbations(main_id);
        ASSERT_EQ(commands.size(), 1u);
        EXPECT_EQ(commands.get_allocator().resource(), memory.frame().resource());
        EXPECT_FLOAT_EQ(commands.front().uv.x, 0.25f);
    }

    const Vertex verts[] = {
        {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f, 1.f}},
        {{1.f, 0.f, 0.f}, {1.f, 0.f, 0.f, 1.f}}
    };
    render.submit(main_id, verts, Topology::LineList, DrawMode::VertexColor, {1,1,1,1}, Mat4{1.f});
    render.submit(alt_id, verts, Topology::LineList, DrawMode::VertexColor, {1,1,1,1}, Mat4{1.f});

    EXPECT_EQ(render.packet_count(main_id), 1u);
    EXPECT_EQ(render.packet_count(alt_id), 1u);
    EXPECT_EQ(render.packets().size(), 2u);
    EXPECT_EQ(render.packets().front().vertices.size(), 2u);
    memory.begin_frame();
    ASSERT_EQ(render.packets().size(), 2u);
    EXPECT_EQ(render.packets().front().vertices.size(), 2u);
    EXPECT_FLOAT_EQ(render.packets().front().vertices.front().pos.x, 0.f);
    render.clear_packets();
    EXPECT_TRUE(render.packets().empty());

    const auto snapshots = render.active_view_snapshots();
    ASSERT_EQ(snapshots.size(), 2u);
    EXPECT_EQ(snapshots.get_allocator().resource(), memory.frame().resource());

    main.reset();
    EXPECT_EQ(render.active_view_count(), 1u);
    render.submit(main_id, verts, Topology::LineList, DrawMode::VertexColor, {1,1,1,1}, Mat4{1.f});
    EXPECT_EQ(render.packet_count(main_id), 0u);
}

TEST(CameraInputController, PerspectiveProfileOrbitsAndZooms) {
    memory::MemoryService memory;
    RenderService render;
    render.set_memory_service(&memory);
    RenderViewId view_id = 0;
    const auto view = render.register_view(RenderViewDescriptor{
        .title = "Surface",
        .kind = RenderViewKind::Main,
        .projection = CameraProjection::Perspective
    }, &view_id);
    render.set_view_domain(view_id, RenderViewDomain{.u_min = -2.f, .u_max = 2.f, .v_min = -2.f, .v_max = 2.f});

    CameraService camera;
    camera.set_render_service(&render);
    InteractionService interaction;
    interaction.set_memory_service(&memory);
    CameraInputController input;

    const CameraState before = render.descriptor(view_id)->camera;
    const auto result = input.dispatch(camera, interaction, render, CameraInputSample{
        .view = view_id,
        .profile = CameraViewProfile::PerspectiveSurface3D,
        .delta = {12.f, -8.f},
        .wheel_delta = 1.f,
        .right_drag = true,
        .enabled = true
    });

    EXPECT_GE(result.count, 2u);
    EXPECT_NE(render.descriptor(view_id)->camera.yaw, before.yaw);
    EXPECT_NE(render.descriptor(view_id)->camera.pitch, before.pitch);
    EXPECT_GT(render.descriptor(view_id)->camera.zoom, before.zoom);
}

TEST(CameraInputController, OrthographicRightDragPansWithoutOrbiting) {
    memory::MemoryService memory;
    RenderService render;
    render.set_memory_service(&memory);
    RenderViewId view_id = 0;
    const auto view = render.register_view(RenderViewDescriptor{
        .title = "Phase",
        .kind = RenderViewKind::Alternate,
        .projection = CameraProjection::Orthographic
    }, &view_id);
    render.set_view_domain(view_id, RenderViewDomain{.u_min = -4.f, .u_max = 4.f, .v_min = -3.f, .v_max = 3.f});

    CameraService camera;
    camera.set_render_service(&render);
    InteractionService interaction;
    interaction.set_memory_service(&memory);
    CameraInputController input;

    const CameraState before = render.descriptor(view_id)->camera;
    const auto result = input.dispatch(camera, interaction, render, CameraInputSample{
        .view = view_id,
        .profile = CameraViewProfile::Orthographic2D,
        .delta = {20.f, 10.f},
        .right_drag = true,
        .enabled = true
    });

    ASSERT_EQ(result.count, 1u);
    EXPECT_EQ(result.commands[0].kind, CameraCommandKind::Pan);
    EXPECT_FLOAT_EQ(render.descriptor(view_id)->camera.yaw, before.yaw);
    EXPECT_FLOAT_EQ(render.descriptor(view_id)->camera.pitch, before.pitch);
    EXPECT_NE(render.descriptor(view_id)->camera.target.x, before.target.x);
}

TEST(CameraInputController, DescriptorProfileOverridesProjectionDefault) {
    memory::MemoryService memory;
    RenderService render;
    render.set_memory_service(&memory);
    RenderViewId view_id = 0;
    const auto view = render.register_view(RenderViewDescriptor{
        .title = "Locked Surface",
        .projection = CameraProjection::Perspective,
        .camera_profile = CameraViewProfile::Locked
    }, &view_id);

    CameraInputController input;
    const auto result = input.build_commands(render, CameraInputSample{
        .view = view_id,
        .profile = CameraViewProfile::Auto,
        .delta = {10.f, 10.f},
        .wheel_delta = 1.f,
        .right_drag = true,
        .left_double_click = true,
        .enabled = true,
        .perturb_seed = 1u
    });

    EXPECT_EQ(result.count, 0u);
    EXPECT_EQ(CameraInputController::resolve_profile(render, view_id, CameraViewProfile::Auto),
              CameraViewProfile::Locked);
}

TEST(CameraInputController, WheelZoomClamps) {
    memory::MemoryService memory;
    RenderService render;
    render.set_memory_service(&memory);
    RenderViewId view_id = 0;
    const auto view = render.register_view(RenderViewDescriptor{.title = "View"}, &view_id);
    CameraService camera;
    camera.set_render_service(&render);
    InteractionService interaction;
    interaction.set_memory_service(&memory);
    CameraInputController input;

    (void)input.dispatch(camera, interaction, render, CameraInputSample{
        .view = view_id,
        .wheel_delta = 1000.f,
        .enabled = true
    });
    EXPECT_FLOAT_EQ(render.descriptor(view_id)->camera.zoom, 30.f);

    (void)input.dispatch(camera, interaction, render, CameraInputSample{
        .view = view_id,
        .wheel_delta = -1000.f,
        .enabled = true
    });
    EXPECT_FLOAT_EQ(render.descriptor(view_id)->camera.zoom, 0.08f);
}

TEST(CameraInputController, ResetUsesViewDomainCenter) {
    memory::MemoryService memory;
    RenderService render;
    render.set_memory_service(&memory);
    RenderViewId view_id = 0;
    const auto view = render.register_view(RenderViewDescriptor{.title = "View"}, &view_id);
    render.set_view_domain(view_id, RenderViewDomain{
        .u_min = 2.f, .u_max = 6.f,
        .v_min = -3.f, .v_max = 1.f,
        .z_min = -2.f, .z_max = 4.f
    });

    CameraService camera;
    camera.set_render_service(&render);
    camera.reset(view_id, CameraPreset::Home);

    EXPECT_FLOAT_EQ(render.descriptor(view_id)->camera.target.x, 4.f);
    EXPECT_FLOAT_EQ(render.descriptor(view_id)->camera.target.y, -1.f);
    EXPECT_FLOAT_EQ(render.descriptor(view_id)->camera.target.z, 1.f);
}

TEST(CameraInputController, FrameSelectionMovesCameraTarget) {
    memory::MemoryService memory;
    RenderService render;
    render.set_memory_service(&memory);
    RenderViewId view_id = 0;
    const auto view = render.register_view(RenderViewDescriptor{.title = "View"}, &view_id);
    CameraService camera;
    camera.set_render_service(&render);
    InteractionService interaction;
    interaction.set_memory_service(&memory);

    interaction.select_view_point(view_id, {1.5f, -2.0f}, {1.5f, -2.0f, 0.f});
    EXPECT_TRUE(camera.frame_selection(interaction));
    EXPECT_FLOAT_EQ(render.descriptor(view_id)->camera.target.x, 1.5f);
    EXPECT_FLOAT_EQ(render.descriptor(view_id)->camera.target.y, -2.0f);
}

TEST(CameraInputController, SurfacePickOnlyEmitsForPerspectiveSurfaceProfile) {
    memory::MemoryService memory;
    RenderService render;
    render.set_memory_service(&memory);
    InteractionService interaction;
    interaction.set_memory_service(&memory);
    CameraService camera;
    camera.set_render_service(&render);
    CameraInputController input;

    RenderViewId surface_view = 0;
    const auto surface = render.register_view(RenderViewDescriptor{
        .title = "Surface",
        .kind = RenderViewKind::Main,
        .projection = CameraProjection::Perspective
    }, &surface_view);
    render.set_view_domain(surface_view, RenderViewDomain{.u_min = -2.f, .u_max = 2.f, .v_min = -4.f, .v_max = 4.f});

    RenderViewId phase_view = 0;
    const auto phase = render.register_view(RenderViewDescriptor{
        .title = "Phase",
        .kind = RenderViewKind::Alternate,
        .projection = CameraProjection::Orthographic
    }, &phase_view);

    (void)input.dispatch(camera, interaction, render, CameraInputSample{
        .view = phase_view,
        .profile = CameraViewProfile::Orthographic2D,
        .normalized_pixel = {0.5f, 0.5f},
        .screen_ndc = {0.f, 0.f},
        .left_double_click = true,
        .enabled = true,
        .perturb_seed = 11u
    });
    EXPECT_TRUE(interaction.consume_surface_picks(phase_view).empty());
    const auto phase_picks = interaction.consume_view_point_picks(phase_view);
    ASSERT_EQ(phase_picks.size(), 1u);
    EXPECT_EQ(phase_picks.front().seed, 11u);

    (void)input.dispatch(camera, interaction, render, CameraInputSample{
        .view = surface_view,
        .profile = CameraViewProfile::PerspectiveSurface3D,
        .normalized_pixel = {0.25f, 0.75f},
        .screen_ndc = {-0.5f, -0.5f},
        .left_click = true,
        .left_double_click = true,
        .enabled = true,
        .perturb_seed = 12u
    });
    const auto picks = interaction.consume_surface_picks(surface_view);
    ASSERT_EQ(picks.size(), 1u);
    EXPECT_FLOAT_EQ(picks.front().fallback_uv.x, -1.f);
    EXPECT_FLOAT_EQ(picks.front().fallback_uv.y, -2.f);
    EXPECT_EQ(picks.front().seed, 12u);
}

TEST(ViewInputService, CapturesDragWithinViewAndIgnoresBlockedClicks) {
    ViewInputService input;
    const RenderViewId view = 42;
    const ViewInputRect rect{.origin = {}, .size = {800.f, 600.f}};

    const auto blocked = input.update(ViewInputUpdate{
        .view = view,
        .rect = rect,
        .cursor = {200.f, 150.f},
        .buttons = ViewPointerButtons{.left_click = true, .left_double_click = true},
        .ui_blocked = true
    });
    EXPECT_FALSE(blocked.enabled);
    EXPECT_FALSE(blocked.left_double_click);

    const auto start = input.update(ViewInputUpdate{
        .view = view,
        .rect = rect,
        .cursor = {200.f, 150.f},
        .buttons = ViewPointerButtons{.right_down = true}
    });
    EXPECT_TRUE(start.enabled);
    EXPECT_TRUE(start.captured);
    EXPECT_TRUE(start.right_drag);

    const auto drag = input.update(ViewInputUpdate{
        .view = view,
        .rect = rect,
        .cursor = {900.f, 700.f},
        .buttons = ViewPointerButtons{.right_down = true},
        .ui_blocked = true
    });
    EXPECT_TRUE(drag.enabled);
    EXPECT_TRUE(drag.captured);
    EXPECT_TRUE(drag.right_drag);
    EXPECT_FLOAT_EQ(drag.normalized_pixel.x, 1.f);
    EXPECT_FLOAT_EQ(drag.normalized_pixel.y, 1.f);

    const auto released = input.update(ViewInputUpdate{
        .view = view,
        .rect = rect,
        .cursor = {900.f, 700.f}
    });
    EXPECT_FALSE(released.enabled);
    EXPECT_FALSE(released.captured);
}

TEST(SimulationClock, AdvancesTickAndTimeAndPauses) {
    SimulationClock clock;

    TickInfo t0 = clock.next(0.25f);
    EXPECT_EQ(t0.tick_index, 1u);
    EXPECT_FLOAT_EQ(t0.dt, 0.25f);
    EXPECT_FLOAT_EQ(t0.time, 0.25f);
    EXPECT_FALSE(t0.paused);

    TickInfo paused = clock.next(0.25f, true);
    EXPECT_EQ(paused.tick_index, 1u);
    EXPECT_FLOAT_EQ(paused.dt, 0.f);
    EXPECT_FLOAT_EQ(paused.time, 0.25f);
    EXPECT_TRUE(paused.paused);

    clock.reset();
    EXPECT_EQ(clock.current().tick_index, 0u);
    EXPECT_FLOAT_EQ(clock.current().time, 0.f);
}

TEST(MemoryService, TracksLifetimeScopeResetsAndCreatesPolicyVectors) {
    ndde::memory::MemoryService memory;

    const u64 frame0 = memory.frame().generation();
    const u64 sim0 = memory.simulation().generation();
    const u64 view0 = memory.view().generation();

    {
        auto frame_vertices = memory.frame().make_vector<Vertex>(3u);
        auto cache_vertices = memory.cache().make_vector<Vertex>();
        auto sim_particles = memory.simulation().make_vector<int>();
        sim_particles.push_back(42);

        EXPECT_EQ(frame_vertices.size(), 3u);
        EXPECT_EQ(cache_vertices.get_allocator().resource(), memory.cache().resource());
        EXPECT_EQ(sim_particles.get_allocator().resource(), memory.simulation().resource());
        EXPECT_EQ(sim_particles.front(), 42);
    }

    memory.begin_frame();
    auto bound_frame_vertices = memory.frame().make_vector<Vertex>();
    EXPECT_EQ(bound_frame_vertices.get_allocator().resource(), memory.frame().resource());
    EXPECT_EQ(memory.frame().generation(), frame0 + 1u);
    EXPECT_EQ(memory.simulation().generation(), sim0);

    memory.reset_simulation();
    EXPECT_EQ(memory.simulation().generation(), sim0 + 1u);
    EXPECT_EQ(memory.view().generation(), view0);

    memory.reset_view();
    EXPECT_EQ(memory.view().generation(), view0 + 1u);
}

TEST(MemoryService, ScopeUniqueOwnsArenaConstructedPolymorphicObjects) {
    ndde::memory::MemoryService memory;
    int destroyed = 0;

    {
        ndde::memory::Unique<ArenaOwnedBase> owned =
            memory.simulation().make_unique<ArenaOwnedDerived>(&destroyed);
        ASSERT_TRUE(owned);
        EXPECT_EQ(destroyed, 0);
    }

    EXPECT_EQ(destroyed, 1);
}

TEST(MemoryService, ScopeUniqueSupportsAccessBeforeScopeReset) {
    ndde::memory::MemoryService memory;

    auto owned = memory.simulation().make_unique<ArenaValue>(41);

    ASSERT_TRUE(owned);
    EXPECT_NE(owned.get(), nullptr);
    EXPECT_EQ(owned->plus_one(), 42);
}

TEST(MemoryService, ScopeUniqueCanBeDestroyedBeforeScopeReset) {
    ndde::memory::MemoryService memory;
    int destroyed = 0;

    {
        auto owned = memory.simulation().make_unique<ArenaOwnedDerived>(&destroyed);
        owned.reset();
        memory.reset_simulation();
    }

    EXPECT_EQ(destroyed, 1);
}

TEST(MemoryService, LifetimeVectorsCanBeUsedBeforeScopeReset) {
    ndde::memory::MemoryService memory;

    auto sim_values = memory.simulation().make_vector<int>();
    auto history_values = memory.history().make_vector<int>();
    sim_values.push_back(7);
    history_values.push_back(11);

    EXPECT_EQ(sim_values.front(), 7);
    EXPECT_EQ(history_values.front(), 11);
}

#ifndef NDEBUG
TEST(MemoryServiceDeathTest, ScopeUniqueAssertsWhenDestroyedAfterScopeReset) {
    EXPECT_DEATH_IF_SUPPORTED({
        ndde::memory::MemoryService memory;
        auto owned = memory.simulation().make_unique<ArenaOwnedDerived>(nullptr);
        memory.reset_simulation();
        owned.reset();
    }, "memory::Unique");
}

TEST(MemoryServiceDeathTest, SimVectorAssertsWhenUsedAfterSimulationScopeReset) {
    EXPECT_DEATH_IF_SUPPORTED({
        ndde::memory::MemoryService memory;
        auto values = memory.simulation().make_vector<int>();
        values.push_back(1);
        memory.reset_simulation();
        (void)values.size();
    }, "memory lifetime vector");
}

TEST(MemoryServiceDeathTest, HistoryVectorAssertsWhenUsedAfterHistoryScopeReset) {
    EXPECT_DEATH_IF_SUPPORTED({
        ndde::memory::MemoryService memory;
        auto values = memory.history().make_vector<int>();
        values.push_back(1);
        memory.reset_history();
        values.push_back(2);
    }, "memory lifetime vector");
}
#endif

TEST(SimulationHost, ExposesOnlyServiceFacade) {
    EngineServices services;
    SimulationHost host = services.simulation_host();

    auto panel = host.panels().register_panel(PanelDescriptor{
        .title = "Host Panel",
        .draw = [] {}
    });
    auto hotkey = host.hotkeys().register_action(HotkeyDescriptor{
        .chord = {.key = 1, .mods = 0},
        .label = "Host Hotkey",
        .callback = [] {}
    });
    RenderViewId view_id = 0;
    auto view = host.render().register_view(RenderViewDescriptor{
        .title = "Host View",
        .kind = RenderViewKind::Main
    }, &view_id);
    host.interaction().set_mouse(view_id, {10.f, 20.f}, {0.f, 0.f}, true);

    EXPECT_EQ(services.panels().active_count(), 1u);
    EXPECT_EQ(services.hotkeys().active_count(), 1u);
    EXPECT_EQ(services.render().active_view_count(), 1u);
    EXPECT_TRUE(services.interaction().mouse_state(view_id).enabled);
    EXPECT_EQ(host.clock().next(0.1f).tick_index, 1u);
    LogRecordId log_id;
    ASSERT_TRUE(host.threads().run_logger_task_sync([&host, &log_id] {
        log_id = host.logger().write(LogSeverity::Info,
                                     LogCategory::Simulation,
                                     {},
                                     "host log");
    }));
    EXPECT_EQ(services.logger().message(log_id), "host log");
    EXPECT_EQ(&host.memory(), &services.memory());
    EXPECT_EQ(&host.metrics(), &services.metrics());
}

TEST(ServiceHandle, HandlesUnregisterNormallyBeforeServiceRebind) {
    ndde::memory::MemoryService memory;
    PanelService panels;
    HotkeyService hotkeys;
    RenderService render;
    panels.set_memory_service(&memory);
    hotkeys.set_memory_service(&memory);
    render.set_memory_service(&memory);

    auto panel = panels.register_panel(PanelDescriptor{.title = "Panel", .draw = [] {}});
    auto hotkey = hotkeys.register_action(HotkeyDescriptor{
        .chord = {.key = 7, .mods = 0},
        .label = "Hotkey",
        .callback = [] {}
    });
    RenderViewId view_id = 0;
    auto view = render.register_view(RenderViewDescriptor{.title = "View"}, &view_id);

    EXPECT_EQ(panels.active_count(), 1u);
    EXPECT_EQ(hotkeys.active_count(), 1u);
    EXPECT_EQ(render.active_view_count(), 1u);

    panel.reset();
    hotkey.reset();
    view.reset();

    EXPECT_EQ(panels.active_count(), 0u);
    EXPECT_EQ(hotkeys.active_count(), 0u);
    EXPECT_EQ(render.active_view_count(), 0u);
}

#ifndef NDEBUG
TEST(ServiceHandleDeathTest, PanelHandleAssertsAfterPanelServiceRebind) {
    EXPECT_DEATH_IF_SUPPORTED({
        ndde::memory::MemoryService first;
        ndde::memory::MemoryService second;
        PanelService panels;
        panels.set_memory_service(&first);
        auto handle = panels.register_panel(PanelDescriptor{.title = "Panel", .draw = [] {}});
        panels.set_memory_service(&second);
        handle.reset();
    }, "service handle");
}

TEST(ServiceHandleDeathTest, HotkeyHandleAssertsAfterHotkeyServiceRebind) {
    EXPECT_DEATH_IF_SUPPORTED({
        ndde::memory::MemoryService first;
        ndde::memory::MemoryService second;
        HotkeyService hotkeys;
        hotkeys.set_memory_service(&first);
        auto handle = hotkeys.register_action(HotkeyDescriptor{
            .chord = {.key = 7, .mods = 0},
            .label = "Hotkey",
            .callback = [] {}
        });
        hotkeys.set_memory_service(&second);
        handle.reset();
    }, "service handle");
}

TEST(ServiceHandleDeathTest, RenderViewHandleAssertsAfterRenderServiceRebind) {
    EXPECT_DEATH_IF_SUPPORTED({
        ndde::memory::MemoryService first;
        ndde::memory::MemoryService second;
        RenderService render;
        render.set_memory_service(&first);
        auto handle = render.register_view(RenderViewDescriptor{.title = "View"});
        render.set_memory_service(&second);
        handle.reset();
    }, "service handle");
}
#endif

class DummySimulation final : public ISimulation {
public:
    std::string_view name() const override { return "Dummy"; }

    void on_register(SimulationHost& host) override {
        panel = host.panels().register_panel(PanelDescriptor{.title = "Dummy Panel", .draw = [] {}});
        hotkey = host.hotkeys().register_action(HotkeyDescriptor{
            .chord = {.key = 4, .mods = 2},
            .label = "Dummy Hotkey",
            .callback = [this] { ++hotkey_calls; }
        });
        view = host.render().register_view(RenderViewDescriptor{.title = "Dummy View"}, &view_id);
    }

    void on_start() override { started = true; }
    void on_tick(const TickInfo& tick) override {
        last_tick = tick;
        ++ticks;
    }
    void on_stop() override {
        stopped = true;
        panel.reset();
        hotkey.reset();
        view.reset();
    }

    PanelHandle panel;
    HotkeyHandle hotkey;
    RenderViewHandle view;
    RenderViewId view_id = 0;
    TickInfo last_tick{};
    int ticks = 0;
    int hotkey_calls = 0;
    bool started = false;
    bool stopped = false;
};

TEST(ISimulation, RegistersServicesAndRollsBackHandles) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    DummySimulation sim;

    sim.on_register(host);
    sim.on_start();
    sim.on_tick(host.clock().next(0.016f));

    EXPECT_TRUE(sim.started);
    EXPECT_EQ(sim.ticks, 1);
    EXPECT_EQ(sim.last_tick.tick_index, 1u);
    EXPECT_EQ(services.panels().active_count(), 1u);
    EXPECT_EQ(services.hotkeys().active_count(), 1u);
    EXPECT_EQ(services.render().active_view_count(), 1u);

    EXPECT_TRUE(services.hotkeys().dispatch({.key = 4, .mods = 2}));
    EXPECT_EQ(sim.hotkey_calls, 1);

    sim.on_stop();
    EXPECT_TRUE(sim.stopped);
    EXPECT_EQ(services.panels().active_count(), 0u);
    EXPECT_EQ(services.hotkeys().active_count(), 0u);
    EXPECT_EQ(services.render().active_view_count(), 0u);
    EXPECT_FALSE(services.hotkeys().dispatch({.key = 4, .mods = 2}));
}

} // namespace
