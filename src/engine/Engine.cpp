// engine/Engine.cpp
#define VOLK_IMPLEMENTATION
#include <volk.h>

#include "engine/Engine.hpp"
#include "app/SceneFactories.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <glm/gtc/matrix_transform.hpp>
#include <format>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <span>
#include <sstream>
#include <stdexcept>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace ndde {

namespace {
std::string_view render_kind_name(RenderViewKind kind) noexcept {
    return kind == RenderViewKind::Main ? "Main" : "Alternate";
}

std::string_view alternate_mode_name(AlternateViewMode mode) noexcept {
    switch (mode) {
        case AlternateViewMode::Contour: return "Contour";
        case AlternateViewMode::LevelCurves: return "Level Curves";
        case AlternateViewMode::VectorField: return "Vector Field";
        case AlternateViewMode::Isoclines: return "Isoclines";
        case AlternateViewMode::Flow: return "Flow";
    }
    return "Unknown";
}

bool primary_view_ui_blocked(const ImGuiIO& io) noexcept {
    const bool mouse_valid = io.MousePos.x > -3.0e37f && io.MousePos.y > -3.0e37f;
    if (!mouse_valid) return true;

    // The rendered surface is the background, not an ImGui window.  A broad
    // WantCaptureMouse gate can remain true because of panel state, so only
    // block the canvas while the pointer is actually over ImGui UI or an item
    // is actively being edited/dragged.
    return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)
        || ImGui::IsAnyItemActive();
}

void draw_wrapped_log_text(std::string_view text, ImVec4 color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

[[nodiscard]] std::filesystem::path absolute_path_or_empty(const std::filesystem::path& path) {
    if (path.empty()) return {};
    return std::filesystem::absolute(path).lexically_normal();
}

[[nodiscard]] std::filesystem::path executable_directory(const std::filesystem::path& executable_path) {
    const std::filesystem::path absolute = absolute_path_or_empty(executable_path);
    if (!absolute.empty() && absolute.has_parent_path()) {
        return absolute.parent_path();
    }
    return std::filesystem::current_path();
}

[[nodiscard]] std::filesystem::path resolve_relative_to(const std::filesystem::path& base,
                                                        const std::filesystem::path& path) {
    if (path.empty()) return base;
    if (path.is_absolute()) return path.lexically_normal();
    return (base / path).lexically_normal();
}

[[nodiscard]] std::filesystem::path resolve_config_path(const std::filesystem::path& executable_dir,
                                                        const std::filesystem::path& requested) {
    if (requested.is_absolute()) return requested.lexically_normal();

    const std::filesystem::path beside_exe = (executable_dir / requested).lexically_normal();
    if (std::filesystem::exists(beside_exe)) {
        return beside_exe;
    }
    return requested;
}

[[nodiscard]] std::filesystem::path prefer_existing_path(const std::filesystem::path& preferred,
                                                        const std::filesystem::path& fallback) {
    if (std::filesystem::exists(preferred)) {
        return preferred;
    }
    return fallback.lexically_normal();
}

} // namespace

void engine_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    auto* glfw_context = static_cast<platform::GlfwContext*>(glfwGetWindowUserPointer(window));
    if (!glfw_context) return;
    auto* engine = static_cast<Engine*>(glfw_context->key_callback_user());
    if (!engine) return;
    engine->on_key_event(key, action, mods);
}

Engine::Engine()
    : m_simulation_host(m_services.simulation_host())
    , m_simulations(m_services.memory()) {
    m_telemetry.set_owner_guard([this](std::string_view api_name) {
        const bool logger_allowed =
            api_name == "TelemetryService::flush" ||
            api_name == "TelemetryService::on_sim_stopped" ||
            api_name == "TelemetryService::on_app_stopping";
        if (logger_allowed && m_services.threads().is_thread_role(ThreadRole::Logger)) {
            return true;
        }
        return m_services.threads().require_thread_role(ThreadRole::Main, api_name);
    });
}

Engine::~Engine() {
    stop_active_simulation_thread();
    stop_render_presentation_thread();
    uninstall_global_hotkeys();

    // ── Telemetry: close any active run before GPU teardown ─────────────────
    if (m_telemetry.enabled()) {
        fire_sim_stopped(m_active_sim,
                         active_runtime().snapshot().sim_time,
                         m_telemetry_tick_count);
        fire_app_stopping();
    }

    m_second_win.destroy();
    m_renderer.destroy();
    m_services.memory().destroy();
    m_swapchain.destroy();
    m_vk.destroy();
    m_glfw.destroy();
}

void Engine::start(const std::filesystem::path& executable_path,
                   const std::filesystem::path& config_path) {
    std::cout << "[Engine] Starting...\n";

    m_executable_dir = executable_directory(executable_path);
    const std::filesystem::path resolved_config = resolve_config_path(m_executable_dir, config_path);
    m_config = AppConfig::load_or_default(resolved_config.string());
    m_shader_dir = prefer_existing_path(m_executable_dir / "shaders",
                                        std::filesystem::current_path() / "shaders");
    m_assets_dir = prefer_existing_path(resolve_relative_to(m_executable_dir, m_config.assets_dir),
                                        resolve_relative_to(std::filesystem::current_path(), m_config.assets_dir));
    m_telemetry_dir = resolve_relative_to(m_executable_dir, m_config.telemetry.output_dir);
    m_capture_dir = resolve_relative_to(m_executable_dir, "captures");

    m_glfw.init(m_config.window.width,
                m_config.window.height,
                m_config.window.title);

    if (volkInitialize() != VK_SUCCESS)
        throw std::runtime_error("[Engine] volkInitialize() failed");

    m_vk.init(m_glfw.window(), m_config.window.title);
    m_swapchain.init(m_vk, m_glfw.width(), m_glfw.height(), m_config.render.vsync);
    m_renderer.init(m_vk, m_swapchain, m_shader_dir.string(), m_assets_dir.string(), m_glfw.window());
    start_render_presentation_thread();
    install_global_hotkeys();

    m_services.memory().init_frame_gpu_arena(m_vk.device(), m_vk.physical_device(),
                                             m_config.simulation.arena_size_mb);

    // Second window for the 2D contour view.
    // Place it to the right of the primary window. glfwGetMonitors lets us
    // find the second monitor; fall back to primary + offset if only one.
    {
        int   mon_count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&mon_count);
        int x = 0, y = 0;
        if (mon_count >= 2) {
            // Put second window on second monitor
            int mx=0, my=0;
            glfwGetMonitorPos(monitors[1], &mx, &my);
            const GLFWvidmode* vm = glfwGetVideoMode(monitors[1]);
            x = mx; y = my;
            m_second_win.init(m_vk, x, y,
                static_cast<u32>(vm->width),
                static_cast<u32>(vm->height),
                "Contour 2D", m_shader_dir.string(), m_config.render.vsync);
        } else {
            // Single monitor: place second window at right half
            int mx=0, my=0;
            glfwGetMonitorPos(monitors[0], &mx, &my);
            const GLFWvidmode* vm = glfwGetVideoMode(monitors[0]);
            const u32 hw = static_cast<u32>(vm->width / 2);
            m_second_win.init(m_vk, mx + static_cast<int>(hw), my,
                hw, static_cast<u32>(vm->height),
                "Contour 2D", m_shader_dir.string(), m_config.render.vsync);
        }
    }

    // Also maximise the primary window to fill its half / first monitor
    glfwMaximizeWindow(m_glfw.window());

    register_global_panels();
    register_default_simulations(m_simulations, [this](std::size_t index) {
        m_pending_sim = index;
    });

    m_active_sim = 0;
    active_runtime().instantiate(m_simulation_host);
    active_runtime().start();
    start_active_simulation_thread();

    // ── Telemetry init ──────────────────────────────────────────────────────
    m_telemetry.set_enabled(m_config.telemetry.enabled);
    if (m_config.telemetry.enabled) {
        m_telemetry.init(
            m_config.telemetry.buffer_records,
            m_telemetry_dir);
        fire_app_started(resolved_config.string());
        fire_sim_started(m_active_sim);
    }

    // ── Engine event bus + logger init ─────────────────────────────────────
    m_services.events().init();
    m_app_started_subscription =
    m_services.events().subscribe<events::AppStarted>(EventChannelId::App, [this](const events::AppStarted&) {
        (void)m_services.threads().enqueue_logger_task([this] {
            (void)m_services.logger().write(LogSeverity::Info, LogCategory::Engine, {}, "[Engine] AppStarted");
        });
    });
    m_sim_switched_subscription =
    m_services.events().subscribe<events::SimSwitched>(EventChannelId::App, [this](const events::SimSwitched& e) {
        (void)m_services.threads().enqueue_logger_task([this, index = e.sim_index] {
            (void)m_services.logger().write(LogSeverity::Info,
                                            LogCategory::Engine,
                                            {},
                                            std::format("[Engine] SimSwitched index={}", index));
        });
    });
    m_services.events().publish(EventChannelId::App, events::AppStarted{});

    m_last_frame_time = glfwGetTime();
    m_running = true;
    (void)m_services.threads().enqueue_logger_task([this] {
        (void)m_services.logger().write(LogSeverity::Info, LogCategory::Engine, {}, "Engine ready");
    });
    std::cout << "[Engine] Ready.\n";
}

