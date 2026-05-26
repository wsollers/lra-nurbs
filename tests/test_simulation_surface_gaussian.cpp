#include "app/SimulationSurfaceGaussian.hpp"
#include "app/SimulationRenderPackets.hpp"
#include "engine/SimulationHost.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>

namespace {

using namespace ndde;

std::optional<Vec2> test_project_world_to_pixel(Vec3 world, const Mat4& mvp, Vec2 viewport_size) {
    const glm::vec4 clip = mvp * glm::vec4(world.x, world.y, world.z, 1.f);
    if (clip.w <= 1e-6f) return std::nullopt;
    return Vec2{
        (clip.x / clip.w + 1.f) * 0.5f * viewport_size.x,
        (1.f - clip.y / clip.w) * 0.5f * viewport_size.y
    };
}

TEST(SimulationSurfaceGaussian, RegistersPanelsHotkeysAndViews) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    SimulationSurfaceGaussian sim;

    sim.on_register(host);

    EXPECT_EQ(services.panels().active_count(), 5u);
    EXPECT_EQ(services.hotkeys().active_count(), 2u);
    EXPECT_EQ(services.render().active_view_count(), 2u);
    EXPECT_TRUE(services.panels().contains("Sim - Controls"));
    EXPECT_TRUE(services.panels().contains("Sim - Swarms"));
    EXPECT_TRUE(services.panels().contains("Sim - Particles"));
    EXPECT_TRUE(services.panels().contains("Sim - Goals"));
    EXPECT_TRUE(services.panels().contains("Sim - Differential Eq"));
    EXPECT_TRUE(services.hotkeys().contains("Reset Gaussian pursuit"));
    EXPECT_TRUE(services.render().contains_view("Surface 3D"));
    EXPECT_TRUE(services.render().contains_view("Surface Alternate"));

    sim.on_stop();
    EXPECT_EQ(services.panels().active_count(), 0u);
    EXPECT_EQ(services.hotkeys().active_count(), 0u);
    EXPECT_EQ(services.render().active_view_count(), 0u);
}

TEST(SimulationSurfaceGaussian, StartSpawnsParticlesAndTickEmitsRenderPackets) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    SimulationSurfaceGaussian sim;

    sim.on_register(host);
    sim.on_start();
    ASSERT_GT(sim.particle_count(), 0u);

    services.render().clear_packets();
    sim.on_tick(host.clock().next(1.f / 60.f));

    EXPECT_GT(services.render().packet_count(sim.main_view_id()), 0u);
    EXPECT_GT(services.render().packet_count(sim.alternate_view_id()), 0u);
    EXPECT_TRUE(sim.context().dirty().particles);
    EXPECT_TRUE(sim.context().dirty().main_view);
    EXPECT_TRUE(sim.context().dirty().alternate_view);

    const SceneSnapshot snapshot = sim.snapshot();
    EXPECT_EQ(snapshot.name, "Surface Simulation");
    EXPECT_EQ(snapshot.particle_count, sim.particle_count());
    EXPECT_GT(snapshot.sim_time, 0.f);
}

TEST(SimulationSurfaceGaussian, HotkeysInvokeSimulationCommands) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    SimulationSurfaceGaussian sim;

    sim.on_register(host);
    sim.on_start();
    const std::size_t initial_count = sim.particle_count();

    EXPECT_TRUE(services.hotkeys().dispatch({.key = 'B', .mods = 2}));
    EXPECT_GT(sim.particle_count(), initial_count);

    EXPECT_TRUE(services.hotkeys().dispatch({.key = 'R', .mods = 2}));
    EXPECT_GT(sim.particle_count(), 0u);
}

TEST(SimulationSurfaceGaussian, PerturbationCommandSurvivesUntilTickAndMarksDirty) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    SimulationSurfaceGaussian sim;

    sim.on_register(host);
    sim.on_start();
    sim.context().clear_frame_state();
    sim.context().queue_perturbation(SurfacePerturbation{
        .uv = {0.25f, -0.25f},
        .amplitude = 0.1f,
        .radius = 0.8f,
        .falloff = 1.f,
        .seed = 9u
    });

    ASSERT_TRUE(sim.context().has_pending_perturbations());
    EXPECT_TRUE(sim.context().dirty().surface);
    sim.on_tick(host.clock().next(1.f / 60.f));
    EXPECT_FALSE(sim.context().has_pending_perturbations());
    EXPECT_TRUE(sim.context().dirty().surface);
}

