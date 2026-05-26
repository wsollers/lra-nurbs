#include "engine/SimulationHost.hpp"
#include "engine/text/TextOverlayService.hpp"

#include <gtest/gtest.h>

#include <fstream>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace {

using namespace ndde;

TEST(TextOverlayService, StoresSubmittedCommands) {
    TextOverlayService service;

    service.submit(TextDrawRequest{
        .view = RenderViewId(7),
        .space = TextCoordinateSpace::Domain,
        .anchor = TextAnchor::Center,
        .position = {1.f, 2.f},
        .color = {0.1f, 0.2f, 0.3f, 1.f},
        .size_px = 18.f,
        .font = TextFontRole::Math,
        .text = "Integral"
    });

    ASSERT_EQ(service.commands().size(), 1u);
    const TextDrawCommand& command = service.commands().front();
    EXPECT_EQ(command.view, RenderViewId(7));
    EXPECT_EQ(command.space, TextCoordinateSpace::Domain);
    EXPECT_EQ(command.anchor, TextAnchor::Center);
    EXPECT_FLOAT_EQ(command.position.x, 1.f);
    EXPECT_EQ(command.font, TextFontRole::Math);
    EXPECT_EQ(command.text, "Integral");
}

TEST(TextOverlayService, IgnoresInvalidCommands) {
    TextOverlayService service;

    service.submit(TextDrawRequest{.view = RenderViewId(0), .text = "missing view"});
    service.submit(TextDrawRequest{.view = RenderViewId(1), .text = ""});
    service.submit(TextDrawRequest{.view = RenderViewId(1), .size_px = 0.f, .text = "bad size"});

    EXPECT_TRUE(service.commands().empty());
}

TEST(TextOverlayService, FiltersCommandsByViewAndClearsFrame) {
    TextOverlayService service;

    service.submit(TextDrawRequest{.view = RenderViewId(1), .text = "one"});
    service.submit(TextDrawRequest{.view = RenderViewId(2), .text = "two"});
    service.submit(TextDrawRequest{.view = RenderViewId(1), .text = "three"});

    EXPECT_EQ(service.command_count(), 3u);
    EXPECT_EQ(service.command_count(RenderViewId(1)), 2u);
    EXPECT_EQ(service.command_count(RenderViewId(2)), 1u);

    const auto view_one = service.commands_for_view(RenderViewId(1));
    ASSERT_EQ(view_one.size(), 2u);
    EXPECT_EQ(view_one[0].text, "one");
    EXPECT_EQ(view_one[1].text, "three");

    service.clear();
    EXPECT_TRUE(service.commands().empty());
}

TEST(TextOverlayService, TracksDefaultFontPath) {
    TextOverlayService service;

    EXPECT_EQ(service.default_font_path().generic_string(),
              "assets/fonts/static/STIXTwoText-Regular.ttf");

    service.set_default_font_path("assets/fonts/MonoTest.ttf");
    EXPECT_EQ(service.default_font_path().generic_string(), "assets/fonts/MonoTest.ttf");
}

TEST(TextOverlayService, RegistersLoadsAndBindsFontThroughResourceManager) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ndde_text_overlay_font.ttf";
    {
        std::ofstream out(path, std::ios::binary);
        out << "cached font bytes";
    }

    ResourceManagerService resources;
    resources.init();
    TextOverlayService service;
    service.set_resource_manager(&resources);

    const auto handle = service.register_and_load_font(ResourceKey{"font.overlay.test"}, path);
    ASSERT_TRUE(handle.has_value());
    EXPECT_EQ(service.active_font_handle(), *handle);
    ASSERT_TRUE(service.active_font_id().has_value());

    const FontResource* font = service.active_font();
    ASSERT_NE(font, nullptr);
    EXPECT_EQ(font->path, path);
    EXPECT_EQ(font->bytes.size(), 17u);
    EXPECT_EQ(resources.current(*handle), service.active_font_id());
}

TEST(TextRenderingVendor, FreeTypeCanCreateAndDestroyLibrary) {
    FT_Library library = nullptr;
    ASSERT_EQ(FT_Init_FreeType(&library), 0);
    ASSERT_NE(library, nullptr);
    EXPECT_EQ(FT_Done_FreeType(library), 0);
}

TEST(EngineServices, OwnsTextOverlayServiceAndPassesItToSimulationHost) {
    EngineServices services;
    SimulationHost host = services.simulation_host();

    host.text().submit(TextDrawRequest{.view = RenderViewId(9), .text = "label"});

    EXPECT_EQ(services.text().command_count(RenderViewId(9)), 1u);
}

} // namespace