void Engine::install_global_hotkeys() {
    GLFWwindow* window = m_glfw.window();
    if (!window) return;
    m_glfw.set_key_callback_user(this);
    glfwSetKeyCallback(window, engine_key_callback);
#if defined(_WIN32)
    std::cout << "[Engine] Global hotkeys: GLFW event callback (Win32 backend)\n";
#elif defined(__linux__)
    std::cout << "[Engine] Global hotkeys: GLFW event callback (Linux backend)\n";
#else
    std::cout << "[Engine] Global hotkeys: GLFW event callback\n";
#endif
}

void Engine::uninstall_global_hotkeys() noexcept {
    GLFWwindow* window = m_glfw.window();
    if (!window) return;
    glfwSetKeyCallback(window, nullptr);
    if (m_glfw.key_callback_user() == this) {
        m_glfw.set_key_callback_user(nullptr);
    }
}

void Engine::switch_simulation(std::size_t index) {
    if (index >= m_simulations.size() || index == m_active_sim) return;
    stop_active_simulation_thread();
    vkDeviceWaitIdle(m_vk.device());
    m_renderer.reset_frame_state();

    // ── Telemetry: close current run ─────────────────────────────────────
    fire_sim_stopped(m_active_sim,
                     active_runtime().snapshot().sim_time,
                     m_telemetry_tick_count);

    active_runtime().stop();
    m_services.render().clear_packets();
    m_services.memory().reset_simulation();
    m_services.memory().reset_cache();
    m_services.memory().reset_history();
    m_active_sim = index;
    active_runtime().instantiate(m_simulation_host);
    active_runtime().start();
    start_active_simulation_thread();

    // ── Telemetry: open new run ──────────────────────────────────────
    fire_sim_started(m_active_sim);

    const std::string sim_name{active_runtime().name()};
    (void)m_services.threads().enqueue_logger_task([this, sim_name] {
        (void)m_services.logger().write(LogSeverity::Info,
                                        LogCategory::Engine,
                                        {},
                                        std::format("Switched simulation: {}", sim_name));
    });
    std::cout << std::format("[Engine] Active simulation: {}\n", active_runtime().name());
}

void Engine::run() {
    while (m_running && !m_glfw.should_close()) {
        m_glfw.poll_events();
        if (m_second_win.should_close()) {
            vkDeviceWaitIdle(m_vk.device());
            m_second_win.destroy();
        }
        if (m_glfw.check_resize()) { handle_resize(); continue; }
        run_frame();
    }
    vkDeviceWaitIdle(m_vk.device());
}

