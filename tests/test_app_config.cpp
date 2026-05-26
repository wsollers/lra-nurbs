#include "engine/AppConfig.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace {

using namespace ndde;

TEST(AppConfig, LoadsThreadedRuntimeFlagFromSimulationSection) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ndde_app_config_threaded_runtime.json";
    {
        std::ofstream out(path);
        out << R"json({
            "simulation": {
                "threaded_runtime": true
            }
        })json";
    }

    const AppConfig config = AppConfig::load_or_default(path.string());
    EXPECT_TRUE(config.simulation.threaded_runtime);

    std::filesystem::remove(path);
}

TEST(AppConfig, LoadsThreadedPresentationFlagFromRenderSection) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ndde_app_config_threaded_presentation.json";
    {
        std::ofstream out(path);
        out << R"json({
            "render": {
                "threaded_presentation": false
            }
        })json";
    }

    const AppConfig config = AppConfig::load_or_default(path.string());
    EXPECT_FALSE(config.render.threaded_presentation);

    std::filesystem::remove(path);
}

TEST(AppConfig, SavesThreadedRuntimeFlag) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ndde_app_config_threaded_runtime_save.json";

    AppConfig config;
    config.simulation.threaded_runtime = true;
    config.save(path.string());

    const AppConfig loaded = AppConfig::load_or_default(path.string());
    EXPECT_TRUE(loaded.simulation.threaded_runtime);

    std::filesystem::remove(path);
}

TEST(AppConfig, SavesThreadedPresentationFlag) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ndde_app_config_threaded_presentation_save.json";

    AppConfig config;
    config.render.threaded_presentation = false;
    config.save(path.string());

    const AppConfig loaded = AppConfig::load_or_default(path.string());
    EXPECT_FALSE(loaded.render.threaded_presentation);

    std::filesystem::remove(path);
}

} // namespace
