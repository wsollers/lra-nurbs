#pragma once
// engine/AppConfig.hpp
// Engine configuration, loaded from engine_config.json at startup.
// All fields have defaults — a missing JSON file is not an error.

#include "math/Scalars.hpp"
#include <string>

namespace ndde {

struct WindowConfig {
    u32         width  = 1280;
    u32         height = 720;
    std::string title  = "NDDE Engine";
};

struct RenderConfig {
    bool vsync                = true;
    u32  max_frames_in_flight = 2;
    bool threaded_presentation = true;
};

struct CameraConfig {
    Vec3 position   = { 0.f, 0.f, 5.f };
    Vec3 target     = { 0.f, 0.f, 0.f };
    f32  fov        = 45.f;
    f32  near_plane = 0.1f;
    f32  far_plane  = 100.f;
};

struct SimulationConfig {
    f32  tau           = 0.5f;   ///< DDE delay constant
    f32  speed         = 1.0f;
    u32  tessellation  = 256;
    u32  arena_size_mb = 128;
    bool threaded_runtime = true;
};

struct TelemetryConfig {
    bool        enabled          = true;
    u64         buffer_records   = u64(512) * u64(1024); ///< ring capacity (~30 MB)
    std::string output_dir       = "telemetry";
    bool        flush_periodic   = true;
    u64         flush_interval   = u64(1000); ///< ticks between mid-run flushes
};

struct AppConfig {
    WindowConfig     window;
    RenderConfig     render;
    CameraConfig     camera;
    SimulationConfig simulation;
    TelemetryConfig  telemetry;
    std::string      assets_dir = "assets";

    /// Load from JSON. Missing file -> silent defaults. Malformed JSON -> throws.
    [[nodiscard]] static AppConfig load_or_default(
        const std::string& path = "engine_config.json");

    /// Write current config back to JSON (useful for template generation).
    void save(const std::string& path = "engine_config.json") const;
};

} // namespace ndde
