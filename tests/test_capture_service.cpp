#include <gtest/gtest.h>

#include "engine/SimulationHost.hpp"
#include "engine/capture/CaptureService.hpp"

#include <filesystem>
#include <fstream>

namespace {

[[nodiscard]] std::filesystem::path test_output_dir(std::string_view name) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "ndde_capture_tests" / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace

TEST(CaptureService, NormalizesStemsLikeTelemetry) {
    EXPECT_EQ(ndde::CaptureService::normalize_stem("Wave Predator-Prey"), "wave_predator_prey");
    EXPECT_EQ(ndde::CaptureService::normalize_stem("  Weird///Name  "), "weird_name");
    EXPECT_EQ(ndde::CaptureService::normalize_stem("***"), "simulation");
}

TEST(CaptureService, StillBothWindowsCreatesPendingArtifactsAndManifest) {
    const std::filesystem::path dir = test_output_dir("still_both");

    ndde::CaptureService capture;
    capture.set_output_dir(dir);
    capture.request_still(ndde::CaptureRequest{
        .target = ndde::CaptureTarget::BothWindows,
        .include_manifest = true
    }, ndde::CaptureRunMetadata{
        .simulation_name = "Wave Predator-Prey",
        .scenario_name = "Wave Predator-Prey",
        .run_id = "wave_predator_prey_0_20260516_230839",
        .simulation_index = ndde::u64(0),
        .tick = ndde::u64(3143),
        .sim_time = ndde::f32(52.383)
    });

    std::vector<ndde::CaptureArtifact> pending = capture.consume_pending_stills();
    ASSERT_EQ(pending.size(), 2u);
    EXPECT_EQ(pending[0].target, ndde::CaptureTarget::MainWindow);
    EXPECT_EQ(pending[1].target, ndde::CaptureTarget::AlternateWindow);
    EXPECT_NE(pending[0].path.string().find("_main_tick0003143_t0052.383.png"), std::string::npos);
    EXPECT_NE(pending[1].path.string().find("_alternate_tick0003143_t0052.383.png"), std::string::npos);

    ASSERT_FALSE(pending[0].manifest_path.empty());
    EXPECT_TRUE(std::filesystem::exists(pending[0].manifest_path));

    {
        std::ifstream manifest(pending[0].manifest_path);
        std::string content((std::istreambuf_iterator<char>(manifest)), {});
        EXPECT_NE(content.find("Run ID: wave_predator_prey_0_20260516_230839"), std::string::npos);
        EXPECT_NE(content.find("Mode: StillPng"), std::string::npos);
        EXPECT_NE(content.find("alternate"), std::string::npos);
    }

    EXPECT_EQ(capture.completed_artifacts().size(), 2u);
    std::filesystem::remove_all(dir);
}

TEST(CaptureService, DoesNotOverwriteExistingArtifactNames) {
    const std::filesystem::path dir = test_output_dir("unique_names");

    ndde::CaptureService capture;
    capture.set_output_dir(dir);

    const ndde::CaptureRunMetadata metadata{
        .simulation_name = "Sim",
        .run_id = "sim_run",
        .tick = ndde::u64(1),
        .sim_time = ndde::f32(0.25)
    };
    capture.request_still(ndde::CaptureRequest{.target = ndde::CaptureTarget::MainWindow}, metadata);
    std::vector<ndde::CaptureArtifact> first = capture.consume_pending_stills();
    ASSERT_EQ(first.size(), 1u);

    std::filesystem::create_directories(first.front().path.parent_path());
    {
        std::ofstream existing(first.front().path);
        existing << "already here";
    }

    capture.request_still(ndde::CaptureRequest{.target = ndde::CaptureTarget::MainWindow}, metadata);
    std::vector<ndde::CaptureArtifact> second = capture.consume_pending_stills();
    ASSERT_EQ(second.size(), 1u);

    EXPECT_NE(first.front().path, second.front().path);
    EXPECT_NE(second.front().path.string().find("_002.png"), std::string::npos);

    std::filesystem::remove_all(dir);
}

TEST(CaptureService, MovieFrameSessionCreatesDirectoriesAndManifest) {
    const std::filesystem::path dir = test_output_dir("movie_frames");

    ndde::CaptureService capture;
    capture.set_output_dir(dir);

    const bool started = capture.start_movie_frames(ndde::MovieFrameSequenceOptions{
        .target = ndde::CaptureTarget::BothWindows,
        .fps = ndde::u32(30)
    }, ndde::CaptureRunMetadata{
        .simulation_name = "Wave Predator-Prey",
        .scenario_name = "Wave Predator-Prey",
        .run_id = "movie_run",
        .tick = ndde::u64(22),
        .sim_time = ndde::f32(1.5)
    });

    EXPECT_TRUE(started);
    EXPECT_TRUE(capture.movie_frames_active());
    const ndde::MovieFrameSequenceStatus status = capture.movie_frame_status();
    EXPECT_TRUE(std::filesystem::is_directory(status.main_frame_dir));
    EXPECT_TRUE(std::filesystem::is_directory(status.alternate_frame_dir));
    EXPECT_TRUE(std::filesystem::exists(status.manifest_path));
    EXPECT_FALSE(capture.start_movie_frames(ndde::MovieFrameSequenceOptions{}, ndde::CaptureRunMetadata{}));

    capture.stop_movie_frames();
    EXPECT_FALSE(capture.movie_frames_active());

    std::filesystem::remove_all(dir);
}

TEST(EngineServices, OwnsCaptureService) {
    const std::filesystem::path dir = test_output_dir("engine_services");
    ndde::EngineServices services;
    services.capture().set_output_dir(dir);
    services.capture().request_still(ndde::CaptureRequest{
        .target = ndde::CaptureTarget::MainWindow,
        .include_manifest = false
    }, ndde::CaptureRunMetadata{
        .simulation_name = "Service Test",
        .run_id = "service_test"
    });

    EXPECT_EQ(services.capture().consume_pending_stills().size(), 1u);
    std::filesystem::remove_all(dir);
}

TEST(CaptureService, ManifestWriteCanDrainThroughLoggerThread) {
    const std::filesystem::path dir = test_output_dir("async_manifest");
    ndde::EngineServices services;
    services.capture().set_output_dir(dir);

    services.capture().request_still(ndde::CaptureRequest{
        .target = ndde::CaptureTarget::MainWindow,
        .include_manifest = true
    }, ndde::CaptureRunMetadata{
        .simulation_name = "Async Manifest",
        .run_id = "async_manifest",
        .tick = ndde::u64(4),
        .sim_time = ndde::f32(0.125)
    });

    const std::vector<ndde::CaptureArtifact> pending = services.capture().consume_pending_stills();
    ASSERT_EQ(pending.size(), 1u);
    ASSERT_FALSE(pending.front().manifest_path.empty());

    ASSERT_TRUE(services.threads().run_logger_task_sync([] {}));
    EXPECT_TRUE(std::filesystem::exists(pending.front().manifest_path));

    std::filesystem::remove_all(dir);
}