void Engine::run_frame() {
    // ── Wall-clock timing ─────────────────────────────────────────────────────
    const double now      = glfwGetTime();
    const double delta_s  = now - m_last_frame_time;
    m_last_frame_time     = now;
    const f32 frame_ms    = static_cast<f32>(delta_s * 1000.0);
    const f32 fps         = (frame_ms > 0.f) ? 1000.f / frame_ms : 0.f;
    m_services.metrics().begin_frame(m_services.clock().current().tick_index,
                                     static_cast<f64>(now));
    m_services.metrics().record_frame_time(frame_ms);

    // Destroy previous-frame packet payloads before releasing frame PMR memory.
    m_services.render().clear_packets();
    m_services.text().clear();
    m_services.memory().begin_frame();

    // ── GUI frame construction ──────────────────────────────────────────────
    m_renderer.imgui_new_frame();
    dispatch_global_hotkeys();

    // ── Populate DebugStats ───────────────────────────────────────────────────
    const auto& sc_ext = m_swapchain.extent();
    m_debug_stats = DebugStats{
        .arena_bytes_used   = m_services.memory().frame_gpu_bytes_used(),   // 0 after reset — updated lazily
        .arena_bytes_total  = m_services.memory().frame_gpu_bytes_total(),
        .arena_utilisation  = 0.f,                             // updated after scene runs
        .arena_vertex_count = 0,
        .draw_calls         = m_renderer.draw_call_count(),    // previous frame
        .swapchain_w        = sc_ext.width,
        .swapchain_h        = sc_ext.height,
        .frame_ms           = frame_ms,
        .fps                = fps,
    };

    // Input is sampled after panels are drawn, so this reflects the previous
    // frame's view-owned click state and avoids stale ImGui hover decisions.
    const RenderViewId tick_main_view = m_services.render().first_active_main_view();
    const bool double_click_this_frame =
        tick_main_view != 0 && m_services.view_input().sample(tick_main_view).left_double_click;
    m_debug_stats.frame_ms = frame_ms;   // already set above, no-op

    // Build the tick with is_double_click populated
    TickInfo tick_info = m_services.clock().next(frame_ms / f32(1000), active_runtime().paused());
    tick_info.is_double_click = double_click_this_frame;
    if (m_config.simulation.threaded_runtime) {
        enqueue_pending_surface_pokes(tick_info);
        (void)m_services.threads().enqueue_simulation_command(SimulationThreadCommand{
            .kind = SimulationThreadCommandKind::Tick,
            .tick = tick_info
        });
    } else {
        ScopedMetricTimer timer(m_services.metrics(), MetricId::SimulationTickMs);
        active_runtime().tick(tick_info);
    }

    // ── Drain service-owned event logs once per frame ─────────────────────
    {
        ScopedMetricTimer timer(m_services.metrics(), MetricId::EventDrainMs);
        m_services.threads().drain_service_mailboxes();
        m_services.events().drain_all(tick_info.time, tick_info.tick_index);
    }

    // ── Telemetry: record this tick ───────────────────────────────────────────
    if (m_telemetry.enabled() && !active_runtime().paused()) {
        ScopedMetricTimer timer(m_services.metrics(), MetricId::TelemetryTickMs);
        const auto snap     = active_runtime().snapshot();
        const f32  wall_ms  = static_cast<f32>(glfwGetTime() * 1000.0)
                            - m_telemetry_sim_start_wall_ms;

        // Base snapshot recording — populates position fields.
        // Simulations that override on_telemetry_tick() push richer rows instead.
        m_telemetry.record_particles(snap.particles,
                                     m_telemetry_tick_count,
                                     snap.sim_time,
                                     wall_ms);

        // Give the active simulation a chance to override / supplement.
        EngineAPI api = make_api();
        active_runtime().record_telemetry_tick(m_telemetry_tick_count, tick_info, api);

        ++m_telemetry_tick_count;

        // Periodic mid-run flush — keeps the ring from filling on long runs.
        if (m_config.telemetry.flush_periodic &&
            m_telemetry_tick_count % m_config.telemetry.flush_interval == u64(0)) {
            (void)m_services.threads().enqueue_logger_task([this] {
                (void)m_telemetry.flush();
            });
        }
    }
    if (m_config.simulation.threaded_runtime) {
        ScopedMetricTimer timer(m_services.metrics(), MetricId::SimulationRenderSubmitMs);
        active_runtime().submit_render();
    }
    {
        ScopedMetricTimer timer(m_services.metrics(), MetricId::ImGuiBuildMs);
        m_services.panels().draw_registered_panels();
        update_render_view_input();
        m_renderer.imgui_build_draw_data();
    }

    bool primary_ok = true;
    bool second_present_ok = true;
    (void)run_render_frame_task([this, &primary_ok, &second_present_ok] {
        {
            ScopedMetricTimer timer(m_services.metrics(), MetricId::FrameAcquireMs);
            primary_ok = m_renderer.begin_frame(m_swapchain);
        }
        if (!primary_ok) {
            return;
        }

        const bool second_ok = m_second_win.valid() && m_second_win.begin_frame();
        flush_render_service();
        m_services.render().clear_packets();
        m_renderer.imgui_record_draw_data();
        {
            ScopedMetricTimer timer(m_services.metrics(), MetricId::FrameSubmitMs);
            primary_ok = m_renderer.end_frame(m_swapchain);
        }

        // Present the auxiliary contour window after the primary window. FIFO
        // present can block, and letting the secondary surface block first makes
        // main-window frame pacing visibly worse on some drivers.
        if (second_ok) {
            ScopedMetricTimer timer(m_services.metrics(), MetricId::FramePresentMs);
            second_present_ok = m_second_win.end_frame();
        }
    });
    if (primary_ok) {
        m_services.metrics().increment(MetricId::FramesSubmitted);
        m_services.metrics().increment(MetricId::FramesPresented);
    } else {
        m_services.metrics().increment(MetricId::FramesSkipped);
    }

    // ── Update arena stats after scene geometry has been written ─────────────
    m_debug_stats.arena_bytes_used   = m_services.memory().frame_gpu_bytes_used();
    m_debug_stats.arena_utilisation  = m_services.memory().frame_gpu_utilisation();
    m_debug_stats.arena_vertex_count =
        m_services.memory().frame_gpu_bytes_used() / static_cast<u64>(sizeof(Vertex));

    if (!second_present_ok) { /* resize handled internally */ }

    if (!primary_ok) handle_resize();

    apply_pending_simulation_switch();
    m_services.metrics().end_frame();
}

void Engine::register_global_panels() {
    m_global_panels.clear();
    m_global_panels.push_back(m_services.panels().register_panel(PanelDescriptor{
        .title = "Engine - Global",
        .category = "Engine",
        .scope = PanelScope::Global,
        .first_use_pos = ImVec2(12.f, 34.f),
        .first_use_size = ImVec2(260.f, 150.f),
        .draw_body = [this] { draw_global_status_panel(); }
    }));
    m_global_panels.push_back(m_services.panels().register_panel(PanelDescriptor{
        .title = "Debug - Coordinates",
        .category = "Debug",
        .scope = PanelScope::Global,
        .first_use_pos = ImVec2(290.f, 34.f),
        .first_use_size = ImVec2(340.f, 220.f),
        .draw_body = [this] { draw_debug_coordinates_panel(); }
    }));
    m_global_panels.push_back(m_services.panels().register_panel(PanelDescriptor{
        .title = "Simulation - Metadata",
        .category = "Debug",
        .scope = PanelScope::Global,
        .first_use_pos = ImVec2(650.f, 34.f),
        .first_use_size = ImVec2(380.f, 420.f),
        .draw_body = [this] { draw_simulation_metadata_panel(); }
    }));
    m_global_panels.push_back(m_services.panels().register_panel(PanelDescriptor{
        .title = "Engine - Log",
        .category = "Engine",
        .scope = PanelScope::Global,
        .first_use_pos = ImVec2(1048.f, 34.f),
        .first_use_size = ImVec2(320.f, 400.f),
        .draw_body = [this] { draw_event_log_panel(); }
    }));
    m_global_panels.push_back(m_services.panels().register_panel(PanelDescriptor{
        .title = "Engine - Threads",
        .category = "Engine",
        .scope = PanelScope::Global,
        .first_use_pos = ImVec2(704.f, 34.f),
        .first_use_size = ImVec2(420.f, 360.f),
        .draw_body = [this] { draw_thread_health_panel(); }
    }));
    m_global_panels.push_back(m_services.panels().register_panel(PanelDescriptor{
        .title = "Engine - Metrics",
        .category = "Engine",
        .scope = PanelScope::Global,
        .first_use_pos = ImVec2(720.f, 420.f),
        .first_use_size = ImVec2(420.f, 300.f),
        .draw_body = [this] { draw_metrics_panel(); }
    }));
}

