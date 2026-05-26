#pragma once
// engine/metadata/SimMetadataService.hpp
// Engine-owned registry for semantic component descriptors and factories.

#include "engine/metadata/MetadataTypes.hpp"
#include "engine/threading/ThreadTypes.hpp"

#include <functional>
#include <span>
#include <string_view>
#include <unordered_map>

namespace ndde {

class ThreadManagementService;

using RuntimeFactory = std::function<MetadataResult<RuntimeComponent>(const ComponentConfig&)>;

class SimMetadataService {
public:
    void set_thread_service(ThreadManagementService* threads,
                            ThreadRole owner_role = ThreadRole::Main) noexcept;

    [[nodiscard]] bool register_component(ComponentDescriptor descriptor);
    [[nodiscard]] bool register_event(EventDescriptor descriptor);
    [[nodiscard]] bool register_factory(ComponentId id, RuntimeFactory factory);

    [[nodiscard]] const ComponentDescriptor* get_descriptor(ComponentId id) const noexcept;
    [[nodiscard]] const EventDescriptor* get_event_descriptor(EventTypeId id) const noexcept;
    [[nodiscard]] std::span<const ComponentDescriptor> descriptors() const noexcept { return m_descriptors; }
    [[nodiscard]] std::span<const EventDescriptor> event_descriptors() const noexcept { return m_event_descriptors; }

    [[nodiscard]] std::vector<const ComponentDescriptor*> query_category(ObjectCategory category) const;
    [[nodiscard]] std::vector<const ComponentDescriptor*> query_capability(Capability capability) const;
    [[nodiscard]] std::vector<const ComponentDescriptor*> query_required_capability(Capability capability) const;

    [[nodiscard]] ValidationReport validate_component_config(ComponentId id,
                                                             const ComponentConfig& config) const;
    [[nodiscard]] ValidationReport validate_scenario(const ScenarioGraph& graph) const;

    [[nodiscard]] MetadataResult<RuntimeComponent> create(ComponentId id,
                                                          const ComponentConfig& config) const;

    void clear();

private:
    std::vector<ComponentDescriptor> m_descriptors;
    std::vector<EventDescriptor> m_event_descriptors;
    std::unordered_map<std::string, u64> m_index_by_id;
    std::unordered_map<std::string, u64> m_event_index_by_id;
    std::unordered_map<std::string, RuntimeFactory> m_factories;
    ThreadManagementService* m_threads = nullptr;
    ThreadRole m_owner_role = ThreadRole::Main;

    [[nodiscard]] bool require_owner_thread(std::string_view api_name) const;

    [[nodiscard]] static std::string key(ComponentId id);
    [[nodiscard]] static std::string key(EventTypeId id);
    [[nodiscard]] static bool has_capability(const ComponentDescriptor& descriptor,
                                             Capability capability) noexcept;
    [[nodiscard]] static bool value_matches(const ParameterSchema& schema,
                                            const ParameterValue& value) noexcept;
    [[nodiscard]] static const ParameterAssignment* find_assignment(const ComponentConfig& config,
                                                                    const std::string& key) noexcept;
    [[nodiscard]] static std::optional<f64> scalar_value(const ParameterValue& value) noexcept;
    [[nodiscard]] static ValidationIssue issue(DiagnosticSeverity severity,
                                               ErrorCode code,
                                               ComponentId component,
                                               std::string message,
                                               std::string suggested_fix = {});
};

} // namespace ndde
