# nurbs_dde Architecture Evolution Contract

**Status:** Codex implementation guide  
**Scope:** Add the new mathematical laboratory architecture while preserving the working engine/runtime design.  
**Core decision:** Do **not** rewrite the engine. Extend the existing system through additive services, contracts, adapters, and phased migration.

---

# 0. Mission Statement

> **“A mathematically rigorous interactive laboratory for geometry, analysis, dynamics, and stochastic control on manifolds.”**

`nurbs_dde` should remain a working interactive engine while evolving into a unified mathematical runtime for:

- surfaces and manifolds,
- stochastic pursuit,
- Markov-modulated behavioral control,
- NURBS/spline paths,
- ODE/DDE systems,
- metric-aware diffusion,
- tangent/charts/curvature debugging,
- numerical analysis,
- differential geometry,
- and later exterior calculus / spectral analysis.

The renderer exists to expose mathematical structure.  
The simulation exists to evolve mathematical state.  
The architecture exists to keep these layers coherent.

---

# 1. Preserve Existing Architecture

The existing architecture is considered the base system.

## 1.1 Existing Rule to Preserve

```text
Engine owns lifecycle and services.
Simulation owns mathematical state.
Services own cross-cutting capabilities.
Renderer receives packets only.
Fields alter dynamics, metric, and visible geometry through one pipeline.
```

## 1.2 Existing Components to Keep

The following must be preserved and evolved, not deleted:

```text
engine::Engine
EngineServices
SimulationRuntime
ISimulation
SimulationHost
RenderService
PanelService
HotkeyService
CameraService
CameraInputController
ViewInputService
InteractionService
SimulationClock
MemoryService
math::ISurface
math::IDeformableSurface
simulation::IField
FieldCompositor
ParticleSystem
BehaviorStack
IParticleBehavior
ScenarioBuilder
SimulationRenderPackets
EventBus / EventLog
IDifferentialSystem
IDelayDifferentialSystem
EulerOdeSolver
Rk4OdeSolver
EulerDdeSolver
DelayHistoryView
```

## 1.3 Core Extension Strategy

Add the following services to `EngineServices`:

```text
SimMetadataService
TelemetryService
DiagnosticsService
QuantityDisplayService
EventBusService
LoggerService
ThreadManagementService
ResourceManagerService
MetricService
```

Then progressively adapt existing systems to register with and use these services.

---

# 2. Architectural Laws

Violations are bugs.

| # | Law | Statement |
|---|-----|-----------|
| 1 | Engine Owns Lifecycle | `Engine` and `EngineServices` own services and frame orchestration. Simulations never receive concrete engine access. |
| 2 | Simulation Owns Math State | Active mathematical state lives in `ISimulation` implementations and reusable math/sim components. |
| 3 | Renderer Receives Packets Only | Vulkan renderer consumes packets. It must not know simulation semantics. |
| 4 | Metadata Describes, Runtime Owns | `SimMetadataService` describes what can exist. Active simulations own what currently exists. |
| 5 | Fields Compose Through One Pipeline | Drift, diffusion, metric factor, and visible displacement flow through `IField` and `FieldCompositor`. |
| 6 | Panels Subscribe, Never Own Math | Panels display and command state; they must not be the only place where mathematical action is encoded. |
| 7 | Events Are Not Telemetry Or Logs | Events are compact discrete happenings. Telemetry is sampled measured state. Logging is formatted narrative text. |
| 8 | Diagnostics Are Centralized | All validation issues, math errors, solver failures, and trust warnings flow through `DiagnosticsService`. |
| 9 | DDE State Is a Function | Delay state is a history segment in `C([-τ,0],M)`, not just the point `x(t-τ)`. |
| 10 | Markov State Is First-Class | Behavioral state is a registered control component, not an enum hidden in the sim loop. |
| 11 | Units Are Strongly Typed | Runtime quantities use `mp-units` wherever practical. UI conversion happens through `QuantityDisplayService`. |
| 12 | Abstract Categories Must Converge | Components should progressively unify beneath `GeometricObject`, `FieldObject`, `OperatorObject`, `DynamicalObject`, `ControlObject`, `ObservableObject`. |
| 13 | Modern C++ by Default | Prefer C++20/23: concepts, `std::expected`, `std::optional`, `std::variant`, `std::span`, RAII, `std::jthread`. |
| 14 | Zero-Copy Where Feasible | Use zero-copy / lock-free / mailbox patterns when correctness and complexity permit. Do not over-engineer prematurely. |
| 15 | Workers Communicate Through Mailboxes | Worker threads do not mutate UI, Vulkan, or active simulation state directly. They report through bounded mailboxes and main-thread drains. |
| 16 | Resources Are IDs Across Boundaries | Large artifacts crossing thread/service/frame boundaries are referenced by `ResourceId`; storage remains in `MemoryService`, renderer, capture, or file owners. |

