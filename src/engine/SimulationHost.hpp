#pragma once
// engine/SimulationHost.hpp
// Narrow service interface passed to simulations.

#include "engine/CameraInputController.hpp"
#include "engine/CameraService.hpp"
#include "engine/capture/CaptureService.hpp"
#include "engine/diagnostics/DiagnosticsService.hpp"
#include "engine/events/EventBusService.hpp"
#include "engine/HotkeyService.hpp"
#include "engine/InteractionService.hpp"
#include "engine/logging/LoggerService.hpp"
#include "engine/metadata/SimMetadataService.hpp"
#include "engine/metricservice/MetricsService.hpp"
#include "engine/PanelService.hpp"
#include "engine/RenderService.hpp"
#include "engine/resources/ResourceManagerService.hpp"
#include "engine/SimulationClock.hpp"
#include "engine/text/TextOverlayService.hpp"
#include "engine/threading/ThreadManagementService.hpp"
#include "engine/ViewInputService.hpp"
#include "memory/MemoryService.hpp"

#include <string_view>

namespace ndde {

class SimulationHost {
public:
    SimulationHost(PanelService& panels,
                   HotkeyService& hotkeys,
                   InteractionService& interaction,
                   RenderService& render,
                   CameraService& camera,
                   DiagnosticsService& diagnostics,
                   EventBusService& events,
                   LoggerService& logger,
                   SimMetadataService& metadata,
                   MetricsService& metrics,
                   ResourceManagerService& resources,
                   TextOverlayService& text,
                   ThreadManagementService& threads,
                   SimulationClock& clock,
                   memory::MemoryService& memory) noexcept
        : m_panels(panels)
        , m_hotkeys(hotkeys)
        , m_interaction(interaction)
        , m_render(render)
        , m_camera(camera)
        , m_diagnostics(diagnostics)
        , m_events(events)
        , m_logger(logger)
        , m_metadata(metadata)
        , m_metrics(metrics)
        , m_resources(resources)
        , m_text(text)
        , m_threads(threads)
        , m_clock(clock)
        , m_memory(memory)
    {}

    [[nodiscard]] PanelService& panels() const noexcept { return m_panels; }
    [[nodiscard]] HotkeyService& hotkeys() const noexcept { return m_hotkeys; }
    [[nodiscard]] InteractionService& interaction() const noexcept { return m_interaction; }
    [[nodiscard]] RenderService& render() const noexcept { return m_render; }
    [[nodiscard]] CameraService& camera() const noexcept { return m_camera; }
    [[nodiscard]] DiagnosticsService& diagnostics() const noexcept { return m_diagnostics; }
    [[nodiscard]] EventBusService& events() const noexcept { return m_events; }
    [[nodiscard]] LoggerService& logger() const noexcept { return m_logger; }
    [[nodiscard]] SimMetadataService& metadata() const noexcept { return m_metadata; }
    [[nodiscard]] MetricsService& metrics() const noexcept { return m_metrics; }
    [[nodiscard]] ResourceManagerService& resources() const noexcept { return m_resources; }
    [[nodiscard]] TextOverlayService& text() const noexcept { return m_text; }
    [[nodiscard]] ThreadManagementService& threads() const noexcept { return m_threads; }
    [[nodiscard]] SimulationClock& clock() const noexcept { return m_clock; }
    [[nodiscard]] memory::MemoryService& memory() const noexcept { return m_memory; }

private:
    PanelService& m_panels;
    HotkeyService& m_hotkeys;
    InteractionService& m_interaction;
    RenderService& m_render;
    CameraService& m_camera;
    DiagnosticsService& m_diagnostics;
    EventBusService& m_events;
    LoggerService& m_logger;
    SimMetadataService& m_metadata;
    MetricsService& m_metrics;
    ResourceManagerService& m_resources;
    TextOverlayService& m_text;
    ThreadManagementService& m_threads;
    SimulationClock& m_clock;
    memory::MemoryService& m_memory;
};

class EngineServices {
public:
    EngineServices() {
        m_panels.set_memory_service(&m_memory);
        m_hotkeys.set_memory_service(&m_memory);
        m_interaction.set_memory_service(&m_memory);
        m_render.set_memory_service(&m_memory);
        m_text.set_memory_service(&m_memory);
        m_text.set_resource_manager(&m_resources);
        m_camera.set_render_service(&m_render);
        m_events.init();
        m_logger.init();
        m_metrics.init();
        m_resources.init();
        m_resources.set_logger_service(&m_logger);
        m_threads.init(ThreadPoolConfig{.enable_logger_thread = true}, ThreadServiceBindings{
            .diagnostics = &m_diagnostics,
            .events = &m_events,
            .logger = &m_logger,
            .metrics = &m_metrics
        });
        m_diagnostics.set_thread_service(&m_threads, ThreadRole::Main);
        m_events.set_owner_guard([this](std::string_view api_name) {
            return m_threads.require_thread_role(ThreadRole::Main, api_name);
        });
        m_logger.set_owner_guard([this](std::string_view api_name) {
            if (m_threads.is_thread_role(ThreadRole::Logger)) {
                return true;
            }
            return m_threads.require_thread_role(ThreadRole::Logger, api_name);
        });
        m_panels.set_thread_service(&m_threads, ThreadRole::Main);
        m_hotkeys.set_thread_service(&m_threads, ThreadRole::Main);
        m_interaction.set_thread_service(&m_threads, ThreadRole::Main);
        m_render.set_thread_service(&m_threads, ThreadRole::Main);
        m_camera.set_thread_service(&m_threads, ThreadRole::Main);
        m_metadata.set_thread_service(&m_threads, ThreadRole::Main);
        m_view_input.set_thread_service(&m_threads, ThreadRole::Main);
        m_resources.set_thread_service(&m_threads, ThreadRole::Main);
        m_text.set_thread_service(&m_threads, ThreadRole::Main);
        m_capture.set_thread_service(&m_threads, ThreadRole::Main);
    }