void Engine::draw_global_status_panel() {
    ImGui::SeparatorText("Simulations");
    for (std::size_t i = 0; i < m_simulations.size(); ++i) {
        auto* sim = m_simulations.get(i);
        if (!sim) continue;
        const std::string label = std::format("Ctrl+{}  {}", i + 1, sim->name());
        if (ImGui::Selectable(label.c_str(), i == m_active_sim))
            m_pending_sim = i;
    }

    ImGui::SeparatorText("Stats");
    const auto sim_snapshot = active_runtime().snapshot();
    ImGui::TextDisabled("%s   %s", sim_snapshot.name.c_str(), sim_snapshot.status.c_str());
    if (sim_snapshot.sim_speed > 0.f || sim_snapshot.particle_count > 0) {
        ImGui::TextDisabled("t %.2f   speed %.2f   %llu particles",
            sim_snapshot.sim_time,
            sim_snapshot.sim_speed,
            static_cast<unsigned long long>(sim_snapshot.particle_count));
    }
    ImGui::TextDisabled("%.1f ms   %.0f fps", m_debug_stats.frame_ms, m_debug_stats.fps);
    ImGui::TextDisabled("%llu verts   %llu / %llu bytes",
        static_cast<unsigned long long>(m_debug_stats.arena_vertex_count),
        static_cast<unsigned long long>(m_debug_stats.arena_bytes_used),
        static_cast<unsigned long long>(m_debug_stats.arena_bytes_total));
    ImGui::TextDisabled("F12 capture   Ctrl+Shift+P pause+capture");
    bool axes = m_services.render().axes_visible();
    if (ImGui::Checkbox("Axes", &axes))
        m_services.render().set_axes_visible(axes);
    ImGui::SeparatorText("Camera");
    if (ImGui::Button("Home")) m_services.camera().reset_main(CameraPreset::Home);
    ImGui::SameLine();
    if (ImGui::Button("Top")) m_services.camera().reset_main(CameraPreset::Top);
    ImGui::SameLine();
    if (ImGui::Button("Front")) m_services.camera().reset_main(CameraPreset::Front);
    ImGui::SameLine();
    if (ImGui::Button("Side")) m_services.camera().reset_main(CameraPreset::Side);
    if (ImGui::Button("Frame Selection"))
        (void)m_services.camera().frame_selection(m_services.interaction());
    ImGui::TextDisabled("RMB drag orbit   MMB/Shift+RMB pan   Wheel zoom");
    ImGui::TextDisabled("Double-click surface perturb");
}

