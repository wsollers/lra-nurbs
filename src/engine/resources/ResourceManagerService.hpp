#pragma once
// engine/resources/ResourceManagerService.hpp
// Engine-owned resource identity and lookup registry.

#include "engine/resources/ResourceTypes.hpp"
#include "engine/logging/LoggerTypes.hpp"
#include "engine/threading/ThreadTypes.hpp"

#include <expected>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ndde {

class ThreadManagementService;
class LoggerService;

class ResourceManagerService {
public:
    ResourceManagerService() = default;
    ~ResourceManagerService() = default;

    ResourceManagerService(const ResourceManagerService&) = delete;
    ResourceManagerService& operator=(const ResourceManagerService&) = delete;
    ResourceManagerService(ResourceManagerService&&) = delete;
    ResourceManagerService& operator=(ResourceManagerService&&) = delete;

    void set_thread_service(ThreadManagementService* threads,
                            ThreadRole owner_role = ThreadRole::Main) noexcept;
    void set_logger_service(LoggerService* logger) noexcept;

    void init(ResourceManagerConfig config = {});
    void shutdown() noexcept;

    [[nodiscard]] ResourceId reserve(ResourceKind kind,
                                     ResourceOwner owner,
                                     ResourceLifetime lifetime);
    [[nodiscard]] ResourceId reserve(ResourceHandle handle);

    [[nodiscard]] ResourceHandle register_handle(ResourceRegistration registration);
    [[nodiscard]] ResourceHandle register_builtin(ResourceRegistration registration);
    [[nodiscard]] ResourceHandle register_path(ResourceRegistration registration,
                                               std::filesystem::path path);

    [[nodiscard]] bool publish(ResourceId id, ResourcePayload payload);
    [[nodiscard]] std::expected<ResourceId, ResourceLoadError> load(ResourceHandle handle);
    [[nodiscard]] std::expected<ResourceId, ResourceLoadError> load_from_path(ResourceHandle handle,
                                                                              std::filesystem::path path);
    [[nodiscard]] bool mark_ready(ResourceId id);
    [[nodiscard]] bool mark_failed(ResourceId id, DiagnosticId diagnostic = {});
    [[nodiscard]] bool release(ResourceId id);

    [[nodiscard]] const ResourceDescriptor* descriptor(ResourceId id) const noexcept;
    [[nodiscard]] const ResourceDescriptor* descriptor(ResourceHandle handle) const noexcept;
    [[nodiscard]] const ResourcePayload* payload(ResourceId id) const noexcept;
    [[nodiscard]] std::optional<ResourceId> current(ResourceHandle handle) const noexcept;
    [[nodiscard]] std::optional<ResourceHandle> find(ResourceKey key) const noexcept;
    [[nodiscard]] std::optional<ResourceHandle> find_by_path(const std::filesystem::path& path) const;

    [[nodiscard]] std::vector<ResourceId> resources_by_owner(ResourceOwner owner) const;
    [[nodiscard]] std::vector<ResourceId> resources_by_kind(ResourceKind kind) const;
    [[nodiscard]] std::vector<ResourceId> resources_by_lifetime(ResourceLifetime lifetime) const;
    [[nodiscard]] std::span<const ResourceDescriptor> descriptors() const noexcept { return m_descriptors; }
    [[nodiscard]] bool initialised() const noexcept { return m_initialised; }

    void sweep_released();
    void clear_lifetime(ResourceLifetime lifetime);

private:
    struct HandleEntry {
        ResourceRegistration registration;
        ResourceGeneration generation = ResourceGeneration{u64(1)};
        std::optional<ResourceId> current;
        std::optional<std::filesystem::path> path;
        bool released = false;
    };

    ResourceManagerConfig m_config;
    std::vector<ResourceDescriptor> m_descriptors;
    std::vector<ResourcePayload> m_payloads;
    std::vector<HandleEntry> m_handles;
    std::unordered_map<u64, u64> m_index_by_id;
    std::unordered_map<u64, u64> m_index_by_handle;
    std::unordered_map<std::string, ResourceHandle> m_handle_by_key;
    std::unordered_map<std::string, ResourceHandle> m_handle_by_path;
    ThreadManagementService* m_threads = nullptr;
    LoggerService* m_logger = nullptr;
    ThreadRole m_owner_role = ThreadRole::Main;
    u64 m_next_id = u64(1);
    u64 m_next_runtime_handle = u64(0x0002'0000'0000'0000);
    bool m_initialised = false;

    [[nodiscard]] ResourceHandle allocate_runtime_handle();
    [[nodiscard]] bool require_owner_thread(std::string_view api_name) const;
    [[nodiscard]] ResourceHandle register_impl(ResourceRegistration registration,
                                               std::optional<std::filesystem::path> path);
    [[nodiscard]] ResourceDescriptor* mutable_descriptor(ResourceId id) noexcept;
    [[nodiscard]] HandleEntry* mutable_handle(ResourceHandle handle) noexcept;
    [[nodiscard]] const HandleEntry* handle_entry(ResourceHandle handle) const noexcept;
    [[nodiscard]] static std::string key_string(ResourceKey key);
    [[nodiscard]] static std::string path_key(const std::filesystem::path& path);
    [[nodiscard]] static u64 payload_byte_count(const ResourcePayload& payload) noexcept;
    static void set_payload_id(ResourcePayload& payload, ResourceId id, ResourceHandle handle);
    void log_resource(LogSeverity severity, ResourceId id, std::string message) const;
};

} // namespace ndde
