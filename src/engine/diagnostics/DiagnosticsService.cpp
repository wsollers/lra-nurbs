#include "engine/diagnostics/DiagnosticsService.hpp"
#include "engine/threading/ThreadManagementService.hpp"

#include <algorithm>

namespace ndde {

bool ValidationReport::ok() const noexcept {
    return !has_errors();
}

bool ValidationReport::has_errors() const noexcept {
    return std::any_of(issues.begin(), issues.end(), [](const ValidationIssue& issue) {
        return issue.severity == DiagnosticSeverity::Error
            || issue.severity == DiagnosticSeverity::Fatal;
    });
}

bool ValidationReport::has_warnings() const noexcept {
    return std::any_of(issues.begin(), issues.end(), [](const ValidationIssue& issue) {
        return issue.severity == DiagnosticSeverity::Warning;
    });
}

void DiagnosticsService::set_thread_service(ThreadManagementService* threads,
                                            ThreadRole owner_role) noexcept {
    m_threads = threads;
    m_owner_role = owner_role;
}

DiagnosticId DiagnosticsService::report(DiagnosticReport report, f64 now_seconds) {
    if (!require_owner_thread("DiagnosticsService::report")) {
        return {};
    }
    if (Diagnostic* existing = find_duplicate(report)) {
        existing->last_seen_seconds = now_seconds;
        ++existing->occurrence_count;
        if (severity_rank(report.severity) > severity_rank(existing->severity))
            existing->severity = report.severity;
        if (!report.message.empty())
            existing->message = std::move(report.message);
        if (!report.suggested_fix.empty())
            existing->suggested_fix = std::move(report.suggested_fix);
        if (!report.facts.empty())
            existing->facts = std::move(report.facts);
        existing->active = true;
        return existing->id;
    }

    Diagnostic diagnostic{
        .id = next_id(),
        .severity = report.severity,
        .lifetime = report.lifetime,
        .code = report.code,
        .source = std::move(report.source),
        .title = std::move(report.title),
        .message = std::move(report.message),
        .suggested_fix = std::move(report.suggested_fix),
        .facts = std::move(report.facts),
        .first_seen_seconds = now_seconds,
        .last_seen_seconds = now_seconds
    };
    const DiagnosticId id = diagnostic.id;
    m_active.push_back(std::move(diagnostic));
    return id;
}

void DiagnosticsService::report(const ValidationReport& report_batch, f64 now_seconds) {
    if (!require_owner_thread("DiagnosticsService::report")) {
        return;
    }
    for (const ValidationIssue& issue : report_batch.issues) {
        (void)report(DiagnosticReport{
            .severity = issue.severity,
            .lifetime = DiagnosticLifetime::UntilResolved,
            .code = issue.code,
            .source = issue.source,
            .title = issue.message,
            .message = issue.message,
            .suggested_fix = issue.suggested_fix,
            .facts = issue.facts
        }, now_seconds);
    }
}

void DiagnosticsService::resolve(DiagnosticId id) {
    if (!require_owner_thread("DiagnosticsService::resolve")) {
        return;
    }
    auto it = std::find_if(m_active.begin(), m_active.end(), [id](const Diagnostic& diagnostic) {
        return diagnostic.id == id;
    });
    if (it != m_active.end())
        resolve_at(it);
}

void DiagnosticsService::resolve_for(ComponentId component) {
    if (!require_owner_thread("DiagnosticsService::resolve_for")) {
        return;
    }
    auto it = m_active.begin();
    while (it != m_active.end()) {
        if (it->source.component && *it->source.component == component) {
            it->active = false;
            m_history.push_back(std::move(*it));
            it = m_active.erase(it);
        } else {
            ++it;
        }
    }
}

void DiagnosticsService::acknowledge(DiagnosticId id) {
    if (!require_owner_thread("DiagnosticsService::acknowledge")) {
        return;
    }
    if (Diagnostic* diagnostic = find_active(id))
        diagnostic->acknowledged = true;
}

void DiagnosticsService::clear_frame_diagnostics() {
    if (!require_owner_thread("DiagnosticsService::clear_frame_diagnostics")) {
        return;
    }
    auto it = m_active.begin();
    while (it != m_active.end()) {
        if (it->lifetime == DiagnosticLifetime::Frame) {
            it->active = false;
            m_history.push_back(std::move(*it));
            it = m_active.erase(it);
        } else {
            ++it;
        }
    }
}

void DiagnosticsService::clear_scenario_diagnostics() {
    if (!require_owner_thread("DiagnosticsService::clear_scenario_diagnostics")) {
        return;
    }
    auto it = m_active.begin();
    while (it != m_active.end()) {
        if (it->lifetime == DiagnosticLifetime::Frame
            || it->lifetime == DiagnosticLifetime::Scenario) {
            it->active = false;
            m_history.push_back(std::move(*it));
            it = m_active.erase(it);
        } else {
            ++it;
        }
    }
}

void DiagnosticsService::clear_all() {
    if (!require_owner_thread("DiagnosticsService::clear_all")) {
        return;
    }
    for (Diagnostic& diagnostic : m_active) {
        diagnostic.active = false;
        m_history.push_back(std::move(diagnostic));
    }
    m_active.clear();
}

bool DiagnosticsService::require_owner_thread(std::string_view api_name) {
    return !m_threads || m_threads->require_thread_role(m_owner_role, api_name);
}

std::vector<Diagnostic> DiagnosticsService::active_for(ComponentId component) const {
    std::vector<Diagnostic> out;
    for (const Diagnostic& diagnostic : m_active) {
        if (diagnostic.source.component && *diagnostic.source.component == component)
            out.push_back(diagnostic);
    }
    return out;
}

std::vector<Diagnostic> DiagnosticsService::active_for(RuntimeNodeId node) const {
    std::vector<Diagnostic> out;
    for (const Diagnostic& diagnostic : m_active) {
        if (diagnostic.source.node && *diagnostic.source.node == node)
            out.push_back(diagnostic);
    }
    return out;
}

std::vector<Diagnostic> DiagnosticsService::active_with(ErrorCode code) const {
    std::vector<Diagnostic> out;
    for (const Diagnostic& diagnostic : m_active) {
        if (diagnostic.code == code)
            out.push_back(diagnostic);
    }
    return out;
}

std::vector<Diagnostic> DiagnosticsService::active_at_or_above(DiagnosticSeverity severity) const {
    std::vector<Diagnostic> out;
    const int minimum = severity_rank(severity);
    for (const Diagnostic& diagnostic : m_active) {
        if (severity_rank(diagnostic.severity) >= minimum)
            out.push_back(diagnostic);
    }
    return out;
}

bool DiagnosticsService::has_errors() const noexcept {
    return std::any_of(m_active.begin(), m_active.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::Error
            || diagnostic.severity == DiagnosticSeverity::Fatal;
    });
}

