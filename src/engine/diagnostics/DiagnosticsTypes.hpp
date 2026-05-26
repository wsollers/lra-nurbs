#pragma once
// engine/diagnostics/DiagnosticsTypes.hpp
// Structured diagnostic records for correctness, validation, and trust issues.

#include "engine/RuntimeIds.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ndde {

struct DiagnosticId {
    u64 value = u64(0);

    friend constexpr bool operator==(DiagnosticId, DiagnosticId) noexcept = default;
};

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error,
    Fatal
};

enum class DiagnosticLifetime {
    Frame,
    UntilResolved,
    Scenario,
    Application
};

enum class ErrorCode {
    Unknown,

    DuplicateComponentId,
    MissingDescriptor,
    MissingFactory,
    MissingCapability,
    InvalidParameter,
    UnitMismatch,

    ChartOutOfDomain,
    SingularMetric,
    NonPositiveMetric,
    SingularMatrix,
    NoCutLocusCrossingViolated,

    SolverDiverged,
    StepSizeTooLarge,
    DDEHistoryUnavailable,
    DDEHistoryWindowTooShort,

    KnotVectorInvalid,
    NURBSWeightsInvalid,
    ContinuityViolation,

    MarkovTransitionInvalid,
    GeneratorRowsDoNotSumToZero,
    NegativeTransitionRate,
    AbsorbingStateUnreachable,

    ReplaySeedMissing,
    MissingCapabilityForTelemetry,
    ThreadFault,
    ThreadRoleViolation,
    ExternalDependencyUnavailable
};

enum class DiagnosticSubsystem {
    Engine,
    Metadata,
    Scenario,
    Metric,
    Field,
    Particle,
    Solver,
    DDEHistory,
    Markov,
    NURBS,
    Telemetry,
    Renderer,
    Threading,
    Replay,
    Unknown
};

struct DiagnosticSource {
    DiagnosticSubsystem subsystem = DiagnosticSubsystem::Unknown;
    std::optional<ComponentId> component;
    std::optional<RuntimeNodeId> node;
    std::string location;
};

using DiagnosticValue = std::variant<bool, i64, f64, std::string>;

struct DiagnosticFact {
    std::string key;
    DiagnosticValue value;
};

struct DiagnosticReport {
    DiagnosticSeverity severity = DiagnosticSeverity::Warning;
    DiagnosticLifetime lifetime = DiagnosticLifetime::UntilResolved;
    ErrorCode code = ErrorCode::Unknown;
    DiagnosticSource source;
    std::string title;
    std::string message;
    std::string suggested_fix;
    std::vector<DiagnosticFact> facts;
};

struct Diagnostic {
    DiagnosticId id;
    DiagnosticSeverity severity = DiagnosticSeverity::Warning;
    DiagnosticLifetime lifetime = DiagnosticLifetime::UntilResolved;
    ErrorCode code = ErrorCode::Unknown;
    DiagnosticSource source;
    std::string title;
    std::string message;
    std::string suggested_fix;
    std::vector<DiagnosticFact> facts;
    f64 first_seen_seconds = f64(0);
    f64 last_seen_seconds = f64(0);
    u64 occurrence_count = u64(1);
    bool active = true;
    bool acknowledged = false;
};

struct ValidationIssue {
    DiagnosticSeverity severity = DiagnosticSeverity::Warning;
    ErrorCode code = ErrorCode::Unknown;
    DiagnosticSource source;
    std::string message;
    std::string suggested_fix;
    std::vector<DiagnosticFact> facts;
};

struct ValidationReport {
    std::vector<ValidationIssue> issues;

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] bool has_errors() const noexcept;
    [[nodiscard]] bool has_warnings() const noexcept;
};

} // namespace ndde