---

# 3. Modern C++ Requirements

## 3.1 Required Style

Use:

```cpp
std::expected<T, MathError>
std::optional<T>
std::variant<...>
std::span<T>
std::jthread
std::stop_token
concepts
RAII handles
strong type aliases
```

Avoid:

```cpp
raw owning pointers
stringly typed event names
simulation code reaching into Vulkan
panels owning unique mathematical logic
untyped unit doubles for physical quantities
exception-heavy mathematical failure paths
```

## 3.2 Error Contract

Mathematical failure should return:

```cpp
using MathResult<T> = std::expected<T, MathError>;
```

Exceptions are acceptable for unrecoverable programming errors, but not for expected math/runtime failures such as:

```text
solver divergence
singular matrix
chart out of domain
invalid knot vector
invalid Markov transition
missing capability
unit mismatch
```

---

# 4. mp-units Contract

`mp-units` is a foundational dependency.

## 4.1 Quantity Kinds

```cpp
enum class QuantityKind {
    Dimensionless,
    Time,
    Frequency,
    Length,
    Angle,
    Area,
    Curvature,
    Speed,
    Acceleration,
    Probability,
    TransitionRate,
    DwellTime,
    NURBSWeight,
    ParameterT,
    MetricFactor,
    Energy
};
```

## 4.2 JSON Wire Format

```json
{
  "value": 0.5,
  "unit": "Hz",
  "quantity_kind": "TransitionRate"
}
```

## 4.3 Runtime Rule

Internal runtime should prefer strongly typed quantities.

UI and JSON can use serialized values, but must pass through `QuantityDisplayService`.

---

# 5. Foundational Abstract Categories

To avoid ontology explosion, component descriptors must classify components under these root categories.

## 5.1 Root Categories

```cpp
enum class ObjectCategory {
    GeometricObject,
    FieldObject,
    OperatorObject,
    DynamicalObject,
    ControlObject,
    ObservableObject
};
```

## 5.2 Meaning

### GeometricObject

Mathematical objects with geometric structure.

Examples:

```text
Surface
Manifold
EmbeddedSurface
Chart
TangentBundle
NURBSCurve
SplinePath
```

### FieldObject

Objects defined over space/state.

Examples:

```text
MetricRipple
DampingField
PotentialField
VectorField
DiffusionField
ShadowConeField
CurvatureField
```

### OperatorObject

Mathematical transformations.

Examples:

```text
Derivative
Jacobian
Hessian
CovariantDerivative
LaplaceBeltrami
FourierTransform
ExteriorDerivative
```

### DynamicalObject

Systems that evolve state.

Examples:

```text
ODE system
DDE system
Particle system
SDE system
Hybrid system
N-body gravity
```

### ControlObject

Behavior and policy systems.

Examples:

```text
Pursue
Avoid
Orbit
Wander
BehaviorStack
MarkovController
StateController
Policy
```

### ObservableObject

Telemetry-producing or diagnostic objects.

Examples:

```text
CurvatureTelemetry
EnergyDiagnostic
MarkovTrace
SplineCurvatureStream
DDEGhostStream
```

---

# 6. Shared Identifier Types

Add stable typed IDs.

```cpp
struct ComponentId {
    std::string value;
};

struct RuntimeNodeId {
    std::uint64_t value;
};

struct StreamId {
    std::string value;
};

struct AgentId {
    std::uint32_t value;
};

struct SplineId {
    std::uint32_t value;
};

struct StateId {
    std::string value;
};

struct EventTypeId {
    std::string value;
};
```

IDs should be cheap to copy and comparable.

---

# 7. Capability / Assumption Model

## 7.1 Capabilities

```cpp
enum class Capability {
    // Geometry
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

    // Fields
    DriftContribution,
    DiffusionContribution,
    MetricContribution,
    SurfaceDisplacementContribution,

    // Dynamics
    ODESystem,
    DDESystem,
    DDEHistory,
    RandomStream,
    StochasticTransition,

    // Control
    ControlLaw,
    CompositeControlLaw,
    StateController,
    MarkovControlLaw,

    // Splines
    BezierCurve,
    BSplineCurve,
    NURBSCurve,
    ArcLengthParameterization,
    GeodesicSpline,

    // Operators
    DerivativeOperator,
    JacobianOperator,
    HessianOperator,
    LaplaceBeltramiOperator,

    // Observability
    TelemetryProducer,
    RenderPacketProducer,
    ReplaySerializable
};
```

## 7.2 Assumptions

```cpp
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
```

---

# 8. Component Descriptor Contract

## 8.1 Descriptor

