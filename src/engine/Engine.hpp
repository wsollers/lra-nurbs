#pragma once
// engine/Engine.hpp

#include "engine/AppConfig.hpp"
#include "engine/EngineAPI.hpp"
#include "engine/IScene.hpp"
#include "engine/SimulationHost.hpp"
#include "engine/SimulationRuntime.hpp"
// engine/events/ headers: included transitively via EngineEventTypes.hpp below.
// Listed explicitly here only so TelemetryService.hpp finds them.
#include "engine/events/AppEvent.hpp"
#include "engine/events/SimEvent.hpp"
#include "platform/GlfwContext.hpp"
#include "platform/VulkanContext.hpp"
#include "renderer/Swapchain.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/SecondWindow.hpp"
#include "memory/Containers.hpp"
#include "telemetry/TelemetryService.hpp"
#include "simulation/events/EngineEventTypes.hpp"
#include <filesystem>
#include <functional>
#include <string>

namespace ndde {

void engine_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&)                 = delete;
    Engine& operator=(Engine&&)      = delete;

    void start(const std::filesystem::path& executable_path = {},
               const std::filesystem::path& config_path = "engine_config.json");
    void run();

    void switch_simulation(std::size_t index);

    [[nodiscard]] const AppConfig& config() const noexcept { return m_config; }
    [[nodiscard]] EngineServices& services() noexcept { return m_services; }
    [[nodiscard]] const EngineServices& services() const noexcept { return m_services; }
    [[nodiscard]] PanelService& getPanelService() noexcept { return m_services.panels(); }
    [[nodiscard]] HotkeyService& getHotkeyService() noexcept { return m_services.hotkeys(); }
    [[nodiscard]] InteractionService& getInteractionService() noexcept { return m_services.interaction(); }
    [[nodiscard]] RenderService& getRenderService() noexcept { return m_services.render(); }
    [[nodiscard]] CameraService& getCameraService() noexcept { return m_services.camera(); }
    [[nodiscard]] SimulationClock& getSimulationClock() noexcept { return m_services.clock(); }

    [[nodiscard]] telemetry::TelemetryService& telemetry() noexcept       { return m_telemetry; }
    [[nodiscard]] const telemetry::TelemetryService& telemetry() const noexcept { return m_telemetry; }

    [[nodiscard]] EventBusService& engine_bus() noexcept { return m_services.events(); }
    [[nodiscard]] events::EventLog& engine_log() noexcept { return m_services.events().log(EventChannelId::App); }

private:
    AppConfig               m_config;
    EngineServices          m_services;
    SimulationHost          m_simulation_host;
    platform::GlfwContext   m_glfw;
    platform::VulkanContext m_vk;
    renderer::Swapchain     m_swapchain;
    renderer::Renderer      m_renderer;
    SimulationRegistry       m_simulations;
    std::size_t              m_active_sim = 0;
    std::size_t              m_pending_sim = static_cast<std::size_t>(-1);
    renderer::SecondWindow   m_second_win; ///< 2D contour window
    bool                     m_running     = false;

    // Per-frame state
    double     m_last_frame_time = 0.0;
    DebugStats m_debug_stats;
    u32        m_surface_perturb_seed = 1;
    memory::PersistentVector<PanelHandle> m_global_panels;
    std::filesystem::path m_executable_dir{"."};
    std::filesystem::path m_shader_dir{"shaders"};
    std::filesystem::path m_assets_dir{"assets"};
    std::filesystem::path m_telemetry_dir{"telemetry"};
    std::filesystem::path m_capture_dir{"captures"};

    // ── Telemetry ──────────────────────────────────────────────────────────────
    telemetry::TelemetryService m_telemetry;
    u64 m_telemetry_tick_count        = u64(0);
    f32 m_telemetry_sim_start_wall_ms = f32(0);

    // ── Event system ───────────────────────────────────────────────────
    Subscription m_app_started_subscription;
    Subscription m_sim_switched_subscription;

    void fire_app_started(const std::string& config_path);
    void fire_app_stopping();
    void fire_sim_started(std::size_t index);
    void fire_sim_stopped(std::size_t index, f32 total_sim_time, u64 total_ticks);

    void run_frame();
    void handle_resize();
    void apply_pending_simulation_switch();
    void register_global_panels();
    void draw_global_status_panel();
    void draw_debug_coordinates_panel();
    void draw_simulation_metadata_panel();
    void draw_event_log_panel();
    void draw_thread_health_panel();
    void draw_metrics_panel();
    void flush_render_service();
    void install_global_hotkeys();
    void uninstall_global_hotkeys() noexcept;
    void on_key_event(int key, int action, int mods);
    void dispatch_global_hotkeys();
    void update_render_view_input();
    void request_capture(bool pause_first);
    void start_active_simulation_thread();
    void stop_active_simulation_thread() noexcept;
    void start_render_presentation_thread();
    void stop_render_presentation_thread() noexcept;
    [[nodiscard]] bool run_render_frame_task(std::function<void()> task);
    void enqueue_pending_surface_pokes(const TickInfo& tick);
    [[nodiscard]] SimulationRuntime& active_runtime();
    [[nodiscard]] const SimulationRuntime& active_runtime() const;
    [[nodiscard]] EngineAPI make_api();

    friend void engine_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
};

} // namespace ndde
