#include "engine/diagnostics/DiagnosticsService.hpp"
#include "engine/SimulationHost.hpp"

#include <gtest/gtest.h>

namespace {

using namespace ndde;

DiagnosticReport metric_report(DiagnosticSeverity severity = DiagnosticSeverity::Error) {
    return DiagnosticReport{
        .severity = severity,
        .lifetime = DiagnosticLifetime::UntilResolved,
        .code = ErrorCode::NonPositiveMetric,
        .source = DiagnosticSource{
            .subsystem = DiagnosticSubsystem::Metric,
            .component = ids::field_metric_ripple,
            .node = RuntimeNodeId{7u},
            .location = "MetricService::metric"
        },
        .title = "Metric factor became non-positive",
        .message = "A conformal metric factor was sampled below zero.",
        .suggested_fix = "Reduce epsilon or amplitude.",
        .facts = {DiagnosticFact{.key = "metric_factor", .value = f64(-0.02)}}
    };
}

TEST(DiagnosticsService, ReportsActiveDiagnosticWithStableId) {
    DiagnosticsService diagnostics;

    const DiagnosticId id = diagnostics.report(metric_report(), f64(1.25));

    ASSERT_NE(id.value, 0u);
    ASSERT_EQ(diagnostics.active().size(), 1u);
    EXPECT_EQ(diagnostics.active().front().id, id);
    EXPECT_EQ(diagnostics.active().front().first_seen_seconds, f64(1.25));
    EXPECT_TRUE(diagnostics.has_errors());
    EXPECT_FALSE(diagnostics.has_fatal());
}

TEST(DiagnosticsService, DuplicateReportIncrementsOccurrenceCount) {
    DiagnosticsService diagnostics;

    const DiagnosticId first = diagnostics.report(metric_report(), f64(1));
    const DiagnosticId second = diagnostics.report(metric_report(), f64(2));

    EXPECT_EQ(first, second);
    ASSERT_EQ(diagnostics.active().size(), 1u);
    EXPECT_EQ(diagnostics.active().front().occurrence_count, 2u);
    EXPECT_EQ(diagnostics.active().front().first_seen_seconds, f64(1));
    EXPECT_EQ(diagnostics.active().front().last_seen_seconds, f64(2));
}

TEST(DiagnosticsService, ResolveMovesDiagnosticToHistory) {
    DiagnosticsService diagnostics;
    const DiagnosticId id = diagnostics.report(metric_report());

    diagnostics.resolve(id);

    EXPECT_TRUE(diagnostics.active().empty());
    ASSERT_EQ(diagnostics.history().size(), 1u);
    EXPECT_EQ(diagnostics.history().front().id, id);
    EXPECT_FALSE(diagnostics.history().front().active);
}

TEST(DiagnosticsService, FiltersByComponentNodeCodeAndSeverity) {
    DiagnosticsService diagnostics;
    (void)diagnostics.report(metric_report());
    (void)diagnostics.report(DiagnosticReport{
        .severity = DiagnosticSeverity::Warning,
        .lifetime = DiagnosticLifetime::Frame,
        .code = ErrorCode::DDEHistoryUnavailable,
        .source = DiagnosticSource{
            .subsystem = DiagnosticSubsystem::DDEHistory,
            .component = ComponentId{"dde.history"},
            .node = RuntimeNodeId{9u},
            .location = "DDEHistoryBuffer::delayed"
        },
        .title = "DDE history unavailable"
    });

    EXPECT_EQ(diagnostics.active_for(ids::field_metric_ripple).size(), 1u);
    EXPECT_EQ(diagnostics.active_for(RuntimeNodeId{9u}).size(), 1u);
    EXPECT_EQ(diagnostics.active_with(ErrorCode::NonPositiveMetric).size(), 1u);
    EXPECT_EQ(diagnostics.active_at_or_above(DiagnosticSeverity::Error).size(), 1u);
}

TEST(DiagnosticsService, ClearsFrameAndScenarioLifetimes) {
    DiagnosticsService diagnostics;
    (void)diagnostics.report(DiagnosticReport{
        .severity = DiagnosticSeverity::Warning,
        .lifetime = DiagnosticLifetime::Frame,
        .code = ErrorCode::ChartOutOfDomain,
        .title = "Hover chart sample failed"
    });
    (void)diagnostics.report(DiagnosticReport{
        .severity = DiagnosticSeverity::Error,
        .lifetime = DiagnosticLifetime::Scenario,
        .code = ErrorCode::SolverDiverged,
        .title = "Solver diverged"
    });
    (void)diagnostics.report(DiagnosticReport{
        .severity = DiagnosticSeverity::Error,
        .lifetime = DiagnosticLifetime::Application,
        .code = ErrorCode::ExternalDependencyUnavailable,
        .title = "Dependency missing"
    });

    diagnostics.clear_frame_diagnostics();
    ASSERT_EQ(diagnostics.active().size(), 2u);
    EXPECT_EQ(diagnostics.history().size(), 1u);

    diagnostics.clear_scenario_diagnostics();
    ASSERT_EQ(diagnostics.active().size(), 1u);
    EXPECT_EQ(diagnostics.active().front().lifetime, DiagnosticLifetime::Application);
    EXPECT_EQ(diagnostics.history().size(), 2u);
}

TEST(DiagnosticsService, ValidationReportCanBeIngested) {
    ValidationReport report;
    report.issues.push_back(ValidationIssue{
        .severity = DiagnosticSeverity::Error,
        .code = ErrorCode::MissingCapability,
        .source = DiagnosticSource{
            .subsystem = DiagnosticSubsystem::Metadata,
            .component = ComponentId{"behavior.metric_brownian"},
            .location = "SimMetadataService::validate_scenario"
        },
        .message = "Inverse metric tensor provider is missing.",
        .suggested_fix = "Connect a metric-capable surface."
    });

    EXPECT_FALSE(report.ok());
    EXPECT_TRUE(report.has_errors());

    DiagnosticsService diagnostics;
    diagnostics.report(report);

    ASSERT_EQ(diagnostics.active().size(), 1u);
    EXPECT_EQ(diagnostics.active().front().code, ErrorCode::MissingCapability);
    EXPECT_EQ(diagnostics.active().front().source.component->value, "behavior.metric_brownian");
}

TEST(EngineServices, OwnsDiagnosticsServiceAndPassesItToSimulationHost) {
    EngineServices services;
    (void)services.diagnostics().report(DiagnosticReport{
        .severity = DiagnosticSeverity::Info,
        .code = ErrorCode::Unknown,
        .title = "Service reachable"
    });

    SimulationHost host = services.simulation_host();

    EXPECT_EQ(services.diagnostics().active().size(), 1u);
    EXPECT_EQ(host.diagnostics().active().size(), 1u);
}

} // namespace
