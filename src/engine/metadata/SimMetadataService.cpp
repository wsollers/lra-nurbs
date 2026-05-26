#include "engine/metadata/SimMetadataService.hpp"
#include "engine/threading/ThreadManagementService.hpp"

#include <algorithm>
#include <iterator>

namespace ndde {

void SimMetadataService::set_thread_service(ThreadManagementService* threads,
                                            ThreadRole owner_role) noexcept {
    m_threads = threads;
    m_owner_role = owner_role;
}

bool SimMetadataService::register_component(ComponentDescriptor descriptor) {
    if (!require_owner_thread("SimMetadataService::register_component")) {
        return false;
    }
    const std::string id_key = key(descriptor.id);
    if (id_key.empty() || m_index_by_id.contains(id_key))
        return false;

    const u64 index = static_cast<u64>(m_descriptors.size());
    m_descriptors.push_back(std::move(descriptor));
    m_index_by_id.emplace(id_key, index);
    return true;
}

bool SimMetadataService::register_event(EventDescriptor descriptor) {
    if (!require_owner_thread("SimMetadataService::register_event")) {
        return false;
    }
    const std::string id_key = key(descriptor.id);
    if (id_key.empty() || m_event_index_by_id.contains(id_key))
        return false;

    const u64 index = static_cast<u64>(m_event_descriptors.size());
    m_event_descriptors.push_back(std::move(descriptor));
    m_event_index_by_id.emplace(id_key, index);
    return true;
}

bool SimMetadataService::register_factory(ComponentId id, RuntimeFactory factory) {
    if (!require_owner_thread("SimMetadataService::register_factory")) {
        return false;
    }
    const std::string id_key = key(id);
    const auto descriptor_it = m_index_by_id.find(id_key);
    if (descriptor_it == m_index_by_id.end() || !factory)
        return false;

    m_factories[id_key] = std::move(factory);
    m_descriptors[descriptor_it->second].factory_available = true;
    return true;
}

const ComponentDescriptor* SimMetadataService::get_descriptor(ComponentId id) const noexcept {
    const auto it = m_index_by_id.find(key(id));
    if (it == m_index_by_id.end())
        return nullptr;
    return &m_descriptors[it->second];
}

const EventDescriptor* SimMetadataService::get_event_descriptor(EventTypeId id) const noexcept {
    const auto it = m_event_index_by_id.find(key(id));
    if (it == m_event_index_by_id.end())
        return nullptr;
    return &m_event_descriptors[it->second];
}

std::vector<const ComponentDescriptor*> SimMetadataService::query_category(ObjectCategory category) const {
    std::vector<const ComponentDescriptor*> result;
    for (const ComponentDescriptor& descriptor : m_descriptors) {
        if (descriptor.category == category)
            result.push_back(&descriptor);
    }
    return result;
}

std::vector<const ComponentDescriptor*> SimMetadataService::query_capability(Capability capability) const {
    std::vector<const ComponentDescriptor*> result;
    for (const ComponentDescriptor& descriptor : m_descriptors) {
        if (has_capability(descriptor, capability))
            result.push_back(&descriptor);
    }
    return result;
}

std::vector<const ComponentDescriptor*> SimMetadataService::query_required_capability(Capability capability) const {
    std::vector<const ComponentDescriptor*> result;
    for (const ComponentDescriptor& descriptor : m_descriptors) {
        if (std::ranges::find(descriptor.required_capabilities, capability) != descriptor.required_capabilities.end())
            result.push_back(&descriptor);
    }
    return result;
}

ValidationReport SimMetadataService::validate_component_config(ComponentId id,
                                                               const ComponentConfig& config) const {
    ValidationReport report;
    const ComponentDescriptor* descriptor = get_descriptor(id);
    if (!descriptor) {
        report.issues.push_back(issue(DiagnosticSeverity::Error,
                                      ErrorCode::MissingDescriptor,
                                      id,
                                      "Component descriptor is not registered.",
                                      "Register the component metadata before validation."));
        return report;
    }

    for (const ParameterSchema& parameter : descriptor->parameters) {
        const ParameterAssignment* assignment = find_assignment(config, parameter.key);
        if (!assignment) {
            if (parameter.required) {
                report.issues.push_back(issue(DiagnosticSeverity::Error,
                                              ErrorCode::InvalidParameter,
                                              id,
                                              "Required parameter '" + parameter.key + "' is missing.",
                                              "Add the missing parameter to the component config."));
            }
            continue;
        }

        if (!value_matches(parameter, assignment->value)) {
            report.issues.push_back(issue(DiagnosticSeverity::Error,
                                          ErrorCode::InvalidParameter,
                                          id,
                                          "Parameter '" + parameter.key + "' has the wrong value type.",
                                          "Use a value matching the parameter schema."));
            continue;
        }

        if (const auto quantity = std::get_if<units::QuantityValue>(&assignment->value)) {
            if (quantity->kind != parameter.quantity_kind) {
                report.issues.push_back(issue(DiagnosticSeverity::Error,
                                              ErrorCode::UnitMismatch,
                                              id,
                                              "Parameter '" + parameter.key + "' has the wrong quantity kind.",
                                              "Use a quantity with the schema's expected kind."));
            }
        }

        if (parameter.domain) {
            const std::optional<f64> scalar = scalar_value(assignment->value);
            if (!scalar)
                continue;

            const ParameterDomain& domain = *parameter.domain;
            const bool below_min = domain.min && (domain.inclusive_min ? *scalar < *domain.min : *scalar <= *domain.min);
            const bool above_max = domain.max && (domain.inclusive_max ? *scalar > *domain.max : *scalar >= *domain.max);
            if (below_min || above_max) {
                report.issues.push_back(issue(DiagnosticSeverity::Error,
                                              ErrorCode::InvalidParameter,
                                              id,
                                              "Parameter '" + parameter.key + "' is outside its valid domain.",
                                              "Clamp or revise the parameter value."));
            }
        }
    }

    return report;
}

ValidationReport SimMetadataService::validate_scenario(const ScenarioGraph& graph) const {
    ValidationReport report;
    for (const ScenarioNode& node : graph.nodes) {
        ValidationReport node_report = validate_component_config(node.component, node.config);
        report.issues.insert(report.issues.end(),
                             std::make_move_iterator(node_report.issues.begin()),
                             std::make_move_iterator(node_report.issues.end()));

        const ComponentDescriptor* descriptor = get_descriptor(node.component);
        if (descriptor && !descriptor->factory_available) {
            report.issues.push_back(ValidationIssue{
                .severity = DiagnosticSeverity::Warning,
                .code = ErrorCode::MissingFactory,
                .source = DiagnosticSource{
                    .subsystem = DiagnosticSubsystem::Metadata,
                    .component = node.component,
                    .node = node.node_id,
                    .location = "SimMetadataService::validate_scenario"
                },
                .message = "Component has no registered runtime factory.",
                .suggested_fix = "Register a factory before constructing this scenario node."
            });
        }
    }
    return report;
}

MetadataResult<RuntimeComponent> SimMetadataService::create(ComponentId id,
                                                            const ComponentConfig& config) const {
    MetadataResult<RuntimeComponent> result;
    result.report = validate_component_config(id, config);
    if (!result.report.ok())
        return result;

    const auto it = m_factories.find(key(id));
    if (it == m_factories.end()) {
        result.report.issues.push_back(issue(DiagnosticSeverity::Error,
                                             ErrorCode::MissingFactory,
                                             id,
                                             "Component has no registered runtime factory.",
                                             "Register a factory for this component ID."));
        return result;
    }

    return it->second(config);
}

void SimMetadataService::clear() {
    if (!require_owner_thread("SimMetadataService::clear")) {
        return;
    }
    m_descriptors.clear();
    m_event_descriptors.clear();
    m_index_by_id.clear();
    m_event_index_by_id.clear();
    m_factories.clear();
}

bool SimMetadataService::require_owner_thread(std::string_view api_name) const {
    return !m_threads || m_threads->require_thread_role(m_owner_role, api_name);
}

std::string SimMetadataService::key(ComponentId id) {
    return std::string{id.value};
}

std::string SimMetadataService::key(EventTypeId id) {
    return std::string{id.value};
}

bool SimMetadataService::has_capability(const ComponentDescriptor& descriptor,
                                        Capability capability) noexcept {
    return std::ranges::find(descriptor.capabilities, capability) != descriptor.capabilities.end();
}

bool SimMetadataService::value_matches(const ParameterSchema& schema,
                                       const ParameterValue& value) noexcept {
    switch (schema.type) {
        case ParameterType::Bool: return std::holds_alternative<bool>(value);
        case ParameterType::Integer: return std::holds_alternative<i64>(value);
        case ParameterType::Scalar: return std::holds_alternative<f64>(value);
        case ParameterType::String: return std::holds_alternative<std::string>(value);
        case ParameterType::Quantity: return std::holds_alternative<units::QuantityValue>(value);
        case ParameterType::ComponentRef: return std::holds_alternative<ComponentId>(value);
    }
    return false;
}

const ParameterAssignment* SimMetadataService::find_assignment(const ComponentConfig& config,
                                                               const std::string& key) noexcept {
    const auto it = std::ranges::find_if(config.parameters, [&key](const ParameterAssignment& assignment) {
        return assignment.key == key;
    });
    return it != config.parameters.end() ? &*it : nullptr;
}

std::optional<f64> SimMetadataService::scalar_value(const ParameterValue& value) noexcept {
    if (const auto scalar = std::get_if<f64>(&value))
        return *scalar;
    if (const auto integer = std::get_if<i64>(&value))
        return static_cast<f64>(*integer);
    if (const auto quantity = std::get_if<units::QuantityValue>(&value))
        return quantity->value;
    return std::nullopt;
}

ValidationIssue SimMetadataService::issue(DiagnosticSeverity severity,
                                          ErrorCode code,
                                          ComponentId component,
                                          std::string message,
                                          std::string suggested_fix) {
    return ValidationIssue{
        .severity = severity,
        .code = code,
        .source = DiagnosticSource{
            .subsystem = DiagnosticSubsystem::Metadata,
            .component = component,
            .location = "SimMetadataService"
        },
        .message = std::move(message),
        .suggested_fix = std::move(suggested_fix)
    };
}

} // namespace ndde
