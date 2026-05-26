#pragma once
// engine/metadata/MetadataTypes.hpp
// Stable component descriptors and lightweight scenario metadata types.

#include "engine/RuntimeIds.hpp"
#include "engine/diagnostics/DiagnosticsTypes.hpp"
#include "units/QuantityTypes.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ndde {

enum class ObjectCategory {
    GeometricObject,
    FieldObject,
    OperatorObject,
    DynamicalObject,
    ControlObject,
    ObservableObject
};

enum class Capability {
    ParameterDomain,
    EmbeddedEvaluation,
    MetricTensor,
    InverseMetricTensor,
    ChristoffelSymbols,
    TangentBundle,
    CotangentBundle,
    ExponentialMap,
    LogarithmMap,
    GeodesicDistance,
    GaussianCurvature,
    MeanCurvature,
    Orientable,
    Compact,
    NoBoundary,

    DriftContribution,
    DiffusionContribution,
    MetricContribution,
    SurfaceDisplacementContribution,

    ODESystem,
    DDESystem,
    DDEHistory,
    RandomStream,
    StochasticTransition,

    ControlLaw,
    CompositeControlLaw,
    StateController,
    MarkovControlLaw,

    BezierCurve,
    BSplineCurve,
    NURBSCurve,
    ArcLengthParameterization,
    GeodesicSpline,

    DerivativeOperator,
    JacobianOperator,
    HessianOperator,
    LaplaceBeltramiOperator,

    TelemetryProducer,
    RenderPacketProducer,
    ReplaySerializable
};

enum class Assumption {
    C0Continuity,
    C1Continuity,
    C2Regularity,
    SmoothEnoughForCurvature,
    PositiveDefiniteMetric,
    ChartDomainValid,
    NoCutLocusCrossing,
    OrientedDomain,
    CompactDomain,
    IrreducibleMarkovChain,
    GeneratorRowsSumToZero,
    NonnegativeOffDiagonalRates,
    KnotVectorNondecreasing,
    NURBSWeightsPositive,
    ReplaySeedAvailable
};

enum class ParameterType {
    Bool,
    Integer,
    Scalar,
    String,
    Quantity,
    ComponentRef
};

enum class Mutability {
    Live,
    RequiresRecompute,
    RequiresComponentRebuild,
    RequiresScenarioRestart
};

enum class UiHint {
    Checkbox,
    Slider,
    Drag,
    Input,
    Combo,
    Color,
    FilePath,
    MultilineText
};

struct ParameterDomain {
    std::optional<f64> min;
    std::optional<f64> max;
    bool inclusive_min = true;
    bool inclusive_max = true;
};

struct ParameterSchema {
    std::string key;
    std::string display_name;
    ParameterType type = ParameterType::Scalar;
    units::QuantityKind quantity_kind = units::QuantityKind::Dimensionless;
    Mutability mutability = Mutability::Live;
    std::optional<ParameterDomain> domain;
    std::optional<UiHint> ui_hint;
    std::string description;
    bool required = false;
};

enum class PortKind {
    Geometry,
    Field,
    Control,
    State,
    Telemetry,
    RenderPacket,
    Event,
    Quantity
};

struct PortDescriptor {
    std::string name;
    PortKind kind = PortKind::State;
    std::string type_name;
    bool required = false;
};

struct DocumentationRef {
    std::string title;
    std::string path;
    std::string section;
};

enum class TrustLevel {
    Experimental,
    Tested,
    Validated,
    Research
};

struct TrustMetadata {
    TrustLevel level = TrustLevel::Experimental;
    std::string summary;
    std::vector<std::string> test_names;
    std::vector<std::string> known_limitations;
};

struct ComponentDescriptor {
    ComponentId id;
    std::string display_name;
    ObjectCategory category = ObjectCategory::GeometricObject;
    std::vector<Capability> capabilities;
    std::vector<Capability> required_capabilities;
    std::vector<Assumption> assumptions;
    std::vector<ParameterSchema> parameters;
    std::vector<PortDescriptor> input_ports;
    std::vector<PortDescriptor> output_ports;
    TrustMetadata trust;
    DocumentationRef docs;
    bool factory_available = false;
};

struct EventDescriptor {
    EventTypeId id;
    std::string display_name;
    EventScope scope = EventScope::Simulation;
    DiagnosticSeverity default_severity = DiagnosticSeverity::Info;
    ComponentId producer = ids::unknown_component;
    DocumentationRef docs;
};

using ParameterValue = std::variant<bool, i64, f64, std::string, units::QuantityValue, ComponentId>;

struct ParameterAssignment {
    std::string key;
    ParameterValue value;
};

struct ComponentConfig {
    std::vector<ParameterAssignment> parameters;
};

struct ScenarioNode {
    RuntimeNodeId node_id;
    ComponentId component;
    ComponentConfig config;
};

struct ScenarioEdge {
    RuntimeNodeId from;
    std::string from_port;
    RuntimeNodeId to;
    std::string to_port;
};

struct ScenarioGraph {
    std::string name;
    std::vector<ScenarioNode> nodes;
    std::vector<ScenarioEdge> edges;
};

struct RuntimeSurface {
    ComponentId id;
    std::string display_name;
};

struct RuntimeField {
    ComponentId id;
    std::string display_name;
};

struct RuntimeSystem {
    ComponentId id;
    std::string display_name;
};

struct RuntimeSimulation {
    ComponentId id;
    std::string display_name;
};

struct RuntimeOpaque {
    ComponentId id;
    std::string display_name;
};

using RuntimeComponent = std::variant<RuntimeSurface, RuntimeField, RuntimeSystem, RuntimeSimulation, RuntimeOpaque>;

template <class T>
struct MetadataResult {
    T value{};
    ValidationReport report{};

    [[nodiscard]] bool ok() const noexcept { return report.ok(); }
};

} // namespace ndde