```cpp
struct ComponentDescriptor {
    ComponentId id;
    std::string display_name;
    ObjectCategory category;

    std::vector<Capability> capabilities;
    std::vector<Capability> required_capabilities;
    std::vector<Assumption> assumptions;

    std::vector<ParameterSchema> parameters;
    std::vector<PortDescriptor> input_ports;
    std::vector<PortDescriptor> output_ports;

    TrustMetadata trust;
    DocumentationRef docs;
};
```

## 8.2 Parameter Schema

```cpp
enum class ParameterType {
    Bool,
    Int,
    Float,
    Vec2,
    Vec3,
    Matrix,
    String,
    Enum,
    Quantity,
    ComponentRef,
    JsonObject
};

enum class Mutability {
    Live,
    RequiresRecompute,
    RequiresComponentRebuild,
    RequiresScenarioRestart
};

struct ParameterSchema {
    std::string key;
    std::string display_name;
    ParameterType type;
    QuantityKind quantity_kind;
    Mutability mutability;
    std::optional<ParameterDomain> domain;
    std::optional<UiHint> ui_hint;
};
```

## 8.3 Ports

```cpp
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
    PortKind kind;
    std::string type_name;
    bool required = false;
};
```

---

# 9. New Service: SimMetadataService

## 9.1 Purpose

Semantic registry and factory service.

## 9.2 Interface

```cpp
class SimMetadataService {
public:
    bool register_component(ComponentDescriptor descriptor);
    bool register_factory(ComponentId id, RuntimeFactory factory);

    const ComponentDescriptor* get_descriptor(ComponentId id) const;

    std::vector<const ComponentDescriptor*> query_category(ObjectCategory category) const;
    std::vector<const ComponentDescriptor*> query_capability(Capability capability) const;

    ValidationReport validate_scenario(const ScenarioGraph& graph) const;

    MathResult<std::unique_ptr<RuntimeComponent>>
    create(ComponentId id, const Json& config) const;
};
```

## 9.3 Factory

```cpp
using RuntimeFactory =
    std::function<MathResult<std::unique_ptr<RuntimeComponent>>(const Json& config)>;
```

## 9.4 Integration Point

Add to:

```cpp
struct EngineServices {
    SimMetadataService metadata;
};
```

`ModuleService` may be added later, but initially registration can happen in `EngineServices` construction or `SceneFactories.cpp`.

---

# 10. New Service: DiagnosticsService

## 10.1 Purpose

Central sink for mathematical, runtime, threading, validation, and trust failures.

## 10.2 Data Types

```cpp
enum class DiagnosticSeverity {
    Info,
    Warning,
    Error,
    Fatal
};

enum class ErrorCode {
    MissingCapability,
    InvalidParameter,
    UnitMismatch,
    ChartOutOfDomain,
    SolverDiverged,
    SingularMatrix,
    KnotVectorInvalid,
    ContinuityViolation,
    MarkovTransitionInvalid,
    AbsorbingStateUnreachable,
    GeneratorRowsDoNotSumToZero,
    NegativeTransitionRate,
    ThreadFault,
    ReplaySeedMissing
};

struct MathError {
    ErrorCode code;
    DiagnosticSeverity severity;
    std::string message;
    std::optional<ComponentId> component;
    std::optional<RuntimeNodeId> node;
    std::string suggested_fix;
};

struct ValidationIssue {
    DiagnosticSeverity severity;
    ErrorCode code;
    std::string message;
    std::string suggested_fix;
    std::optional<ComponentId> component;
};

struct ValidationReport {
    std::vector<ValidationIssue> issues;

    bool ok() const;
    bool has_errors() const;
};
```

## 10.3 Interface

```cpp
class DiagnosticsService {
public:
    void report(MathError error);
    void report(ValidationReport report);

    std::span<const MathError> active_errors() const;
    std::vector<MathError> errors_for(ComponentId id) const;

    void clear_frame_warnings();
};
```

---

# 11. New Service: EventBusService

## 11.1 Purpose

Typed discrete event routing for compact runtime happenings.

`EventBusService` is owned by `EngineServices`. It owns app, scenario,
simulation, UI, and worker channels. Events are not telemetry and not logs:
events answer "what happened", telemetry answers "what was sampled", and
`LoggerService` owns formatted narrative text.

## 11.2 Event Examples

```cpp
struct ScenarioLoadedEvent {
    RuntimeNodeId scenario = {};
    RuntimeNodeId root_node = {};
};

struct CaptureEvent {
    u64 pursuer_id = u64(0);
    u64 prey_id = u64(0);
    f32 sim_time = f32(0);
    u64 tick = u64(0);
};

struct MarkovTransitionEvent {
    u64 agent_id = u64(0);
    RuntimeNodeId from_state = {};
    RuntimeNodeId to_state = {};
    RuntimeNodeId trigger = {};
    f32 sim_time = f32(0);
    u64 tick = u64(0);
};

struct MarkovDwellEvent {
    u64 agent_id = u64(0);
    RuntimeNodeId state = {};
    f32 dwell_seconds = f32(0);
    f32 sim_time = f32(0);
    u64 tick = u64(0);
};

struct SplineKnotChangedEvent {
    RuntimeNodeId spline = {};
    u32 knot_index = u32(0);
    f64 new_value = f64(0);
};

struct ThreadFaultEvent {
    RuntimeNodeId worker = {};
    DiagnosticId diagnostic = {};
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
};
```