void Engine::draw_debug_coordinates_panel() {
    const ImGuiIO& io = ImGui::GetIO();
    const Vec2 fb{static_cast<f32>(m_glfw.width()), static_cast<f32>(m_glfw.height())};
    ImGui::SeparatorText("Mouse");
    ImGui::TextDisabled("display %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
    ImGui::TextDisabled("framebuffer %.0f x %.0f", fb.x, fb.y);
    ImGui::TextDisabled("mouse %.1f, %.1f", io.MousePos.x, io.MousePos.y);

    const RenderViewId main = m_services.render().first_active_main_view();
    if (main != 0) {
        const RenderViewDomain d = m_services.render().view_domain(main);
        const float nx = io.DisplaySize.x > 0.f ? std::clamp(io.MousePos.x / io.DisplaySize.x, 0.f, 1.f) : 0.f;
        const float ny = io.DisplaySize.y > 0.f ? std::clamp(io.MousePos.y / io.DisplaySize.y, 0.f, 1.f) : 0.f;
        const Vec2 uv{
            d.u_min + nx * (d.u_max - d.u_min),
            d.v_max - ny * (d.v_max - d.v_min)
        };
        ImGui::SeparatorText("Active Main View");
        ImGui::TextDisabled("domain u[%.2f, %.2f] v[%.2f, %.2f]", d.u_min, d.u_max, d.v_min, d.v_max);
        ImGui::TextDisabled("mapped uv %.3f, %.3f", uv.x, uv.y);
        if (const auto* desc = m_services.render().descriptor(main)) {
            const auto& cam = desc->camera;
            ImGui::TextDisabled("camera yaw %.2f pitch %.2f zoom %.2f", cam.yaw, cam.pitch, cam.zoom);
            ImGui::TextDisabled("target %.2f, %.2f, %.2f", cam.target.x, cam.target.y, cam.target.z);
            ImGui::TextDisabled("view %.0f x %.0f aspect %.3f",
                desc->viewport_size.x, desc->viewport_size.y, desc->viewport_aspect);
        }
    }
    const HoverMetadata& hover = m_services.interaction().hover_metadata();
    ImGui::SeparatorText("Hover");
    ImGui::TextDisabled("view %llu   mouse %.1f, %.1f",
        static_cast<unsigned long long>(hover.view),
        hover.mouse_pixel.x,
        hover.mouse_pixel.y);
    if (hover.surface.hit) {
        ImGui::TextDisabled("surface uv %.3f, %.3f", hover.surface.uv.x, hover.surface.uv.y);
        ImGui::TextDisabled("world %.3f, %.3f, %.3f",
            hover.surface.world.x, hover.surface.world.y, hover.surface.world.z);
    } else {
        ImGui::TextDisabled("surface: no hit");
    }
    if (hover.particle.hit) {
        ImGui::TextDisabled("particle %llu idx %u  d %.1f px",
            static_cast<unsigned long long>(hover.particle.particle_id),
            hover.particle.trail_index,
            hover.particle.pixel_distance);
        ImGui::TextDisabled("k %.5f   tau %.5f", hover.particle.curvature, hover.particle.torsion);
        ImGui::TextDisabled("k_n %.5f   k_g %.5f",
            hover.particle.normal_curvature,
            hover.particle.geodesic_curvature);
    } else {
        ImGui::TextDisabled("particle: no trail snap");
    }
    if (hover.view_point.hit) {
        ImGui::TextDisabled("2D point %.3f, %.3f", hover.view_point.point.x, hover.view_point.point.y);
        ImGui::TextDisabled("2D world %.3f, %.3f, %.3f",
            hover.view_point.world.x,
            hover.view_point.world.y,
            hover.view_point.world.z);
    }
    auto kind_name = [](InteractionTargetKind kind) {
        switch (kind) {
            case InteractionTargetKind::SurfacePoint: return "SurfacePoint";
            case InteractionTargetKind::ViewPoint2D: return "ViewPoint2D";
            case InteractionTargetKind::Particle: return "Particle";
            case InteractionTargetKind::TrailSample: return "TrailSample";
            case InteractionTargetKind::None:
            default: return "None";
        }
    };
    const InteractionTarget selected = m_services.interaction().selected_target();
    ImGui::SeparatorText("Selection");
    ImGui::TextDisabled("kind %s   view %llu",
        kind_name(selected.kind),
        static_cast<unsigned long long>(selected.view));
    if (selected.valid) {
        if (selected.kind == InteractionTargetKind::SurfacePoint) {
            ImGui::TextDisabled("uv %.3f, %.3f", selected.uv.x, selected.uv.y);
        }
        if (selected.kind == InteractionTargetKind::ViewPoint2D) {
            ImGui::TextDisabled("point %.3f, %.3f", selected.point2d.x, selected.point2d.y);
        }
        ImGui::TextDisabled("world %.3f, %.3f, %.3f",
            selected.world.x, selected.world.y, selected.world.z);
        if (selected.kind == InteractionTargetKind::TrailSample || selected.kind == InteractionTargetKind::Particle) {
            ImGui::TextDisabled("particle %llu trail %u",
                static_cast<unsigned long long>(selected.particle_id),
                selected.trail_index);
            ImGui::TextDisabled("k %.5f   tau %.5f", selected.curvature, selected.torsion);
            ImGui::TextDisabled("k_n %.5f   k_g %.5f",
            selected.normal_curvature,
            selected.geodesic_curvature);
        }
    }
}

void Engine::draw_simulation_metadata_panel() {
    const SimulationMetadata metadata = active_runtime().metadata();
    const SceneSnapshot snapshot = active_runtime().snapshot();
    ImGui::SeparatorText("Simulation");
    ImGui::TextDisabled("%s", metadata.name.c_str());
    if (!metadata.surface_name.empty())
        ImGui::TextDisabled("%s", metadata.surface_name.c_str());
    if (!metadata.surface_formula.empty())
        ImGui::TextDisabled("%s", metadata.surface_formula.c_str());
    ImGui::TextDisabled("surface: %s derivatives   %s   %s",
        metadata.surface_has_analytic_derivatives ? "analytic" : "finite-diff",
        metadata.surface_deformable ? "deformable" : "static",
        metadata.surface_time_varying ? "time-varying" : "time-invariant");
    ImGui::TextDisabled("status %s   paused %s", metadata.status.c_str(), metadata.paused ? "yes" : "no");
    ImGui::TextDisabled("t %.2f   speed %.2f   particles %llu",
        metadata.sim_time,
        metadata.sim_speed,
        static_cast<unsigned long long>(metadata.particle_count));

    ImGui::SeparatorText("Render Views");
    for (const RenderViewSnapshot& view : m_services.render().active_view_snapshots()) {
        if (ImGui::TreeNode(std::format("{}##view{}", view.title, view.id).c_str())) {
            ImGui::TextDisabled("id %llu   %s", static_cast<unsigned long long>(view.id), render_kind_name(view.kind).data());
            if (view.kind == RenderViewKind::Alternate)
                ImGui::TextDisabled("mode %s", alternate_mode_name(view.alternate_mode).data());
            ImGui::TextDisabled("domain u[%.2f, %.2f] v[%.2f, %.2f]",
                view.domain.u_min, view.domain.u_max, view.domain.v_min, view.domain.v_max);
            ImGui::TextDisabled("axes %s   frenet %s   osc %s",
                view.overlays.show_axes ? "on" : "off",
                view.overlays.show_hover_frenet ? "on" : "off",
                view.overlays.show_osculating_circle ? "on" : "off");
            ImGui::TextDisabled("darboux %s   diffusion %s   ghost %s   metric %s",
                view.overlays.show_darboux_frame ? "on" : "off",
                view.overlays.show_diffusion_ellipse ? "on" : "off",
                view.overlays.show_ghost_marker ? "on" : "off",
                view.overlays.show_metric_ellipse ? "on" : "off");
            ImGui::TreePop();
        }
    }

    ImGui::SeparatorText("Particles");
    for (const auto& particle : snapshot.particles) {
        ImGui::TextDisabled("#%llu %s", static_cast<unsigned long long>(particle.id), particle.label.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%.2f, %.2f, %.2f)", particle.x, particle.y, particle.z);
    }
}

void Engine::draw_event_log_panel() {
    const ImGuiStyle& style = ImGui::GetStyle();
    const f32 footer_height =
        (ImGui::GetTextLineHeightWithSpacing() * 2.0f) +
        style.ItemSpacing.y +
        style.WindowPadding.y;

    if (ImGui::BeginChild("engine_log_scroll", ImVec2(0.f, -footer_height), false,
            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        const bool was_at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

        // Engine-scoped narrative log records from LoggerService.
        if (ImGui::TreeNodeEx("Engine Events", ImGuiTreeNodeFlags_DefaultOpen)) {
            const ImVec4 disabled = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            for (const LogSnapshotEntry& entry : m_services.logger().snapshot()) {
                if (entry.record.category != LogCategory::Engine) {
                    continue;
                }
                draw_wrapped_log_text(entry.message, disabled);
            }
            ImGui::TreePop();
        }

        // Sim-scoped events from EventLog, severity-tinted.
        const auto& sim_entries = m_services.events().log(EventChannelId::Simulation).entries();
        if (!sim_entries.empty()) {
            ImGui::Separator();
            if (ImGui::TreeNodeEx("Sim Events", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto& entry : sim_entries) {
                    ImVec4 color;
                    using S = ndde::events::EventSeverity;
                    switch (entry.severity) {
                        case S::Info:     color = {0.7f, 0.7f, 0.7f, 1.f}; break;
                        case S::Notice:   color = {0.4f, 0.8f, 1.0f, 1.f}; break;
                        case S::Warning:  color = {1.0f, 0.9f, 0.2f, 1.f}; break;
                        case S::Alert:    color = {1.0f, 0.5f, 0.1f, 1.f}; break;
                        case S::Critical: color = {1.0f, 0.2f, 0.2f, 1.f}; break;
                        default:          color = {0.7f, 0.7f, 0.7f, 1.f}; break;
                    }
                    draw_wrapped_log_text(entry.text, color);
                }
                ImGui::TreePop();
            }
        }

        if (was_at_bottom) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    // Stats
    ImGui::Separator();
    ImGui::TextDisabled("Ring: %llu queued  %llu dropped",
        static_cast<unsigned long long>(m_services.events().log(EventChannelId::Simulation).approx_queued()),
        static_cast<unsigned long long>(m_services.events().log(EventChannelId::Simulation).total_dropped()));
    ImGui::TextDisabled("Logger: %llu records dropped",
        static_cast<unsigned long long>(m_services.logger().dropped_records()));
}

void Engine::draw_thread_health_panel() {
    ThreadManagementService& threads = m_services.threads();
    const ThreadStats stats = threads.stats();
    ImGui::SeparatorText("Queues");
    ImGui::TextDisabled("Workers: %u", static_cast<unsigned>(stats.worker_count));
    ImGui::TextDisabled("Queued jobs: %llu", static_cast<unsigned long long>(stats.queued_jobs));
    ImGui::TextDisabled("Completed results: %llu", static_cast<unsigned long long>(stats.completed_results));
    ImGui::TextDisabled("Simulation thread: %s", threads.simulation_thread_running() ? "running" : "stopped");
    ImGui::TextDisabled("Render thread: %s", threads.render_thread_running() ? "running" : "stopped");
    ImGui::TextDisabled("Drops: results=%llu logs=%llu diagnostics=%llu events=%llu",
        static_cast<unsigned long long>(stats.dropped_results),
        static_cast<unsigned long long>(stats.dropped_logs),
        static_cast<unsigned long long>(stats.dropped_diagnostics),
        static_cast<unsigned long long>(stats.dropped_events));

    const auto thread_faults = m_services.diagnostics().active_with(ErrorCode::ThreadFault);
    const auto role_violations = m_services.diagnostics().active_with(ErrorCode::ThreadRoleViolation);
    ImGui::SeparatorText("Thread Faults");
    ImGui::TextDisabled("Faults: %zu  Role violations: %zu",
        thread_faults.size(), role_violations.size());
    for (const Diagnostic& issue : thread_faults) {
        ImGui::TextWrapped("%s", issue.message.c_str());
    }
    for (const Diagnostic& issue : role_violations) {
        ImGui::TextWrapped("%s", issue.message.c_str());
    }

    ImGui::SeparatorText("Jobs");
    const std::span<const ThreadJobStatus> jobs = threads.jobs();
    if (ImGui::BeginTable("thread_jobs", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY,
                          ImVec2(0.f, 132.f))) {
        ImGui::TableSetupColumn("Id");
        ImGui::TableSetupColumn("Owner");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("Priority");
        ImGui::TableSetupColumn("Worker");
        ImGui::TableHeadersRow();
        for (const ThreadJobStatus& job : jobs) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%llu", static_cast<unsigned long long>(job.id.value));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%.*s", static_cast<int>(job.owner.value.size()), job.owner.value.data());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%u", static_cast<unsigned>(job.state));
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("%u", static_cast<unsigned>(job.priority));
            ImGui::TableSetColumnIndex(4);
            ImGui::TextDisabled("%llu", static_cast<unsigned long long>(job.worker_index));
        }
        ImGui::EndTable();
    }
}

void Engine::draw_metrics_panel() {
    const MetricSummary frame = m_services.metrics().short_summary(MetricId::FrameMs);
    ImGui::TextDisabled("Median %.1f FPS   P95 %.2f ms   Latest %.2f ms",
        m_services.metrics().median_fps(),
        frame.p95,
        frame.latest);

    ImGui::SeparatorText("Frame");
    if (ImGui::BeginTable("metric_frame_table", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("Latest");
        ImGui::TableSetupColumn("Median");
        ImGui::TableSetupColumn("P95");
        ImGui::TableSetupColumn("Max");
        ImGui::TableHeadersRow();

        const auto draw_row = [this](MetricId id) {
            const u32 slot = static_cast<u32>(id);
            const MetricSummary summary = m_services.metrics().short_summary(id);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(metric_descriptors[slot].name.data(),
                                   metric_descriptors[slot].name.data() + metric_descriptors[slot].name.size());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", summary.latest);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", summary.median);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", summary.p95);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f", summary.max);
        };

        draw_row(MetricId::FrameMs);
        draw_row(MetricId::FrameFps);
        draw_row(MetricId::ImGuiBuildMs);
        draw_row(MetricId::EventDrainMs);
        draw_row(MetricId::SimulationTickMs);
        draw_row(MetricId::SimulationRenderSubmitMs);
        draw_row(MetricId::TelemetryTickMs);
        draw_row(MetricId::RenderTaskWaitMs);
        draw_row(MetricId::FrameAcquireMs);
        draw_row(MetricId::FrameSubmitMs);
        draw_row(MetricId::FramePresentMs);
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Counters");
    ImGui::TextDisabled("Frames submitted %llu   presented %llu   skipped %llu",
        static_cast<unsigned long long>(m_services.metrics().counter_value(MetricId::FramesSubmitted)),
        static_cast<unsigned long long>(m_services.metrics().counter_value(MetricId::FramesPresented)),
        static_cast<unsigned long long>(m_services.metrics().counter_value(MetricId::FramesSkipped)));
    ImGui::TextDisabled("Jobs submitted %llu   completed %llu   failed %llu",
        static_cast<unsigned long long>(m_services.metrics().counter_value(MetricId::JobsSubmitted)),
        static_cast<unsigned long long>(m_services.metrics().counter_value(MetricId::JobsCompleted)),
        static_cast<unsigned long long>(m_services.metrics().counter_value(MetricId::JobsFailed)));
}

void Engine::apply_pending_simulation_switch() {
    if (m_pending_sim == static_cast<std::size_t>(-1)) return;
    const std::size_t next = m_pending_sim;
    m_pending_sim = static_cast<std::size_t>(-1);
    switch_simulation(next);
}

void Engine::on_key_event(int key, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;

    const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    const bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    if (ctrl && !shift && key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        const std::size_t index = static_cast<std::size_t>(key - GLFW_KEY_1);
        if (index < m_simulations.size())
            m_pending_sim = index;
        return;
    }
    if (!ctrl && !shift && key == GLFW_KEY_F12) {
        request_capture(false);
        return;
    }
    if (ctrl && shift && key == GLFW_KEY_P) {
        request_capture(true);
        return;
    }
    if (!ctrl && !shift && key == GLFW_KEY_LEFT) {
        m_services.camera().orbit_main(-28.f, 0.f);
        return;
    }
    if (!ctrl && !shift && key == GLFW_KEY_RIGHT) {
        m_services.camera().orbit_main(28.f, 0.f);
        return;
    }

    (void)m_services.hotkeys().dispatch(KeyChord{.key = key, .mods = mods});
}

void Engine::dispatch_global_hotkeys() {
    // Global hotkeys are delivered by the GLFW key callback installed after
    // ImGui. This function intentionally remains as a frame-loop hook for
    // future event queue draining, but no longer polls key state.
}

void Engine::update_render_view_input() {
    ImGuiIO& io = ImGui::GetIO();
    GLFWwindow* primary_window = m_glfw.window();
    m_services.render().set_viewport_size(RenderViewKind::Main,
        Vec2{static_cast<f32>(m_glfw.width()), static_cast<f32>(m_glfw.height())});
    if (m_second_win.valid()) {
        m_services.render().set_viewport_size(RenderViewKind::Alternate,
            Vec2{static_cast<f32>(m_second_win.width()), static_cast<f32>(m_second_win.height())});
    }
    Vec2 primary_pixel{io.MousePos.x, io.MousePos.y};
    bool primary_right_down = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    bool primary_middle_down = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    if (primary_window) {
        double cx = 0.0;
        double cy = 0.0;
        glfwGetCursorPos(primary_window, &cx, &cy);
        primary_pixel = {
            static_cast<f32>(cx),
            static_cast<f32>(cy)
        };
        primary_right_down = glfwGetMouseButton(primary_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        primary_middle_down = glfwGetMouseButton(primary_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    }

    const RenderViewId main_view = m_services.render().first_active_main_view();
    ViewInputSample primary_sample{.view = main_view};
    if (main_view != 0 && io.DisplaySize.x > 0.f && io.DisplaySize.y > 0.f) {
        primary_sample = m_services.view_input().update(ViewInputUpdate{
            .view = main_view,
            .rect = ViewInputRect{
                .origin = {},
                .size = {static_cast<f32>(m_glfw.width()), static_cast<f32>(m_glfw.height())}
            },
            .cursor = primary_pixel,
            .buttons = ViewPointerButtons{
                .left_click = ImGui::IsMouseClicked(ImGuiMouseButton_Left),
                .left_double_click = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left),
                .right_down = primary_right_down,
                .middle_down = primary_middle_down,
                .shift_down = io.KeyShift
            },
            .wheel_delta = io.MouseWheel,
            .ui_blocked = primary_view_ui_blocked(io)
        });
        m_services.interaction().set_mouse(main_view,
            primary_sample.pixel,
            primary_sample.screen_ndc,
            primary_sample.enabled);
    }
    const RenderViewId alternate_view = m_services.render().first_active_alternate_view();
    ViewInputSample second_sample{.view = alternate_view};
    if (m_second_win.valid() && alternate_view != 0 && m_second_win.width() > 0 && m_second_win.height() > 0) {
        const Vec2 pixel = m_second_win.cursor_position();
        second_sample = m_services.view_input().update(ViewInputUpdate{
            .view = alternate_view,
            .rect = ViewInputRect{
                .origin = {},
                .size = {static_cast<f32>(m_second_win.width()), static_cast<f32>(m_second_win.height())}
            },
            .cursor = pixel,
            .buttons = ViewPointerButtons{
                .right_down = m_second_win.mouse_button_down(GLFW_MOUSE_BUTTON_RIGHT),
                .middle_down = m_second_win.mouse_button_down(GLFW_MOUSE_BUTTON_MIDDLE),
                .shift_down = io.KeyShift
            },
            .wheel_delta = io.MouseWheel,
            .ui_blocked = !m_second_win.hovered()
        });
        m_services.interaction().set_mouse(alternate_view,
            second_sample.pixel,
            second_sample.screen_ndc,
            second_sample.enabled);
    }

    if (!primary_sample.enabled && !second_sample.enabled) return;

    if (second_sample.enabled && alternate_view != 0) {
        (void)m_services.camera_input().dispatch(
            m_services.camera(),
            m_services.interaction(),
            m_services.render(),
            CameraInputSample{
                .view = alternate_view,
                .profile = CameraViewProfile::Auto,
                .pixel = second_sample.pixel,
                .normalized_pixel = second_sample.normalized_pixel,
                .screen_ndc = second_sample.screen_ndc,
                .delta = second_sample.delta,
                .wheel_delta = second_sample.wheel_delta,
                .right_drag = second_sample.right_drag,
                .middle_drag = second_sample.middle_drag,
                .shift = second_sample.shift,
                .left_click = second_sample.left_click,
                .left_double_click = second_sample.left_double_click,
                .enabled = second_sample.enabled
            });
        return;
    }

    if (main_view != 0 && primary_sample.enabled) {
        (void)m_services.camera_input().dispatch(
            m_services.camera(),
            m_services.interaction(),
            m_services.render(),
            CameraInputSample{
                .view = main_view,
                .profile = CameraViewProfile::Auto,
                .pixel = primary_sample.pixel,
                .normalized_pixel = primary_sample.normalized_pixel,
                .screen_ndc = primary_sample.screen_ndc,
                .delta = primary_sample.delta,
                .wheel_delta = primary_sample.wheel_delta,
                .right_drag = primary_sample.right_drag,
                .middle_drag = primary_sample.middle_drag,
                .shift = primary_sample.shift,
                .left_click = primary_sample.left_click,
                .left_double_click = primary_sample.left_double_click,
                .enabled = primary_sample.enabled,
                .perturb_seed = primary_sample.left_double_click ? m_surface_perturb_seed++ : 0u
            });
    }
}

void Engine::request_capture(bool pause_first) {
    if (pause_first)
        active_runtime().pause();

    const TickInfo tick = m_services.clock().current();
    m_services.capture().set_output_dir(m_capture_dir);
    m_services.capture().request_still(CaptureRequest{
        .mode = CaptureMode::StillPng,
        .target = CaptureTarget::BothWindows,
        .pause_before_capture = pause_first,
        .include_manifest = true
    }, CaptureRunMetadata{
        .simulation_name = std::string(active_runtime().name()),
        .scenario_name = std::string(active_runtime().name()),
        .simulation_index = static_cast<u64>(m_active_sim),
        .tick = tick.tick_index,
        .sim_time = tick.time,
        .wall_seconds = static_cast<f32>(glfwGetTime())
    });

    for (const CaptureArtifact& artifact : m_services.capture().consume_pending_stills()) {
        if (artifact.target == CaptureTarget::MainWindow)
            m_renderer.request_png_capture(artifact.path);
        else if (artifact.target == CaptureTarget::AlternateWindow && m_second_win.valid())
            m_second_win.request_png_capture(artifact.path);
    }
    (void)m_services.threads().enqueue_logger_task([this] {
        (void)m_services.logger().write(LogSeverity::Info, LogCategory::Capture, {}, "PNG capture requested");
    });
}

void Engine::start_active_simulation_thread() {
    if (!m_config.simulation.threaded_runtime) {
        return;
    }
    SimulationRuntime* runtime = &active_runtime();
    ThreadManagementService& threads = m_services.threads();
    if (threads.simulation_thread_running()) {
        return;
    }
    const bool started = threads.start_simulation_thread(
        [runtime, &threads](std::stop_token, std::span<const SimulationThreadCommand> commands) {
            runtime->process_thread_commands(commands, &threads);
        });
    if (started) {
        (void)m_services.threads().enqueue_logger_task([this] {
            (void)m_services.logger().write(LogSeverity::Info,
                                            LogCategory::Engine,
                                            {},
                                            "Simulation thread started");
        });
    } else {
        (void)m_services.threads().enqueue_logger_task([this] {
            (void)m_services.logger().write(LogSeverity::Warning,
                                            LogCategory::Engine,
                                            {},
                                            "Simulation thread could not start");
        });
    }
}

void Engine::stop_active_simulation_thread() noexcept {
    m_services.threads().stop_simulation_thread();
}

void Engine::start_render_presentation_thread() {
    if (!m_config.render.threaded_presentation) {
        return;
    }
    ThreadManagementService& threads = m_services.threads();
    if (threads.render_thread_running()) {
        return;
    }
    const bool started = threads.start_render_thread(
        [](std::stop_token, std::span<const RenderThreadCommand>) {});
    if (started) {
        (void)m_services.threads().enqueue_logger_task([this] {
            (void)m_services.logger().write(LogSeverity::Info,
                                            LogCategory::Engine,
                                            {},
                                            "Render presentation thread started");
        });
    } else {
        (void)m_services.threads().enqueue_logger_task([this] {
            (void)m_services.logger().write(LogSeverity::Warning,
                                            LogCategory::Engine,
                                            {},
                                            "Render presentation thread could not start");
        });
    }
}

void Engine::stop_render_presentation_thread() noexcept {
    m_services.threads().stop_render_thread();
}

bool Engine::run_render_frame_task(std::function<void()> task) {
    const auto started = std::chrono::steady_clock::now();
    const bool result = m_services.threads().run_render_task_sync(std::move(task));
    m_services.metrics().record_duration(MetricId::RenderTaskWaitMs,
                                          std::chrono::steady_clock::now() - started);
    return result;
}

void Engine::enqueue_pending_surface_pokes(const TickInfo& tick) {
    const RenderViewId main_view = m_services.render().first_active_main_view();
    if (main_view == 0) {
        return;
    }

    const auto picks = m_services.interaction().consume_surface_picks(main_view);
    if (picks.empty()) {
        return;
    }

    const SimulationSnapshot snapshot = active_runtime().snapshot();
    (void)m_services.events().log(EventChannelId::Simulation).push_engine_string(std::format(
            "[{:>7.2f}] DEBUG  surface-pick queue count={} threaded=yes paused={}",
            snapshot.sim_time,
            picks.size(),
            tick.paused ? "yes" : "no"),
        events::EventSeverity::Info);

    for (const SurfacePickRequest& pick : picks) {
        (void)m_services.threads().enqueue_simulation_command(SimulationThreadCommand{
            .kind = SimulationThreadCommandKind::SurfacePoke,
            .tick = tick,
            .surface_poke = SimulationSurfacePoke{
                .view = static_cast<u64>(main_view),
                .uv = pick.fallback_uv,
                .fallback_uv = pick.fallback_uv,
                .screen_ndc = pick.screen_ndc,
                .amplitude = pick.amplitude,
                .radius = pick.radius,
                .falloff = pick.falloff,
                .ray_hit = false,
                .seed = pick.seed
            }
        });
    }
}

SimulationRuntime& Engine::active_runtime() {
    auto* runtime = m_simulations.get(m_active_sim);
    if (!runtime) throw std::runtime_error("[Engine] No active simulation runtime");
    return *runtime;
}

const SimulationRuntime& Engine::active_runtime() const {
    const auto* runtime = m_simulations.get(m_active_sim);
    if (!runtime) throw std::runtime_error("[Engine] No active simulation runtime");
    return *runtime;
}

void Engine::flush_render_service() {
    for (const RenderPacket& packet : m_services.render().packets()) {
        if (packet.vertices.empty()) continue;
        auto slice = m_services.memory().allocate_frame_vertices(static_cast<u32>(packet.vertices.size()));
        auto verts = slice.vertices();
        std::memcpy(verts, packet.vertices.data(), packet.vertices.size() * sizeof(Vertex));

        renderer::DrawCall dc{
            .slice = slice,
            .topology = packet.topology,
            .mode = packet.mode,
            .color = packet.color,
            .mvp = packet.mvp
        };

        const RenderTarget target = m_services.render().view_kind(packet.view) == RenderViewKind::Alternate
            ? RenderTarget::Contour2D
            : RenderTarget::Primary3D;
        switch (target) {
            case RenderTarget::Primary3D:
                m_renderer.draw(dc);
                break;
            case RenderTarget::Contour2D:
                if (m_second_win.valid()) m_second_win.draw(dc);
                break;
        }
    }
}

void Engine::handle_resize() {
    const u32 w = m_glfw.width();
    const u32 h = m_glfw.height();
    if (w == 0 || h == 0) return;
    vkDeviceWaitIdle(m_vk.device());
    m_swapchain.recreate(m_vk, w, h, m_config.render.vsync);
    m_renderer.on_swapchain_recreated(m_swapchain);
    std::cout << std::format("[Engine] Swapchain {}x{}\n", w, h);
}

EngineAPI Engine::make_api() {
    EngineAPI api;

    api.acquire = [this](u32 n) -> memory::ArenaSlice {
        return m_services.memory().allocate_frame_vertices(n);
    };

    api.submit_to = [this](RenderTarget target,
                             const memory::ArenaSlice& slice,
                             Topology topology, DrawMode mode,
                             Vec4 color, Mat4 mvp) {
        renderer::DrawCall dc{
            .slice = slice, .topology = topology,
            .mode  = mode,  .color    = color, .mvp = mvp
        };
        switch (target) {
            case RenderTarget::Primary3D:
                m_renderer.draw(dc);
                break;
            case RenderTarget::Contour2D:
                if (m_second_win.valid()) m_second_win.draw(dc);
                break;
        }
    };

    api.push_math_font = [this](bool small) {
        ImFont* f = small ? m_renderer.font_math_small() : m_renderer.font_math_body();
        if (f) ImGui::PushFont(f);
    };
    api.pop_math_font = []() { ImGui::PopFont(); };
    api.config          = [this]() -> const AppConfig& { return m_config; };
    api.viewport_size   = [this]() -> Vec2 {
        return Vec2{ static_cast<f32>(m_glfw.width()),
                     static_cast<f32>(m_glfw.height()) };
    };
    api.viewport_size2  = [this]() -> Vec2 {
        if (!m_second_win.valid()) return {};
        return Vec2{ static_cast<f32>(m_second_win.width()),
                     static_cast<f32>(m_second_win.height()) };
    };
    api.debug_stats     = [this]() -> const DebugStats& { return m_debug_stats; };

    api.switch_simulation = [this](std::size_t index) {
        m_pending_sim = index;
    };

    api.record_telemetry = [this](const telemetry::TelemetryRecord& r) -> bool {
        return m_telemetry.record(r);
    };

    api.record_telemetry_ext = [this](const telemetry::TelemetryExtRecord& r) -> bool {
        return m_telemetry.record_ext(r);
    };

    return api;
}

// ── Telemetry lifecycle helpers ───────────────────────────────────────────────────

void Engine::fire_app_started(const std::string& config_path) {
    (void)config_path;
    m_telemetry.on_app_started(events::AppStarted{
        .wall_time   = std::chrono::system_clock::now()
    });
}

void Engine::fire_app_stopping() {
    (void)m_services.threads().run_logger_task_sync([this] {
        m_telemetry.on_app_stopping(events::AppStopping{});
    });
}

void Engine::fire_sim_started(std::size_t index) {
    m_telemetry_tick_count        = u64(0);
    m_telemetry_sim_start_wall_ms = static_cast<f32>(glfwGetTime() * 1000.0);
    m_telemetry.on_sim_started(events::SimStarted{
        .sim_name  = active_runtime().name(),
        .sim_index = static_cast<u64>(index),
        .wall_time = std::chrono::system_clock::now()
    });
    // Dispatch on the engine bus so subscribers (log panel) are notified.
    m_services.events().publish(EventChannelId::App, ndde::events::SimSwitched{
        .sim_index = static_cast<u64>(index)
    });
    // EventBusService drains app/simulation logs once per frame in run_frame().
}

void Engine::fire_sim_stopped(std::size_t index,
                               f32 total_sim_time,
                               u64 total_ticks) {
    events::SimStopped stopped{
        .sim_name        = active_runtime().name(),
        .sim_index       = static_cast<u64>(index),
        .total_sim_time  = total_sim_time,
        .total_ticks     = total_ticks,
        .total_records   = m_telemetry.total_records(),
        .dropped_records = m_telemetry.dropped(),
        .wall_time       = std::chrono::system_clock::now()
    };
    (void)m_services.threads().run_logger_task_sync([this, stopped] {
        m_telemetry.on_sim_stopped(stopped);
    });
}

} // namespace ndde
