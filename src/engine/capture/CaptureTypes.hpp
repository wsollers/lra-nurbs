#pragma once
// engine/capture/CaptureTypes.hpp
// Structured capture requests, naming metadata, and artifact records.

#include "math/Scalars.hpp"

#include <filesystem>
#include <string>

namespace ndde {

enum class CaptureTarget : u8 {
    MainWindow,
    AlternateWindow,
    BothWindows
};

enum class CaptureMode : u8 {
    StillPng,
    MovieFrameSequence
};

struct CaptureRunMetadata {
    std::string simulation_name;
    std::string scenario_name;
    std::string run_id;
    u64 simulation_index = u64(0);
    u64 tick = u64(0);
    f32 sim_time = f32(0);
    f32 wall_seconds = f32(0);
    std::string started_at_utc;
};

struct CaptureRequest {
    CaptureMode mode = CaptureMode::StillPng;
    CaptureTarget target = CaptureTarget::MainWindow;
    bool pause_before_capture = false;
    bool include_manifest = true;
    std::string label;
};

struct CaptureArtifact {
    CaptureMode mode = CaptureMode::StillPng;
    CaptureTarget target = CaptureTarget::MainWindow;
    std::filesystem::path path;
    std::filesystem::path manifest_path;
    u32 width = u32(0);
    u32 height = u32(0);
    u64 tick = u64(0);
    f32 sim_time = f32(0);
};

struct MovieFrameSequenceOptions {
    CaptureTarget target = CaptureTarget::BothWindows;
    u32 fps = u32(60);
    u32 max_width = u32(0);
    u32 max_height = u32(0);
    bool write_frame_index = false;
};

struct MovieFrameSequenceStatus {
    bool active = false;
    CaptureTarget target = CaptureTarget::BothWindows;
    u64 frames_written = u64(0);
    f32 elapsed_seconds = f32(0);
    std::filesystem::path main_frame_dir;
    std::filesystem::path alternate_frame_dir;
    std::filesystem::path manifest_path;
};

} // namespace ndde