    [[nodiscard]] PanelService& panels() noexcept { return m_panels; }
    [[nodiscard]] HotkeyService& hotkeys() noexcept { return m_hotkeys; }
    [[nodiscard]] InteractionService& interaction() noexcept { return m_interaction; }
    [[nodiscard]] RenderService& render() noexcept { return m_render; }
    [[nodiscard]] CameraService& camera() noexcept { return m_camera; }
    [[nodiscard]] CameraInputController& camera_input() noexcept { return m_camera_input; }
    [[nodiscard]] ViewInputService& view_input() noexcept { return m_view_input; }
    [[nodiscard]] CaptureService& capture() noexcept { return m_capture; }
    [[nodiscard]] DiagnosticsService& diagnostics() noexcept { return m_diagnostics; }
    [[nodiscard]] EventBusService& events() noexcept { return m_events; }
    [[nodiscard]] LoggerService& logger() noexcept { return m_logger; }
    [[nodiscard]] SimMetadataService& metadata() noexcept { return m_metadata; }
    [[nodiscard]] MetricsService& metrics() noexcept { return m_metrics; }
    [[nodiscard]] ResourceManagerService& resources() noexcept { return m_resources; }
    [[nodiscard]] TextOverlayService& text() noexcept { return m_text; }
    [[nodiscard]] ThreadManagementService& threads() noexcept { return m_threads; }
    [[nodiscard]] SimulationClock& clock() noexcept { return m_clock; }
    [[nodiscard]] memory::MemoryService& memory() noexcept { return m_memory; }

    [[nodiscard]] SimulationHost simulation_host() noexcept {
        return SimulationHost(m_panels, m_hotkeys, m_interaction, m_render, m_camera,
                              m_diagnostics, m_events, m_logger, m_metadata, m_metrics, m_resources, m_text,
                              m_threads, m_clock, m_memory);
    }

private:
    memory::MemoryService m_memory;
    PanelService m_panels;
    HotkeyService m_hotkeys;
    InteractionService m_interaction;
    RenderService m_render;
    CameraService m_camera;
    CaptureService m_capture;
    DiagnosticsService m_diagnostics;
    EventBusService m_events;
    LoggerService m_logger;
    SimMetadataService m_metadata;
    MetricsService m_metrics;
    ResourceManagerService m_resources;
    TextOverlayService m_text;
    ThreadManagementService m_threads;
    CameraInputController m_camera_input;
    ViewInputService m_view_input;
    SimulationClock m_clock;
};

} // namespace ndde
