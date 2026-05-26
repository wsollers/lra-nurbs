#pragma once
// telemetry/TelemetryWriter.hpp
// Responsible only for I/O: CSV rows and the Markdown metadata stub.
// Has no knowledge of the ring buffer — it receives a span of records.
//
// Two output files per simulation run:
//   <stem>.csv      — primary records (one row per TelemetryRecord)
//   <stem>_ext.csv  — sparse pursuer/target extension records
//   <stem>.md       — human-readable run metadata and schema reference

#include "telemetry/TelemetryRecord.hpp"
#include "engine/events/SimEvent.hpp"
#include "math/Scalars.hpp"
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>

namespace ndde::telemetry {

struct RunMetadata {
    std::string           sim_name;
    u64                   sim_index      = u64(0);
    u64                   total_ticks    = u64(0);
    u64                   total_records  = u64(0);
    u64                   total_ext      = u64(0);
    u64                   dropped        = u64(0);
    f32                   total_sim_time = f32(0);
    std::string           started_at;    ///< ISO-8601 UTC string
    std::string           stopped_at;
    std::filesystem::path csv_path;
    std::filesystem::path ext_csv_path;
};

class TelemetryWriter {
public:
    TelemetryWriter() = default;
    ~TelemetryWriter() { close_streams(); }

    TelemetryWriter(const TelemetryWriter&)            = delete;
    TelemetryWriter& operator=(const TelemetryWriter&) = delete;
    TelemetryWriter(TelemetryWriter&&)                 = delete;
    TelemetryWriter& operator=(TelemetryWriter&&)      = delete;

    // ── Configuration ─────────────────────────────────────────────────────────

    /// Set the output directory. Created on first open() if absent.
    void set_output_dir(std::filesystem::path dir) {
        m_output_dir = std::move(dir);
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Open both CSV files and write their headers. Called at SimStarted.
    /// Returns the primary CSV path (for logging / EngineAPI exposure).
    [[nodiscard]] std::filesystem::path open(const events::SimStarted& event);

    /// Append a batch of primary records. Call from flush() or sim-stop.
    /// Thread-safety: single consumer only — no internal lock.
    void write_batch(std::span<const TelemetryRecord> records);

    /// Append a batch of extension records.
    void write_ext_batch(std::span<const TelemetryExtRecord> records);

    /// Flush, close both CSVs, and write the Markdown stub. Called at SimStopped.
    void close(const events::SimStopped& event, u64 dropped_count);

    [[nodiscard]] bool is_open() const noexcept { return m_csv.is_open(); }
    [[nodiscard]] const std::filesystem::path& current_csv_path() const noexcept {
        return m_meta.csv_path;
    }

private:
    std::filesystem::path m_output_dir{"telemetry"};
    RunMetadata           m_meta;
    std::ofstream         m_csv;
    std::ofstream         m_ext_csv;

    void close_streams();
    void write_markdown_stub(const RunMetadata& meta);

    [[nodiscard]] static std::string iso8601_now();
    [[nodiscard]] std::filesystem::path make_stem(std::string_view sim_name,
                                                   u64 index) const;
};

} // namespace ndde::telemetry
