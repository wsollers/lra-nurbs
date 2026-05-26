#pragma once
// engine/logging/LoggerTypes.hpp
// Engine-owned human-readable logging records and references.

#include "engine/RuntimeIds.hpp"
#include "engine/diagnostics/DiagnosticsTypes.hpp"
#include "engine/events/EventBusService.hpp"

#include <optional>
#include <string>

namespace ndde {

struct LogRecordId {
    u64 value = u64(0);

    friend constexpr bool operator==(LogRecordId, LogRecordId) noexcept = default;
};

enum class LogSeverity : u8 {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

enum class LogCategory : u8 {
    Engine,
    Simulation,
    Diagnostics,
    Capture,
    Telemetry,
    Metadata,
    Resource,
    Renderer,
    Worker
};

struct LogSourceRef {
    ComponentId component = ids::unknown_component;
    RuntimeNodeId node = {};
};

struct EventRef {
    EventChannelId channel = EventChannelId::App;
    EventTypeId type = {};
    u64 sequence = u64(0);
    u64 tick = u64(0);
    f32 sim_time = f32(0);
};

struct LoggerConfig {
    u64 max_records = u64(8192);
    u64 max_string_bytes = u64(4 * 1024 * 1024);
    bool write_file = false;
};

struct LogRecord {
    LogRecordId id = {};
    LogSeverity severity = LogSeverity::Info;
    LogCategory category = LogCategory::Engine;
    LogSourceRef source = {};
    std::optional<EventRef> event;
    std::optional<DiagnosticId> diagnostic;
    std::optional<ResourceId> resource;
    u64 tick = u64(0);
    f32 sim_time = f32(0);
    u32 message_size = u32(0);
};

struct LogSnapshotEntry {
    LogRecord record;
    std::string message;
};

} // namespace ndde
