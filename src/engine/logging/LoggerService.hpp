#pragma once
// engine/logging/LoggerService.hpp
// Engine-owned human-readable logging service.

#include "engine/logging/LoggerTypes.hpp"

#include <span>
#include <string_view>
#include <functional>
#include <mutex>
#include <vector>

namespace ndde {

class LoggerService {
public:
    LoggerService() = default;
    ~LoggerService() = default;

    LoggerService(const LoggerService&) = delete;
    LoggerService& operator=(const LoggerService&) = delete;
    LoggerService(LoggerService&&) = delete;
    LoggerService& operator=(LoggerService&&) = delete;

    void set_owner_guard(std::function<bool(std::string_view)> guard);

    void init(LoggerConfig config = {});
    void shutdown() noexcept;

    [[nodiscard]] LogRecordId write(LogSeverity severity,
                                    LogCategory category,
                                    LogSourceRef source,
                                    std::string_view message);

    [[nodiscard]] LogRecordId write_event(EventRef event,
                                          LogSeverity severity,
                                          LogCategory category,
                                          std::string_view message);

    [[nodiscard]] LogRecordId write_diagnostic(DiagnosticId diagnostic,
                                               LogSeverity severity,
                                               std::string_view message);

    [[nodiscard]] LogRecordId write_resource(ResourceId resource,
                                             LogSeverity severity,
                                             std::string_view message);

    [[nodiscard]] std::span<const LogRecord> records() const noexcept;
    [[nodiscard]] std::vector<LogSnapshotEntry> snapshot() const;
    [[nodiscard]] std::string_view message(LogRecordId id) const noexcept;
    [[nodiscard]] u64 dropped_records() const noexcept { return m_dropped_records; }
    [[nodiscard]] u64 dropped_string_bytes() const noexcept { return m_dropped_string_bytes; }
    [[nodiscard]] u64 string_bytes() const noexcept { return m_string_bytes; }
    [[nodiscard]] bool initialised() const noexcept { return m_initialised; }

    [[nodiscard]] std::vector<LogRecord> records_at_or_above(LogSeverity severity) const;
    [[nodiscard]] std::vector<LogRecord> records_in_category(LogCategory category) const;

    void clear();
    void drain_sinks();

private:
    struct StoredRecord {
        LogRecord record;
        std::string message;
    };

    LoggerConfig m_config;
    std::vector<StoredRecord> m_store;
    std::vector<LogRecord> m_record_view;
    std::function<bool(std::string_view)> m_owner_guard;
    mutable std::mutex m_mutex;
    u64 m_next_id = u64(1);
    u64 m_string_bytes = u64(0);
    u64 m_dropped_records = u64(0);
    u64 m_dropped_string_bytes = u64(0);
    bool m_initialised = false;

    [[nodiscard]] LogRecordId append(LogRecord record, std::string_view message);
    [[nodiscard]] bool require_owner_thread(std::string_view api_name) const;
    void enforce_capacity();
    void evict_oldest() noexcept;
    void rebuild_record_view();
    [[nodiscard]] static bool severity_at_or_above(LogSeverity value, LogSeverity threshold) noexcept;
};

} // namespace ndde
