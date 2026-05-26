#include "engine/resources/ResourceManagerService.hpp"
#include "engine/logging/LoggerService.hpp"
#include "engine/threading/ThreadManagementService.hpp"

#include <algorithm>
#include <format>
#include <fstream>

namespace ndde {

void ResourceManagerService::set_thread_service(ThreadManagementService* threads,
                                                ThreadRole owner_role) noexcept {
    m_threads = threads;
    m_owner_role = owner_role;
}

void ResourceManagerService::set_logger_service(LoggerService* logger) noexcept {
    m_logger = logger;
}

void ResourceManagerService::init(ResourceManagerConfig config) {
    shutdown();
    m_config = config;
    m_descriptors.reserve(static_cast<std::size_t>(std::min<u64>(m_config.max_resources, u64(4096))));
    m_payloads.reserve(m_descriptors.capacity());
    m_handles.reserve(static_cast<std::size_t>(std::min<u64>(m_config.max_handles, u64(4096))));
    m_initialised = true;
}

void ResourceManagerService::shutdown() noexcept {
    m_descriptors.clear();
    m_payloads.clear();
    m_handles.clear();
    m_index_by_id.clear();
    m_index_by_handle.clear();
    m_handle_by_key.clear();
    m_handle_by_path.clear();
    m_config = {};
    m_next_id = u64(1);
    m_next_runtime_handle = u64(0x0002'0000'0000'0000);
    m_initialised = false;
}

ResourceId ResourceManagerService::reserve(ResourceKind kind,
                                           ResourceOwner owner,
                                           ResourceLifetime lifetime) {
    if (!require_owner_thread("ResourceManagerService::reserve")) {
        return {};
    }
    if (!m_initialised) {
        init();
    }
    if (m_descriptors.size() >= static_cast<std::size_t>(m_config.max_resources)) {
        return {};
    }

    const ResourceId id{m_next_id++};
    const u64 index = static_cast<u64>(m_descriptors.size());
    m_descriptors.push_back(ResourceDescriptor{
        .id = id,
        .kind = kind,
        .owner = owner,
        .lifetime = lifetime
    });
    m_payloads.push_back(ResourcePayload{FileArtifactResource{.id = id}});
    m_index_by_id.emplace(id.value, index);
    return id;
}

ResourceId ResourceManagerService::reserve(ResourceHandle handle) {
    if (!require_owner_thread("ResourceManagerService::reserve")) {
        return {};
    }
    HandleEntry* entry = mutable_handle(handle);
    if (!entry || entry->released) {
        return {};
    }
    const ResourceId id = reserve(entry->registration.kind,
                                  entry->registration.owner,
                                  entry->registration.lifetime);
    ResourceDescriptor* desc = mutable_descriptor(id);
    if (!desc) {
        return {};
    }
    desc->handle = handle;
    desc->origin = entry->registration.origin;
    desc->producer = entry->registration.producer;
    desc->generation = entry->generation;
    return id;
}

ResourceHandle ResourceManagerService::register_handle(ResourceRegistration registration) {
    if (!require_owner_thread("ResourceManagerService::register_handle")) {
        return {};
    }
    return register_impl(std::move(registration), std::nullopt);
}

ResourceHandle ResourceManagerService::register_builtin(ResourceRegistration registration) {
    if (!require_owner_thread("ResourceManagerService::register_builtin")) {
        return {};
    }
    if (registration.handle.value == u64(0)) {
        return {};
    }
    if (registration.origin == ResourceOrigin::Generated) {
        registration.origin = ResourceOrigin::ConstexprRegistered;
    }
    return register_impl(std::move(registration), std::nullopt);
}

ResourceHandle ResourceManagerService::register_path(ResourceRegistration registration,
                                                     std::filesystem::path path) {
    if (!require_owner_thread("ResourceManagerService::register_path")) {
        return {};
    }
    registration.origin = ResourceOrigin::FilePath;
    return register_impl(std::move(registration), std::move(path));
}

bool ResourceManagerService::publish(ResourceId id, ResourcePayload payload) {
    if (!require_owner_thread("ResourceManagerService::publish")) {
        return false;
    }
    ResourceDescriptor* desc = mutable_descriptor(id);
    if (!desc || desc->state != ResourceState::Reserved) {
        return false;
    }

    set_payload_id(payload, id, desc->handle);
    desc->byte_count = payload_byte_count(payload);
    desc->state = ResourceState::Pending;

    const auto payload_index = m_index_by_id.find(id.value);
    if (payload_index == m_index_by_id.end()) {
        return false;
    }
    m_payloads[payload_index->second] = std::move(payload);

    if (HandleEntry* entry = mutable_handle(desc->handle)) {
        entry->current = id;
        desc->generation = entry->generation;
    }
    return true;
}

std::expected<ResourceId, ResourceLoadError> ResourceManagerService::load(ResourceHandle handle) {
    if (!require_owner_thread("ResourceManagerService::load")) {
        return std::unexpected(ResourceLoadError{.code = ResourceLoadErrorCode::Unknown, .handle = handle});
    }
    const HandleEntry* entry = handle_entry(handle);
    if (!entry || entry->released) {
        log_resource(LogSeverity::Error, {}, std::format(
            "[ResourceManager] load failed: invalid handle {:#x}",
            handle.value));
        return std::unexpected(ResourceLoadError{.code = ResourceLoadErrorCode::InvalidHandle, .handle = handle});
    }
    if (!entry->path) {
        log_resource(LogSeverity::Error, {}, std::format(
            "[ResourceManager] load failed: no path registered for handle {:#x}",
            handle.value));
        return std::unexpected(ResourceLoadError{.code = ResourceLoadErrorCode::NoPathRegistered, .handle = handle});
    }
    return load_from_path(handle, *entry->path);
}

std::expected<ResourceId, ResourceLoadError> ResourceManagerService::load_from_path(ResourceHandle handle,
                                                                                   std::filesystem::path path) {
    if (!require_owner_thread("ResourceManagerService::load_from_path")) {
        return std::unexpected(ResourceLoadError{
            .code = ResourceLoadErrorCode::Unknown,
            .handle = handle,
            .path = std::move(path)
        });
    }
    HandleEntry* entry = mutable_handle(handle);
    if (!entry || entry->released) {
        log_resource(LogSeverity::Error, {}, std::format(
            "[ResourceManager] load failed: invalid handle {:#x} path '{}'",
            handle.value,
            path.string()));
        return std::unexpected(ResourceLoadError{
            .code = ResourceLoadErrorCode::InvalidHandle,
            .handle = handle,
            .path = std::move(path)
        });
    }
    if (!std::filesystem::exists(path)) {
        log_resource(LogSeverity::Error, {}, std::format(
            "[ResourceManager] load failed: file not found handle {:#x} path '{}'",
            handle.value,
            path.string()));
        return std::unexpected(ResourceLoadError{
            .code = ResourceLoadErrorCode::FileNotFound,
            .handle = handle,
            .path = std::move(path)
        });
    }

    const ResourceId id = reserve(handle);
    if (id.value == u64(0)) {
        log_resource(LogSeverity::Error, {}, std::format(
            "[ResourceManager] load failed: reserve failed handle {:#x} path '{}'",
            handle.value,
            path.string()));
        return std::unexpected(ResourceLoadError{
            .code = ResourceLoadErrorCode::Unknown,
            .handle = handle,
            .path = std::move(path)
        });
    }

    ResourceDescriptor* desc = mutable_descriptor(id);
    if (desc) {
        desc->origin = ResourceOrigin::FilePath;
    }

    const u64 size = static_cast<u64>(std::filesystem::file_size(path));
    ResourcePayload loaded = PathBackedResource{
        .id = id,
        .handle = handle,
        .path = path,
        .expected_kind = entry->registration.kind,
        .byte_count = size
    };
    if (entry->registration.kind == ResourceKind::Font) {
        std::vector<byte> bytes(static_cast<std::size_t>(size));
        std::ifstream in(path, std::ios::binary);
        if (!in || (size > u64(0) &&
                    !in.read(reinterpret_cast<char*>(bytes.data()),
                             static_cast<std::streamsize>(bytes.size())))) {
            log_resource(LogSeverity::Error, id, std::format(
                "[ResourceManager] load failed: could not read font bytes resource {} handle {:#x} path '{}'",
                id.value,
                handle.value,
                path.string()));
            return std::unexpected(ResourceLoadError{
                .code = ResourceLoadErrorCode::Unknown,
                .handle = handle,
                .path = std::move(path)
            });
        }
        loaded = FontResource{
            .id = id,
            .handle = handle,
            .path = path,
            .byte_count = size,
            .bytes = std::move(bytes)
        };
    }
    if (!publish(id, std::move(loaded))) {
        log_resource(LogSeverity::Error, id, std::format(
            "[ResourceManager] load failed: publish failed resource {} handle {:#x} path '{}'",
            id.value,
            handle.value,
            path.string()));
        return std::unexpected(ResourceLoadError{
            .code = ResourceLoadErrorCode::Unknown,
            .handle = handle,
            .path = std::move(path)
        });
    }
    (void)mark_ready(id);
    entry->path = path;
    m_handle_by_path[path_key(path)] = handle;
    log_resource(LogSeverity::Info, id, std::format(
        "[ResourceManager] loaded resource {} handle {:#x} kind {} bytes {} path '{}'",
        id.value,
        handle.value,
        static_cast<unsigned>(entry->registration.kind),
        size,
        path.string()));
    return id;
}

bool ResourceManagerService::mark_ready(ResourceId id) {
    if (!require_owner_thread("ResourceManagerService::mark_ready")) {
        return false;
    }
    ResourceDescriptor* desc = mutable_descriptor(id);
    if (!desc || desc->state == ResourceState::Released) {
        return false;
    }
    desc->state = ResourceState::Ready;
    return true;
}

bool ResourceManagerService::mark_failed(ResourceId id, DiagnosticId diagnostic) {
    if (!require_owner_thread("ResourceManagerService::mark_failed")) {
        return false;
    }
    ResourceDescriptor* desc = mutable_descriptor(id);
    if (!desc || desc->state == ResourceState::Released) {
        return false;
    }
    desc->state = ResourceState::Failed;
    if (diagnostic.value != u64(0)) {
        desc->diagnostic = diagnostic;
    }
    return true;
}

bool ResourceManagerService::release(ResourceId id) {
    if (!require_owner_thread("ResourceManagerService::release")) {
        return false;
    }
    ResourceDescriptor* desc = mutable_descriptor(id);
    if (!desc || desc->state == ResourceState::Released) {
        return false;
    }
    desc->state = ResourceState::Released;
    if (HandleEntry* entry = mutable_handle(desc->handle)) {
        if (entry->current && *entry->current == id) {
            entry->current = std::nullopt;
            ++entry->generation.value;
        }
    }
    return true;
}

const ResourceDescriptor* ResourceManagerService::descriptor(ResourceId id) const noexcept {
    const auto it = m_index_by_id.find(id.value);
    if (it == m_index_by_id.end()) {
        return nullptr;
    }
    return &m_descriptors[it->second];
}

const ResourceDescriptor* ResourceManagerService::descriptor(ResourceHandle handle) const noexcept {
    const HandleEntry* entry = handle_entry(handle);
    if (!entry || !entry->current) {
        return nullptr;
    }
    return descriptor(*entry->current);
}

const ResourcePayload* ResourceManagerService::payload(ResourceId id) const noexcept {
    const auto it = m_index_by_id.find(id.value);
    if (it == m_index_by_id.end()) {
        return nullptr;
    }
    return &m_payloads[it->second];
}

std::optional<ResourceId> ResourceManagerService::current(ResourceHandle handle) const noexcept {
    const HandleEntry* entry = handle_entry(handle);
    if (!entry) {
        return std::nullopt;
    }
    return entry->current;
}

std::optional<ResourceHandle> ResourceManagerService::find(ResourceKey key) const noexcept {
    const auto it = m_handle_by_key.find(key_string(key));
    if (it == m_handle_by_key.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<ResourceHandle> ResourceManagerService::find_by_path(const std::filesystem::path& path) const {
    const auto it = m_handle_by_path.find(path_key(path));
    if (it == m_handle_by_path.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<ResourceId> ResourceManagerService::resources_by_owner(ResourceOwner owner) const {
    std::vector<ResourceId> result;
    for (const ResourceDescriptor& desc : m_descriptors) {
        if (desc.owner == owner && desc.state != ResourceState::Released) {
            result.push_back(desc.id);
        }
    }
    return result;
}

std::vector<ResourceId> ResourceManagerService::resources_by_kind(ResourceKind kind) const {
    std::vector<ResourceId> result;
    for (const ResourceDescriptor& desc : m_descriptors) {
        if (desc.kind == kind && desc.state != ResourceState::Released) {
            result.push_back(desc.id);
        }
    }
    return result;
}

std::vector<ResourceId> ResourceManagerService::resources_by_lifetime(ResourceLifetime lifetime) const {
    std::vector<ResourceId> result;
    for (const ResourceDescriptor& desc : m_descriptors) {
        if (desc.lifetime == lifetime && desc.state != ResourceState::Released) {
            result.push_back(desc.id);
        }
    }
    return result;
}

void ResourceManagerService::sweep_released() {
    if (!require_owner_thread("ResourceManagerService::sweep_released")) {
        return;
    }
    // Keep stable vector indices for now. A compacting implementation would need
    // to rewrite all indexes and is unnecessary until resource churn appears.
}

void ResourceManagerService::clear_lifetime(ResourceLifetime lifetime) {
    if (!require_owner_thread("ResourceManagerService::clear_lifetime")) {
        return;
    }
    for (ResourceDescriptor& desc : m_descriptors) {
        if (desc.lifetime == lifetime && desc.state != ResourceState::Released) {
            (void)release(desc.id);
        }
    }
}

ResourceHandle ResourceManagerService::allocate_runtime_handle() {
    return ResourceHandle{m_next_runtime_handle++};
}

bool ResourceManagerService::require_owner_thread(std::string_view api_name) const {
    return !m_threads || m_threads->require_thread_role(m_owner_role, api_name);
}

ResourceHandle ResourceManagerService::register_impl(ResourceRegistration registration,
                                                     std::optional<std::filesystem::path> path) {
    if (!m_initialised) {
        init();
    }
    if (m_handles.size() >= static_cast<std::size_t>(m_config.max_handles)) {
        return {};
    }

    if (registration.handle.value == u64(0)) {
        registration.handle = allocate_runtime_handle();
    }
    if (registration.handle.value == u64(0) || m_index_by_handle.contains(registration.handle.value)) {
        return {};
    }

    const std::string key = key_string(registration.key);
    if (!key.empty() && m_handle_by_key.contains(key)) {
        return {};
    }

    if (path) {
        const std::string canonical_path = path_key(*path);
        if (m_handle_by_path.contains(canonical_path)) {
            return {};
        }
        m_handle_by_path.emplace(canonical_path, registration.handle);
    }

    const u64 index = static_cast<u64>(m_handles.size());
    if (!key.empty()) {
        m_handle_by_key.emplace(key, registration.handle);
    }
    m_index_by_handle.emplace(registration.handle.value, index);
    m_handles.push_back(HandleEntry{
        .registration = std::move(registration),
        .path = std::move(path)
    });
    return m_handles.back().registration.handle;
}

ResourceDescriptor* ResourceManagerService::mutable_descriptor(ResourceId id) noexcept {
    const auto it = m_index_by_id.find(id.value);
    if (it == m_index_by_id.end()) {
        return nullptr;
    }
    return &m_descriptors[it->second];
}

ResourceManagerService::HandleEntry* ResourceManagerService::mutable_handle(ResourceHandle handle) noexcept {
    const auto it = m_index_by_handle.find(handle.value);
    if (it == m_index_by_handle.end()) {
        return nullptr;
    }
    return &m_handles[it->second];
}

const ResourceManagerService::HandleEntry* ResourceManagerService::handle_entry(ResourceHandle handle) const noexcept {
    const auto it = m_index_by_handle.find(handle.value);
    if (it == m_index_by_handle.end()) {
        return nullptr;
    }
    return &m_handles[it->second];
}

std::string ResourceManagerService::key_string(ResourceKey key) {
    return std::string{key.value};
}

std::string ResourceManagerService::path_key(const std::filesystem::path& path) {
    return std::filesystem::absolute(path).lexically_normal().string();
}

u64 ResourceManagerService::payload_byte_count(const ResourcePayload& payload) noexcept {
    return std::visit([](const auto& item) -> u64 {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, CpuMeshResource>) {
            return (item.vertex_count + item.index_count) * u64(sizeof(Vertex));
        } else if constexpr (std::is_same_v<T, RenderSnapshotResource>) {
            return item.vertex_count * u64(sizeof(Vertex));
        } else if constexpr (std::is_same_v<T, FontResource>) {
            return static_cast<u64>(item.bytes.size());
        } else {
            return item.byte_count;
        }
    }, payload);
}

void ResourceManagerService::set_payload_id(ResourcePayload& payload, ResourceId id, ResourceHandle handle) {
    std::visit([id, handle](auto& item) {
        item.id = id;
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, PathBackedResource>) {
            item.handle = handle;
        } else if constexpr (std::is_same_v<T, FontResource>) {
            item.handle = handle;
        }
    }, payload);
}

void ResourceManagerService::log_resource(LogSeverity severity, ResourceId id, std::string message) const {
    if (!m_logger) return;
    auto write = [logger = m_logger, severity, id, message = std::move(message)] {
        (void)logger->write_resource(id, severity, message);
    };
    if (m_threads && m_threads->enqueue_logger_task(write)) {
        return;
    }
    write();
}

} // namespace ndde
