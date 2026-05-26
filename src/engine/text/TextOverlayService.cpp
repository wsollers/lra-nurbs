#include "engine/text/TextOverlayService.hpp"

#include <algorithm>
#include <utility>

namespace ndde {

void TextOverlayService::set_memory_service(memory::MemoryService* memory) noexcept {
    m_memory = memory;
}

void TextOverlayService::set_thread_service(ThreadManagementService* threads,
                                            ThreadRole owner_role) noexcept {
    m_threads = threads;
    m_owner_role = owner_role;
}

void TextOverlayService::set_resource_manager(ResourceManagerService* resources) noexcept {
    m_resources = resources;
}

void TextOverlayService::set_default_font_path(std::filesystem::path path) {
    if (!require_owner_thread("TextOverlayService::set_default_font_path")) return;
    m_default_font_path = std::move(path);
}

const FontResource* TextOverlayService::active_font() const noexcept {
    if (!m_resources || !m_active_font_id) return nullptr;
    const ResourcePayload* payload = m_resources->payload(*m_active_font_id);
    if (!payload || !std::holds_alternative<FontResource>(*payload)) return nullptr;
    return &std::get<FontResource>(*payload);
}

bool TextOverlayService::bind_font_resource(ResourceHandle handle) {
    if (!require_owner_thread("TextOverlayService::bind_font_resource")) return false;
    if (!m_resources || handle.value == u64(0)) return false;
    const std::optional<ResourceId> id = m_resources->current(handle);
    if (!id) return false;
    const ResourceDescriptor* descriptor = m_resources->descriptor(*id);
    const ResourcePayload* payload = m_resources->payload(*id);
    if (!descriptor || descriptor->kind != ResourceKind::Font ||
        descriptor->state != ResourceState::Ready || !payload ||
        !std::holds_alternative<FontResource>(*payload)) {
        return false;
    }
    m_active_font_handle = handle;
    m_active_font_id = *id;
    return true;
}

std::expected<ResourceHandle, ResourceLoadError>
TextOverlayService::register_and_load_font(ResourceKey key, std::filesystem::path path) {
    if (!require_owner_thread("TextOverlayService::register_and_load_font")) {
        return std::unexpected(ResourceLoadError{.code = ResourceLoadErrorCode::Unknown});
    }
    if (!m_resources) {
        return std::unexpected(ResourceLoadError{.code = ResourceLoadErrorCode::Unknown});
    }
    const ResourceHandle handle = m_resources->register_path(ResourceRegistration{
        .key = key,
        .kind = ResourceKind::Font,
        .origin = ResourceOrigin::FilePath,
        .owner = ResourceOwner::Renderer,
        .lifetime = ResourceLifetime::Persistent
    }, path);
    if (handle.value == u64(0)) {
        return std::unexpected(ResourceLoadError{
            .code = ResourceLoadErrorCode::InvalidHandle,
            .path = std::move(path)
        });
    }
    const auto loaded = m_resources->load(handle);
    if (!loaded) {
        return std::unexpected(loaded.error());
    }
    if (!bind_font_resource(handle)) {
        return std::unexpected(ResourceLoadError{
            .code = ResourceLoadErrorCode::Unknown,
            .handle = handle,
            .path = std::move(path)
        });
    }
    m_default_font_path = std::move(path);
    return handle;
}

void TextOverlayService::submit(TextDrawRequest request) {
    if (!require_owner_thread("TextOverlayService::submit")) return;
    if (request.view == RenderViewId(0) || request.text.empty() || request.size_px <= f32(0)) {
        return;
    }

    m_commands.push_back(TextDrawCommand{
        .view = request.view,
        .space = request.space,
        .anchor = request.anchor,
        .position = request.position,
        .color = request.color,
        .size_px = request.size_px,
        .font = request.font,
        .text = std::string{request.text}
    });
}

void TextOverlayService::clear() {
    if (!require_owner_thread("TextOverlayService::clear")) return;
    m_commands.clear();
}

std::vector<TextDrawCommand> TextOverlayService::commands_for_view(RenderViewId view) const {
    std::vector<TextDrawCommand> out;
    if (view == RenderViewId(0)) return out;
    for (const TextDrawCommand& command : m_commands) {
        if (command.view == view) {
            out.push_back(command);
        }
    }
    return out;
}

std::size_t TextOverlayService::command_count(RenderViewId view) const noexcept {
    if (view == RenderViewId(0)) return m_commands.size();
    return static_cast<std::size_t>(std::count_if(m_commands.begin(), m_commands.end(),
        [view](const TextDrawCommand& command) { return command.view == view; }));
}

bool TextOverlayService::require_owner_thread(std::string_view api_name) {
    return !m_threads || m_threads->require_thread_role(m_owner_role, api_name);
}

} // namespace ndde