## 11.3 Interface

```cpp
class EventBusService {
public:
    template<class Event>
    Subscription subscribe(EventChannelId channel,
                           std::function<void(const Event&)> handler);

    template<class Event>
    u64 publish(EventChannelId channel, const Event& event);

    bool enqueue_worker_record(events::EventRecord record);
    u64 drain_worker_mailbox();
};
```

## 11.4 Contract

- Events are discrete happenings.
- Do not use event bus for high-frequency telemetry samples.
- Events must be typed structs.
- Events carry compact IDs/scalars/enums, not strings, paths, blobs, or
  formatted text.
- `EventBusService` assigns monotonic per-channel sequence numbers.
- Worker threads submit compact records through the worker mailbox.
- `SimMetadataService` registers `EventDescriptor` values for discoverability.
- Text formatting belongs to `LoggerService` or compact `EventLog` display
  adapters, not to typed event payloads.

---

# 12. New Service: TelemetryService

## 12.1 Purpose

Named stream-based sampled data routing.

## 12.2 Stream Descriptor

```cpp
enum class StreamRate {
    PerTick,
    PerFrame,
    OnChange,
    Manual
};

struct TelemetryStreamDescriptor {
    StreamId id;
    std::string display_name;
    QuantityKind quantity_kind;
    std::string value_type_name;
    StreamRate rate;
    std::optional<RuntimeNodeId> owner;
};
```

## 12.3 Sample

```cpp
template<class T>
struct TelemetrySample {
    double sim_time_seconds;
    T value;
};
```

## 12.4 Interface

```cpp
class TelemetryService {
public:
    void register_stream(TelemetryStreamDescriptor descriptor);

    template<class T>
    void publish(StreamId id, double sim_time_seconds, const T& value);

    template<class T>
    std::optional<TelemetrySample<T>> latest(StreamId id) const;

    template<class T>
    std::vector<TelemetrySample<T>> query(StreamId id, TimeRange range) const;
};
```

## 12.5 Required Initial Streams

```text
sim.time
agent.N.position
agent.N.velocity
agent.N.markov.state_id
agent.N.markov.dwell_time
agent.N.metric.factor
agent.N.curvature.K
dde.N.ghost_position
spline.N.t_param
spline.N.s_param
spline.N.curvature_k
```

---

# 13. New Service: QuantityDisplayService

## 13.1 Purpose

Converts and formats `mp-units` quantities for UI, JSON, telemetry, and parameter editing.

## 13.2 Interface

```cpp
struct QuantityValue {
    double value;
    std::string unit;
    QuantityKind kind;
};

class QuantityDisplayService {
public:
    std::string format(QuantityValue value) const;

    MathResult<QuantityValue>
    parse(std::string_view text, QuantityKind expected_kind) const;

    MathResult<QuantityValue>
    convert(QuantityValue value, std::string_view target_unit) const;
};
```

---

# 14. New Service: MetricService

## 14.1 Purpose

Shared metric-aware geometric computation.

This is the bridge between the current surface system and future manifold support.

## 14.2 Core Types

```cpp
struct ChartPoint {
    double u;
    double v;
};

struct TangentVector {
    ChartPoint base;
    glm::dvec2 components;
};

struct MetricTensor2 {
    double g00;
    double g01;
    double g10;
    double g11;
};

struct Christoffel2 {
    double gamma[2][2][2];
};
```

## 14.3 Interface

```cpp
class MetricService {
public:
    MathResult<MetricTensor2>
    metric(const math::ISurface& surface, ChartPoint p, double t) const;

    MathResult<MetricTensor2>
    inverse_metric(const math::ISurface& surface, ChartPoint p, double t) const;

    MathResult<Christoffel2>
    christoffel(const math::ISurface& surface, ChartPoint p, double t) const;

    MathResult<double>
    gaussian_curvature(const math::ISurface& surface, ChartPoint p, double t) const;

    MathResult<glm::dvec2>
    shortest_periodic_delta(const math::ISurface& surface, ChartPoint from, ChartPoint to) const;

    MathResult<double>
    distance_approx(const math::ISurface& surface, ChartPoint from, ChartPoint to, double t) const;

    MathResult<TangentVector>
    log_approx(const math::ISurface& surface, ChartPoint from, ChartPoint to, double t) const;
};
```

## 14.4 Immediate Adapter

Initially implement `MetricService` using existing `math::ISurface` derivative helpers.

