#pragma once
// engine/text/TextOverlayService.hpp
// Renderer-neutral frame text command service.

#include "engine/RenderService.hpp"
#include "engine/resources/ResourceManagerService.hpp"
#include "engine/threading/ThreadManagementService.hpp"
#include "math/GeometryTypes.hpp"
#include "math/Scalars.hpp"
#include "memory/MemoryService.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ndde {

enum class TextCoordinateSpace : u8 {
    ScreenPixels,
    NormalizedViewport,
    Domain
};

enum class TextAnchor : u8 {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center
};

enum class TextFontRole : u8 {
    Body,
    Mono,
    Math,
    Small
};

struct TextDrawCommand {
    RenderViewId view = RenderViewId(0);
    TextCoordinateSpace space = TextCoordinateSpace::NormalizedViewport;
    TextAnchor anchor = TextAnchor::TopLeft;
    Vec2 position{};
    Vec4 color{1.f, 1.f, 1.f, 1.f};
    f32 size_px = 14.f;
    TextFontRole font = TextFontRole::Mono;
    std::string text;
};

struct TextDrawRequest {
    RenderViewId view = RenderViewId(0);
    TextCoordinateSpace space = TextCoordinateSpace::NormalizedViewport;
    TextAnchor anchor = TextAnchor::TopLeft;
    Vec2 position{};
    Vec4 color{1.f, 1.f, 1.f, 1.f};
    f32 size_px = 14.f;
    TextFontRole font = TextFontRole::Mono;
    std::string_view text;
};

class TextOverlayService {
public:
    void set_memory_service(memory::MemoryService* memory) noexcept;
    void set_thread_service(ThreadManagementService* threads,
                            ThreadRole owner_role = ThreadRole::Main) noexcept;
    void set_resource_manager(ResourceManagerService* resources) noexcept;

    void set_default_font_path(std::filesystem::path path);
    [[nodiscard]] const std::filesystem::path& default_font_path() const noexcept { return m_default_font_path; }
    [[nodiscard]] ResourceHandle active_font_handle() const noexcept { return m_active_font_handle; }
    [[nodiscard]] std::optional<ResourceId> active_font_id() const noexcept { return m_active_font_id; }
    [[nodiscard]] const FontResource* active_font() const noexcept;
    [[nodiscard]] bool bind_font_resource(ResourceHandle handle);
    [[nodiscard]] std::expected<ResourceHandle, ResourceLoadError>
    register_and_load_font(ResourceKey key, std::filesystem::path path);

    void submit(TextDrawRequest request);
    void clear();

    [[nodiscard]] std::span<const TextDrawCommand> commands() const noexcept { return m_commands; }
    [[nodiscard]] std::vector<TextDrawCommand> commands_for_view(RenderViewId view) const;
    [[nodiscard]] std::size_t command_count(RenderViewId view = RenderViewId(0)) const noexcept;

private:
    std::vector<TextDrawCommand> m_commands;
    std::filesystem::path m_default_font_path{"assets/fonts/static/STIXTwoText-Regular.ttf"};
    ResourceManagerService* m_resources = nullptr;
    ResourceHandle m_active_font_handle{};
    std::optional<ResourceId> m_active_font_id;
    memory::MemoryService* m_memory = nullptr;
    ThreadManagementService* m_threads = nullptr;
    ThreadRole m_owner_role = ThreadRole::Main;

    [[nodiscard]] bool require_owner_thread(std::string_view api_name);
};

} // namespace ndde