TEST(SimulationSurfaceGaussian, RenderSurfacePerturbationCommandMarksSurfaceDirty) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    SimulationSurfaceGaussian sim;

    sim.on_register(host);
    sim.on_start();
    sim.context().clear_frame_state();

    services.interaction().queue_surface_pick(SurfacePickRequest{
        .view = sim.main_view_id(),
        .fallback_uv = {0.4f, -0.6f},
        .screen_ndc = {0.f, 0.f},
        .amplitude = 0.2f,
        .radius = 0.75f,
        .falloff = 1.2f,
        .seed = 12u
    });

    sim.on_tick(host.clock().next(1.f / 60.f));
    EXPECT_TRUE(sim.context().dirty().surface);
    EXPECT_TRUE(sim.context().math_cache().surface_revision > 0u);
    EXPECT_FALSE(sim.context().has_pending_perturbations());
}

TEST(SimulationSurfaceGaussian, FrenetOverlayEmitsAdditionalMainPackets) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    SimulationSurfaceGaussian sim;

    sim.on_register(host);
    sim.on_start();
    auto* view = services.render().descriptor(sim.main_view_id());
    ASSERT_NE(view, nullptr);

    view->overlays.show_hover_frenet = false;
    view->overlays.show_osculating_circle = false;
    services.interaction().set_mouse(sim.main_view_id(), {}, {}, false);
    services.render().clear_packets();
    sim.on_tick(host.clock().next(1.f / 60.f));
    const std::size_t without_overlay = services.render().packet_count(sim.main_view_id());

    ASSERT_FALSE(sim.context().particles().empty());
    const AnimatedCurve& particle = sim.context().particles().front();
    ASSERT_GE(particle.trail_size(), 4u);
    const u32 hover_idx = particle.trail_size() / 2u;
    services.render().set_viewport_size(sim.main_view_id(), Vec2{1920.f, 1080.f});
    const Mat4 mvp = surface_main_mvp(sim.context().surface(), view, sim.context().time());
    const auto pixel = test_project_world_to_pixel(particle.trail_pt(hover_idx), mvp, view->viewport_size);
    ASSERT_TRUE(pixel.has_value());
    services.interaction().set_mouse(sim.main_view_id(), *pixel, {}, true, 24.f);
    view->overlays.show_hover_frenet = true;
    view->overlays.show_osculating_circle = true;
    services.render().clear_packets();
    sim.on_tick(host.clock().next(1.f / 60.f));
    const std::size_t with_overlay = services.render().packet_count(sim.main_view_id());

    EXPECT_GT(with_overlay, without_overlay);
}

TEST(SimulationSurfaceGaussian, AnalysisOverlaysEmitDarbouxMetricDiffusionAndGhostGeometry) {
    EngineServices services;
    SimulationHost host = services.simulation_host();
    SimulationSurfaceGaussian sim;

    sim.on_register(host);
    sim.on_start();
    auto* view = services.render().descriptor(sim.main_view_id());
    ASSERT_NE(view, nullptr);

    ASSERT_FALSE(sim.context().particles().empty());
    const AnimatedCurve& particle = sim.context().particles().front();
    ASSERT_GE(particle.trail_size(), 4u);
    const u32 hover_idx = particle.trail_size() / 2u;

    services.render().set_viewport_size(sim.main_view_id(), Vec2{1920.f, 1080.f});
    const Mat4 mvp = surface_main_mvp(sim.context().surface(), view, sim.context().time());
    const auto pixel = test_project_world_to_pixel(particle.trail_pt(hover_idx), mvp, view->viewport_size);
    ASSERT_TRUE(pixel.has_value());
    services.interaction().set_mouse(sim.main_view_id(), *pixel, {}, true, 24.f);

    view->overlays.show_hover_frenet = false;
    view->overlays.show_osculating_circle = false;
    view->overlays.show_darboux_frame = true;
    view->overlays.show_diffusion_ellipse = true;
    view->overlays.show_ghost_marker = true;
    view->overlays.show_metric_ellipse = true;

    services.render().clear_packets();
    sim.on_tick(host.clock().next(1.f / 60.f));

    const auto& hover = services.interaction().hover_metadata();
    EXPECT_TRUE(hover.particle.hit);
    EXPECT_TRUE(std::isfinite(hover.particle.normal_curvature));
    EXPECT_TRUE(std::isfinite(hover.particle.geodesic_curvature));

    bool found_overlay_packet = false;
    for (const RenderPacket& packet : services.render().packets()) {
        if (packet.view == sim.main_view_id()
            && packet.topology == Topology::LineList
            && packet.vertices.size() >= 6u) {
            found_overlay_packet = true;
        }
    }
    EXPECT_TRUE(found_overlay_packet);
}

} // namespace