bool DiagnosticsService::has_fatal() const noexcept {
    return std::any_of(m_active.begin(), m_active.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::Fatal;
    });
}

Diagnostic* DiagnosticsService::find_active(DiagnosticId id) noexcept {
    auto it = std::find_if(m_active.begin(), m_active.end(), [id](const Diagnostic& diagnostic) {
        return diagnostic.id == id;
    });
    return it != m_active.end() ? &*it : nullptr;
}

const Diagnostic* DiagnosticsService::find_duplicate(const DiagnosticReport& report) const noexcept {
    auto it = std::find_if(m_active.begin(), m_active.end(), [&report](const Diagnostic& diagnostic) {
        return diagnostic.severity == report.severity
            && diagnostic.code == report.code
            && diagnostic.title == report.title
            && diagnostic.source.subsystem == report.source.subsystem
            && diagnostic.source.location == report.source.location
            && same_component(diagnostic.source.component, report.source.component)
            && same_node(diagnostic.source.node, report.source.node);
    });
    return it != m_active.end() ? &*it : nullptr;
}

Diagnostic* DiagnosticsService::find_duplicate(const DiagnosticReport& report) noexcept {
    return const_cast<Diagnostic*>(
        static_cast<const DiagnosticsService*>(this)->find_duplicate(report));
}

void DiagnosticsService::resolve_at(std::vector<Diagnostic>::iterator it) {
    it->active = false;
    m_history.push_back(std::move(*it));
    m_active.erase(it);
}

bool DiagnosticsService::same_component(std::optional<ComponentId> lhs,
                                        std::optional<ComponentId> rhs) noexcept {
    if (lhs.has_value() != rhs.has_value())
        return false;
    return !lhs || *lhs == *rhs;
}

bool DiagnosticsService::same_node(std::optional<RuntimeNodeId> lhs,
                                   std::optional<RuntimeNodeId> rhs) noexcept {
    if (lhs.has_value() != rhs.has_value())
        return false;
    return !lhs || *lhs == *rhs;
}

} // namespace ndde