Later, generalize to `IManifold`.

---

# 15. Surface / Manifold Evolution

## 15.1 Current

```cpp
math::ISurface
math::IDeformableSurface
```

## 15.2 Target

Do not remove `ISurface`.

Add:

```cpp
class IManifold;
class IEmbeddedSurface : public IManifold;
```

Then adapt:

```cpp
math::ISurface -> IEmbeddedSurface adapter
```

## 15.3 IManifold Interface

```cpp
class IManifold {
public:
    virtual ~IManifold() = default;

    virtual int dimension() const = 0;
    virtual std::span<const ChartDescriptor> charts() const = 0;

    virtual MathResult<MetricTensor>
    metric(const ChartPointN& p, ChartId chart, double t) const = 0;

    virtual MathResult<MetricTensor>
    inverse_metric(const ChartPointN& p, ChartId chart, double t) const = 0;

    virtual MathResult<TangentVectorN>
    log_map(const ChartPointN& from, const ChartPointN& to, ChartId chart, double t) const = 0;

    virtual MathResult<ChartPointN>
    exp_map(const ChartPointN& from, const TangentVectorN& v, ChartId chart, double t) const = 0;
};
```

## 15.4 IEmbeddedSurface Interface

```cpp
class IEmbeddedSurface : public IManifold {
public:
    virtual glm::dvec3 evaluate(ChartPoint p, double t) const = 0;
    virtual glm::dvec3 du(ChartPoint p, double t) const = 0;
    virtual glm::dvec3 dv(ChartPoint p, double t) const = 0;
    virtual glm::dvec3 normal(ChartPoint p, double t) const = 0;
};
```

---

# 16. Field Pipeline Evolution

## 16.1 Existing

```text
IField contributes:
- drift
- diffusion scaling
- conformal metric factor
- rendered displacement
```

## 16.2 Required Interface

```cpp
class IField {
public:
    virtual ~IField() = default;

    virtual std::string_view name() const = 0;

    virtual glm::dvec2 drift_contribution(const FieldSampleContext& ctx) const;
    virtual glm::dvec2 diffusion_contribution(const FieldSampleContext& ctx) const;
    virtual double metric_factor_contribution(const FieldSampleContext& ctx) const;
    virtual double surface_displacement_contribution(const FieldSampleContext& ctx) const;

    virtual bool expired(double sim_time_seconds) const;
};
```

## 16.3 Required Additions

Add first-class:

```text
PotentialField
VectorField
DiffusionField
ShadowConeField
CurvatureField
```

## 16.4 Immediate Fix

`MetricRipple::diffusion_contribution()` must be implemented so Brownian noise responds to the conformal metric.

---

# 17. Particle / Behavior / Control Evolution

## 17.1 Existing

```text
ParticleSystem
BehaviorStack
IParticleBehavior
```

## 17.2 Target

```text
IParticleBehavior becomes compatible with IControlLaw.
BehaviorStack becomes CompositeControlLaw.
MarkovController becomes StateController<IControlLaw>.
```

## 17.3 Control Interface

```cpp
struct ControlContext {
    AgentId agent;
    double sim_time_seconds;
    const math::ISurface* surface;
    const FieldCompositor* fields;
    const MetricService* metric;
    const DDEHistoryBuffer* history;
    const TelemetryService* telemetry;
};

struct ControlOutput {
    glm::dvec2 parameter_velocity;
    std::optional<TangentVector> tangent_control;
};

class IControlLaw {
public:
    virtual ~IControlLaw() = default;
    virtual std::string_view name() const = 0;

    virtual MathResult<ControlOutput>
    compute_control(const ParticleState& particle, const ControlContext& ctx) = 0;
};
```

## 17.4 Adapter

```cpp
class ParticleBehaviorControlLawAdapter : public IControlLaw {
public:
    explicit ParticleBehaviorControlLawAdapter(std::shared_ptr<IParticleBehavior> behavior);
};
```

This allows current behaviors to be reused without rewrite.

---

# 18. Markov Control System

## 18.1 Purpose

A Markov controller switches between named control laws.

## 18.2 Types

```cpp
struct StateDescriptor {
    StateId id;
    std::string display_name;
    glm::vec4 color;
    ComponentId control_law_component;
    std::vector<Assumption> assumptions;
};

struct CTMCRate {
    double lambda_hz;
};

struct ConditionPredicate {
    std::string name;
    std::function<bool(const SimulationStateView&)> predicate;
    std::optional<double> hysteresis_band;
};

struct HybridTrigger {
    CTMCRate base_rate;
    ConditionPredicate gate;
};

using TransitionTrigger =
    std::variant<CTMCRate, ConditionPredicate, HybridTrigger>;

struct TransitionEdge {
    StateId from;
    StateId to;
    TransitionTrigger trigger;
};
```

## 18.3 Markov Graph

