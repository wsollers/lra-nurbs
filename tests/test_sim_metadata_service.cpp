#include "engine/metadata/SimMetadataService.hpp"
#include "engine/SimulationHost.hpp"

#include <gtest/gtest.h>

namespace {

using namespace ndde;

ComponentDescriptor metric_ripple_descriptor() {
    return ComponentDescriptor{
        .id = ids::field_metric_ripple,
        .display_name = "Metric Ripple",
        .category = ObjectCategory::FieldObject,
        .capabilities = {
            Capability::MetricContribution,
            Capability::SurfaceDisplacementContribution,
            Capability::DiffusionContribution
        },
        .required_capabilities = {
            Capability::ParameterDomain
        },
        .assumptions = {
            Assumption::PositiveDefiniteMetric,
            Assumption::SmoothEnoughForCurvature
        },
        .parameters = {
            ParameterSchema{
                .key = "amplitude",
                .display_name = "Amplitude",
                .type = ParameterType::Quantity,
                .quantity_kind = units::QuantityKind::MetricFactor,
                .domain = ParameterDomain{.min = f64(0), .max = f64(2)},
                .description = "Ripple amplitude",
                .required = true
            }
        },
        .trust = TrustMetadata{
            .level = TrustLevel::Tested,
            .summary = "Metric ripple contributes metric factor and surface displacement."
        },
        .docs = DocumentationRef{
            .title = "Surface effects",
            .path = "docs/SIM_METADATA_SERVICE.md"
        }
    };
}

ComponentConfig valid_metric_ripple_config() {
    return ComponentConfig{
        .parameters = {
            ParameterAssignment{
                .key = "amplitude",
                .value = units::QuantityValue{
                    .value = f64(0.25),
                    .unit = "dimensionless",
                    .kind = units::QuantityKind::MetricFactor
                }
            }
        }
    };
}

TEST(SimMetadataService, RegistersAndFindsDescriptorById) {
    SimMetadataService metadata;

    ASSERT_TRUE(metadata.register_component(metric_ripple_descriptor()));

    const ComponentDescriptor* descriptor = metadata.get_descriptor(ids::field_metric_ripple);
    ASSERT_NE(descriptor, nullptr);
    EXPECT_EQ(descriptor->display_name, "Metric Ripple");
    EXPECT_EQ(descriptor->category, ObjectCategory::FieldObject);
    EXPECT_EQ(metadata.descriptors().size(), 1u);
}

TEST(SimMetadataService, RejectsDuplicateComponentId) {
    SimMetadataService metadata;

    EXPECT_TRUE(metadata.register_component(metric_ripple_descriptor()));
    EXPECT_FALSE(metadata.register_component(metric_ripple_descriptor()));
    EXPECT_EQ(metadata.descriptors().size(), 1u);
}

TEST(SimMetadataService, QueriesByCategoryAndCapability) {
    SimMetadataService metadata;
    ASSERT_TRUE(metadata.register_component(metric_ripple_descriptor()));
    ASSERT_TRUE(metadata.register_component(ComponentDescriptor{
        .id = ids::system_gravity_planar_n_body,
        .display_name = "Planar N-Body Gravity",
        .category = ObjectCategory::DynamicalObject,
        .capabilities = {Capability::ODESystem, Capability::TelemetryProducer},
        .trust = TrustMetadata{.level = TrustLevel::Tested}
    }));

    EXPECT_EQ(metadata.query_category(ObjectCategory::FieldObject).size(), 1u);
    EXPECT_EQ(metadata.query_category(ObjectCategory::DynamicalObject).size(), 1u);
    EXPECT_EQ(metadata.query_capability(Capability::MetricContribution).size(), 1u);
    EXPECT_EQ(metadata.query_capability(Capability::ODESystem).size(), 1u);
    EXPECT_EQ(metadata.query_required_capability(Capability::ParameterDomain).size(), 1u);
}

TEST(SimMetadataService, ValidatesRequiredParameterTypeDomainAndQuantityKind) {
    SimMetadataService metadata;
    ASSERT_TRUE(metadata.register_component(metric_ripple_descriptor()));

    EXPECT_TRUE(metadata.validate_component_config(ids::field_metric_ripple,
                                                   valid_metric_ripple_config()).ok());

    ValidationReport missing = metadata.validate_component_config(ids::field_metric_ripple, ComponentConfig{});
    ASSERT_TRUE(missing.has_errors());
    EXPECT_EQ(missing.issues.front().code, ErrorCode::InvalidParameter);

    ValidationReport wrong_type = metadata.validate_component_config(ids::field_metric_ripple, ComponentConfig{
        .parameters = {ParameterAssignment{.key = "amplitude", .value = f64(0.25)}}
    });
    ASSERT_TRUE(wrong_type.has_errors());
    EXPECT_EQ(wrong_type.issues.front().code, ErrorCode::InvalidParameter);

    ValidationReport wrong_quantity = metadata.validate_component_config(ids::field_metric_ripple, ComponentConfig{
        .parameters = {ParameterAssignment{
            .key = "amplitude",
            .value = units::QuantityValue{
                .value = f64(0.25),
                .unit = "m",
                .kind = units::QuantityKind::Length
            }
        }}
    });
    ASSERT_TRUE(wrong_quantity.has_errors());
    EXPECT_EQ(wrong_quantity.issues.front().code, ErrorCode::UnitMismatch);

    ValidationReport out_of_domain = metadata.validate_component_config(ids::field_metric_ripple, ComponentConfig{
        .parameters = {ParameterAssignment{
            .key = "amplitude",
            .value = units::QuantityValue{
                .value = f64(3),
                .unit = "dimensionless",
                .kind = units::QuantityKind::MetricFactor
            }
        }}
    });
    ASSERT_TRUE(out_of_domain.has_errors());
    EXPECT_EQ(out_of_domain.issues.front().code, ErrorCode::InvalidParameter);
}

TEST(SimMetadataService, ReportsMissingDescriptorAndMissingFactory) {
    SimMetadataService metadata;

    ValidationReport missing_descriptor =
        metadata.validate_component_config(ComponentId{"missing.component"}, ComponentConfig{});
    ASSERT_TRUE(missing_descriptor.has_errors());
    EXPECT_EQ(missing_descriptor.issues.front().code, ErrorCode::MissingDescriptor);

    ASSERT_TRUE(metadata.register_component(metric_ripple_descriptor()));
    MetadataResult<RuntimeComponent> missing_factory =
        metadata.create(ids::field_metric_ripple, valid_metric_ripple_config());
    ASSERT_FALSE(missing_factory.ok());
    EXPECT_EQ(missing_factory.report.issues.front().code, ErrorCode::MissingFactory);
}

TEST(SimMetadataService, FactoryCreationReturnsRuntimeVariant) {
    SimMetadataService metadata;
    ASSERT_TRUE(metadata.register_component(metric_ripple_descriptor()));
    ASSERT_TRUE(metadata.register_factory(ids::field_metric_ripple,
        [](const ComponentConfig&) {
            return MetadataResult<RuntimeComponent>{
                .value = RuntimeField{
                    .id = ids::field_metric_ripple,
                    .display_name = "Metric Ripple Instance"
                }
            };
        }));

    const ComponentDescriptor* descriptor = metadata.get_descriptor(ids::field_metric_ripple);
    ASSERT_NE(descriptor, nullptr);
    EXPECT_TRUE(descriptor->factory_available);

    MetadataResult<RuntimeComponent> result =
        metadata.create(ids::field_metric_ripple, valid_metric_ripple_config());

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(std::holds_alternative<RuntimeField>(result.value));
    EXPECT_EQ(std::get<RuntimeField>(result.value).display_name, "Metric Ripple Instance");
}

TEST(SimMetadataService, ScenarioValidationWarnsWhenFactoryIsMissing) {
    SimMetadataService metadata;
    ASSERT_TRUE(metadata.register_component(metric_ripple_descriptor()));

    ValidationReport report = metadata.validate_scenario(ScenarioGraph{
        .name = "missing factory scenario",
        .nodes = {
            ScenarioNode{
                .node_id = RuntimeNodeId{3u},
                .component = ids::field_metric_ripple,
                .config = valid_metric_ripple_config()
            }
        }
    });

    ASSERT_TRUE(report.has_warnings());
    EXPECT_EQ(report.issues.front().code, ErrorCode::MissingFactory);
    ASSERT_TRUE(report.issues.front().source.node.has_value());
    EXPECT_EQ(report.issues.front().source.node->value, 3u);
}

TEST(SimMetadataService, RegistersAndFindsEventDescriptorById) {
    SimMetadataService metadata;

    ASSERT_TRUE(metadata.register_event(EventDescriptor{
        .id = EventTypeId{"event.sim.agent_captured"},
        .display_name = "Agent Captured",
        .scope = EventScope::Simulation,
        .default_severity = DiagnosticSeverity::Info,
        .producer = ids::simulation_wave_predator_prey,
        .docs = DocumentationRef{
            .title = "EventBusService",
            .path = "docs/EVENT_BUS_SERVICE.md",
            .section = "Simulation Events"
        }
    }));
    EXPECT_FALSE(metadata.register_event(EventDescriptor{
        .id = EventTypeId{"event.sim.agent_captured"},
        .display_name = "Duplicate"
    }));

    const EventDescriptor* descriptor =
        metadata.get_event_descriptor(EventTypeId{"event.sim.agent_captured"});
    ASSERT_NE(descriptor, nullptr);
    EXPECT_EQ(descriptor->display_name, "Agent Captured");
    EXPECT_EQ(descriptor->scope, EventScope::Simulation);
    EXPECT_EQ(descriptor->producer, ids::simulation_wave_predator_prey);
    EXPECT_EQ(metadata.event_descriptors().size(), 1u);
}

TEST(SimMetadataService, ClearRemovesEventDescriptors) {
    SimMetadataService metadata;
    ASSERT_TRUE(metadata.register_event(EventDescriptor{
        .id = EventTypeId{"event.sim.field_added"},
        .display_name = "Field Added"
    }));

    metadata.clear();

    EXPECT_TRUE(metadata.event_descriptors().empty());
    EXPECT_EQ(metadata.get_event_descriptor(EventTypeId{"event.sim.field_added"}), nullptr);
}

TEST(EngineServices, OwnsMetadataServiceAndPassesItToSimulationHost) {
    EngineServices services;
    ASSERT_TRUE(services.metadata().register_component(metric_ripple_descriptor()));

    SimulationHost host = services.simulation_host();

    EXPECT_NE(host.metadata().get_descriptor(ids::field_metric_ripple), nullptr);
    EXPECT_EQ(host.metadata().descriptors().size(), 1u);
}

} // namespace