```cpp
struct MarkovGraph {
    std::vector<StateDescriptor> states;
    std::vector<TransitionEdge> edges;
};
```

## 18.4 Markov Controller

```cpp
class MarkovController : public IControlLaw {
public:
    MarkovController(MarkovGraph graph, StateId initial_state);

    MathResult<ControlOutput>
    compute_control(const ParticleState& particle, const ControlContext& ctx) override;

    MathResult<void>
    force_transition(StateId to_state, std::string_view reason);

    StateId current_state() const;
    double dwell_time_seconds() const;
};
```

## 18.5 State Controller for Collections

The architecture must support attaching a state controller to a collection of Markov-controlled agents.

```cpp
class MarkovCollectionController {
public:
    void attach_agent(AgentId agent, MarkovController controller);
    void detach_agent(AgentId agent);

    MarkovController* controller_for(AgentId agent);
    std::span<const AgentId> controlled_agents() const;

    void tick_all(double sim_time_seconds, const SimulationStateView& view);
};
```

## 18.6 Matrix Construction

Support CTMC generator matrix construction.

```cpp
struct MarkovMatrixSpec {
    std::vector<StateId> states;
    std::vector<std::vector<double>> Q;
    StateId initial_state;
};
```

Validation rules:

```text
Q is square.
Q rows match states.
Q[i][j] >= 0 for i != j.
Q[i][i] <= 0.
sum_j Q[i][j] == 0 within tolerance.
```

Builder:

```cpp
MarkovGraph from_generator_matrix(const MarkovMatrixSpec& spec);
```

## 18.7 Fluent Builder

```cpp
auto markov =
    MarkovBuilder()
        .state("Wander", wanderLaw)
        .state("Pursue", pursueLaw)
        .rate("Wander", "Pursue", 0.25_Hz)
        .condition("Pursue", "Freeze", capturePredicate)
        .initial("Wander")
        .build();
```

Matrix:

```cpp
auto markov =
    MarkovBuilder()
        .states({"Wander", "Pursue", "Freeze"})
        .generator_matrix(Q)
        .control_laws({wanderLaw, pursueLaw, freezeLaw})
        .initial("Wander")
        .build();
```

## 18.8 Required Events

```cpp
MarkovTransitionEvent
MarkovDwellEvent
MarkovInvalidTransitionEvent
```

## 18.9 Required Telemetry

```text
agent.N.markov.state_id
agent.N.markov.dwell_time
agent.N.markov.transition_rate
agent.N.markov.stationary_probability
```

---

# 19. ScenarioBuilder Evolution

## 19.1 Existing Purpose

`ScenarioBuilder` remains the declarative construction path for agent scenarios.

## 19.2 New API

```cpp
class ScenarioBuilder {
public:
    ScenarioBuilder& name(std::string name);

    ScenarioBuilder& surface(std::shared_ptr<math::ISurface> surface);

    ScenarioBuilder& add_agent(AgentSpec spec);

    ScenarioBuilder& add_field(std::shared_ptr<IField> field);

    ScenarioBuilder& with_markov_controller(AgentId agent, MarkovGraph graph);

    ScenarioBuilder& with_markov_matrix(AgentId agent, MarkovMatrixSpec spec);

    ScenarioBuilder& with_state_controller(
        std::vector<AgentId> agents,
        std::shared_ptr<MarkovCollectionController> controller);

    ScenarioBuilder& with_nurbs_path(AgentId agent, NURBSCurveSpec spec);

    ScenarioBuilder& with_dde_history(AgentId agent, DDEHistorySpec spec);

    ScenarioBuilder& enable_telemetry(StreamSelection selection);

    MathResult<Scenario> build();
};
```

## 19.3 Contract

- Existing agent construction must continue to work.
- New Markov support should be optional.
- No existing simulation should break if it does not use Markov.
- Matrix-based Markov construction should call the same validation path as graph-based construction.

---

# 20. NURBS / Spline Architecture

## 20.1 Purpose

NURBS are foundational, not rendering decoration.

They support:

```text
agent paths
prey paths
delay ghosts
curvature analysis
arc-length traversal
geometry debugging
Markov-triggered path behavior
```

## 20.2 Core Interface

```cpp
class INURBSCurve {
public:
    virtual ~INURBSCurve() = default;

    virtual glm::dvec3 evaluate(double t) const = 0;
    virtual glm::dvec3 tangent(double t) const = 0;
    virtual double curvature(double t) const = 0;

    virtual MathResult<double> arc_length(double a, double b) const = 0;
    virtual MathResult<double> parameter_at_arc_length(double s) const = 0;
};
```

## 20.3 Spec

```cpp
struct NURBSCurveSpec {
    int degree;
    std::vector<glm::dvec3> control_points;
    std::vector<double> weights;
    std::vector<double> knot_vector;
};
```

## 20.4 Validation

```text
degree >= 1
control_points.size() == weights.size()
weights > 0
knot_vector nondecreasing
knot_vector.size() == control_points.size() + degree + 1
```

## 20.5 Builder

```cpp
auto curve =
    NURBSBuilder()
        .degree(3)
        .control_points(points)
        .weights(weights)
        .knot_vector(knots)
        .build();
```

## 20.6 Required Events

```cpp
SplineControlPointMovedEvent
SplineKnotChangedEvent
SplineReparameterizedEvent
ParticleTraversalEvent
```

## 20.7 Required Telemetry

```text
spline.N.t_param
spline.N.s_param
spline.N.position
spline.N.tangent
spline.N.curvature_k
```

---

# 21. DDE History Architecture

## 21.1 Rule

```text
DDE state is a function segment in C([-τ,0],M), not a finite point.
```

## 21.2 History Buffer

```cpp
struct HistorySample {
    double sim_time_seconds;
    ChartPoint position;
    glm::dvec2 velocity;
    std::optional<StateId> markov_state;
};

class DDEHistoryBuffer {
public:
    void push(HistorySample sample);

    MathResult<HistorySample>
    sample_at(double sim_time_seconds) const;

    MathResult<HistorySample>
    delayed(double current_time_seconds, double tau_seconds) const;

    std::vector<HistorySample>
    segment(double current_time_seconds, double tau_seconds) const;
};
```

## 21.3 Required Uses

- DDE pursuit,
- delayed prey ghosts,
- Markov delayed transition predicates,
- replay,
- DDE panel visualization.

---

# 22. Render Packet Extensions

## 22.1 Renderer Rule

Renderer remains simulation-neutral.

## 22.2 RenderSnapshot

Add optional overlays.

```cpp
struct MarkovStateOverlay {
    AgentId agent;
    StateId state;
    glm::vec4 color;
    std::string label;
};

struct SplineOverlay {
    SplineId spline;
    std::vector<glm::vec3> polyline;
    std::vector<glm::vec3> control_polygon;
};

struct DDEGhostOverlay {
    AgentId agent;
    std::vector<glm::vec3> ghost_curve;
};

struct RenderSnapshot {
    std::uint64_t frame_index;
    double sim_time_seconds;

    std::vector<ParticleRenderPacket> particles;
    std::vector<SplineOverlay> spline_overlays;
    std::vector<MarkovStateOverlay> markov_state_overlays;
    std::vector<DDEGhostOverlay> dde_ghost_overlays;
};
```

Do not make Vulkan aware of Markov semantics. It only receives label/color geometry.

---

# 23. Panel Requirements

## 23.1 Metadata Registry Panel

Shows:

```text
registered components
categories
capabilities
assumptions
parameters
factory availability
```

## 23.2 Metric Panel

Shows:

```text
metric tensor
inverse metric
curvature
metric factor
distance approximation
```

## 23.3 Markov Panel

Shows:

```text
current state
dwell time
transition graph
generator matrix Q
stationary distribution π
recent transitions
```

## 23.4 NURBS Panel

Shows:

```text
degree
control points
weights
knots
continuity
curvature
arc length
```

## 23.5 DDE Panel

Shows:

```text
history segment
ghost position
delay τ
state labels along ghost path
```

## 23.6 Diagnostics Panel

Shows:

```text
active errors
warnings
validation issues
suggested fixes
```

---

# 24. New Service: ThreadManagementService

Detailed design: `docs/THREAD_MANAGEMENT_SERVICE.md`.

## 24.1 Purpose

Engine-owned application threading: GUI/main, simulation, render, logger/I/O,
worker lifecycle, job submission, cancellation, shutdown, and cross-thread
communication.

`ThreadManagementService` should be added to `EngineServices` soon. The goal is
not only background jobs; the goal is to split the app into clear owner threads
before the runtime grows around single-thread assumptions.

```cpp
class EngineServices {
public:
    ThreadManagementService& threads() noexcept;
};
```

## 24.2 Target Layout

```text
UI thread
Simulation thread
Renderer thread
Worker pool
Telemetry/file I/O worker
Logger/I/O thread
```

The current runtime is mostly main-threaded. This target layout is a phased
future state, not permission for workers to access arbitrary services.

## 24.3 Queues And Mailboxes

```cpp
template<class T>
class ConcurrentQueue;

template<class T>
class LatestValueMailbox;
```

Current concrete mailbox:

```text
worker thread -> EventBusService::enqueue_worker_record(EventRecord)
main thread   -> EventBusService::drain_worker_mailbox()
```

## 24.4 Contracts

- UI sends commands to simulation via queue.
- Simulation publishes latest render snapshot via mailbox.
- Renderer consumes latest snapshot and may discard stale frames.
- Event delivery crossing threads must be safe.
- Shutdown uses `std::jthread` and `std::stop_token`.
- Logger/I/O drains log, telemetry, capture, and file sinks outside the frame
  hot path.
- Workers do not call ImGui, GLFW, Vulkan swapchain/presentation, or mutable
  simulation APIs directly.
- Worker failures are reported through `DiagnosticsService`.
- Worker narrative text is routed through `LoggerService`, not event payloads.
- Compact worker happenings are routed through `EventBusService` worker mailbox.

---

# 25. New Service: ResourceManagerService

Detailed design: `docs/RESOURCE_MANAGER_SERVICE.md`.

## 25.1 Purpose

Addressable resource identity, lookup, ownership transfer, and lifecycle
tracking for large artifacts that cross thread, frame, or service boundaries.

`ResourceManagerService` does not replace `MemoryService`, arenas, or
`BufferManager`. It records what an artifact is, who owns it, which lifetime it
belongs to, and how another service can refer to it by `ResourceId`.

```cpp
class EngineServices {
public:
    ResourceManagerService& resources() noexcept;
};
```

## 25.2 Contract

- Use `MemoryService` scopes and existing buffer managers for storage.
- Register artifacts that need cross-thread or cross-service identity.
- Support constexpr built-in handles for known engine resources.
- Support path-backed registration and lazy loading.
- Allocate runtime handles from a handle pool with generation/stale checks.
- Pass `ResourceId` through events, logs, diagnostics, and thread results.
- Do not pass large buffers, file paths, or raw pointers through event payloads.
- Renderer-owned Vulkan resources remain render-thread owned.
- Capture artifacts may map `CaptureArtifactId` to `ResourceId`.

---

# 26. Replay Contract

## 26.1 Required Replay Data

```cpp
struct ReplaySession {
    Json scenario_json;
    std::uint64_t rng_seed;
    std::vector<EventRecord> events;
    std::vector<MarkovTransitionEvent> markov_transitions;
    std::vector<SplineEditRecord> spline_edits;
    std::vector<TelemetryRecord> telemetry;
};
```

## 25.2 Required First Version

Minimum viable replay:

```text
scenario JSON
RNG seed
Markov transition log
spline edit log
```

---

# 26. Migration Plan

## Phase A — Add Services Without Behavior Change

- Add `SimMetadataService`.
- Add `DiagnosticsService`.
- Add `TelemetryService`.
- Add `QuantityDisplayService`.
- Add `MetricService`.
- Register current `ISurface`, `IField`, and behavior types as descriptors.
- No existing simulation behavior should change.

## Phase B — Wire Telemetry and Diagnostics

- Populate current telemetry streams.
- Replace placeholder metric/curvature telemetry defaults.
- Route validation issues through diagnostics.
- Add diagnostics panel.

## Phase C — Add Control Abstraction

- Add `IControlLaw`.
- Add adapter from `IParticleBehavior`.
- Make `BehaviorStack` implement or wrap as `CompositeControlLaw`.
- Add MarkovController.
- Add Markov collection controller.

## Phase D — Extend ScenarioBuilder

- Add `.with_markov_controller`.
- Add `.with_markov_matrix`.
- Add `.with_state_controller`.
- Add `.with_nurbs_path`.
- Add `.with_dde_history`.

## Phase E — Add NURBS

- Add NURBS spec.
- Add NURBS builder.
- Add validation.
- Add traversal telemetry.
- Add render overlays.

## Phase F — Promote Metric Geometry

- Move Brownian diffusion through `MetricService`.
- Move pursuit through `MetricService`.
- Add geodesic approximation.
- Add Lyapunov telemetry.

## Phase G — Add Manifold Interfaces

- Add `IManifold`.
- Add `IEmbeddedSurface`.
- Adapt existing `ISurface`.
- Begin using manifold vocabulary in new code.

---

# 27. Acceptance Tests

## 27.1 No Regression

Existing active simulation still runs.

## 27.2 Metadata

Registry panel shows:

```text
current active surface
current fields
current behaviors
current simulation
```

## 27.3 Telemetry

Telemetry panel shows live:

```text
sim.time
agent position
curvature K
metric factor
```

## 27.4 Markov

A simple agent can switch:

```text
Wander -> Pursue -> Freeze
```

via both:

```text
graph builder
matrix builder
```

## 27.5 DDE

DDE ghost can be sampled at:

```text
t - τ
```

and rendered.

## 27.6 NURBS

A particle traverses a NURBS path and publishes:

```text
t parameter
arc-length parameter
curvature
```

---

# 28. Final Notes for Codex

Do not implement everything at once.

The first implementation goal is not feature completeness.

The first goal is architectural insertion:

```text
new services exist
current systems register
telemetry flows
diagnostics flow
Markov control can be attached
ScenarioBuilder can build old and new scenarios
```

After that, add mathematical depth incrementally.
